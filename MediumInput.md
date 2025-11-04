Number of machine classes: 2

machine class:
{
    Number of machines: 6
    CPU type: X86
    Number of cores: 8
    Memory: 16384
    S-States: [120, 100, 70, 40, 15, 5, 0]
    P-States: [12, 8, 6, 4]
    C-States: [12, 3, 1, 0]
    MIPS: [1000, 800, 600, 400]
    GPUs: yes
}

machine class:
{
    Number of machines: 6
    CPU type: ARM
    Number of cores: 12
    Memory: 12288
    S-States: [100, 80, 55, 35, 15, 5, 0]
    P-States: [10, 7, 5, 3]
    C-States: [10, 3, 1, 0]
    MIPS: [800, 600, 400, 300]
    GPUs: no
}

Number of workloads: 3

task class:
{
    Start time: 80000
    End time: 1500000
    Inter arrival: 12000
    Expected runtime: 300000
    Memory: 8
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA0
    CPU type: X86
    Task type: WEB
    Seed: 101
}

task class:
{
    Start time: 100000
    End time: 1800000
    Inter arrival: 10000
    Expected runtime: 350000
    Memory: 10
    VM type: LINUX
    GPU enabled: yes
    SLA type: SLA1
    CPU type: X86
    Task type: AI
    Seed: 202
}

task class:
{
    Start time: 150000
    End time: 2000000
    Inter arrival: 15000
    Expected runtime: 300000
    Memory: 12
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA2
    CPU type: ARM
    Task type: STREAM
    Seed: 303
}
