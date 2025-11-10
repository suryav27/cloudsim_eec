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

class Scheduler {
public:
    void Init();
    void NewTask(Time_t now, TaskId_t task_id);
    void TaskComplete(Time_t now, TaskId_t task_id);
    void PeriodicCheck(Time_t now);
    void Shutdown(Time_t time);
    void MigrationComplete(Time_t time, VMId_t vm_id);

    bool PlaceOrDeferTask(TaskId_t task_id);  // ← ADD THIS LINE

    // Temporary: make accessible to StateChangeComplete
    std::vector<VMRec> vmrecs;
    std::unordered_map<TaskId_t, size_t> task_to_vm_index;

private:
    std::vector<MachineId_t> machines;
    std::unordered_map<MachineId_t, unsigned> machine_tasks;
};

#endif /* Scheduler_hpp */
