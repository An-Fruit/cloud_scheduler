// E-Eco Scheduler
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

#define TIMER_DECREMENT 100000

static Scheduler Scheduler;
static vector<MachineId_t> fully_on;
static vector<MachineId_t> idle;
static vector<TaskId_t> task_queue;

static unordered_map<MachineId_t, bool> changing_state;

void lower_level();
void increase_level(TaskId_t task_id);

void Scheduler::Init() {
    cout << "E-Eco Scheduler!" << endl;
    SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    SimOutput("Scheduler::Init(): Initializing scheduler", 1);
    for(unsigned i = 0; i < Machine_GetTotal(); i++) {
        MachineId_t machine_id = MachineId_t(i);
        fully_on.push_back(machine_id);
        changing_state[machine_id] = false;
    }

}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    TaskInfo_t task_info = GetTaskInfo(task_id);
    VMId_t new_vm = VM_Create(task_info.required_vm, task_info.required_cpu);  
    bool found_first = false;
    MachineId_t best_option;
    // Find the machine with the smallest ultization (least amout of active tasks)
    for (MachineId_t id : fully_on) {
        MachineInfo_t curr_machine = Machine_GetInfo(id);
        if (curr_machine.cpu == task_info.required_cpu &&
                curr_machine.memory_size - curr_machine.memory_used >= task_info.required_memory + 8 &&
                    !changing_state[id]) {
            if (!found_first) {
                best_option = id;
                found_first = true;
            } else {
                MachineInfo_t best_option_machine = Machine_GetInfo(best_option);
                if (curr_machine.active_tasks < best_option_machine.active_tasks) {
                    best_option = id;
                }
                
                if (task_info.gpu_capable) {
                    if (best_option_machine.gpus && !curr_machine.gpus) {
                        best_option = best_option_machine.machine_id;
                    }
                }
            }
            // SimOutput("Match ID: " + to_string (best_option), 1);
        }
    }
    if (!found_first) {
        increase_level(task_id);
        task_queue.push_back(task_id);
    } else {
        VM_Attach(new_vm, best_option);
        VM_AddTask(new_vm, task_id, task_info.priority);
    }
    
}

void lower_level() {
    for (int i = 0; i < fully_on.size(); i++) {
        if (fully_on.size() == 1)
            break;
        if (idle.size() == Machine_GetTotal() * .5)
            break;
        MachineId_t m_id = fully_on[i];
        MachineInfo_t m_info = Machine_GetInfo(m_id);
        if (!changing_state[m_id] && m_info.active_tasks == 0) {
            Machine_SetState(m_id, S3);
            changing_state[m_id] = true;
            fully_on.erase(fully_on.begin() + i);
            idle.push_back(m_id);
            i--;
        }
    }
}

void increase_level(TaskId_t task_id) {
    TaskInfo_t task_info = GetTaskInfo(task_id);
    for (int i = 0; i < idle.size(); i++) {
        MachineInfo_t idle_machine = Machine_GetInfo(idle[i]);
        if (!changing_state[idle_machine.machine_id] && idle_machine.cpu == task_info.required_cpu 
                && idle_machine.memory_size >= task_info.required_memory + 8) {
            Machine_SetState(idle_machine.machine_id, S0);
            changing_state[idle_machine.machine_id] = true;
            idle.erase(idle.begin() + i);
            fully_on.push_back(idle_machine.machine_id);
        }
    }
}


void Scheduler::PeriodicCheck(Time_t now) {
    // This method should be called from SchedulerCheck()
    // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
    // Unlike the other invocations of the scheduler, this one doesn't report any specific event
    // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary
}

void Scheduler::Shutdown(Time_t time) {
    // Do your final reporting and bookkeeping here.
    // Report about the total energy consumed
    // Report about the SLA compliance
    // Shutdown everything to be tidy :-)
    for(auto & vm: vms) {
        VM_Shutdown(vm);
    }
    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    // Do any bookkeeping necessary for the data structures
    // Decide if a machine is to be turned off, slowed down, or VMs to be migrated according to your policy
    // This is an opportunity to make any adjustments to optimize performance/energy
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 1);
    lower_level();
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
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 4);
    Scheduler.PeriodicCheck(time);

}

void SimulationComplete(Time_t time) {
    // This function is called before the simulation terminates Add whatever you feel like.
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;     // SLA3 do not have SLA violation issues
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
    SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 4);
    
    Scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {
    increase_level (task_id);
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
    changing_state[machine_id] = false;
    MachineInfo_t m_info = Machine_GetInfo(machine_id);
    if (m_info.s_state == S0) {
        for (int i = 0; i < task_queue.size(); i++) {
            TaskId_t t_id = task_queue[i];
            TaskInfo_t t_info = GetTaskInfo(t_id);
            if (t_info.required_cpu == m_info.cpu &&
                m_info.memory_size - m_info.memory_used >= t_info.required_memory + 8) {
                
                VMId_t new_vm = VM_Create(t_info.required_vm, t_info.required_cpu);
                VM_Attach(new_vm, machine_id);
                VM_AddTask(new_vm, t_id, t_info.priority);
                task_queue.erase(task_queue.begin() + i);
                i--;
            }
        }
    }
}