Number of machine classes: 2

machine class:
{
    Number of machines: 4
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
    Memory: 16384
    S-States: [100, 80, 55, 35, 15, 5, 0]
    P-States: [10, 7, 5, 3]
    C-States: [10, 3, 1, 0]
    MIPS: [800, 600, 400, 300]
    GPUs: no
}

Number of workloads: 5

task class:
{
    Start time: 50000
    End time: 800000
    Inter arrival: 8000
    Expected runtime: 250000
    Memory: 6
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
    End time: 1000000
    Inter arrival: 15000
    Expected runtime: 300000
    Memory: 8
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
    End time: 1200000
    Inter arrival: 12000
    Expected runtime: 350000
    Memory: 10
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA1
    CPU type: ARM
    Task type: CRYPTO
    Seed: 303
}

task class:
{
    Start time: 200000
    End time: 1400000
    Inter arrival: 10000
    Expected runtime: 200000
    Memory: 8
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA0
    CPU type: ARM
    Task type: STREAM
    Seed: 404
}

task class:
{
    Start time: 250000
    End time: 1600000
    Inter arrival: 18000
    Expected runtime: 400000
    Memory: 12
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA2
    CPU type: ARM
    Task type: STREAM
    Seed: 505
}
