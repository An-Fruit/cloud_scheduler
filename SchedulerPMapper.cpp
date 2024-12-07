//PMapper Scheduler
#include "Scheduler.hpp"
#include <assert.h>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <stdexcept>

static Scheduler Scheduler;
static uint64_t total_tasks = 0;
static uint64_t tasks_completed = 0;



//tracks migrating VMs and destinations
static unordered_set<VMId_t> migrating_VMs;
static unordered_set<MachineId_t> migration_destinations;

//track if state inconsistent
static unordered_map<MachineId_t, bool> changing_state;

//event queue
static vector<TaskId_t> wakeup_tasks;
static vector<VMId_t> wakeup_migrations;

static set<MachineId_t> awake;
static unordered_map<TaskId_t, VMId_t> task_to_vm;
static unordered_map<MachineId_t, unsigned> reserved_mem;



/**
 * Compare PMs based on relative utilization. In this case, utilization will
 * be the proportion of memory used.
 * @return true if A < B, false otherwise
 */
struct MachineUtilComparator{
    bool operator()(MachineId_t a, MachineId_t b) const
    {
        return Machine_GetInfo(a).active_tasks < Machine_GetInfo(b).active_tasks;
    }
};


struct MachineEnergyComparator{
    bool operator()(MachineId_t a, MachineId_t b) const
    {
        return Machine_GetInfo(a).energy_consumed < Machine_GetInfo(b).energy_consumed;
    }
};

/**
 * Runs on startup, initializes parameters/data structures
 */
void Scheduler::Init() {
    cout << "PMapper Scheduler!" << endl;
    SimOutput("Scheduler::Init(): Total number of PMs is " + to_string(Machine_GetTotal()), 3);
    SimOutput("Scheduler::Init(): Initializing scheduler", 1);

    //initialize all PMs
    for(unsigned i = 0; i < Machine_GetTotal(); i++) {
        MachineId_t machine_id = MachineId_t(i);
        this->machines.push_back(machine_id);
        MachineInfo_t machine_info = Machine_GetInfo(machine_id);
        //queue empty initially
        awake.insert(machine_id);
        changing_state[machine_id] = false;
        //dump info
        // print_machine_info(this->machines[i]);
    }
}

static bool IsMigrating(VMId_t vm_id){
    return migrating_VMs.count(vm_id) > 0;
}

bool CPUCompatible(MachineId_t machine_id, TaskId_t task_id){
    return Machine_GetCPUType(machine_id) == GetTaskInfo(task_id).required_cpu;
}

bool TaskMemoryFits(MachineId_t machine_id, TaskId_t task_id){
    MachineInfo_t info = Machine_GetInfo(machine_id);
    return GetTaskMemory(task_id) + info.memory_used + VM_MEMORY_OVERHEAD <= info.memory_size;
}

bool IsAwake(MachineId_t machine){
    return awake.count(machine) > 0;
}

/**
 * Helper function, returns true if a task and machine are gpu compatible (i.e.
 * it needs the gpu and the machine has a GPU)
 */
bool GPUCompatible(MachineId_t machine_id, TaskId_t task_id){
    MachineInfo_t machine_info = Machine_GetInfo(machine_id);
    TaskInfo_t task_info = GetTaskInfo(task_id);
    return !task_info.gpu_capable || (task_info.gpu_capable && machine_info.gpus);
}

/**
 * Helper method to try to shutdown a machine given its ID.
 * Updates all relevant metadata structures, and will only shut down
 * if it has no active VMs, no VMs migrating to it, and no VMs that are
 * migrating from it.
 * @param machine_id the machine we are trying to shut down
 * @return true if the machine got shut down, false otherwise
 */
bool Scheduler::TryShutdown(MachineId_t machine_id){
    //make sure nobody is migrating to this VM
    if(migration_destinations.count(machine_id) > 0 || !IsAwake(machine_id) || changing_state[machine_id]){
        return false;
    }
    MachineInfo_t machine_info = Machine_GetInfo(machine_id);
    //make sure we don't have active VMs
    bool safe_shutdown = machine_info.active_vms == 0;
    //make sure we dono't have migrating VMs
    if(safe_shutdown){
        for(VMId_t vm : this->vms){
            if(IsMigrating(vm)){
                safe_shutdown = false;
                break;
            }
        }
    }
    if(safe_shutdown){
        awake.erase(awake.find(machine_id));
        Machine_SetState(machine_id, S5);
        changing_state[machine_id] = true;
        return true;
    }
    return false;
}


static void NewTaskAllocationSLA(TaskId_t task_id){
     //sort all PMs in order of utilization

    std::sort(Scheduler.machines.begin(), Scheduler.machines.end(), MachineUtilComparator());

    MachineId_t dest = 0XDEADBEEF;
    bool found = false;
    //find machine and VM that can accommodate the task
    //note: some of these PMs can be sleeping or shut down
    for(unsigned i = 0; i < Scheduler.machines.size(); i++){
        MachineId_t potential_dest = Scheduler.machines[i];
        MachineInfo_t dest_info = Machine_GetInfo(potential_dest);
        if(CPUCompatible(potential_dest, task_id) 
                && TaskMemoryFits(potential_dest, task_id)){
            dest = potential_dest;
            found = true;
            break;
        }
    }

    //destination machine found. migrate the task there.
    if(found){
        MachineInfo_t dest_info = Machine_GetInfo(dest);
        if(IsAwake(dest) && !changing_state[dest]){
            //Since this happens with a new task, we don't migrate.
            //Instead, we create a new VM
            TaskInfo_t task_info = GetTaskInfo(task_id);
            VMId_t new_vm = VM_Create(task_info.required_vm, dest_info.cpu);
            Scheduler.vms.push_back(new_vm);
            VM_Attach(new_vm, dest);
            VM_AddTask(new_vm, task_id, task_info.priority);
            task_to_vm[task_id] = new_vm;
        } else{
            //we couldn't find an awake machine, put it on the queue
            //and when a machine wakes up, it will try to allocate it
            wakeup_tasks.push_back(task_id);
            //try to wake up machine if possible
            if(!changing_state[dest]){
                Machine_SetState(dest, S0);
                changing_state[dest] = true;
            }
        }
    } else{
        //failure case: no destination machine w/ compatible CPUs and enough
        //memory.
        throw std::runtime_error("Unable to find machine to migrate task " 
                    + to_string(task_id) + " to after SLA Violation");
    }
}


/**
 * Runs whenever a new task is scheduled. 
 * @param now the time of the task
 * @param task_id the ID of the new task that we want to schedule
 */
void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    total_tasks++;
    TaskInfo_t task_info = GetTaskInfo(task_id);
    bool found_machine = false;
    sort(this->machines.begin(), this->machines.end(), MachineEnergyComparator());
    //1st pass: awake machines
    for(MachineId_t machine_id : this->machines){
        MachineInfo_t machine_info = Machine_GetInfo(machine_id);
        //make sure machine is awake and meets requirements
        if(CPUCompatible(machine_id, task_id) 
            && TaskMemoryFits(machine_id, task_id)
                && GPUCompatible(machine_id, task_id)
                    && IsAwake(machine_id) && !changing_state[machine_id]){
            VMId_t new_vm = VM_Create(task_info.required_vm, task_info.required_cpu);
            this->vms.push_back(new_vm);
            VM_Attach(new_vm, machine_id);
            VM_AddTask(new_vm, task_id, task_info.priority);
            task_to_vm[task_id] = new_vm;
            found_machine = true;
            break;
        }
    }


    if(!found_machine){
        NewTaskAllocationSLA(task_id);
    } else{
        //turn unused PMs off
        for(MachineId_t machine_id : this->machines){
            TryShutdown(machine_id);
        }
    }
}



/**
 * Return true if it is possible to migrate a VM to a given machine.
 * This is determined by memory/CPU requirements, as well as the fact that
 * we can't migrate to a sleeping machine. the VM cannot already be migrating.
 * @param vm_id the ID of the VM we want to migrate
 * @param machine_id the ID of the machine we want to migrate to
 * @return true if we can migrate, false otherwise.
 */
static bool CanMigrateVM(VMId_t vm_id, MachineId_t machine_id){
    VMInfo_t vm_info = VM_GetInfo(vm_id);
    MachineInfo_t machine_info = Machine_GetInfo(machine_id);
    if(changing_state[machine_id] || !IsAwake(machine_id)
        || vm_info.cpu != machine_info.cpu || IsMigrating(vm_id)){
        return false;
    }
    unsigned total_vm_mem = VM_MEMORY_OVERHEAD;
    for(TaskId_t task : vm_info.active_tasks){
        total_vm_mem += GetTaskMemory(task);
    }
    return total_vm_mem + machine_info.memory_used + reserved_mem[machine_id] < machine_info.memory_size;
}



/**
 * Runs whenever a task is completed. 
 */
void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    tasks_completed++;
    //shut down the VM that task_id is located in. 
    VMId_t task_vm = task_to_vm[task_id];
    if(IsMigrating(task_vm)){
        return;
    }
    VM_Shutdown(task_vm);
    this->vms.erase(remove(this->vms.begin(), this->vms.end(), task_vm), this->vms.end());
    
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);
    std::sort(this->machines.begin(), this->machines.end(), MachineUtilComparator());
    //get the smallest VM from the least utilized machine

    //find start of utilized machines
    unsigned i = 0;
    for(; i < this->machines.size(); i++){
        if(Machine_GetInfo(machines[i]).active_tasks > 0){
            break;
        }
    }
    MachineId_t lowest_util_machine = machines[i];
    //find least utilized VM on this machine that is not migrating or 
    VMId_t smallest_vm = 0XDEADBEEF;
    unsigned lowest_util = UINT32_MAX;
    for(VMId_t vm : this->vms){
        VMInfo_t vm_info = VM_GetInfo(vm);
        if(vm_info.machine_id == lowest_util_machine && !IsMigrating(vm)){
            if(vm_info.active_tasks.size() < lowest_util){
                lowest_util = vm_info.active_tasks.size();
                smallest_vm = vm;
            }
        }
    }

    if(smallest_vm != 0XDEADBEEF){
        //get 2nd half of machines (more utilized machines, and migrate there)
        VMInfo_t smallest_info = VM_GetInfo(smallest_vm);
        unsigned mid = (i + this->machines.size())/2;
        for(;mid < this->machines.size(); mid++){
            MachineId_t potential = this->machines[mid];
            if(IsAwake(potential) && !changing_state[potential]  && CanMigrateVM(smallest_vm, potential)){
                migrating_VMs.insert(smallest_vm);
                unsigned needed_mem = VM_MEMORY_OVERHEAD;
                for(TaskId_t task : smallest_info.active_tasks){
                    needed_mem += GetTaskMemory(task);
                }
                reserved_mem[potential] += needed_mem;
                VM_Migrate(smallest_vm, potential);
                //calculate the memory we need to reserve on the machine
                migration_destinations.insert(potential);
                break;
            }
        }
    }
}



/**
 * @param time the time when the migration has been completed
 * @param vm_id the identifier of the VM that was migrated
 */
void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    //update metadata structures
    VMInfo_t vm_info = VM_GetInfo(vm_id);

    MachineId_t dest_loc = vm_info.machine_id;
    unsigned vm_mem = 0;
    for(TaskId_t task : vm_info.active_tasks){
        vm_mem += GetTaskMemory(task);
    }

    reserved_mem[dest_loc] -= vm_mem;
    migration_destinations.erase(dest_loc);
    migrating_VMs.erase(vm_id);

    //the task might have completed while the VM was migrating. If this is the
    //case, we shut down here when we're done
    if(vm_info.active_tasks.size() == 0){
        VM_Shutdown(vm_id);
        this->vms.erase(remove(this->vms.begin(), this->vms.end(), vm_id), this->vms.end());
    }
}



void Scheduler::PeriodicCheck(Time_t now) {

}

/**
 * Called just before simulation terminates. Shut down all VMs and PMs, and
 * give a report on SLA violations and energy consumed.
 */
void Scheduler::Shutdown(Time_t time) {
    // Do your final reporting and bookkeeping here.
    // Report about the total energy consumed
    // Report about the SLA compliance
    // Shutdown everything to be tidy :-)
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;     // SLA3 do not have SLA violation issues
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
    SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 4);
    cout << "total tasks: " << total_tasks << " completed tasks: " << tasks_completed << endl;

    //shut down all VMs
    for(auto & vm: this->vms) {
        VMInfo_t vm_info = VM_GetInfo(vm);
        if(vm_info.active_tasks.size() == 0 && !IsMigrating(vm))
            VM_Shutdown(vm);
    }
    this->vms.clear();

    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}




// Public interface below

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 4);
    Scheduler.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 4);
    Scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 4);
    Scheduler.TaskComplete(time, task_id);
}



/**
 * Runs when memory on a machine is overcommitted
 * @param time the time of the warning
 * @param machine_id the ID of the machine whose memory is overcommitted
 */
void MemoryWarning(Time_t time, MachineId_t machine_id) {
    SimOutput("MemoryWarning(): Overflow at machine " + to_string(machine_id) + " was detected at time " + to_string(time), 1);
    //run the SLA violation routine on one of the tasks on the machine
    
    //try to find least utilized machine that is awake that can acommodate

    // for(VMId_t vm : Scheduler.vms){
    //     VMInfo_t vm_info = VM_GetInfo(vm);
    //     if(vm_info.machine_id == machine_id && vm_info.active_tasks.size() > 0){
    //         SLAWarning(time, vm_info.active_tasks[0]);
    //     }
    // }
}




void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
    Scheduler.MigrationComplete(time, vm_id);
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 5);
    Scheduler.PeriodicCheck(time);
}

void SimulationComplete(Time_t time) {
    Scheduler.Shutdown(time);
}



/**
 * Runs whenever the SLA on the given task is violated.
 * @param task_id the ID of the task whose SLA has been violated
 */
void SLAWarning(Time_t time, TaskId_t task_id) {
    //sort all PMs in order of utilization

    std::sort(Scheduler.machines.begin(), Scheduler.machines.end(), MachineUtilComparator());

    MachineId_t dest = 0XDEADBEEF;
    bool found = false;
    //find machine and VM that can accommodate the task
    //note: some of these PMs can be sleeping or shut down
    for(unsigned i = 0; i < Scheduler.machines.size(); i++){
        MachineId_t potential_dest = Scheduler.machines[i];
        MachineInfo_t dest_info = Machine_GetInfo(potential_dest);
        if(CPUCompatible(potential_dest, task_id) 
                && TaskMemoryFits(potential_dest, task_id)){
            dest = potential_dest;
            found = true;
            break;
        }
    }

    //destination machine found. migrate the task there.
    if(found){
        MachineInfo_t dest_info = Machine_GetInfo(dest);
        VMId_t vm_to_migrate = task_to_vm[task_id];


        if(IsAwake(dest) && !changing_state[dest]){
            //destination machine active, can migrate immediately
            //update migration mapping
            if(CanMigrateVM(vm_to_migrate, dest)){
                VMInfo_t src_vm_info = VM_GetInfo(vm_to_migrate);
                migrating_VMs.insert(vm_to_migrate);
                unsigned needed_mem = VM_MEMORY_OVERHEAD;
                for(TaskId_t task : src_vm_info.active_tasks){
                    needed_mem += GetTaskMemory(task);
                }
                reserved_mem[dest] += needed_mem;
                VM_Migrate(vm_to_migrate, dest);
                migration_destinations.insert(dest);
            }
        } else{
            //no awake machines, try to put one on standby
            //and put it in queue
            //assigning to a machine is handled when a machine wakes up
            //NOTE: some of these could have already finished by the time
            //the machine wakes up. What to do in that case?
            wakeup_migrations.push_back(vm_to_migrate);
            if(!changing_state[dest]){
                Machine_SetState(dest, S0);
                changing_state[dest] = true;
            }
        }
    } else{
        //failure case: no destination machine w/ compatible CPUs and enough
        //memory.
        throw std::runtime_error("Unable to find machine to migrate task " 
                    + to_string(task_id) + " to after SLA Violation");
    }
}


/**
 * Runs whenever a S-State change request (i.e. to shut down a machine, wake
 * up from sleep, etc.) is complete. This means that the info returned in
 * Machine_GetInfo should(?) be fully accurate once again.
 * @param time the time the change completed
 * @param machine_id the ID of the machine whose state has changed
 */
void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    MachineInfo_t machine_info = Machine_GetInfo(machine_id);
    changing_state[machine_id] = false;
    //just updated to awake state
    if(machine_info.s_state == S0){
        awake.insert(machine_id);

        //add all the tasks that were waiting to be moved to this machine
        //this should be from NewTaskAllocationSLA()
        vector<TaskId_t>::iterator wakeup_task_it = wakeup_tasks.begin();
        while(wakeup_task_it != wakeup_tasks.end()){
            TaskId_t task_id = *wakeup_task_it;
            TaskInfo_t task_info = GetTaskInfo(task_id);
            if(CPUCompatible(machine_id, task_id) && TaskMemoryFits(machine_id, task_id)){
                VMId_t new_vm = VM_Create(task_info.required_vm, machine_info.cpu);
                Scheduler.vms.push_back(new_vm);
                VM_Attach(new_vm, machine_id);
                VM_AddTask(new_vm, task_id, task_info.priority);
                task_to_vm[task_id] = new_vm;

                //added task, remove from queue
                wakeup_task_it = wakeup_tasks.erase(wakeup_task_it);
            } else{
                wakeup_task_it++;
            }
        }

        //migrate all VMs that needed to be migrated
        //NOTE: some of these could have shut down between when they were added
        //      to the queue and the present
        vector<TaskId_t>::iterator wakeup_vm_it = wakeup_migrations.begin();
        while(wakeup_vm_it != wakeup_migrations.end()){
            VMId_t vm_id = *wakeup_vm_it;
            VMInfo_t vm_info = VM_GetInfo(vm_id);
            //see if we have enough memory to migrate there
            if(CanMigrateVM(vm_id, machine_id)){
                unsigned needed_mem = VM_MEMORY_OVERHEAD;
                for(TaskId_t task : vm_info.active_tasks){
                    needed_mem += GetTaskMemory(task);
                }
                reserved_mem[machine_id] += needed_mem;
                VM_Migrate(vm_id, machine_id);
                migration_destinations.insert(machine_id);
                //migrated VM, remove from queue
                wakeup_vm_it = wakeup_migrations.erase(wakeup_vm_it);
            } else{
                wakeup_vm_it++;
            }
        }
    } else{
        //this can happen. 
        if(awake.count(machine_id) > 0){
            awake.erase(awake.find(machine_id));
        }
    }
}