//
//  Scheduler.hpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#ifndef Scheduler_hpp
#define Scheduler_hpp

#include <vector>
#include <unordered_map>
#include "Interfaces.h"

struct VMRec {
    VMId_t      id;
    MachineId_t host;
    VMType_t    vm_type;      // LINUX / WINDOWS...
    CPUType_t   cpu_type;     // X86 / ARM
    bool        host_has_gpu; // machine has a GPU?
    size_t      running_tasks = 0; // we’ll track #active tasks ourselves
};

class Scheduler {
public:
    void Init();
    void NewTask(Time_t now, TaskId_t task_id);
    void TaskComplete(Time_t now, TaskId_t task_id);
    void PeriodicCheck(Time_t now);
    void Shutdown(Time_t time);
    void MigrationComplete(Time_t time, VMId_t vm_id);
private:
    std::vector<VMRec> vmrecs;
    std::vector<MachineId_t> machines;
    std::unordered_map<TaskId_t, size_t> task_to_vm_index; // task → index in vmrecs
};




#endif /* Scheduler_hpp */
