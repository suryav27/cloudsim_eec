//
//  Scheduler.cpp
//  CloudSim EEC (UT Austin)
//
//  Greedy Power-Aware Scheduler with PARALLEL HPC support
//
//

#include "Scheduler.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <deque>
#include <iostream>
#include <algorithm>
#include <climits>

static bool migrating = false;
static std::vector<PendingAttach> pending_attaches;

// HPC management: allow multiple VMs across POWER hosts
static std::deque<TaskId_t> hpc_queue;
static std::vector<VMId_t> hpc_vms;           // Track all HPC VMs
static std::vector<MachineId_t> power_hosts;   // Track all POWER hosts

// Helper Methods
static inline double host_energy_bias(const MachineInfo_t& mi) {
    return (!mi.p_states.empty() ? mi.p_states[0] : 1.0);
}

static inline double host_free_mem(const MachineInfo_t& mi) {
    return (mi.memory_used < mi.memory_size)
        ? (mi.memory_size - mi.memory_used)
        : 0.0;
}

static inline Time_t sla_overrun_threshold(SLAType_t sla) {
    switch (sla) {
        case SLA0: return 8'000'000;
        case SLA1: return 3'000'000;
        case SLA2: return 5'000'000;
        case SLA3: return 2'000'000'000;  // ~33 minutes for long HPC tasks
        default:   return 5'000'000;
    }
}

static inline bool is_hpc_power_aix(CPUType_t cpu, VMType_t vm) {
    return (cpu == POWER && vm == AIX);
}

// Init
void Scheduler::Init() {
    unsigned total = Machine_GetTotal();

    vmrecs.clear();
    machines.clear();
    task_start_time.clear();
    task_sla.clear();
    task_to_vm_index.clear();
    pending_attaches.clear();
    hpc_queue.clear();
    hpc_vms.clear();
    power_hosts.clear();

    machines.reserve(total);

    for (unsigned i = 0; i < total; ++i) {
        MachineId_t mid = MachineId_t(i);
        machines.push_back(mid);

        auto mi = Machine_GetInfo(mid);
        if (mi.s_state != S0)
            Machine_SetState(mid, S0);
        
        // Track POWER hosts separately
        if (mi.cpu == POWER) {
            power_hosts.push_back(mid);
        }
    }

    SimOutput("Scheduler::Init(): Machines discovered = " +
              std::to_string(machines.size()) +
              " (POWER hosts: " + std::to_string(power_hosts.size()) + ")" +
              ". Greedy Power-Aware + Parallel HPC", 4);
}

// Migration Complete
void Scheduler::MigrationComplete(Time_t /*time*/, VMId_t vm_id) {
    SimOutput("[INFO] Migration completed for VM " + std::to_string(vm_id), 3);
}

// Helper: Find best available HPC VM (or create new one)
size_t Scheduler::FindOrCreateHPCVM(Time_t now, TaskId_t task_id, 
                                     unsigned mem_req, Priority_t pr) {
    
    // First, try to find an existing HPC VM that's idle and awake
    for (size_t i = 0; i < vmrecs.size(); ++i) {
        const auto &rec = vmrecs[i];
        
        if (rec.cpu_type != POWER || rec.vm_type != AIX)
            continue;
            
        auto mi = Machine_GetInfo(rec.host);
        
        if (mi.s_state != S0)
            continue;
            
        if (rec.running_tasks > 0)
            continue;
            
        if (host_free_mem(mi) < mem_req)
            continue;
            
        return i;
    }
    
    MachineId_t best_host = (MachineId_t)(-1);
    double best_score = 1e18;
    
    for (auto m : power_hosts) {
        auto mi = Machine_GetInfo(m);
        
        if (host_free_mem(mi) < mem_req)
            continue;
        
        if (mi.s_state == S0) {
            double load = (mi.active_vms > 0) ? 
                (double)mi.active_vms / (mi.num_cpus > 0 ? mi.num_cpus : 1) : 0.0;
            double score = load * host_energy_bias(mi);
            
            if (score < best_score) {
                best_score = score;
                best_host = m;
            }
        }
    }
    
    if (best_host == (MachineId_t)(-1)) {
        for (auto m : power_hosts) {
            auto mi = Machine_GetInfo(m);
            if (host_free_mem(mi) >= mem_req) {
                best_host = m;
                break;
            }
        }
    }
    
    if (best_host == (MachineId_t)(-1) && !power_hosts.empty()) {
        best_host = power_hosts[0];
    }
    
    if (best_host == (MachineId_t)(-1)) {
        SimOutput("[FATAL] No POWER host available for HPC task!", 0);
        return SIZE_MAX;
    }
    
    // Create new HPC VM
    VMId_t new_vm = VM_Create(AIX, POWER);
    hpc_vms.push_back(new_vm);
    
    auto hi = Machine_GetInfo(best_host);
    
    if (hi.s_state != S0) {
        Machine_SetState(best_host, S0);
        
        SimOutput("[INFO] POWER host " + std::to_string(best_host) +
                  " waking; deferring HPC task " + std::to_string(task_id), 3);
        
        pending_attaches.push_back(
            PendingAttach{new_vm, best_host, task_id, pr}
        );
        
        vmrecs.push_back(VMRec{
            new_vm, best_host, AIX, POWER,
            hi.gpus, 0
        });
        
        return SIZE_MAX;  
    }
    
    // Host awake - attach now
    VM_Attach(new_vm, best_host);
    auto host_info = Machine_GetInfo(best_host);
    
    vmrecs.push_back(VMRec{
        new_vm, best_host, AIX, POWER,
        host_info.gpus, 0
    });
    
    SimOutput("[INFO] Created new HPC VM " + std::to_string(new_vm) +
              " on host " + std::to_string(best_host), 3);
    
    return vmrecs.size() - 1;
}

// New Task Assignment
void Scheduler::NewTask(Time_t now, TaskId_t task_id) {

    CPUType_t need_cpu  = RequiredCPUType(task_id);
    VMType_t  need_vm   = RequiredVMType(task_id);
    SLAType_t sla       = RequiredSLA(task_id);
    bool      need_gpu  = IsTaskGPUCapable(task_id);
    unsigned  mem_req   = GetTaskMemory(task_id);

    Priority_t pr = (sla == SLA0 ? HIGH_PRIORITY :
                    (sla == SLA1 ? MID_PRIORITY : LOW_PRIORITY));

    // Proactively wake sleeping hosts for SLA0 tasks to reduce latency
    if (sla == SLA0) {
        for (auto m : machines) {
            auto mi = Machine_GetInfo(m);
            if (mi.cpu == need_cpu && mi.s_state != S0 && mi.active_vms == 0) {
                Machine_SetState(m, S0);
                SimOutput("[DEBUG] Proactively waking host " + 
                          std::to_string(m) + " for SLA0 burst", 3);
            }
        }
    }

    // HPC PATH: Parallel execution across POWER hosts
    if (is_hpc_power_aix(need_cpu, need_vm)) {
        
        size_t vm_idx = FindOrCreateHPCVM(now, task_id, mem_req, pr);
        
        if (vm_idx == SIZE_MAX) {
            return;
        }
        
        VMId_t vm = vmrecs[vm_idx].id;
        VM_AddTask(vm, task_id, pr);
        vmrecs[vm_idx].running_tasks++;
        
        task_to_vm_index[task_id] = vm_idx;
        task_start_time[task_id]  = now;
        task_sla[task_id]         = sla;
        
        SimOutput("[INFO] HPC task " + std::to_string(task_id) +
                  " assigned to VM " + std::to_string(vm) +
                  " on host " + std::to_string(vmrecs[vm_idx].host), 4);
        
        return;
    }

    // NORMAL PATH: Greedy power-aware scheduling
    size_t chosen_idx = SIZE_MAX;
    double best_score = 1e18;

    // Try existing awake VMs, selective based on SLA priority
    for (size_t i = 0; i < vmrecs.size(); ++i) {
        const auto &rec = vmrecs[i];
        auto mi = Machine_GetInfo(rec.host);

        if (mi.s_state != S0) continue;
        if (rec.cpu_type != need_cpu) continue;
        if (rec.vm_type  != need_vm)  continue;
        if (need_gpu && !rec.host_has_gpu) continue;
        if (host_free_mem(mi) < mem_req) continue;

        double cpu_count = (mi.num_cpus > 0 ? mi.num_cpus : 1);
        double load_ratio = (rec.running_tasks + 1.0) / cpu_count;

        if (sla == SLA0 && rec.running_tasks >= 2 * mi.num_cpus) continue;

        if (sla == SLA1 && load_ratio > 0.75) continue;
        
        double score = load_ratio * host_energy_bias(mi);

        if (score < best_score) {
            best_score = score;
            chosen_idx = i;
        }
    }

    VMId_t chosen_vm = VMId_t(-1);
    MachineId_t chosen_host = MachineId_t(-1);

    // Need new VM
    if (chosen_idx == SIZE_MAX) {

        MachineInfo_t mi{};
        chosen_host = (MachineId_t)(-1);
        best_score = 1e18;

        for (auto m : machines) {
            mi = Machine_GetInfo(m);
            if (mi.cpu != need_cpu) continue;
            if (need_gpu && !mi.gpus) continue;
            if (host_free_mem(mi) < mem_req) continue;
            
            double score;
            if (sla == SLA0 || sla == SLA1) {
                // Prioritize lightly loaded hosts for critical tasks
                score = (double)mi.active_vms + (mi.s_state != S0 ? 100.0 : 0.0);
            } else {
                // Energy-aware for lower priority
                score = host_energy_bias(mi) * (1.0 + mi.active_vms);
            }
            
            if (score < best_score) {
                best_score = score;
                chosen_host = m;
            }
        }

        if (chosen_host == (MachineId_t)(-1)) {
            // Fallback: any host with matching CPU
            for (auto m : machines) {
                mi = Machine_GetInfo(m);
                if (mi.cpu == need_cpu) {
                    chosen_host = m;
                    break;
                }
            }
        }

        if (chosen_host == (MachineId_t)(-1)) {
            SimOutput("[FATAL] No host available for new task.", 0);
            return;
        }

        auto hi = Machine_GetInfo(chosen_host);
        VMId_t vm = VM_Create(need_vm, need_cpu);

        if (hi.s_state != S0) {
            Machine_SetState(chosen_host, S0);

            SimOutput("[INFO] Host " + std::to_string(chosen_host) +
                      " waking; deferring attach for task " +
                      std::to_string(task_id), 3);

            pending_attaches.push_back(
                PendingAttach{vm, chosen_host, task_id, pr}
            );

            auto host_info = Machine_GetInfo(chosen_host);
            vmrecs.push_back(VMRec{
                vm, chosen_host, need_vm, need_cpu,
                host_info.gpus, 0
            });

            return;
        }

        VM_Attach(vm, chosen_host);
        auto host_info = Machine_GetInfo(chosen_host);

        vmrecs.push_back(VMRec{
            vm, chosen_host, need_vm, need_cpu,
            host_info.gpus, 0
        });

        chosen_idx = vmrecs.size() - 1;
        chosen_vm  = vm;
    }
    else {
        chosen_vm   = vmrecs[chosen_idx].id;
        chosen_host = vmrecs[chosen_idx].host;
    }

    // Assign non-HPC task
    VM_AddTask(chosen_vm, task_id, pr);
    vmrecs[chosen_idx].running_tasks++;

    task_to_vm_index[task_id] = chosen_idx;
    task_start_time[task_id]  = now;
    task_sla[task_id]         = sla;
    
    // For SLA0 tasks, boost all cores on host to highest P-state for performance
    if (sla == SLA0) {
        auto mi = Machine_GetInfo(chosen_host);
        if (!mi.p_states.empty() && mi.p_state != 0) {
            for (unsigned core = 0; core < mi.num_cpus; ++core) {
                Machine_SetCorePerformance(chosen_host, core, P0);
            }
            SimOutput("[DEBUG] Boosted host " + std::to_string(chosen_host) + 
                      " to P0 for SLA0 task", 3);
        }
    }

    SimOutput("[INFO] Task " + std::to_string(task_id) +
              " assigned to VM " + std::to_string(chosen_vm) +
              " on host " + std::to_string(chosen_host), 4);
}

// HandleWakeComplete
void Scheduler::HandleWakeComplete(Time_t now, MachineId_t machine_id) {

    for (size_t i = 0; i < pending_attaches.size();) {
        auto &p = pending_attaches[i];

        if (p.host == machine_id) {

            VM_Attach(p.vm, p.host);
            VM_AddTask(p.vm, p.task_id, p.pr);

            // Look up VMRec
            size_t idx = SIZE_MAX;
            for (size_t j = 0; j < vmrecs.size(); ++j) {
                if (vmrecs[j].id == p.vm) {
                    idx = j;
                    break;
                }
            }

            if (idx == SIZE_MAX) {
                SimOutput("[FATAL] VMRec lookup failed in wake-complete!", 0);
                exit(1);
            }

            vmrecs[idx].running_tasks++;
            task_to_vm_index[p.task_id] = idx;
            task_start_time[p.task_id]  = now;
            task_sla[p.task_id]         = RequiredSLA(p.task_id);

            SimOutput("[INFO] Deferred attach complete for task " +
                      std::to_string(p.task_id) +
                      " on host " + std::to_string(machine_id), 3);

            pending_attaches.erase(pending_attaches.begin() + i);
        }
        else {
            ++i;
        }
    }
}

// PeriodicCheck
void Scheduler::PeriodicCheck(Time_t now) {

    static std::unordered_map<MachineId_t, Time_t> last_active;
    static std::unordered_set<TaskId_t> warned;
    static Time_t last_check = 0;

    // Only run detailed checks every 60 seconds to reduce overhead
    if (now - last_check < 60'000'000) {
        return;
    }
    last_check = now;

    // SLA overrun logging
    for (auto &kv : task_start_time) {
        TaskId_t tid  = kv.first;
        Time_t   start = kv.second;

        SLAType_t sla = task_sla.count(tid) ? task_sla.at(tid) : SLA1;
        Time_t limit  = sla_overrun_threshold(sla);

        if (now > start + limit && !warned.count(tid)) {
            SimOutput("[WARN] Task " + std::to_string(tid) +
                      " exceeded SLA window (start=" +
                      std::to_string(start) +
                      ", now=" + std::to_string(now) + ")", 1);
            warned.insert(tid);
        }
    }

    // Power management - more conservative during high load
    for (auto m : machines) {
        auto mi = Machine_GetInfo(m);

        if (mi.active_vms == 0) {
            if (last_active[m] == 0)
                last_active[m] = now;

            // Longer idle timeout (10s) to avoid sleep during bursts
            if (now - last_active[m] > 10'000'000 && mi.s_state == S0) {
                Machine_SetState(m, S2);
                SimOutput("[DEBUG] Host "
                          + std::to_string(m)
                          + " → S2 (idle sleep)", 3);
            }
        }
        else {
            if (mi.s_state != S0) {
                Machine_SetState(m, S0);
                SimOutput("[DEBUG] Host "
                          + std::to_string(m)
                          + " → S0 (wake active)", 3);
            }
            last_active[m] = now;
        }
    }
}

// TaskComplete
void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {

    auto it = task_to_vm_index.find(task_id);

    if (it != task_to_vm_index.end()) {
        size_t idx = it->second;

        if (idx < vmrecs.size() &&
            vmrecs[idx].running_tasks > 0) {
            vmrecs[idx].running_tasks--;
        }

        task_to_vm_index.erase(it);
    }

    task_start_time.erase(task_id);
    task_sla.erase(task_id);

    SimOutput("[DEBUG] TaskComplete(): Task " +
              std::to_string(task_id) +
              " completed at time " +
              std::to_string(now), 3);
}

// Shutdown
void Scheduler::Shutdown(Time_t time) {

    for (auto &vm : vmrecs)
        VM_Shutdown(vm.id);

    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is "
              + std::to_string(time), 4);
}

// Global Interface Functions
static Scheduler scheduler;

void InitScheduler() { scheduler.Init(); }
void HandleNewTask(Time_t t, TaskId_t id) { scheduler.NewTask(t, id); }
void HandleTaskCompletion(Time_t t, TaskId_t id) { scheduler.TaskComplete(t, id); }
void MigrationDone(Time_t t, VMId_t vm) { scheduler.MigrationComplete(t, vm); migrating = false; }
void SchedulerCheck(Time_t t) { scheduler.PeriodicCheck(t); }

void SLAWarning(Time_t t, TaskId_t id) {
    SimOutput("[WARN] SLA warning for task " + std::to_string(id), 1);
}

void MemoryWarning(Time_t t, MachineId_t mid) {
    SimOutput("[WARN] Memory overflow at machine " + std::to_string(mid), 0);
}

void StateChangeComplete(Time_t t, MachineId_t mid) {
    SimOutput("[INFO] Wake complete for host "
              + std::to_string(mid), 3);
    scheduler.HandleWakeComplete(t, mid);
}

void SimulationComplete(Time_t time) {

    std::cout << "SLA violation report\n"
              << "SLA0: " << GetSLAReport(SLA0) << "%\n"
              << "SLA1: " << GetSLAReport(SLA1) << "%\n"
              << "SLA2: " << GetSLAReport(SLA2) << "%\n"
              << "SLA3: " << GetSLAReport(SLA3) << "%\n"
              << "Total Energy: " << Machine_GetClusterEnergy()
              << " KW-Hour\n"
              << "Simulation finished in "
              << (double(time)/1'000'000.0)
              << " seconds\n";

    scheduler.Shutdown(time);
}