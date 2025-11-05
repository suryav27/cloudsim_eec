Number of machine classes: 2

machine class:
{
    Number of machines: 2
    CPU type: X86
    Number of cores: 8
    Memory: 8192
    S-States: [120, 100, 70, 40, 15, 5, 0]
    P-States: [12, 8, 6, 4]
    C-States: [12, 3, 1, 0]
    MIPS: [1000, 800, 600, 400]
    GPUs: yes
}

machine class:
{
    Number of machines: 2
    CPU type: ARM
    Number of cores: 16
    Memory: 8192
    S-States: [100, 80, 55, 35, 15, 5, 0]
    P-States: [10, 7, 5, 3]
    C-States: [10, 3, 1, 0]
    MIPS: [800, 600, 400, 300]
    GPUs: no
}

Number of workloads: 6

task class:
{
    Start time: 0
    End time: 1000000
    Inter arrival: 3000
    Expected runtime: 150000
    Memory: 2
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA0
    CPU type: X86
    Task type: WEB
    Seed: 101
}

task class:
{
    Start time: 0
    End time: 1000000
    Inter arrival: 4000
    Expected runtime: 180000
    Memory: 16
    VM type: LINUX
    GPU enabled: yes
    SLA type: SLA1
    CPU type: X86
    Task type: AI
    Seed: 202
}

task class:
{
    Start time: 0
    End time: 1000000
    Inter arrival: 5000
    Expected runtime: 200000
    Memory: 8
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA1
    CPU type: ARM
    Task type: CRYPTO
    Seed: 303
}

task class:
{
    Start time: 0
    End time: 1000000
    Inter arrival: 3500
    Expected runtime: 150000
    Memory: 10
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA0
    CPU type: ARM
    Task type: STREAM
    Seed: 404
}

task class:
{
    Start time: 0
    End time: 1000000
    Inter arrival: 6000
    Expected runtime: 100000
    Memory: 20
    VM type: LINUX
    GPU enabled: yes
    SLA type: SLA2
    CPU type: X86
    Task type: AI
    Seed: 505
}

task class:
{
    Start time: 0
    End time: 1000000
    Inter arrival: 7000
    Expected runtime: 90000
    Memory: 4
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA2
    CPU type: ARM
    Task type: STREAM
    Seed: 606
}
