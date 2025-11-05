//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include "Scheduler.hpp"
#include <algorithm> // Required for std::sort

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

double Scheduler::GetMachineUtilization(MachineId_t m) {
    auto mi = Machine_GetInfo(m);
    double used = mi.memory_used;
    double total = mi.memory_size;
    return (total == 0) ? 0 : (used / total);
}



void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
    // Find record for vm_id; refresh host from VM_GetInfo
    for (auto &rec : vmrecs) {
        if (rec.id == vm_id) {
            auto vmi = VM_GetInfo(vm_id);
            rec.host = vmi.machine_id; // authoritative new host
            break;
        }
    }
    migrating = false;

}

void Scheduler::FlushPending() {
    // Sort pending tasks in decreasing order of memory (BFD requirement)
    std::sort(pending.begin(), pending.end(),
        [](const PendingItem &a, const PendingItem &b) {
            return a.mem > b.mem; // descending
        });

    // Place each pending task using the same best-fit logic as NewTask
    for (auto &item : pending) {
        TaskId_t tid = item.tid;
        unsigned mem_req = item.mem;
        CPUType_t need_cpu = item.cpu;
        VMType_t  need_vm  = item.vm;
        SLAType_t sla      = item.sla;
        bool      need_gpu = item.gpu;

        Priority_t pr = (sla == SLA0 ? HIGH_PRIORITY :
                        (sla == SLA1 ? MID_PRIORITY : LOW_PRIORITY));

        // --- Best-fit placement identical to your NewTask code ---
        double best_gap = 1e18;
        double best_tiebreak_util = -1.0;
        size_t best_idx = SIZE_MAX;

        for (size_t i = 0; i < vmrecs.size(); ++i) {
            const auto& rec = vmrecs[i];
            if (rec.cpu_type != need_cpu) continue;
            if (need_gpu && !rec.host_has_gpu) continue;
            if (rec.vm_type != need_vm) continue;

            auto mi = Machine_GetInfo(rec.host);
            unsigned free_mem = mi.memory_size - mi.memory_used;
            if (free_mem < mem_req) continue;

            double gap = (double)(free_mem - mem_req);
            double util = GetMachineUtilization(rec.host);
            if (gap < best_gap || (gap == best_gap && util > best_tiebreak_util)) {
                best_gap = gap;
                best_tiebreak_util = util;
                best_idx = i;
            }
        }

        VMId_t chosen_vm;
        size_t chosen_idx;

        if (best_idx == SIZE_MAX) {
            // Create new host+VM if no fit (same as your code)
            MachineId_t chosen_host = (MachineId_t)(-1);
            for (auto m : machines) {
                auto mi = Machine_GetInfo(m);
                if (mi.cpu != need_cpu) continue;
                if (need_gpu && !mi.gpus) continue;
                unsigned free_mem = mi.memory_size - mi.memory_used;
                if (free_mem >= mem_req) {
                    if (mi.s_state != S0) Machine_SetState(m, S0);
                    chosen_host = m;
                    break;
                }
            }
            if (chosen_host == (MachineId_t)(-1)) {
                SimOutput("FlushPending(): No compatible host for task " + to_string(tid), 0);
                continue;
            }

            VMId_t vm = VM_Create(need_vm, need_cpu);
            VM_Attach(vm, chosen_host);
            auto host_info = Machine_GetInfo(chosen_host);
            vmrecs.push_back(VMRec{
                .id            = vm,
                .host          = chosen_host,
                .vm_type       = need_vm,
                .cpu_type      = need_cpu,
                .host_has_gpu  = host_info.gpus,
                .running_tasks = 0,
                .vm_mem_used   = 0,
                .tasks         = {}
            });
            chosen_vm  = vm;
            chosen_idx = vmrecs.size() - 1;
        } else {
            chosen_vm  = vmrecs[best_idx].id;
            chosen_idx = best_idx;
        }

        VM_AddTask(chosen_vm, tid, pr);
        vmrecs[chosen_idx].running_tasks++;
        vmrecs[chosen_idx].vm_mem_used += mem_req;
        vmrecs[chosen_idx].tasks.push_back(tid);
        task_to_vm_index[tid] = chosen_idx;
    }

    pending.clear();
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
    // --- Query task requirements ---
    CPUType_t need_cpu = RequiredCPUType(task_id);   // X86 / ARM
    VMType_t  need_vm  = RequiredVMType(task_id);    // LINUX / WIN / ...
    SLAType_t sla      = RequiredSLA(task_id);       // SLA0..SLA3
    bool      need_gpu = IsTaskGPUCapable(task_id);  // GPU flag
    unsigned  mem_req  = GetTaskMemory(task_id);     // memory requirement (units consistent with MachineInfo)

    // Buffer this task for batch placement (BFD batching)
    pending.push_back({task_id, mem_req, need_cpu, need_vm, need_gpu, sla, now});

    // If we’ve collected enough tasks, flush and place them all
    if (pending.size() >= BFD_BATCH) {
        FlushPending();
    }
    return;
    // --- Priority from SLA (simple mapping) ---
    Priority_t pr = (sla == SLA0 ? HIGH_PRIORITY :
                    (sla == SLA1 ? MID_PRIORITY   : LOW_PRIORITY));

    // --- Best-Fit search among existing VMs (consolidation) ---
    double best_gap = 1e18;
    double best_tiebreak_util = -1.0;
    size_t best_idx = SIZE_MAX;

    for (size_t i = 0; i < vmrecs.size(); ++i) {
        const auto& rec = vmrecs[i];

        // Hard constraints
        if (rec.cpu_type != need_cpu) continue;
        if (need_gpu && !rec.host_has_gpu) continue;
        if (rec.vm_type != need_vm) continue;

        // Current host capacity
        auto mi = Machine_GetInfo(rec.host);
        if (mi.memory_used > mi.memory_size) continue;              // defensive
        unsigned free_mem = mi.memory_size - mi.memory_used;
        if (free_mem < mem_req) continue;

        // Best-fit gap (smaller is better)
        double gap = static_cast<double>(free_mem) - static_cast<double>(mem_req);

        // Tie-break: prefer higher host utilization (tighter packing)
        double util = GetMachineUtilization(rec.host);

        if (gap < best_gap || (gap == best_gap && util > best_tiebreak_util)) {
            best_gap = gap;
            best_tiebreak_util = util;
            best_idx = i;
        }
    }

    VMId_t chosen_vm;
    size_t chosen_idx;

    // --- If no existing VM fits, create one on the best host (prefer already-on, utilized hosts) ---
    if (best_idx == SIZE_MAX) {
        MachineId_t chosen_host = (MachineId_t)(-1);
        double best_host_util = -1.0;
        double best_host_gap  = 1e18;

        // First pass: prefer hosts that are already S0 (awake) to avoid wakeup penalties
        for (auto m : machines) {
            auto mi = Machine_GetInfo(m);
            if (mi.cpu != need_cpu) continue;
            if (need_gpu && !mi.gpus) continue;

            if (mi.memory_used > mi.memory_size) continue;
            unsigned free_mem = mi.memory_size - mi.memory_used;
            if (free_mem < mem_req) continue;

            if (mi.s_state == S0) {
                double gap  = static_cast<double>(free_mem) - static_cast<double>(mem_req);
                double util = GetMachineUtilization(m);
                if (gap < best_host_gap || (gap == best_host_gap && util > best_host_util)) {
                    best_host_gap  = gap;
                    best_host_util = util;
                    chosen_host = m;
                }
            }
        }

        // Second pass: if nothing woke, allow waking a sleeping compatible host
        if (chosen_host == (MachineId_t)(-1)) {
            for (auto m : machines) {
                auto mi = Machine_GetInfo(m);
                if (mi.cpu != need_cpu) continue;
                if (need_gpu && !mi.gpus) continue;

                if (mi.memory_used > mi.memory_size) continue;
                unsigned free_mem = mi.memory_size - mi.memory_used;
                if (free_mem < mem_req) continue;

                // Wake this one and take it
                if (mi.s_state != S0) Machine_SetState(m, S0);
                chosen_host = m;
                break;
            }
        }

        if (chosen_host == (MachineId_t)(-1)) {
            SimOutput("NewTask(): No compatible host for task " + to_string(task_id), 0);
            return; // could queue instead
        }

        // Create and attach a new compatible VM
        VMId_t vm = VM_Create(need_vm, need_cpu);
        VM_Attach(vm, chosen_host);

        auto host_info = Machine_GetInfo(chosen_host);
        vmrecs.push_back(VMRec{
            .id            = vm,
            .host          = chosen_host,
            .vm_type       = need_vm,
            .cpu_type      = need_cpu,
            .host_has_gpu  = host_info.gpus,
            .running_tasks = 0,
            .vm_mem_used   = 0,
            .tasks         = {}
        });

        chosen_vm  = vm;
        chosen_idx = vmrecs.size() - 1;
    } else {
        chosen_idx = best_idx;
        chosen_vm  = vmrecs[best_idx].id;
    }

    // --- Assign task and update scheduler bookkeeping ---
    VM_AddTask(chosen_vm, task_id, pr);
    vmrecs[chosen_idx].running_tasks++;
    vmrecs[chosen_idx].vm_mem_used += mem_req;
    vmrecs[chosen_idx].tasks.push_back(task_id);
    task_to_vm_index[task_id] = chosen_idx;

}

void Scheduler::PeriodicCheck(Time_t now) {
    // This method should be called from SchedulerCheck()
    // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
    // Unlike the other invocations of the scheduler, this one doesn't report any specific event
    // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary
    // Example migration trigger: source host underutilized (by mem or tasks)
    static constexpr double LOW_UTIL_THRESHOLD = 0.30;  // host underutilized if below
    static constexpr Time_t IDLE_TO_S3 = 50000;         // 50 ms to go to S3
    static constexpr Time_t IDLE_TO_S5 = 200000;        // 200 ms to go to S5

    if (!pending.empty() && (now - last_flush) > BFD_MAX_WAIT) {
        FlushPending();
        last_flush = now;
    }

    // --------- Try to consolidate: migrate VMs off underutilized hosts ----------
    // Strategy: for each VM whose host is underutilized, try to move it to a better (more utilized) compatible host,
    // using a best-fit-by-free-memory destination to pack tightly.
    for (size_t s = 0; s < vmrecs.size(); ++s) {
        auto &src_vm = vmrecs[s];
        if (src_vm.running_tasks == 0) continue; // nothing to migrate

        auto mi_src_host = Machine_GetInfo(src_vm.host);
        double src_util  = GetMachineUtilization(src_vm.host);
        if (src_util >= LOW_UTIL_THRESHOLD) continue; // only nudge clearly underutilized hosts

        // Choose best destination host (best-fit on free mem; tie-break by higher util)
        MachineId_t best_dst_host = (MachineId_t)(-1);
        double      best_gap      = 1e18;
        double      best_util     = -1.0;

        for (size_t d = 0; d < vmrecs.size(); ++d) {
            if (d == s) continue;
            const auto &dst_vm = vmrecs[d];

            // VM-level constraints must remain valid on destination host
            if (dst_vm.cpu_type != src_vm.cpu_type) continue;
            if (src_vm.host_has_gpu && !dst_vm.host_has_gpu) continue;
            if (dst_vm.vm_type != src_vm.vm_type) continue;

            auto mi_dst = Machine_GetInfo(dst_vm.host);
            if (mi_dst.memory_used > mi_dst.memory_size) continue; // defensive
            unsigned free_mem = mi_dst.memory_size - mi_dst.memory_used;

            if (free_mem < src_vm.vm_mem_used) continue; // must fit the whole VM’s footprint

            double gap  = static_cast<double>(free_mem) - static_cast<double>(src_vm.vm_mem_used);
            double util = GetMachineUtilization(dst_vm.host);

            if (gap < best_gap || (gap == best_gap && util > best_util)) {
                best_gap  = gap;
                best_util = util;
                best_dst_host = dst_vm.host;
            }
        }

        // Perform one migration at a time to avoid thrashing
        if (best_dst_host != (MachineId_t)(-1)) {
            VM_Migrate(src_vm.id, best_dst_host);
            migrating = true;
            SimOutput("PeriodicCheck(): Migrating VM " + to_string(src_vm.id) +
                      " from host " + to_string(src_vm.host) +
                      " to host " + to_string(best_dst_host), 3);
            // Let MigrationDone()/MigrationComplete() fix up the host field when sim confirms.
            break; // one migration per check is usually enough; remove 'break' to be more aggressive
        }
    }

    // --------- Power management with hysteresis (S3 then S5) ----------
    for (auto m : machines) {
        auto mi = Machine_GetInfo(m);

        if (mi.active_vms == 0) {
            // track idle time
            if (!idle_since.count(m)) idle_since[m] = now;
            Time_t idle = now - idle_since[m];

            if (idle >= IDLE_TO_S5) {
                if (mi.s_state != S5) Machine_SetState(m, S5);
            } else if (idle >= IDLE_TO_S3) {
                if (mi.s_state == S0) Machine_SetState(m, S3);
            }
        } else {
            // host is in use → must be awake
            idle_since.erase(m);
            if (mi.s_state != S0) Machine_SetState(m, S0);
        }
    }
}


void Scheduler::Shutdown(Time_t time) {
    // Do your final reporting and bookkeeping here.
    // Report about the total energy consumed
    // Report about the SLA compliance
    // Shutdown everything to be tidy :-)
    if (!pending.empty()) {
        FlushPending();   // flush any leftover unplaced tasks
    }
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
    if (idx < vmrecs.size()) {
        auto mem_req = GetTaskMemory(task_id);
        if (vmrecs[idx].vm_mem_used >= mem_req)
            vmrecs[idx].vm_mem_used -= mem_req;
    }
                
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

