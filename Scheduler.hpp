#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <vector>
#include <unordered_map>

#include "Interfaces.h"            // Provided by CloudSim EEC
#include "Internal_Interfaces.h"   // Provided by CloudSim EEC
#include "SimTypes.h"              // Defines VMId_t, MachineId_t, etc.


// -----------------------------------------------------------
// Records of created VMs
// -----------------------------------------------------------

struct VMRec {
    VMId_t      id;
    MachineId_t host;
    VMType_t    vm_type;
    CPUType_t   cpu_type;
    bool        host_has_gpu;
    unsigned    running_tasks;
};

// -----------------------------------------------------------
// Pending Attach for sleeping hosts waking up
// -----------------------------------------------------------

struct PendingAttach {
    VMId_t      vm;
    MachineId_t host;
    TaskId_t    task_id;
    Priority_t  pr;
};

// -----------------------------------------------------------
// Scheduler Class
// -----------------------------------------------------------

class Scheduler {
public:
    Scheduler() = default;

    void Init();
    void NewTask(Time_t time, TaskId_t task);
    void TaskComplete(Time_t time, TaskId_t task);
    void MigrationComplete(Time_t time, VMId_t vm);
    void PeriodicCheck(Time_t time);

    // UPDATED SIGNATURE (must match Scheduler.cpp)
    void HandleWakeComplete(Time_t now, MachineId_t machine_id);

    void Shutdown(Time_t time);
    size_t FindOrCreateHPCVM(Time_t now, TaskId_t task_id, 
                         unsigned mem_req, Priority_t pr);

private:
    std::vector<MachineId_t> machines;   // All machines
    std::vector<VMRec>       vmrecs;     // All VMs created

    std::unordered_map<TaskId_t, size_t> task_to_vm_index;
    std::unordered_map<TaskId_t, Time_t> task_start_time;
    std::unordered_map<TaskId_t, SLAType_t> task_sla;
};

#endif // SCHEDULER_HPP
