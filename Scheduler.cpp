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
#include <set>
#include <algorithm>

static bool migrating = false;
static unsigned active_machines = -1;
static unordered_map<MachineId_t, vector<VMId_t>> machine_to_VMs;
static unordered_map<TaskId_t, VMId_t> task_to_VM;

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
 * Compare machines based on relative utilization. In this case, utilization will
 * be the proportion of memory used.
 * @return true if A < B, false otherwise
 */
struct MachineUtilComparator{
    bool operator()(MachineId_t a, MachineId_t b) const
    {
        MachineInfo_t a_inf = Machine_GetInfo(a);
        MachineInfo_t b_inf = Machine_GetInfo(b);
        float a_util = static_cast<float>(a_inf.memory_used)/static_cast<float>(a_inf.memory_size);
        float b_util = static_cast<float>(b_inf.memory_used)/static_cast<float>(b_inf.memory_size);
        return a_util < b_util;
    }
};



void Scheduler::Init() {
    // Find the parameters of the clusters
    // Get the total number of machines
    // For each machine:
    //      Get the type of the machine
    //      Get the memory of the machine
    //      Get the number of CPUs
    //      Get if there is a GPU or not
    // 
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
        //turn on all machines for now
        Machine_SetState(machine_id, S0);
        active_machines++;
        //dump info
        // print_machine_info(machines[i]);
    }    
}

/**
 * Called by simulator when VM is done migrating due to previous call to 
 * VM_Migrate(). When this function is finished, the VM is established & can
 * take new tasks. 
 * @param time the time when the migration has been completed
 * @param vm_id the identifier of the VM that was migrated
 */
void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    
    //update the mappings from machines to VMs by adding this VM to its new
    //machine
    VMInfo_t vm_info = VM_GetInfo(vm_id);
    MachineId_t new_machine = vm_info.machine_id;
    machine_to_VMs[new_machine].push_back(vm_id);
    
    

    migrating = false;
}



/**
 * Runs whenever a new task is scheduled. This function operates according to
 * the greedy algorithm, which finds the 1st available machine to attach the
 * task to.
 * @param now the time of the task
 * @param task_id the ID of the new task that we want to schedule
 */
void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    TaskInfo_t task_info = GetTaskInfo(task_id);
    Priority_t task_priority = sla_to_priority(task_info.required_sla);

    vector<MachineId_t> candidates;
    for(unsigned j = 0; j < active_machines; j++){
        //get load factor of cur machine (a.k.a. memory use)
        MachineId_t cur_machine = machines[j];
        MachineInfo_t machine_info = Machine_GetInfo(cur_machine);
        unsigned machine_load = machine_info.memory_used;
        unsigned machine_capacity = machine_info.memory_size;
        unsigned task_load = task_info.required_memory;
        //check for matching CPU and capacity
        if(machine_info.cpu == task_info.required_cpu 
                && machine_load + task_load <= machine_capacity){
            //prioritize matching GPUs, add immediately & finish
            if(machine_info.gpus == task_info.gpu_capable){
                    
                //find 1st available VM and add task
                vector<VMId_t> cur_machine_vms = machine_to_VMs[cur_machine];
                unsigned N = cur_machine_vms.size();
                for(unsigned k = 0; k < N; k++){
                    VMInfo_t vm_info = VM_GetInfo(cur_machine_vms[k]);
                    if(vm_info.vm_type == task_info.required_vm){
                        VM_AddTask(cur_machine_vms[k], task_id, task_priority);
                        task_to_VM[task_id] = cur_machine_vms[k];
                        return;
                    }
                }
                
                //no available VMs, create one & add task
                VMId_t new_vm = VM_Create(task_info.required_vm, machine_info.cpu);
                VM_Attach(new_vm, cur_machine);
                machine_to_VMs[cur_machine].push_back(new_vm);
                vms.push_back(new_vm);
                VM_AddTask(new_vm, task_id, task_priority);
                task_to_VM[task_id] = new_vm;
                return;
            } else{
                //this is still a potential machine to use
                candidates.push_back(cur_machine);
            }
        }
    }
    
    //first pass failed to find best option, now just find the 1st
    //machine w/ a VM that works
    for(unsigned i = 0; i < candidates.size(); i++){
        MachineId_t cur_machine = candidates[i];
        vector<VMId_t> cur_vms = machine_to_VMs[cur_machine];
        for(unsigned j = 0; j < cur_vms.size(); j++){
            VMInfo_t vm_info = VM_GetInfo(cur_vms[j]);
            if(vm_info.vm_type == task_info.required_vm){
                VM_AddTask(cur_vms[j], task_id, task_priority);
                task_to_VM[task_id] = cur_vms[j];
                return;
            }
        }

        //no valid VMs, must create one
        VMId_t new_vm = VM_Create(task_info.required_vm, Machine_GetCPUType(cur_machine));
        VM_Attach(new_vm, cur_machine);
        machine_to_VMs[cur_machine].push_back(new_vm);
        vms.push_back(new_vm);
        VM_AddTask(new_vm, task_id, task_priority);
        task_to_VM[task_id] = new_vm;
        return;
    }
    //should never be unable to add a task to a machine
    assert(false);  //TODO: remove this
}

/**Greedy algorithm does nothing with this*/
void Scheduler::PeriodicCheck(Time_t now) {
    // This method should be called from SchedulerCheck()
    // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
    // Unlike the other invocations of the scheduler, this one doesn't report any specific event
    // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary 
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

    //shut down all VMs
    //TODO: an exception is occurring here. Why?
    for(auto & vm: vms) {
        VM_Shutdown(vm);
    }
    //shut down all machines
    for(unsigned i = 0; i < Machine_GetTotal(); i++){
        MachineId_t machine_id = MachineId_t(i);
        Machine_SetState(machine_id, S5);
    }
    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}



/**
 * Runs whenever a task is completed. This is done according to the greedy
 * algorithm.
 */
void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    // Do any bookkeeping necessary for the data structures
    // Decide if a machine is to be turned off, slowed down, or VMs to be migrated according to your policy
    // This is an opportunity to make any adjustments to optimize performance/energy
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);

    //sort machines in ascending order based on utilization
    std::sort(machines.begin(), machines.end(), MachineUtilComparator());
    for(unsigned j = 0; j < machines.size(); j++){
        print_machine_info(machines[j]);
    }

    //move workloads
    for(unsigned j = 0; j < machines.size(); j++){
        MachineId_t cur_machine_id = machines[j];
        MachineInfo_t machine_info = Machine_GetInfo(cur_machine_id);
        //don't bother w/ machines where utilization is 0
        if(machine_info.memory_used == 0){
            continue;
        }

        //move all tasks to another more utilized machine if possible.
        vector<VMId_t> vms = machine_to_VMs[cur_machine_id];
        for(VMId_t task_vm : vms){
            // print_vm_info(task_vm);
            VMInfo_t task_vm_info = VM_GetInfo(task_vm);
            //migrate tasks to more utilized machines
            vector<MachineId_t> candidates;
            for(TaskId_t task_id : task_vm_info.active_tasks){
                candidates.clear();
                TaskInfo_t task_info = GetTaskInfo(task_id);
                for(unsigned k = j + 1; k < machines.size(); k++){
                    MachineId_t migration_machine_id = machines[k];
                    MachineInfo_t migration_machine_info = Machine_GetInfo(migration_machine_id);
                    unsigned task_util = task_info.required_memory;
                    unsigned machine_util = migration_machine_info.memory_used;
                    unsigned machine_cap = migration_machine_info.memory_size;
                    //check if machine meets requirements
                    if(migration_machine_info.s_state == S0 
                        && task_info.required_cpu == migration_machine_info.cpu
                            && task_util + machine_util < machine_cap){

                        if(task_info.gpu_capable == migration_machine_info.gpus){
                            candidates.insert(candidates.begin(), 
                                                migration_machine_id);
                        } else{
                            candidates.push_back(migration_machine_id);
                        }
                    }
                }

                //best candidate should be 1st element.
                if(candidates.size() > 0){
                    MachineId_t candidate = candidates[0];
                    vector<VMId_t> vms = machine_to_VMs[candidate];
                    for(VMId_t candidate_vm : vms){
                        VMInfo_t cand_vm_info = VM_GetInfo(candidate_vm);
                        //move task to new VM
                        if(task_info.required_vm == cand_vm_info.vm_type){
                            VM_RemoveTask(task_vm, task_id);
                            //check if we can shut down the current VM/machine
                            VM_AddTask(candidate_vm, task_id, 
                                sla_to_priority(task_info.required_sla));
                            task_to_VM[task_id] = candidate_vm;
                        }
                    }
                }
            }

            //turn off current VM if possible
            VMInfo_t info_after_move = VM_GetInfo(task_vm);
            if(info_after_move.active_tasks.size() == 0){
                //TODO: this is causing exception. Why?
                // print_vm_info(task_vm);
                try{
                    cout << "here 1" << endl;
                    VM_Shutdown(task_vm);
                    cout << "here 2" << endl;
                } catch(...){
                    //do nothing
                }
            }
        }        

        //turn off machine if possible.
        MachineInfo_t updated_machine_info = Machine_GetInfo(cur_machine_id);
        if(machine_info.active_vms == 0 && machine_info.active_tasks == 0){
            cout << "shut down machine :)" << endl;
            Machine_SetState(cur_machine_id, S5);
        }
    }

}

// Public interface below

static Scheduler Scheduler;

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

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);

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
 * Greedy policy, we should migrate this task to another machine
 * @param task_id the ID of the task whose SLA has been violated
 * TODO: figure out how to get all the machines so we can move them to others
 */
void SLAWarning(Time_t time, TaskId_t task_id) {
    TaskInfo_t task_info = GetTaskInfo(task_id);
    VMId_t task_vm = task_to_VM[task_id];
        
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
    // printf ("TIME: %lu State: %u\n", time, Machine_GetInfo(machine_id).s_state);
    MachineInfo_t machine_info = Machine_GetInfo(machine_id);
}


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