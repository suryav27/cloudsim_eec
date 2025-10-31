Number of machine classes: 2

machine class:
{
    Number of machines: 5
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
    Number of machines: 5
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
    End time: 1200000
    Inter arrival: 12000
    Expected runtime: 200000
    Memory: 8
    VM type: LINUX
    GPU enabled: yes
    SLA type: SLA0
    CPU type: X86
    Task type: AI
    Seed: 555
}

task class:
{
    Start time: 100000
    End time: 1400000
    Inter arrival: 30000
    Expected runtime: 100000
    Memory: 6
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA1
    CPU type: ARM
    Task type: CRYPTO
    Seed: 666
}

task class:
{
    Start time: 200000
    End time: 1500000
    Inter arrival: 25000
    Expected runtime: 150000
    Memory: 4
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA2
    CPU type: X86
    Task type: WEB
    Seed: 777
}

task class:
{
    Start time: 1000000
    End time: 2200000
    Inter arrival: 50000
    Expected runtime: 400000
    Memory: 8
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA2
    CPU type: ARM
    Task type: STREAM
    Seed: 888
}
