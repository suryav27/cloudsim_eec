//
//  Scheduler.cpp
//  CloudSim
//
//  Greedy Power-Aware Scheduler (Final Stable Version)
//  Features:
//   - Deferred VM attach for sleeping hosts
//   - Force completion + auto-shutdown safeguard
//   - Prevents hang on SLA warnings
//
//  Author: Modified by ChatGPT & Suryamukhi Venigalla
//

#include "Scheduler.hpp"
#include <unordered_map>
#include <vector>
#include <iostream>

static bool migrating = false;
static std::vector<PendingAttach> pending_attaches;

// ---------------- Helpers ----------------

static inline double host_energy_bias(const MachineInfo_t& mi) {
    return (!mi.p_states.empty() ? mi.p_states[0] : 1.0);
}

static inline double host_free_mem(const MachineInfo_t& mi) {
    return (mi.memory_used < mi.memory_size)
        ? (mi.memory_size - mi.memory_used) : 0.0;
}

static inline Time_t sla_overrun_threshold(SLAType_t sla) {
    switch (sla) {
        case SLA0: return 8000000;
        case SLA1: return 3000000;
        default:   return 5000000;
    }
}

// ---------------- Scheduler ----------------

void Scheduler::Init() {
    unsigned total = Machine_GetTotal();
    if (total == 0) {
        SimOutput("Scheduler::Init(): No machines available", 0);
        return;
    }

    vmrecs.clear();
    machines.clear();
    task_start_time.clear();
    task_sla.clear();
    pending_attaches.clear();

    machines.reserve(total);
    for (unsigned i = 0; i < total; ++i) {
        MachineId_t mid = MachineId_t(i);
        machines.push_back(mid);
        auto mi = Machine_GetInfo(mid);
        if (mi.s_state != S0) Machine_SetState(mid, S0);
    }

    SimOutput("Scheduler::Init(): Machines discovered = " +
              std::to_string(machines.size()) +
              ". Using Greedy Power-Aware Scheduler (Final Stable).", 2);
}

void Scheduler::MigrationComplete(Time_t /*time*/, VMId_t vm_id) {
    SimOutput("[INFO] Migration completed for VM " + std::to_string(vm_id), 3);
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    CPUType_t need_cpu = RequiredCPUType(task_id);
    VMType_t  need_vm  = RequiredVMType(task_id);
    SLAType_t sla      = RequiredSLA(task_id);
    bool      need_gpu = IsTaskGPUCapable(task_id);
    unsigned  mem_req  = GetTaskMemory(task_id);

    Priority_t pr = (sla == SLA0 ? HIGH_PRIORITY :
                    (sla == SLA1 ? MID_PRIORITY : LOW_PRIORITY));

    // --- Try existing awake VMs ---
    size_t chosen_idx = SIZE_MAX;
    double best_score = 1e18;

    for (size_t i = 0; i < vmrecs.size(); ++i) {
        const auto &rec = vmrecs[i];
        auto mi = Machine_GetInfo(rec.host);
        if (mi.s_state != S0) continue;
        if (rec.cpu_type != need_cpu) continue;
        if (rec.vm_type  != need_vm)  continue;
        if (need_gpu && !rec.host_has_gpu) continue;
        if (host_free_mem(mi) < mem_req) continue;

        double load_ratio = (rec.running_tasks + 1.0) / (double)mi.num_cpus;
        double energy_bias = host_energy_bias(mi);
        double score = load_ratio * energy_bias;

        if (score < best_score) {
            best_score = score;
            chosen_idx = i;
        }
    }

    VMId_t chosen_vm = VMId_t(-1);
    MachineId_t chosen_host = MachineId_t(-1);

    if (chosen_idx == SIZE_MAX) {
        MachineInfo_t mi{};
        for (auto m : machines) {
            mi = Machine_GetInfo(m);
            if (mi.cpu != need_cpu) continue;
            if (need_gpu && !mi.gpus) continue;
            if (host_free_mem(mi) < mem_req) continue;
            chosen_host = m;
            break;
        }

        if (chosen_host == (MachineId_t)(-1) && !machines.empty()) {
            chosen_host = machines[0];
            mi = Machine_GetInfo(chosen_host);
        }

        auto hi = Machine_GetInfo(chosen_host);
        VMId_t vm = VM_Create(need_vm, need_cpu);

        // If host is sleeping, wake and defer attach
        if (hi.s_state != S0) {
            Machine_SetState(chosen_host, S0);
            SimOutput("[INFO] Host " + std::to_string(chosen_host) +
                      " waking — deferring attach for task " + std::to_string(task_id), 3);

            pending_attaches.push_back(PendingAttach{vm, chosen_host, task_id, pr});

            auto host_info = Machine_GetInfo(chosen_host);
            vmrecs.push_back(VMRec{
                .id            = vm,
                .host          = chosen_host,
                .vm_type       = need_vm,
                .cpu_type      = need_cpu,
                .host_has_gpu  = host_info.gpus,
                .running_tasks = 0
            });
            return;
        }

        VM_Attach(vm, chosen_host);
        auto host_info = Machine_GetInfo(chosen_host);
        vmrecs.push_back(VMRec{
            .id            = vm,
            .host          = chosen_host,
            .vm_type       = need_vm,
            .cpu_type      = need_cpu,
            .host_has_gpu  = host_info.gpus,
            .running_tasks = 0
        });
        chosen_idx = vmrecs.size() - 1;
        chosen_vm = vm;
    } else {
        chosen_vm = vmrecs[chosen_idx].id;
        chosen_host = vmrecs[chosen_idx].host;
    }

    VM_AddTask(chosen_vm, task_id, pr);
    vmrecs[chosen_idx].running_tasks++;
    task_to_vm_index[task_id] = chosen_idx;
    task_start_time[task_id] = now;
    task_sla[task_id] = sla;

    SimOutput("[INFO] Task " + std::to_string(task_id) +
              " → VM " + std::to_string(chosen_vm) +
              " on Host " + std::to_string(chosen_host), 3);
}

void Scheduler::HandleWakeComplete(MachineId_t machine_id) {
    for (size_t i = 0; i < pending_attaches.size();) {
        auto &p = pending_attaches[i];
        if (p.host == machine_id) {
            VM_Attach(p.vm, p.host);
            VM_AddTask(p.vm, p.task_id, p.pr);
            SimOutput("[INFO] Deferred attach complete on host " +
                      std::to_string(p.host) + " for task " + std::to_string(p.task_id), 3);
            pending_attaches.erase(pending_attaches.begin() + i);
        } else {
            ++i;
        }
    }
}

void Scheduler::PeriodicCheck(Time_t now) {
    static std::unordered_map<MachineId_t, Time_t> last_active;
    static int idle_checks = 0;
    static Time_t last_activity_time = 0;

    // --- SLA timeout safeguard ---
    if (!task_start_time.empty()) {
        std::vector<TaskId_t> to_force_complete;
        for (const auto &kv : task_start_time) {
            TaskId_t tid = kv.first;
            Time_t start_time = kv.second;
            SLAType_t sla = task_sla.count(tid) ? task_sla[tid] : SLA1;
            if (now > start_time + sla_overrun_threshold(sla)) {
                to_force_complete.push_back(tid);
            }
        }
        for (auto tid : to_force_complete) {
            SimOutput("[WARN] Force-completing overrun task " + std::to_string(tid), 1);
            TaskComplete(now, tid);
        }
    }

    // --- Power management ---
    for (auto m : machines) {
        auto mi = Machine_GetInfo(m);
        if (mi.active_vms == 0) {
            if (last_active[m] == 0) last_active[m] = now;
            if (now - last_active[m] > 5000000 && mi.s_state == S0) {
                Machine_SetState(m, S2);
                SimOutput("[DEBUG] Host " + std::to_string(m) + " -> S2 (idle sleep)", 3);
            }
        } else {
            if (mi.s_state != S0) {
                Machine_SetState(m, S0);
                SimOutput("[DEBUG] Host " + std::to_string(m) + " -> S0 (wake active)", 3);
            }
            last_active[m] = now;
        }
    }

    // --- Force completion safeguard (prevents hangs) ---
    bool any_active = false;
    for (auto &r : vmrecs) {
        if (r.running_tasks > 0) { any_active = true; break; }
    }

    if (!any_active && task_to_vm_index.empty()) {
        idle_checks++;
    } else {
        idle_checks = 0;
        last_activity_time = now;
    }

    if (idle_checks > 15) {
        SimOutput("[INFO] Cluster idle for extended period — forcing shutdown.", 1);
        Shutdown(now);
        return;
    }
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    auto it = task_to_vm_index.find(task_id);
    if (it != task_to_vm_index.end()) {
        size_t idx = it->second;
        if (idx < vmrecs.size() && vmrecs[idx].running_tasks > 0)
            vmrecs[idx].running_tasks--;
        task_to_vm_index.erase(it);
    }
    task_start_time.erase(task_id);
    task_sla.erase(task_id);
    SimOutput("[DEBUG] TaskComplete(): Task " + std::to_string(task_id) +
              " completed at time " + std::to_string(now), 3);
}

void Scheduler::Shutdown(Time_t time) {
    for (auto &vm : vmrecs) {
        VM_Shutdown(vm.id);
    }
    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + std::to_string(time), 4);
}

// ---------------- Global Interface ----------------

static Scheduler scheduler;

void InitScheduler() { scheduler.Init(); }
void HandleNewTask(Time_t t, TaskId_t id) { scheduler.NewTask(t, id); }
void HandleTaskCompletion(Time_t t, TaskId_t id) { scheduler.TaskComplete(t, id); }
void MemoryWarning(Time_t t, MachineId_t m) { SimOutput("[WARN] Memory overflow at machine " + std::to_string(m), 0); }
void MigrationDone(Time_t t, VMId_t vm) { scheduler.MigrationComplete(t, vm); migrating = false; }
void SchedulerCheck(Time_t t) { scheduler.PeriodicCheck(t); }

void SimulationComplete(Time_t time) {
    std::cout << "SLA violation report\n"
              << "SLA0: " << GetSLAReport(SLA0) << "%\n"
              << "SLA1: " << GetSLAReport(SLA1) << "%\n"
              << "SLA2: " << GetSLAReport(SLA2) << "%\n"
              << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour\n"
              << "Simulation run finished in " << double(time)/1000000 << " seconds\n";
    scheduler.Shutdown(time);
}

void SLAWarning(Time_t t, TaskId_t id) { SimOutput("[WARN] SLA warning for task " + std::to_string(id), 1); }

void StateChangeComplete(Time_t t, MachineId_t mid) {
    SimOutput("[INFO] State change complete for host " + std::to_string(mid), 3);
    scheduler.HandleWakeComplete(mid);
}
