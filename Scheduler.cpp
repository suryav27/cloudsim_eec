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

    SimOutput("Scheduler::Init(): Initializing " + to_string(total) +
              " machines for DVFS power-aware scheduling.", 2);

    for (unsigned i = 0; i < total; ++i) {
        MachineId_t mid = MachineId_t(i);
        machines.push_back(mid);

        // Explicitly start each host in the deepest sleep state (S5)
        Machine_SetState(mid, S5);

        // Initialize DVFS bookkeeping
        machine_pstate[mid] = P3;           // lowest-performance, lowest-power
        last_p_change[mid]  = 0;            // reset cooldown timer
    }

    SimOutput("Scheduler::Init(): Machines set to S5 (powered-off). "
              "They will wake on demand when tasks arrive.", 2);
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
    CPUType_t need_cpu = RequiredCPUType(task_id);   // X86 / ARM
    VMType_t  need_vm  = RequiredVMType(task_id);    // LINUX / WIN / ...
    SLAType_t sla      = RequiredSLA(task_id);       // SLA0..SLA3
    bool      need_gpu = IsTaskGPUCapable(task_id);  // true if GPU needed
    unsigned  mem_req  = GetTaskMemory(task_id);     // memory requirement (MB or consistent unit)

    // Priority policy (based on SLA)
    Priority_t pr = (sla == SLA0 ? HIGH_PRIORITY :
                     sla == SLA1 ? MID_PRIORITY :
                                   LOW_PRIORITY);

    // --- 2) Try to find an existing VM that fits the requirements ---
    size_t chosen_idx = SIZE_MAX;
    size_t least_tasks = SIZE_MAX;

    for (size_t i = 0; i < vmrecs.size(); ++i) {
        const auto& rec = vmrecs[i];
        if (rec.cpu_type != need_cpu) continue;
        if (need_gpu && !rec.host_has_gpu) continue;
        if (rec.vm_type != need_vm) continue;

        auto mi = Machine_GetInfo(rec.host);
        unsigned free_mem = mi.memory_size - mi.memory_used;
        if (free_mem < mem_req) continue;

        // Prefer VM with fewest running tasks
        if (rec.running_tasks < least_tasks) {
            least_tasks = rec.running_tasks;
            chosen_idx = i;
        }
    }

    VMId_t chosen_vm;

    // --- 3) If no existing VM fits, create a new one on a compatible host ---
    if (chosen_idx == SIZE_MAX) {
        MachineId_t host = (MachineId_t)(-1);

        for (auto m : machines) {
            MachineInfo_t mi = Machine_GetInfo(m);

            // Try to reuse an already awake host first
            if (mi.s_state == S0 && mi.cpu == need_cpu &&
                (!need_gpu || mi.gpus)) {
                host = m;
                break;
            }

            // Otherwise wake one sleeping host only if absolutely needed
            if (host == (MachineId_t)(-1) && mi.s_state != S0 &&
                mi.cpu == need_cpu && (!need_gpu || mi.gpus)) {
                Machine_SetState(m, S0);
                SimOutput("DVFS: Waking host " + to_string(m), 2);
                host = m;
                break;
            }

            // --- Re-read fields after wake ---
            bool gpu_cap = mi.gpus;
            CPUType_t host_cpu = mi.cpu;

            // --- Debug: show what we’re checking ---
            SimOutput("Task " + to_string(task_id) +
                    " needsGPU=" + to_string(need_gpu) +
                    " host=" + to_string(m) +
                    " s_state=" + to_string(mi.s_state) +
                    " cpu=" + to_string(host_cpu) +
                    " mem=" + to_string(mi.memory_size) +
                    " gpu=" + to_string(gpu_cap), 2);

            // --- Compatibility checks ---
            if (host_cpu != need_cpu) continue;
            if (need_gpu && !gpu_cap) continue;

            unsigned free_mem = (mi.memory_size > mi.memory_used)
                                ? mi.memory_size - mi.memory_used
                                : mi.memory_size;
            if (free_mem < mem_req) continue;

            host = m;
            break;
        }


        if (host == (MachineId_t)(-1)) {
            SimOutput("NewTask(): No compatible host for task " + to_string(task_id), 0);
            return;
        }

        VMId_t vm = VM_Create(need_vm, need_cpu);
        VM_Attach(vm, host);

        auto host_info = Machine_GetInfo(host);
        vmrecs.push_back(VMRec{
            .id            = vm,
            .host          = host,
            .vm_type       = need_vm,
            .cpu_type      = need_cpu,
            .host_has_gpu  = host_info.gpus,
            .running_tasks = 0
        });

        chosen_idx = vmrecs.size() - 1;
        chosen_vm  = vm;
    } else {
        chosen_vm = vmrecs[chosen_idx].id;
    }
    // --- 4) Assign the task to the VM and update bookkeeping ---
    VM_AddTask(chosen_vm, task_id, pr);
    vmrecs[chosen_idx].running_tasks++;
    task_to_vm_index[task_id] = chosen_idx;

    // --- 5) DVFS adjustment for the host after placement ---
    auto host = vmrecs[chosen_idx].host;
    auto mi   = Machine_GetInfo(host);
    double util = 0.0;
    if (mi.num_cpus > 0)
        util = std::min(1.0, double(vmrecs[chosen_idx].running_tasks) / double(mi.num_cpus));

    MaybeAdjustPState(host, util, now);

    // Apply the new P-state
    for (unsigned core = 0; core < mi.num_cpus; ++core)
        Machine_SetCorePerformance(host, core, machine_pstate[host]);

    SimOutput("DVFS: Host " + to_string(host) +
              " util=" + to_string(util) +
              " → P-state " + to_string(machine_pstate[host]), 3);
}

void Scheduler::MaybeAdjustPState(MachineId_t m, double util, Time_t now) {
    CPUPerformance_t target = PickPState(util);
    auto cur_it = machine_pstate.find(m);
    CPUPerformance_t current = (cur_it == machine_pstate.end()) ? P0 : cur_it->second;

    bool increasing_perf = (target < current);
    Time_t since = (last_p_change.count(m) ? now - last_p_change[m] : PSTATE_COOLDOWN + 1);

    if (increasing_perf || since > PSTATE_COOLDOWN) {
        machine_pstate[m] = target;
        last_p_change[m] = now;
        string level = (target == P0 ? "P0 (Max Freq)" :
                        target == P1 ? "P1 (High)" :
                        target == P2 ? "P2 (Medium)" : "P3 (Low)");
        SimOutput("DVFS: Adjusted Host " + to_string(m) + " → " + level, 3);
    }
    auto mi = Machine_GetInfo(m); // Retrieve machine info
    for (unsigned core = 0; core < mi.num_cpus; ++core)
        Machine_SetCorePerformance(m, core, machine_pstate[m]);

}

void Scheduler::PeriodicCheck(Time_t now) {
    // This method should be called from SchedulerCheck()
    // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
    // Unlike the other invocations of the scheduler, this one doesn't report any specific event
    // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary
    // 1) Build per-host running task count
    // 1) Count how many running tasks are on each host
    std::unordered_map<MachineId_t, unsigned> tasks_on_host;

    for (const auto &rec : vmrecs)
        tasks_on_host[rec.host] += rec.running_tasks;

    // 2) Iterate over all known machines
    static std::unordered_map<MachineId_t, Time_t> idle_since;
    static constexpr Time_t IDLE_TO_S3 = 100000;   // 100 ms
    static constexpr Time_t IDLE_TO_S5 = 500000;   // 500 ms

    for (auto m : machines) {
        auto mi = Machine_GetInfo(m);

        // --- If machine is asleep (S3/S5), skip P-state tuning ---
        if (mi.s_state != S0) continue;

        // --- Estimate utilization: active tasks / number of cores ---
        double util = 0.0;
        if (tasks_on_host.count(m) && mi.num_cpus > 0)
            util = std::min(1.0, double(tasks_on_host[m]) / double(mi.num_cpus));

        // --- Adjust P-state based on utilization ---
        MaybeAdjustPState(m, util, now);

        // --- Apply the P-state to all cores ---
        for (unsigned core = 0; core < mi.num_cpus; ++core) {
            Machine_SetCorePerformance(m, core, machine_pstate[m]);
        }

        SimOutput("DVFS: Host " + to_string(m) +
                  " util=" + to_string(util) +
                  " → P" + to_string(machine_pstate[m]), 3);

        // --- Sleep / wake management ---
        if (tasks_on_host[m] == 0) {
            // No active tasks → potentially go to sleep
            if (!idle_since.count(m)) idle_since[m] = now;
            Time_t idle_time = now - idle_since[m];

            if (idle_time > IDLE_TO_S5 && mi.s_state != S5) {
                Machine_SetState(m, S5);
                SimOutput("Host " + to_string(m) + " → S5 (Power Off)", 3);
            } else if (idle_time > IDLE_TO_S3 && mi.s_state == S0) {
                Machine_SetState(m, S3);
                SimOutput("Host " + to_string(m) + " → S3 (Sleep)", 3);
            }
        } else {
            // Active → ensure it’s awake
            idle_since.erase(m);
            if (mi.s_state != S0)
                Machine_SetState(m, S0);
        }
    }
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

void Scheduler::HandleSLAWarning(Time_t time, TaskId_t task_id) {
    auto it = task_to_vm_index.find(task_id);
    if (it != task_to_vm_index.end()) {
        size_t idx = it->second;
        if (idx < vmrecs.size()) {
            MachineId_t host = vmrecs[idx].host;
            auto cur = machine_pstate[host];
            if (cur != P0) {
                machine_pstate[host] = CPUPerformance_t(cur - 1);
                SimOutput("DVFS: SLA boost on Host " + to_string(host), 3);
            }
        }
    }
}


void SLAWarning(Time_t time, TaskId_t task_id) {
    scheduler.HandleSLAWarning(time, task_id);
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
}

