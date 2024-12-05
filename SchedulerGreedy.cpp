//  THIS VERSION CONTAINS THE GREEDY ALGORITHM SCHEDULER AS DETAILED
//  IN THE SLIDES
//
//  Scheduler.cpp
//  CloudSim
//  
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//
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
static bool migrating = false;
static unsigned active_machines = -1;
static uint64_t total_tasks = 0;
static uint64_t tasks_completed = 0;
//tracks VMs on a physical machine. update when migrating or when creating new VMs
static unordered_map<MachineId_t, vector<VMId_t>> machine_to_VMs;
//maps VMs to their source/destination when migrating. update when starting
//migration and when migration completes.
static unordered_map<VMId_t, pair<MachineId_t, MachineId_t>> migrating_VMs;

//maps machines to tasks that are queued for that machine when it wakes up
//this is for the SLA violation routine when it looks for machines on standby.
static unordered_map<MachineId_t, vector<TaskId_t>> wakeup_migration;

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
 * @param machine_id the ID of the machine we want to calculate utilization for
 * @return a value indicating the overall utilization of the machine
 */
static double machine_util(MachineId_t machine_id){
    MachineInfo_t info = Machine_GetInfo(machine_id);
    return info.active_tasks;
}



/**
 * Compare machines based on relative utilization. In this case, utilization will
 * be the proportion of memory used.
 * @return true if A < B, false otherwise
 */
struct MachineUtilComparator{
    bool operator()(MachineId_t a, MachineId_t b) const
    {
        return machine_util(a) < machine_util(b);
    }
};   



/**
 * Runs on startup, initializes parameters/data structures
 */
void Scheduler::Init() {
    // Find the parameters of the clusters
    // Get the total number of machines
    // For each machine:
    //      Get the type of the machine
    //      Get the memory of the machine
    //      Get the number of CPUs
    //      Get if there is a GPU or not
    // 
    cout << "Greedy Scheduler!" << endl;
    SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    SimOutput("Scheduler::Init(): Initializing scheduler", 1);

    //initialize all machines
    active_machines = 0;
    for(unsigned i = 0; i < Machine_GetTotal(); i++) {
        MachineId_t machine_id = MachineId_t(i);
        machines.push_back(machine_id);
        MachineInfo_t machine_info = Machine_GetInfo(machine_id);
        //initialize mapping from machines to VMs to empty vectors
        machine_to_VMs[machine_id] = {};
        wakeup_migration[machine_id] = {};
        //turn on all machines for now
        Machine_SetState(machine_id, S0);
        active_machines++;
        //dump info
        // print_machine_info(machines[i]);
    }    
}



bool CPUCompatible(MachineId_t machine_id, TaskId_t task_id){
    return Machine_GetCPUType(machine_id) == GetTaskInfo(task_id).required_cpu;
}

bool TaskMemoryFits(MachineId_t machine_id, TaskId_t task_id){
    MachineInfo_t info = Machine_GetInfo(machine_id);
    return GetTaskMemory(task_id) + info.memory_used + VM_MEMORY_OVERHEAD <= info.memory_size;
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
 * Helper function that puts all inactive 'empty' machines to sleep, and removes
 * their IDs from the metadata structure 
 * a.k.a. vector<MachineId_t> Scheduler::machines
 * @param machines list of machines we search through and put to sleep before
 *                 removal from the list
 */
void TurnOffUnused(vector<MachineId_t> &machines){
    machines.erase(
        std::remove_if(machines.begin(), machines.end(), [](MachineId_t machine_id)
            {
                MachineInfo_t machine_info = Machine_GetInfo(machine_id);
                bool flag = machine_info.active_tasks == 0 
                                && machine_info.active_vms == 0;
                if(flag){
                    Machine_SetState(machine_id, S5);
                    active_machines--;
                }
                return flag;
            }
        ),
        machines.end()
    );

    //comment out for actual runs
    // cout << "machines after turning off unused:\n" << "[";
    // for(MachineId_t machine_id : machines){
    //     cout << machine_id << " ";
    // }
    // cout << "]" << endl;

}

/**
 * Runs whenever a new task is scheduled. This function operates according to
 * the greedy algorithm, which finds the 1st available machine to attach the
 * task to based on utilization.
 * @param now the time of the task
 * @param task_id the ID of the new task that we want to schedule
 * pre: Scheduler::machines contains only active machines
 */
void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    total_tasks++;
    const unsigned N = machines.size();
    unsigned j = 0;
    TaskInfo_t task_info = GetTaskInfo(task_id);
    vector<MachineId_t> candidates;
    while(j < N){
        MachineId_t machine_id = machines[j];
        MachineInfo_t machine_info = Machine_GetInfo(machine_id);
        if(CPUCompatible(machine_id, task_id) 
            && TaskMemoryFits(machine_id, task_id)
                && GPUCompatible(machine_id, task_id)){
            candidates.push_back(machine_id);
            break;
        }
        j++;
    }

    //unallocated workload = SLA violation
    if(candidates.size() == 0 || machine_util(candidates[0]) > .7){
        SLAWarning(now, task_id);
    } else{
        //find the right VM
        vector<VMId_t> machine_VMs = machine_to_VMs[candidates[0]];
        bool found = false;
        for(VMId_t vm : machine_VMs){
            VMInfo_t vm_info = VM_GetInfo(vm);
            if(task_info.required_vm == vm_info.vm_type){
                VM_AddTask(vm, task_id, task_info.priority);
                found = true;
                break;
            }
        }
        
        if(!found){
            VMId_t new_vm = VM_Create(task_info.required_vm, task_info.required_cpu);
            VM_Attach(new_vm, candidates[0]);
            machine_to_VMs[candidates[0]].push_back(new_vm);
            VM_AddTask(new_vm, task_id, task_info.priority);
        }
    }

    //turn unused machines off
    TurnOffUnused(machines);
    // cout << "Machine Size: " << machines.size() << endl;
}



/**
 * Return true if it is possible to migrate a VM to a given machine.
 * This is determined by memory/CPU requirements, as well as the fact that
 * we can't migrate to a sleeping machine. the VM cannot already be migrating.
 * @param vm_id the ID of the VM we want to migrate
 * @param machine_id the ID of the machine we want to migrate to
 * @return true if we can migrate, false otherwise.
 */
bool CanMigrate(VMId_t vm_id, MachineId_t machine_id){
    VMInfo_t vm_info = VM_GetInfo(vm_id);
    MachineInfo_t machine_info = Machine_GetInfo(machine_id);
    if(machine_info.s_state != S0 || vm_info.cpu != machine_info.cpu){
        return false;
    }
    unsigned total_vm_mem = VM_MEMORY_OVERHEAD;
    for(TaskId_t task : vm_info.active_tasks){
        total_vm_mem += GetTaskMemory(task);
    }
    return total_vm_mem + machine_info.memory_used < machine_info.memory_size;
}



/**
 * Runs whenever a task is completed. This is done according to the greedy
 * algorithm.
 * Do any bookkeeping necessary for the data structures
 * Decide if a machine is to be turned off, slowed down, or VMs to be migrated according to your policy
 * This is an opportunity to make any adjustments to optimize performance/energy
 */
void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    tasks_completed++;
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);
    std::sort(machines.begin(), machines.end(), MachineUtilComparator());
    for(unsigned j = 0; j < machines.size(); j++){
        MachineId_t src_pm = machines[j];
        MachineInfo_t src_info = Machine_GetInfo(src_pm);
        if(src_info.active_tasks > 0){
            //migrate workloads to more utilized machines if possible
            vector<VMId_t> src_VMs = machine_to_VMs[src_pm];
            unordered_set<VMId_t> vms_to_migrate;
            for(VMId_t src_VM : src_VMs){
                bool found_dest = false;
                MachineId_t dest = 0XDEADBEEF;
                unsigned k = j + 1;
                while(k < machines.size() && !found_dest){
                    dest = machines[k];
                    found_dest = CanMigrate(src_VM, dest);
                    k++;
                }

                if(found_dest){
                    vms_to_migrate.insert(src_VM);
                    migrating_VMs[src_VM] = {src_pm, dest};
                    cout << "starting migration" << endl;
                    VM_Migrate(src_VM, dest);
                }
            }

            vector<VMId_t>::iterator it = src_VMs.begin();
            while(it != src_VMs.end()){
                VMId_t vm = *it;
                if(vms_to_migrate.count(vm)){
                    //remove VM from source machine's mapping
                    it = src_VMs.erase(it);
                } else{
                    it++;
                }
            }
        }
    }
}



/**
 * Called by simulator when VM is done migrating due to previous call to 
 * VM_Migrate(). When this function is finished, the VM is established & can
 * take new tasks. 
 * When we're done migrating, check the source machine
 * to see if it's empty. If it is, put it in deep sleep. (This is esentially
 * part of the task completion algo for Greedy)
 * @param time the time when the migration has been completed
 * @param vm_id the identifier of the VM that was migrated
 */
void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    pair<MachineId_t, MachineId_t> src_dest = migrating_VMs[vm_id];
    MachineId_t src = src_dest.first;
    MachineId_t dest = src_dest.second;
    //done migrating, add to destination machine mapping
    machine_to_VMs[dest].push_back(vm_id);
    //put source to sleep if no more VMs/tasks left.
    MachineInfo_t src_info = Machine_GetInfo(src);
    if(src_info.active_tasks == 0 && src_info.active_vms == 0){
        Machine_SetState(src, S5);
        active_machines--;
    }

    //update metadata structures
    migrating_VMs.erase(vm_id);
    // cout << "Finished migrating VM " << vm_id << " at time " << time << endl;
}



// This method should be called from SchedulerCheck()
// SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
// Unlike the other invocations of the scheduler, this one doesn't report any specific event
// Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary 
// For the Greedy Algorithm, nothing is done.
void Scheduler::PeriodicCheck(Time_t now) {
    // cout << "total tasks: " << total_tasks << " completed tasks: " << tasks_completed << endl;
}

/**
 * Called just before simulation terminates. Shut down all VMs and machines, and
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
    for(auto & vm: vms) {
        VMInfo_t vm_info = VM_GetInfo(vm);
        if(vm_info.active_tasks.size() == 0 && migrating_VMs.count(vm) == 0)
            VM_Shutdown(vm);
    }
    //shut down all machines
    for(unsigned i = 0; i < Machine_GetTotal(); i++){
        MachineId_t machine_id = MachineId_t(i);
        Machine_SetState(machine_id, S5);
        active_machines--;
    }
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
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
    //run the SLA violation routine
    vector<VMId_t> machine_VMs = machine_to_VMs[machine_id];
    for(VMId_t vm : machine_VMs){
        VMInfo_t vm_info = VM_GetInfo(vm);
        if(vm_info.active_tasks.size() > 0){
            SLAWarning(time, vm_info.active_tasks[0]);
            return;
        }
    }
}




void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
    Scheduler.MigrationComplete(time, vm_id);
    migrating = false;
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
 * Greedy policy, we should migrate this task to another machine
 * @param task_id the ID of the task whose SLA has been violated
 * TODO: for some reason there is infinite looping sometimes and it gets stuck
 */
void SLAWarning(Time_t time, TaskId_t task_id) {
    //sort all machines in order of utilization
    vector<MachineId_t> machines;
    for(unsigned i =0; i < Machine_GetTotal(); i++){
        machines.push_back(MachineId_t(i));
    }

    std::sort(machines.begin(), machines.end(), MachineUtilComparator());

    MachineId_t dest;
    bool found;
    //find machine and VM that can accommodate the task
    for(unsigned i = 0; i < machines.size(); i++){
        MachineId_t potential_dest = machines[i];
        MachineInfo_t dest_info = Machine_GetInfo(potential_dest);
        if(CPUCompatible(potential_dest, task_id) 
                && TaskMemoryFits(potential_dest, task_id)){
            dest = potential_dest;
            found = true;
            break;
        }
    }

    //destination machine found. Now remove the task from where it
    //is currently, and add it to the destination machine
    if(found){
        MachineInfo_t dest_info = Machine_GetInfo(dest);
        if(dest_info.s_state == S0){
            //destination machine active, can migrate immediately
            TaskInfo_t task_info = GetTaskInfo(task_id);
            VMId_t new_vm = VM_Create(task_info.required_vm, dest_info.cpu);
            VM_Attach(new_vm, dest);
            machine_to_VMs[dest].push_back(new_vm);
            //TODO: how do we get the VM this thing is on?
            // VM_RemoveTask(src_vm_id, task_id);
            VM_AddTask(new_vm, task_id, task_info.priority);
        } else{
            //destination machine asleep, wake up and migrate in 
            //StateChangeComplete()

            //add tasks to queue for when it wakes up
            wakeup_migration[dest].push_back(task_id);
            Machine_SetState(dest, S0);
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
 * up from sleep, etc.) is complete
 * @param time the time the change completed
 * @param machine_id the ID of the machine whose state has changed
 */
void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    MachineInfo_t machine_info = Machine_GetInfo(machine_id);
    //only S0 S-State is not 'sleeping'
    if(machine_info.s_state == S0){
        //we only turn machines back on during an SLA warning
        //therefore we need to migrate some task to this machine
        //destination machine active, can migrate immediately
        vector<TaskId_t> tasks_to_migrate = wakeup_migration[machine_id];
        for(TaskId_t task_id : tasks_to_migrate){
            TaskInfo_t task_info = GetTaskInfo(task_id);
            VMId_t new_vm = VM_Create(task_info.required_vm, machine_info.cpu);
            VM_Attach(new_vm, machine_id);
            VM_AddTask(new_vm, task_id, task_info.priority);
            machine_to_VMs[machine_id].push_back(new_vm);
        }

        //tasks migrated, no need to add again later
        wakeup_migration[machine_id].clear();
        active_machines++;
    }
}



// BELOW THIS COMMENT ARE ALL HELPER FUNCTIONS

/**
 * Helper function to convert SLA requirements into priorities for 
 * scheduling. This function should change if needed based on the scheduler.
 * @param sla the SLA we want to convert
 * @return the equivalent task priority.
 */
static Priority_t sla_to_priority(SLAType_t sla){
    switch(sla){
        case SLA0:
            return HIGH_PRIORITY;
        case SLA1:
            return HIGH_PRIORITY;
        case SLA2:
            return MID_PRIORITY;
        case SLA3:
            return LOW_PRIORITY;
        default:
            break;
    }
    return MID_PRIORITY;
}


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