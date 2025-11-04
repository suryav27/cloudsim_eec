Number of machine classes: 2

machine class:
{
    Number of machines: 3
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
    Number of machines: 3
    CPU type: ARM
    Number of cores: 16
    Memory: 16384
    S-States: [100, 80, 55, 35, 15, 5, 0]
    P-States: [10, 7, 5, 3]
    C-States: [10, 3, 1, 0]
    MIPS: [800, 600, 400, 300]
    GPUs: no
}

Number of workloads: 4

task class:
{
    Start time: 50000
    End time: 600000
    Inter arrival: 10000
    Expected runtime: 200000
    Memory: 6
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA0
    CPU type: X86
    Task type: WEB
    Seed: 111
}

task class:
{
    Start time: 120000
    End time: 900000
    Inter arrival: 15000
    Expected runtime: 250000
    Memory: 8
    VM type: LINUX
    GPU enabled: yes
    SLA type: SLA1
    CPU type: X86
    Task type: AI
    Seed: 222
}

task class:
{
    Start time: 200000
    End time: 1100000
    Inter arrival: 20000
    Expected runtime: 300000
    Memory: 10
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA1
    CPU type: ARM
    Task type: CRYPTO
    Seed: 333
}

task class:
{
    Start time: 300000
    End time: 1400000
    Inter arrival: 25000
    Expected runtime: 350000
    Memory: 8
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA2
    CPU type: ARM
    Task type: STREAM
    Seed: 444
}
