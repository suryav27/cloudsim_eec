//
//  Scheduler.cpp
//  CloudSim – SLA-Aware Consolidation (final hardened wake-safe version)
//

#include "Scheduler.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <unordered_set>
#include <deque>

using namespace std;

static constexpr double UPPER_UTIL_THRESH   = 0.80;
static constexpr double LOWER_UTIL_THRESH   = 0.25;
static constexpr double SLA_VIOLATION_LIMIT = 5.0;
static constexpr unsigned MIGRATION_PERIOD  = 5;

static unsigned tick_counter = 0;

// Track ongoing states
static unordered_set<VMId_t> migrating_vms;
static unordered_set<MachineId_t> waking_machines;
static deque<TaskId_t> pending_tasks;

// Deferred migrations
struct PendingMigration {
    VMId_t vm;
    MachineId_t dst;
};
static deque<PendingMigration> pending_migrations;

// ---------- helpers ----------
static unsigned CountTasksOnMachine(const vector<VMRec>& vms, MachineId_t m) {
    unsigned sum = 0;
    for (const auto& r : vms)
        if (r.host == m) sum += static_cast<unsigned>(r.running_tasks);
    return sum;
}

static bool PickLeastLoadedHost(const vector<MachineId_t>& machines,
                                const vector<VMRec>& vms,
                                CPUType_t cpu,
                                bool need_gpu,
                                MachineId_t& out_host) {
    double best_util = numeric_limits<double>::infinity();
    bool found = false;
    for (auto m : machines) {
        auto mi = Machine_GetInfo(m);
        if (mi.cpu != cpu) continue;
        if (need_gpu && !mi.gpus) continue;
        unsigned t = CountTasksOnMachine(vms, m);
        double util = (mi.num_cpus == 0 ? 1.0 : double(t)/double(mi.num_cpus));
        if (util < best_util) {
            best_util = util;
            out_host = m;
            found = true;
        }
    }
    return found;
}

// Try to place a task safely; defer if host is sleeping
bool TryPlaceTask(vector<VMRec>& vmrecs,
                  const vector<MachineId_t>& machines,
                  unordered_map<TaskId_t, size_t>& task_to_vm_index,
                  TaskId_t task_id) {
    CPUType_t cpu      = RequiredCPUType(task_id);
    VMType_t  vm_type  = RequiredVMType(task_id);
    SLAType_t sla      = RequiredSLA(task_id);
    bool      need_gpu = IsTaskGPUCapable(task_id);

    Priority_t pr = (sla == SLA0 ? HIGH_PRIORITY :
                    (sla == SLA1 ? MID_PRIORITY : LOW_PRIORITY));

    size_t best_vm = SIZE_MAX;
    size_t best_tasks = numeric_limits<size_t>::max();

    // Reuse existing ready VM
    for (size_t i = 0; i < vmrecs.size(); ++i) {
        const auto& rec = vmrecs[i];
        if (rec.cpu_type != cpu || rec.vm_type != vm_type) continue;
        if (migrating_vms.count(rec.id)) continue;
        auto mi = Machine_GetInfo(rec.host);
        if (need_gpu && !mi.gpus) continue;
        if (mi.s_state != S0) continue;
        if (rec.running_tasks < best_tasks) {
            best_tasks = rec.running_tasks;
            best_vm = i;
        }
    }

    if (best_vm == SIZE_MAX) {
        // Pick a host
        MachineId_t host = (MachineId_t)(-1);
        if (!PickLeastLoadedHost(machines, vmrecs, cpu, need_gpu, host)) {
            SimOutput("NewTask(): No compatible host for task " + to_string(task_id), 0);
            return false;
        }

        auto mi = Machine_GetInfo(host);
        if (mi.s_state != S0) {
            if (!waking_machines.count(host)) {
                Machine_SetState(host, S0);
                waking_machines.insert(host);
                SimOutput("NewTask(): Host " + to_string(host) +
                          " waking up; deferring task " + to_string(task_id), 2);
            }
            return false;
        }

        VMId_t vm = VM_Create(vm_type, cpu);
        VM_Attach(vm, host);
        vmrecs.push_back(VMRec{vm, host, vm_type, cpu, mi.gpus, 0});
        best_vm = vmrecs.size() - 1;
    }

    VM_AddTask(vmrecs[best_vm].id, task_id, pr);
    vmrecs[best_vm].running_tasks++;
    task_to_vm_index[task_id] = best_vm;

    SimOutput("SLA-Aware Consolidation: Assigned task " + to_string(task_id) +
              " → machine " + to_string(vmrecs[best_vm].host), 2);
    return true;
}

static bool PickDestForConsolidation(const vector<MachineId_t>& machines,
                                     const vector<VMRec>& vms,
                                     CPUType_t cpu,
                                     MachineId_t& out_dst) {
    double best_util = numeric_limits<double>::infinity();
    bool found = false;
    for (auto m : machines) {
        auto mi = Machine_GetInfo(m);
        if (mi.cpu != cpu) continue;
        unsigned t = CountTasksOnMachine(vms, m);
        double util = (mi.num_cpus == 0 ? 1.0 : double(t) / double(mi.num_cpus));
        if (util < UPPER_UTIL_THRESH && util < best_util) {
            best_util = util;
            out_dst = m;
            found = true;
        }
    }
    return found;
}

// ---------- Scheduler ----------
void Scheduler::Init() {
    unsigned total = Machine_GetTotal();
    SimOutput("InitScheduler(): Initializing SLA-Aware Consolidation Scheduler", 2);
    vmrecs.clear();
    machines.clear();
    machines.reserve(total);

    for (unsigned i = 0; i < total; ++i) {
        MachineId_t mid = MachineId_t(i);
        machines.push_back(mid);
        auto mi = Machine_GetInfo(mid);
        if (mi.s_state != S0) Machine_SetState(mid, S0);
    }
    SimOutput("SLA-Aware Consolidation Scheduler initialized, total machines = " + to_string(total), 2);
}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    migrating_vms.erase(vm_id);
    SimOutput("MigrationComplete(): VM " + to_string(vm_id) +
              " migration done at " + to_string(time), 3);
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    if (!TryPlaceTask(vmrecs, machines, task_to_vm_index, task_id))
        pending_tasks.push_back(task_id);
}

void Scheduler::PeriodicCheck(Time_t now) {
    tick_counter++;

    // Retry deferred tasks
    size_t pend = pending_tasks.size();
    for (size_t i = 0; i < pend; ++i) {
        TaskId_t tid = pending_tasks.front();
        pending_tasks.pop_front();
        if (!TryPlaceTask(vmrecs, machines, task_to_vm_index, tid))
            pending_tasks.push_back(tid);
    }

    // Retry pending migrations when destination is awake
    size_t mig_count = pending_migrations.size();
    for (size_t i = 0; i < mig_count; ++i) {
        PendingMigration pm = pending_migrations.front();
        pending_migrations.pop_front();
        auto mi = Machine_GetInfo(pm.dst);
        if (mi.s_state == S0 && !waking_machines.count(pm.dst)) {
            try {
                VM_Migrate(pm.vm, pm.dst);
                migrating_vms.insert(pm.vm);
                SimOutput("Deferred migration now executing: VM " +
                          to_string(pm.vm) + " → " + to_string(pm.dst), 2);
            } catch (...) {
                SimOutput("Deferred migration failed for VM " + to_string(pm.vm), 1);
            }
        } else {
            pending_migrations.push_back(pm);
        }
    }

    // Periodic consolidation
    if (tick_counter % MIGRATION_PERIOD != 0) return;

    double s0 = GetSLAReport(SLA0), s1 = GetSLAReport(SLA1), s2 = GetSLAReport(SLA2);
    if (s0 > SLA_VIOLATION_LIMIT || s1 > SLA_VIOLATION_LIMIT || s2 > SLA_VIOLATION_LIMIT) {
        for (auto m : machines) {
            auto mi = Machine_GetInfo(m);
            if (mi.s_state != S0) Machine_SetState(m, S0);
        }
        SimOutput("SLA degraded → waking all machines.", 2);
        return;
    }

    // Consolidation logic
    for (auto src : machines) {
        auto mi_src = Machine_GetInfo(src);
        unsigned src_tasks = CountTasksOnMachine(vmrecs, src);
        double src_util = (mi_src.num_cpus == 0 ? 1.0 : double(src_tasks)/double(mi_src.num_cpus));

        if (src_tasks == 0) {
            if (mi_src.s_state == S0) {
                Machine_SetState(src, S3);
                SimOutput("Machine " + to_string(src) + " idle → S3", 3);
            }
            continue;
        }
        if (src_util >= LOWER_UTIL_THRESH) continue;

        // pick VM on src
        VMId_t vmid = (VMId_t)(-1);
        for (const auto& r : vmrecs)
            if (r.host == src && !migrating_vms.count(r.id)) { vmid = r.id; break; }
        if (vmid == (VMId_t)(-1)) continue;

        CPUType_t cpu = mi_src.cpu;
        MachineId_t dst;
        if (!PickDestForConsolidation(machines, vmrecs, cpu, dst) || dst == src)
            continue;

        auto mi_dst = Machine_GetInfo(dst);
        if (mi_dst.s_state != S0) {
            if (!waking_machines.count(dst)) {
                Machine_SetState(dst, S0);
                waking_machines.insert(dst);
                SimOutput("Dest " + to_string(dst) + " asleep → waking, deferring migration", 2);
            }
            pending_migrations.push_back({vmid, dst});
            continue;
        }

        try {
            VM_Migrate(vmid, dst);
            migrating_vms.insert(vmid);
            SimOutput("Migrating VM " + to_string(vmid) + " → " + to_string(dst), 2);
        } catch (...) {
            SimOutput("Migration failed for VM " + to_string(vmid), 1);
        }
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
    SimOutput("TaskComplete(): Task " + to_string(task_id) + " done", 3);
}

void Scheduler::Shutdown(Time_t time) {
    for (auto & r : vmrecs) VM_Shutdown(r.id);
    for (auto m : machines) Machine_SetState(m, S5);
    SimOutput("SLA-Aware Consolidation: Simulation complete", 2);

    cout << "SLA violation report\n";
    cout << "SLA0: " << GetSLAReport(SLA0) << "%\n";
    cout << "SLA1: " << GetSLAReport(SLA1) << "%\n";
    cout << "SLA2: " << GetSLAReport(SLA2) << "%\n";
    cout << "Total Energy: " << Machine_GetClusterEnergy() << " KW-Hour\n";
    cout << "Simulation Time: " << double(time)/1e6 << " seconds\n";
}

// ---------- Simulator callbacks ----------
static Scheduler scheduler;
void InitScheduler() { scheduler.Init(); }
void HandleNewTask(Time_t t, TaskId_t id) { scheduler.NewTask(t, id); }
void HandleTaskCompletion(Time_t t, TaskId_t id) { scheduler.TaskComplete(t, id); }
void MemoryWarning(Time_t t, MachineId_t m) { SimOutput("MemoryWarning(): machine " + to_string(m), 1); }
void MigrationDone(Time_t t, VMId_t id) { scheduler.MigrationComplete(t, id); }
void SchedulerCheck(Time_t t) { scheduler.PeriodicCheck(t); }
void SimulationComplete(Time_t t) { scheduler.Shutdown(t); }
void SLAWarning(Time_t t, TaskId_t id) { SimOutput("SLAWarning(): task " + to_string(id), 1); }

void StateChangeComplete(Time_t t, MachineId_t m) {
    waking_machines.erase(m);
    SimOutput("StateChangeComplete(): machine " + to_string(m) + " now awake (S0)", 3);
}
