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
    void HandleSLAWarning(Time_t time, TaskId_t task_id);

private:
    std::vector<VMRec> vmrecs;
    std::vector<MachineId_t> machines;
    std::unordered_map<TaskId_t, size_t> task_to_vm_index; // task → index in vmrecs
    std::unordered_map<MachineId_t, CPUPerformance_t> machine_pstate;
    std::unordered_map<MachineId_t, Time_t> last_p_change;
    static constexpr Time_t PSTATE_COOLDOWN = 20000; // 20 ms

    CPUPerformance_t PickPState(double util) {
        if (util >= 0.80) return P0;
        if (util >= 0.50) return P1;
        if (util >= 0.25) return P2;
        return P3;
    }
    void MaybeAdjustPState(MachineId_t m, double util, Time_t now);
};




#endif /* Scheduler_hpp */
