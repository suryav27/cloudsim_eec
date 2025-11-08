//
//  Scheduler.hpp
//  CloudSim
//
//  Fixed Greedy Power-Aware Scheduler Header
//  Handles deferred VM attaches safely
//

#ifndef Scheduler_hpp
#define Scheduler_hpp

#include <vector>
#include <unordered_map>
#include "Interfaces.h"

struct VMRec {
    VMId_t      id;
    MachineId_t host;
    VMType_t    vm_type;
    CPUType_t   cpu_type;
    bool        host_has_gpu;
    size_t      running_tasks = 0;
};

struct PendingAttach {
    VMId_t vm;
    MachineId_t host;
    TaskId_t task_id;
    Priority_t pr;
};

class Scheduler {
public:
    void Init();
    void NewTask(Time_t now, TaskId_t task_id);
    void TaskComplete(Time_t now, TaskId_t task_id);
    void PeriodicCheck(Time_t now);
    void Shutdown(Time_t time);
    void MigrationComplete(Time_t time, VMId_t vm_id);
    void HandleWakeComplete(MachineId_t machine_id); // ✅ new function

private:
    std::vector<VMRec> vmrecs;
    std::vector<MachineId_t> machines;
    std::unordered_map<TaskId_t, size_t> task_to_vm_index;
    std::unordered_map<TaskId_t, Time_t> task_start_time;
    std::unordered_map<TaskId_t, SLAType_t> task_sla;
};

#endif /* Scheduler_hpp */
