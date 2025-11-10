Number of machine classes: 2

machine class:
{
    Number of machines: 16
    CPU type: X86
    Number of cores: 8
    Memory: 16384
    S-States: [120, 100, 100, 80, 40, 10, 0]
    P-States: [12, 8, 6, 4]
    C-States: [12, 3, 1, 0]
    MIPS: [3000, 2400, 2000, 1500]
    GPUs: no
}

machine class:
{
    Number of machines: 8
    CPU type: X86
    Number of cores: 4
    Memory: 8192
    S-States: [40, 20, 16, 12, 10, 4, 0]
    P-States: [4, 2, 2, 1]
    C-States: [4, 1, 1, 0]
    MIPS: [1500, 1200, 1000, 600]
    GPUs: yes
}

Number of workloads: 4

task class:
{
    Start time: 60000
    End time: 97200000000
    Inter arrival: 120000
    Expected runtime: 800000
    Memory: 8
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA0
    CPU type: X86
    Task type: WEB
    Seed: 11111
}

task class:
{
    Start time: 100000
    End time: 86400000000
    Inter arrival: 150000
    Expected runtime: 900000
    Memory: 6
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA1
    CPU type: X86
    Task type: STREAM
    Seed: 22222
}

task class:
{
    Start time: 200000
    End time: 86400000000
    Inter arrival: 200000
    Expected runtime: 1200000
    Memory: 10
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA2
    CPU type: X86
    Task type: CRYPTO
    Seed: 33333
}

task class:
{
    Start time: 300000
    End time: 97200000000
    Inter arrival: 180000
    Expected runtime: 1000000
    Memory: 12
    VM type: LINUX
    GPU enabled: yes
    SLA type: SLA1
    CPU type: X86
    Task type: AI
    Seed: 44444
}
