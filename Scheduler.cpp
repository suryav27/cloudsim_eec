//
//  Scheduler.cpp
//  CloudSim – SLA-Aware (minimal, wake-safe, no defers, no migrations)
//  Always keeps hosts S0, attaches immediately, never queues/deadlocks.
//

#include "Scheduler.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>
using namespace std;

// -------------------- Helpers --------------------
static Priority_t PriFromSLA(SLAType_t sla) {
    return (sla == SLA0 ? HIGH_PRIORITY :
           (sla == SLA1 ? MID_PRIORITY  : LOW_PRIORITY));
}

static bool HostCompatible(const MachineInfo_t& mi, CPUType_t need_cpu, bool need_gpu) {
    if (mi.cpu != need_cpu) return false;
    if (need_gpu && !mi.gpus) return false;
    return true;
}

// Find the least-loaded **awake (S0)** compatible host.
static bool PickAwakeHost(const vector<MachineId_t>& machines,
                          const vector<VMRec>& vmrecs,
                          CPUType_t need_cpu, bool need_gpu,
                          MachineId_t& out_host)
{
    bool found = false;
    double best_util = numeric_limits<double>::infinity();

    for (auto m : machines) {
        auto mi = Machine_GetInfo(m);
        if (!HostCompatible(mi, need_cpu, need_gpu)) continue;

        // keep everything awake
        if (mi.s_state != S0) Machine_SetState(m, S0);

        double denom = (mi.num_cpus > 0 ? double(mi.num_cpus) : 1.0);
        double util  = double(mi.active_vms) / denom;

        if (!found || util < best_util) {
            found = true;
            best_util = util;
            out_host = m;
        }
    }
    return found;
}

// Try to reuse an existing VM (same VM/CPU type) on an awake host.
static ssize_t PickExistingVMReady(const vector<VMRec>& vmrecs,
                                   VMType_t need_vm, CPUType_t need_cpu, bool need_gpu)
{
    ssize_t best = -1;
    size_t best_load = numeric_limits<size_t>::max();

    for (size_t i = 0; i < vmrecs.size(); ++i) {
        const auto& r = vmrecs[i];
        if (r.vm_type != need_vm || r.cpu_type != need_cpu) continue;

        auto hi = Machine_GetInfo(r.host);
        if (hi.s_state != S0) {
            Machine_SetState(r.host, S0); // enforce awake
        }
        if (need_gpu && !hi.gpus) continue;

        if (r.running_tasks < best_load) {
            best_load = r.running_tasks;
            best = (ssize_t)i;
        }
    }
    return best;
}

// Scheduler
void Scheduler::Init() {
    unsigned total = Machine_GetTotal();

    vmrecs.clear();
    machines.clear();
    task_to_vm_index.clear();
    machine_tasks.clear();

    machines.reserve(total);
    for (unsigned i = 0; i < total; ++i) {
        MachineId_t mid = (MachineId_t)i;
        machines.push_back(mid);
        auto mi = Machine_GetInfo(mid);
        if (mi.s_state != S0) Machine_SetState(mid, S0);  // keep awake, always
    }
    SimOutput("SLA-Aware (minimal): initialized with " + to_string(total) + " machines", 2);
}

void Scheduler::MigrationComplete(Time_t /*time*/, VMId_t /*vm_id*/) {
    // No migrations in this minimal scheduler.
}

void Scheduler::NewTask(Time_t /*now*/, TaskId_t task_id) {
    CPUType_t need_cpu = RequiredCPUType(task_id);
    VMType_t  need_vm  = RequiredVMType(task_id);
    SLAType_t sla      = RequiredSLA(task_id);
    bool      need_gpu = IsTaskGPUCapable(task_id);
    Priority_t pr      = PriFromSLA(sla);

    // 1) Try to reuse an existing ready VM
    ssize_t vm_idx = PickExistingVMReady(vmrecs, need_vm, need_cpu, need_gpu);
    if (vm_idx >= 0) {
        auto host = vmrecs[(size_t)vm_idx].host;
        auto hi = Machine_GetInfo(host);
        if (hi.s_state != S0) Machine_SetState(host, S0);

        VM_AddTask(vmrecs[(size_t)vm_idx].id, task_id, pr);
        vmrecs[(size_t)vm_idx].running_tasks++;
        task_to_vm_index[task_id] = (size_t)vm_idx;
        machine_tasks[host]++;

        SimOutput("SLA-Aware (minimal): Assigned task " + to_string(task_id) +
                  " → existing VM on host " + to_string(host), 2);
        return;
    }

    // 2) Create VM on least-loaded awake compatible host
    MachineId_t host = (MachineId_t)(-1);
    if (!PickAwakeHost(machines, vmrecs, need_cpu, need_gpu, host)) {
        SimOutput("NewTask(): No compatible host for task " + to_string(task_id), 0);
        return;
    }

    // Ensure host awake (belt-and-suspenders)
    auto hi = Machine_GetInfo(host);
    if (hi.s_state != S0) Machine_SetState(host, S0);

    VMId_t vm = VM_Create(need_vm, need_cpu);
    // Attach must be safe because we never let host sleep
    VM_Attach(vm, host);

    vmrecs.push_back(VMRec{vm, host, need_vm, need_cpu, hi.gpus, 0});
    size_t idx = vmrecs.size() - 1;

    VM_AddTask(vm, task_id, pr);
    vmrecs[idx].running_tasks++;
    task_to_vm_index[task_id] = idx;
    machine_tasks[host]++;

    SimOutput("SLA-Aware (minimal): Created VM " + to_string(vm) +
              " on host " + to_string(host) + ", assigned task " + to_string(task_id), 2);
}

void Scheduler::PeriodicCheck(Time_t /*now*/) {
    // No migrations or sleeping; nothing to do.
}

void Scheduler::TaskComplete(Time_t /*now*/, TaskId_t task_id) {
    auto it = task_to_vm_index.find(task_id);
    if (it != task_to_vm_index.end()) {
        size_t idx = it->second;
        if (idx < vmrecs.size() && vmrecs[idx].running_tasks > 0)
            vmrecs[idx].running_tasks--;

        // decrement per-host task count
        if (idx < vmrecs.size()) {
            MachineId_t h = vmrecs[idx].host;
            auto mit = machine_tasks.find(h);
            if (mit != machine_tasks.end() && mit->second > 0) mit->second--;
        }
        task_to_vm_index.erase(it);
    }
}

void Scheduler::Shutdown(Time_t time) {
    for (auto & r : vmrecs) VM_Shutdown(r.id);

    cout << "SLA violation report\n";
    cout << "SLA0: " << GetSLAReport(SLA0) << "%\n";
    cout << "SLA1: " << GetSLAReport(SLA1) << "%\n";
    cout << "SLA2: " << GetSLAReport(SLA2) << "%\n";
    cout << "Total Energy: " << Machine_GetClusterEnergy() << " KW-Hour\n";
    cout << "Simulation Time: " << double(time)/1e6 << " seconds\n";
}

// -------------------- Simulator callbacks --------------------
static Scheduler scheduler;
void InitScheduler() { scheduler.Init(); }
void HandleNewTask(Time_t t, TaskId_t id) { scheduler.NewTask(t, id); }
void HandleTaskCompletion(Time_t t, TaskId_t id) { scheduler.TaskComplete(t, id); }
void MemoryWarning(Time_t t, MachineId_t m) { SimOutput("MemoryWarning(): machine " + to_string(m), 1); }
void MigrationDone(Time_t t, VMId_t id) { scheduler.MigrationComplete(t, id); }
void SchedulerCheck(Time_t t) { scheduler.PeriodicCheck(t); }
void SimulationComplete(Time_t t) { scheduler.Shutdown(t); }
void SLAWarning(Time_t, TaskId_t id) { SimOutput("[WARN] SLA warning for task " + to_string(id), 1); }
void StateChangeComplete(Time_t, MachineId_t) { /* unused in this minimal scheduler */ }
