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
    Number of machines: 4
    CPU type: ARM
    Number of cores: 16
    Memory: 8192
    S-States: [100, 80, 55, 35, 15, 5, 0]
    P-States: [10, 7, 5, 3]
    C-States: [10, 3, 1, 0]
    MIPS: [800, 600, 400, 300]
    GPUs: no
}

Number of workloads: 4

task class:
{
    Start time: 100000
    End time: 300000
    Inter arrival: 120000
    Expected runtime: 50000
    Memory: 4
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA0
    CPU type: X86
    Task type: WEB
    Seed: 1001
}

task class:
{
    Start time: 600000
    End time: 800000
    Inter arrival: 100000
    Expected runtime: 60000
    Memory: 4
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA1
    CPU type: ARM
    Task type: CRYPTO
    Seed: 1002
}

task class:
{
    Start time: 1000000
    End time: 1600000
    Inter arrival: 200000
    Expected runtime: 150000
    Memory: 8
    VM type: LINUX
    GPU enabled: yes
    SLA type: SLA0
    CPU type: X86
    Task type: AI
    Seed: 1003
}

task class:
{
    Start time: 1800000
    End time: 2500000
    Inter arrival: 250000
    Expected runtime: 80000
    Memory: 6
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA2
    CPU type: ARM
    Task type: STREAM
    Seed: 1004
}
