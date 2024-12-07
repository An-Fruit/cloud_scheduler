//Greedy Scheduler
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



//track migrating VMs
static unordered_set<VMId_t> migrating_VMs;
//keep track so we never power gate this
static unordered_set<MachineId_t> migration_destinations;

//track which machines are between states
static unordered_map<MachineId_t, bool> changing_state;
//maps PMs to tasks that are queued for that machine when it wakes up
//this is for the the manual SLA violation routine that is run
//by Scheduler::NewTask(). This is because no VMs have been created,
//so we need to create these when the Machine done setting state to S0
static vector<TaskId_t> wakeup_tasks;

//for the actual SLA routine, when it looks for PMs on standby to migrate to.
static vector<VMId_t> wakeup_migrations;

//tracks which PMs have not been put to sleep or ordered to put to sleep
static set<MachineId_t> awake;
static unordered_map<TaskId_t, VMId_t> task_to_vm;
//when we migrate, we must reserve memory to avoid overflow
static unordered_map<MachineId_t, unsigned> reserved_mem;

static Priority_t sla_to_priority(SLAType_t sla);
static void print_vm_info(VMId_t vm);
static void print_machine_info(MachineId_t machine);
static void print_task_info(TaskId_t task);
static string cpu_tostring(CPUType_t cpu);
static string vm_type_tostring(VMType_t vm);
static string pstate_tostring(CPUPerformance_t state);
static string sstate_tostring(MachineState_t state);
static string priority_tostring(Priority_t priority);
static string sla_tostring(SLAType_t sla);



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



/**
 * Runs on startup, initializes parameters/data structures
 */
void Scheduler::Init() {
    cout << "Greedy Scheduler!" << endl;
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
        // cout << "shutting down machine " << machine_id << endl;
        Machine_SetState(machine_id, S5);
        changing_state[machine_id] = true;
        // cout << "changing_state[" << machine_id << "] true" << endl;
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
    //TODO: change to prioritize awake PMs
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
                // cout << "[newtaskallocsla] request to turn on machine " << dest << endl;
                Machine_SetState(dest, S0);
                changing_state[dest] = true;
                // cout << "changing_state[" << dest << "] true" << endl;
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
 * Runs whenever a new task is scheduled. This function operates according to
 * the greedy algorithm, which finds the 1st available machine to attach the
 * task to based on utilization.
 * @param now the time of the task
 * @param task_id the ID of the new task that we want to schedule
 */
void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    total_tasks++;
    TaskInfo_t task_info = GetTaskInfo(task_id);
    bool found_machine = false;
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


    //unallocated workload = SLA violation
    if(!found_machine){
        // cout << "couldn't find machine on 1st pass in newtask" << endl;
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
 * Runs whenever a task is completed. This is done according to the greedy
 * algorithm. We try to move tasks from the PM that the completed task
 * was located on to more utilized PMs to consolidate.
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
    for(unsigned j = 0; j < this->machines.size(); j++){
        MachineId_t src_pm = this->machines[j];
        if(IsAwake(src_pm) && !changing_state[src_pm]
            && Machine_GetInfo(src_pm).active_vms > 0){
            
            //migrate workloads to more utilized machines if possible
            for(VMId_t src_VM : this->vms){
                VMInfo_t src_vm_info = VM_GetInfo(src_VM);
                for(unsigned k = j + 1; k < this->machines.size(); k++){
                    MachineId_t potential = this->machines[k];
                    if(IsAwake(potential) && !changing_state[potential]  && CanMigrateVM(src_VM, potential)){
                        migrating_VMs.insert(src_VM);
                        // cout << "[task complete] migrating VM " << src_VM << " to machine " << potential << endl;
                        unsigned needed_mem = VM_MEMORY_OVERHEAD;
                        for(TaskId_t task : src_vm_info.active_tasks){
                            needed_mem += GetTaskMemory(task);
                        }
                        // cout << "adding to reserved mem " << needed_mem << endl;
                        reserved_mem[potential] += needed_mem;
                        VM_Migrate(src_VM, potential);
                        //calculate the memory we need to reserve on the machine
                        migration_destinations.insert(potential);
                        break;
                    }
                }
            }
        }
    }
}



/**
 * Called by simulator when VM is done migrating due to previous migration
 * request. When this function is finished, the VM is established & can
 * take new tasks. 
 * When we're done migrating, check the source machine
 * to see if it's empty. If it is, put it in deep sleep. (This is esentially
 * part of the task completion algo for Greedy)
 * @param time the time when the migration has been completed
 * @param vm_id the identifier of the VM that was migrated
 */
void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // cout << "fin moving VM " << vm_id << " to machine " << VM_GetInfo(vm_id).machine_id << endl;
    //update metadata structures
    VMInfo_t vm_info = VM_GetInfo(vm_id);

    MachineId_t dest_loc = vm_info.machine_id;
    unsigned vm_mem = 0;
    for(TaskId_t task : vm_info.active_tasks){
        vm_mem += GetTaskMemory(task);
    }

    // cout << "removing from reserved mem " << vm_mem << endl;
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



// This method should be called from SchedulerCheck()
// SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
// Unlike the other invocations of the scheduler, this one doesn't report any specific event
// Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary 
// For the Greedy Algorithm, nothing is done.
void Scheduler::PeriodicCheck(Time_t now) {
    // cout << "total tasks: " << total_tasks << " completed tasks: " << tasks_completed << " time: " << now << endl;
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
 * Runs whenever the SLA on the given task is violated. According to the
 * Greedy policy, we should migrate this task to another machine.
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
                // cout << "[sla warning] migrating VM " << vm_to_migrate << " to machine " << dest << endl;
                unsigned needed_mem = VM_MEMORY_OVERHEAD;
                for(TaskId_t task : src_vm_info.active_tasks){
                    needed_mem += GetTaskMemory(task);
                }
                // cout << "adding to reserved mem " << needed_mem << endl;
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
                // cout << "[sla warning] request to turn on machine " << dest << endl;
                Machine_SetState(dest, S0);
                changing_state[dest] = true;
                // cout << "changing[" << dest << "] true" << endl;
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
    // cout << "changing_state[" << machine_id << "] false" << endl;
    //just updated to awake state
    if(machine_info.s_state == S0){
        // cout << "machine " << machine_id << " awake" << endl;
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
                // cout << "[state change fin] migrating VM " << vm_id << " to machine " << machine_id << endl;
                unsigned needed_mem = VM_MEMORY_OVERHEAD;
                for(TaskId_t task : vm_info.active_tasks){
                    needed_mem += GetTaskMemory(task);
                }
                // cout << "adding to reserved mem " << needed_mem << endl;
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
        // cout << "machine " << machine_id << " fully down" << endl;
    }
}

  

// BELOW THIS COMMENT ARE ALL HELPER FUNCTIONS THAT PRINT STUFF


/**
 * Helper function to print all information about a given task
 * @param task the ID of the task we want info about
 * pre: none
 * post: print relevant info about the task
 */
static void print_task_info(TaskId_t task)
{
    TaskInfo_t inf = GetTaskInfo(task);
    printf("------------TASK INFO FOR ID [%u]-------------\n", task);
    //completion status
    printf("TASK %s\n", inf.completed ? "COMPLETED" : "UNFINISHED");
    printf ("total instructions: %lu\n remaining: %lu\n", 
                inf.total_instructions, inf.remaining_instructions);
    //arrival/completion times
    printf("time arrived: %lu\n", inf.arrival);
    if(inf.completed){
        printf("time completed: %lu\n", inf.completion);
    }
    printf("expected completion time: %lu\n", inf.target_completion);
    //resources (gpu, memory, cpu, VM)
    printf(inf.gpu_capable ? "GPU BOOST\n" : "NO GPU BOOST\n");
    printf("priority: %s\n", priority_tostring(inf.priority).c_str());
    printf("CPU type required: %s\n", cpu_tostring(inf.required_cpu).c_str());
    printf("Memory required: %u\n", inf.required_memory);
    printf("VM required: %s\n", vm_type_tostring(inf.required_vm).c_str());
    printf("SLA required: %s\n", sla_tostring(inf.required_sla).c_str());
    printf("-----------------------------------------------------\n");
}



/**
 * Helper function to print all information about a physical machine.
 * @param machine the id of the machine we want to print info about
 * pre: none
 * post: print out all auxiliary info about the machine
 */
static void print_machine_info(MachineId_t machine)
{
    MachineInfo_t inf = Machine_GetInfo(machine);
    printf("------------MACHINE INFO FOR ID [%u]------------\n", machine);
    //general machine info
    printf("S-State: %s\n", sstate_tostring(inf.s_state).c_str());
    printf("Total energy consumed: %lu\n", inf.energy_consumed);
    //cpu info
    printf("# of CPUs: %u\n", inf.num_cpus);
    printf("CPU type: %s\n", cpu_tostring(inf.cpu).c_str());
    printf("CPU P State: %s\n", pstate_tostring(inf.p_state).c_str());
    //memory info
    printf("Amt of memory: %u\n", inf.memory_size);
    printf("Memory in use: %u\n", inf.memory_used);
    printf("Memory reserved: %u\n", reserved_mem[machine]);
    //GPU 
    printf("GPU %s\n", inf.gpus ? "ENABLED" : "DISABLED");
    //tasks/VMs
    printf("# active tasks: %u\n # active VMs: %u\n",
                 inf.active_tasks, inf.active_vms);
    printf("-----------------------------------------------\n");
}



/**
 * Helper function to translate SLA enum to string
 * @param sla the SLA requirement we want to translate
 * @return the string equivalent of sla
 */
static string sla_tostring(SLAType_t sla)
{
    string ret = "UNDEFINED SLA REQUIREMENT";
    switch(sla)
    {
        case SLA0:
            ret = "SLA0";
            break;
        case SLA1:
            ret = "SLA1";
            break;
        case SLA2:
            ret = "SLA2";
            break;
        case SLA3:
            ret = "SLA3";
            break;
        default:
            break;
    }
    return ret;
}



/**
 * Helper function to translate priority enum to string
 * @param priority the enum that we want to translate to a string
 * @return the string representation of the enum
 */
static string priority_tostring(Priority_t priority)
{
    string ret = "UNDEFINED TASK PRIORITY";
    switch(priority)
    {
        case HIGH_PRIORITY:
            ret = "HIGH PRIORITY";
            break;
        case MID_PRIORITY:
            ret = "MEDIUM PRIORITY";
            break;
        case LOW_PRIORITY:
            ret = "LOW PRIORITY";
            break;
        default:
            break;
    }
    return ret;
}



/**
 * Helper function to translate machine S-state enum into a string
 * @param state the machine s-state
 * @return the string equivalent of state
 */
static string sstate_tostring(MachineState_t state)
{
    string ret = "UNDEFINED MACHINE S-STATE";
    switch(state)
    {
        case S0:
            ret = "S0";
            break;
        case S0i1:
            ret = "S0i1";
            break;
        case S1:
            ret = "S1";
            break;
        case S2:
            ret = "S2";
            break;
        case S3:
            ret = "S3";
            break;
        case S4:
            ret = "S4";
            break;
        case S5:
            ret = "S5";
            break;
        default:
            break;
    }
    return ret;
}



/**
 * Helper function to translate CPU p-state enum to a string
 * @param state the p-state we want to translate into a string
 * @return  C++ STL string containing the p-state
 */
static string pstate_tostring(CPUPerformance_t state)
{
    string ret = "UNDEFINED CPU P-STATE";
    switch(state)
    {
        case P0:
            ret = "P0";
            break;
        case P1:
            ret = "P1";
            break;
        case P2:
            ret = "P2";
            break;
        case P3:
            ret = "P3";
            break;
        default:
            break;
    }
    return ret;
}



/**
 * Helper function to print all information about a virtual machine.
 * @param vm the id of the VM we want to print info about
 * pre: none
 * post: print out all auxiliary info about the VM
 */
static void print_vm_info (VMId_t vm)
{
    VMInfo_t inf = VM_GetInfo(vm);
    printf ("------------VM Info for ID [%u]------------\n", vm);
    //print all active tasks
    const int N = inf.active_tasks.size();
    printf("# active tasks: %d\n", N);
    printf("tasks: [");
    if(N > 0){
        printf("%u", inf.active_tasks[0]);
    }
    for(int i = 1; i < N; i++){
        printf(", %u", inf.active_tasks[i]);
    }
    printf("]\n");

    //print cpu type
    printf("CPU type: %s\n", cpu_tostring(inf.cpu).c_str());

    //machine ID
    printf("Machine ID: %u\n", inf.machine_id);

    //vm type
    printf("VM Type: %s\n", vm_type_tostring(inf.vm_type).c_str());
    printf ("------------------------------------\n");
}



/**
 * Helper function that returns the string form of the VM type enum
 * @param vm the enum we want to convert to a string
 * @return a C++ STL string representing the VM type
 */
static string vm_type_tostring(VMType_t vm)
{
    string ret = "UNDEFINED VM TYPE";
    switch(vm){
        case LINUX:
            ret = "LINUX";
            break;
        case LINUX_RT:
            ret = "LINUX_RT";
            break;
        case WIN:
            ret = "WINDOWS";
            break;
        case AIX:
            ret = "AIX";
            break;
        default:
            break;
    }
    return ret;
}



/**
 * Helper function that returns the string literal form of the cpu enum
 * @param cpu the type of CPU we want to return for
 * @return a C++ STL string representing the CPU type
 */
static string cpu_tostring (CPUType_t cpu)
{
    string ret = "UNDEFINED CPU TYPE";
    switch (cpu){
        case ARM:
            ret  = "ARM";
            break;
        
        case POWER:
            ret = "POWER";
            break;
        
        case RISCV:
            ret = "RISCV";
            break;
        
        case X86:
            ret = "X86";
            break;
        
        default:
            break;
    }
    return ret;
}