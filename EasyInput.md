Number of machine classes: 1

machine class:
{
    Number of machines: 8
    CPU type: X86
    Number of cores: 8
    Memory: 16384
    S-States: [120, 100, 70, 40, 15, 5, 0]
    P-States: [12, 8, 6, 4]
    C-States: [12, 3, 1, 0]
    MIPS: [1000, 800, 600, 400]
    GPUs: no
}

Number of workloads: 2

task class:
{
    Start time: 100000
    End time: 2000000
    Inter arrival: 20000
    Expected runtime: 250000
    Memory: 512
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA0
    CPU type: X86
    Task type: WEB
    Seed: 123
}

task class:
{
    Start time: 200000
    End time: 2500000
    Inter arrival: 30000
    Expected runtime: 350000
    Memory: 1024
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA1
    CPU type: X86
    Task type: STREAM
    Seed: 234
}
