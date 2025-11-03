//
//  Scheduler.cpp
//  CloudSim
//
//  Greedy Power-Aware Scheduler (Stable Version)
//  Author: Modified by ChatGPT & Suryamukhi Venigalla
//

#include "Scheduler.hpp"
#include <unordered_map>

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
        // Make sure hosts are awake at the beginning
        if (mi.s_state != S0) Machine_SetState(mid, S0);
    }

    SimOutput("Scheduler::Init(): Machines discovered = " +
              to_string(machines.size()) +
              ". Using Greedy Power-Aware Scheduler.", 2);
}

// ------------------------------------------------------------
// Migration completion handler
// ------------------------------------------------------------
void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    SimOutput("[INFO] Migration completed for VM " + to_string(vm_id), 3);
}

// ------------------------------------------------------------
// Greedy Power-Aware Task Assignment
// ------------------------------------------------------------
void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    CPUType_t need_cpu = RequiredCPUType(task_id);
    VMType_t  need_vm  = RequiredVMType(task_id);
    SLAType_t sla      = RequiredSLA(task_id);
    bool      need_gpu = IsTaskGPUCapable(task_id);
    unsigned  mem_req  = GetTaskMemory(task_id);

    Priority_t pr = (sla == SLA0 ? HIGH_PRIORITY :
                     sla == SLA1 ? MID_PRIORITY : LOW_PRIORITY);

    size_t chosen_idx = SIZE_MAX;
    double best_score = 1e18; // lower = better energy efficiency

    // 🔹 1. Prefer existing VMs on awake machines
    for (size_t i = 0; i < vmrecs.size(); ++i) {
        const auto &rec = vmrecs[i];
        auto mi = Machine_GetInfo(rec.host);

        if (mi.s_state != S0) continue; // skip sleeping hosts
        if (rec.cpu_type != need_cpu) continue;
        if (rec.vm_type != need_vm) continue;
        if (need_gpu && !rec.host_has_gpu) continue;

        unsigned free_mem = (mi.memory_size > mi.memory_used)
                            ? mi.memory_size - mi.memory_used : 0;
        if (free_mem < mem_req) continue;

        double load_ratio = (rec.running_tasks + 1.0) / (double)mi.num_cpus;
        double energy_bias = mi.p_states[0]; // base energy draw
        double score = load_ratio * energy_bias;

        if (score < best_score) {
            best_score = score;
            chosen_idx = i;
        }
    }

    VMId_t chosen_vm;
    MachineId_t chosen_host;

    // 🔹 2. If no existing VM found, wake up a suitable host and create one
    if (chosen_idx == SIZE_MAX) {
        chosen_host = (MachineId_t)(-1);
        for (auto m : machines) {
            auto mi = Machine_GetInfo(m);
            if (mi.cpu != need_cpu) continue;
            if (need_gpu && !mi.gpus) continue;

            unsigned free_mem = (mi.memory_size > mi.memory_used)
                                ? mi.memory_size - mi.memory_used : 0;
            if (free_mem < mem_req) continue;

            // Wake up sleeping host if necessary
            if (mi.s_state != S0) {
                Machine_SetState(m, S0);
                SimOutput("[DEBUG] Host " + to_string(m) +
                          " waking up for new task", 3);
            }

            chosen_host = m;
            break;
        }

        // 🔹 3. Safety fallback if still none found
        if (chosen_host == (MachineId_t)(-1) && !machines.empty()) {
            chosen_host = machines[0];
            Machine_SetState(chosen_host, S0);
            SimOutput("[SAFETY] Forcing host 0 awake — no compatible host found!", 0);
        }

        VMId_t vm = VM_Create(need_vm, need_cpu);
        VM_Attach(vm, chosen_host);

        auto mi = Machine_GetInfo(chosen_host);
        vmrecs.push_back(VMRec{
            .id            = vm,
            .host          = chosen_host,
            .vm_type       = need_vm,
            .cpu_type      = need_cpu,
            .host_has_gpu  = mi.gpus,
            .running_tasks = 0
        });
        chosen_idx = vmrecs.size() - 1;
        chosen_vm = vm;
    } else {
        chosen_vm = vmrecs[chosen_idx].id;
        chosen_host = vmrecs[chosen_idx].host;
    }

    // 🔹 4. Assign task
    VM_AddTask(chosen_vm, task_id, pr);
    vmrecs[chosen_idx].running_tasks++;
    task_to_vm_index[task_id] = chosen_idx;

    SimOutput("[INFO] Task " + to_string(task_id) + " → VM " +
              to_string(chosen_vm) + " on Host " +
              to_string(chosen_host), 3);
}

// ------------------------------------------------------------
// Periodic Monitoring (Energy Tracking + Safe Sleep)
// ------------------------------------------------------------
void Scheduler::PeriodicCheck(Time_t now) {
    static std::unordered_map<MachineId_t, Time_t> last_active;

    double total_energy = Machine_GetClusterEnergy();
    SimOutput("[DEBUG] PeriodicCheck: cluster energy=" + to_string(total_energy), 3);

    for (auto m : machines) {
        auto mi = Machine_GetInfo(m);

        // Only consider machines with NO active VMs
        if (mi.active_vms == 0) {
            if (last_active[m] == 0) last_active[m] = now;
            if (now - last_active[m] > 5000000) { // 5s idle threshold
                if (mi.s_state == S0) {
                    Machine_SetState(m, S2);
                    SimOutput("[DEBUG] Host " + to_string(m) +
                              " -> S2 (sleep after idle)", 3);
                }
            }
        } else {
            // Machine active, keep awake
            if (mi.s_state != S0) {
                Machine_SetState(m, S0);
                SimOutput("[DEBUG] Host " + to_string(m) +
                          " -> S0 (wake active)", 3);
            }
            last_active[m] = now; // reset idle timer
        }
    }
}

// ------------------------------------------------------------
// Task Completion (reduce VM load)
// ------------------------------------------------------------
void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    auto it = task_to_vm_index.find(task_id);
    if (it != task_to_vm_index.end()) {
        size_t idx = it->second;
        if (idx < vmrecs.size() && vmrecs[idx].running_tasks > 0)
            vmrecs[idx].running_tasks--;
        task_to_vm_index.erase(it);
    }

    SimOutput("[INFO] Task " + to_string(task_id) +
              " completed at time " + to_string(now), 3);
}

// ------------------------------------------------------------
// Shutdown + Reporting
// ------------------------------------------------------------
void Scheduler::Shutdown(Time_t time) {
    for (auto &vm : vmrecs) {
        VM_Shutdown(vm.id);
    }

    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}

// ------------------------------------------------------------
// Public interface to simulator
// ------------------------------------------------------------
static Scheduler scheduler;

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 4);
    scheduler.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) +
              " at time " + to_string(time), 4);
    scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) +
              " completed at time " + to_string(time), 4);
    scheduler.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    SimOutput("[WARN] Memory overflow at machine " +
              to_string(machine_id) + " at time " + to_string(time), 0);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) +
              " was completed at time " + to_string(time), 4);
    scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    scheduler.PeriodicCheck(time);
}

void SimulationComplete(Time_t time) {
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;

    scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {
    SimOutput("[WARN] SLA warning for task " + to_string(task_id), 1);
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    SimOutput("[INFO] State change complete for host " +
              to_string(machine_id), 3);
}


