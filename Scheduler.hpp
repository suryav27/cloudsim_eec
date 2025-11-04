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

struct PendingItem { TaskId_t tid; unsigned mem; CPUType_t cpu; VMType_t vm; bool gpu; SLAType_t sla; Time_t arr; };


struct VMRec {
    VMId_t      id;
    MachineId_t host;
    VMType_t    vm_type;      // LINUX / WINDOWS...
    CPUType_t   cpu_type;     // X86 / ARM
    bool        host_has_gpu; // machine has a GPU?
    size_t      running_tasks = 0; // we’ll track #active tasks ourselves

    unsigned    vm_mem_used = 0;                 // sum of GetTaskMemory(...)
    std::vector<TaskId_t> tasks;                 // migration choices
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
    double GetMachineUtilization(MachineId_t m);
    std::vector<PendingItem> pending;
    std::unordered_map<MachineId_t, Time_t> idle_since;
    Time_t last_flush = 0;
    static constexpr size_t BFD_BATCH = 16;
    static constexpr Time_t  BFD_MAX_WAIT = 30000;
    static constexpr Time_t IDLE_TO_S3 = 50000;
    static constexpr Time_t IDLE_TO_S5 = 200000;
    std::vector<VMRec> vmrecs;
    std::vector<MachineId_t> machines;
    std::unordered_map<TaskId_t, size_t> task_to_vm_index; // task → index in vmrecs
};




#endif /* Scheduler_hpp */
