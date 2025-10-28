//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include "Scheduler.hpp"

static bool migrating = false;


void Scheduler::Init() {
    unsigned total = Machine_GetTotal();
    if (total == 0) {
        SimOutput("Scheduler::Init(): No machines available", 0);
        return;
    }

    vmrecs.clear();
    machines.clear();
    machines.reserve(total);

    for (unsigned i = 0; i < total; ++i) {
        MachineId_t mid = MachineId_t(i);
        machines.push_back(mid);

        auto mi = Machine_GetInfo(mid);
        // Make sure hosts are awake; don’t attach VMs here.
        if (mi.s_state != S0) Machine_SetState(mid, S0);
    }

    SimOutput("Scheduler::Init(): Machines discovered = " + to_string(machines.size()) + ". VMs will be created on demand.", 2);
}





void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    // Get the task parameters
    //  IsGPUCapable(task_id);
    //  GetMemory(task_id);
    //  RequiredVMType(task_id);
    //  RequiredSLA(task_id);
    //  RequiredCPUType(task_id);
    // Decide to attach the task to an existing VM, 
    //      vm.AddTask(taskid, Priority_T priority); or
    // Create a new VM, attach the VM to a machine
    //      VM vm(type of the VM)
    //      vm.Attach(machine_id);
    //      vm.AddTask(taskid, Priority_t priority) or
    // Turn on a machine, create a new VM, attach it to the VM, then add the task
    //
    // Turn on a machine, migrate an existing VM from a loaded machine....
    //
    // Other possibilities as desired
    CPUType_t need_cpu = RequiredCPUType(task_id);   // X86 or ARM
    VMType_t  need_vm  = RequiredVMType(task_id);    // LINUX / WIN / ...
    SLAType_t sla      = RequiredSLA(task_id);       // SLA0..SLA3
    bool      need_gpu = IsTaskGPUCapable(task_id);  // true if GPU needed
    unsigned  mem_req  = GetTaskMemory(task_id);     // bytes (per Interfaces.h)

    // Priority policy (example)
    Priority_t pr = (sla == SLA0 ? HIGH_PRIORITY :
                     sla == SLA1 ? MID_PRIORITY   : LOW_PRIORITY);

    // 1) Pick the best existing VM (least running_tasks) that satisfies constraints
    size_t chosen_idx = SIZE_MAX;
    size_t least_tasks = SIZE_MAX;

    for (size_t i = 0; i < vmrecs.size(); ++i) {
        const auto& rec = vmrecs[i];
        if (rec.cpu_type != need_cpu) continue;             // CPU family must match
        if (need_gpu && !rec.host_has_gpu) continue;        // need GPU
        if (rec.vm_type != need_vm) continue;               // OS type (LINUX/WIN/...)

        // Optional memory fit (only if your MachineInfo has memory_used/size)
        auto mi = Machine_GetInfo(rec.host);
        if (mi.memory_size >= mi.memory_used) {
            unsigned free_mem = mi.memory_size - mi.memory_used;
            if (free_mem < mem_req) continue;
        }
        // choose least-loaded VM
        if (rec.running_tasks < least_tasks) {
            least_tasks = rec.running_tasks;
            chosen_idx  = i;
        }
    }

    VMId_t chosen_vm;

    // 2) If none exists, create a new compatible VM on a compatible host
    if (chosen_idx == SIZE_MAX) {
        MachineId_t host = (MachineId_t)(-1);
        for (auto m : machines) {
            auto mi = Machine_GetInfo(m);
            if (mi.cpu != need_cpu) continue;
            if (need_gpu && !mi.gpus) continue;

            // Optional memory fit
            if (mi.memory_size >= mi.memory_used) {
                unsigned free_mem = mi.memory_size - mi.memory_used;
                if (free_mem < mem_req) continue;
            }

            if (mi.s_state != S0) Machine_SetState(m, S0);  // wake host
            host = m; break;
        }
        if (host == (MachineId_t)(-1)) {
            SimOutput("NewTask(): No compatible host for task " + to_string(task_id), 0);
            return; // or queue it
        }

        VMId_t vm = VM_Create(need_vm, need_cpu);
        VM_Attach(vm, host);

        auto mi = Machine_GetInfo(host);
        vmrecs.push_back(VMRec{
            .id           = vm,
            .host         = host,
            .vm_type      = need_vm,
            .cpu_type     = need_cpu,
            .host_has_gpu = mi.gpus,
            .running_tasks= 0
        });
        chosen_idx = vmrecs.size() - 1;
        chosen_vm  = vm;
    } else {
        chosen_vm  = vmrecs[chosen_idx].id;   // <- SET chosen from chosen_idx
    }

    // 3) Assign the task and update our bookkeeping
    VM_AddTask(chosen_vm, task_id, pr);
    vmrecs[chosen_idx].running_tasks++;
    task_to_vm_index[task_id] = chosen_idx;
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
    for(auto & vm: vmrecs) {
        VM_Shutdown(vm.id);
    }
    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    // Do any bookkeeping necessary for the data structures
    // Decide if a machine is to be turned off, slowed down, or VMs to be migrated according to your policy
    // This is an opportunity to make any adjustments to optimize performance/energy
    auto it = task_to_vm_index.find(task_id);
    if (it != task_to_vm_index.end()) {
        size_t idx = it->second;
        if (idx < vmrecs.size() && vmrecs[idx].running_tasks > 0)
            vmrecs[idx].running_tasks--;
        task_to_vm_index.erase(it);
    }
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);
}

// Public interface below

static Scheduler scheduler;

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 4);
    scheduler.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 4);
    scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 4);
    scheduler.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
    scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 4);
    scheduler.PeriodicCheck(time);
    // static unsigned counts = 0;
    // counts++;
    // if(counts == 10) {
    //     migrating = true;
    //     VM_Migrate(1, 9);
    // }
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
    
    scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {
    
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
}

