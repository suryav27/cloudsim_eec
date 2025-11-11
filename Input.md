
Number of machine classes: 4

machine class:
{
   Number of machines: 6
   CPU type: X86
   Number of cores: 8
   Memory: 16384
   S-States: [150, 120, 100, 80, 40, 10, 0]
   P-States: [12, 9, 6, 3]
   C-States: [12, 3, 1, 0]
   MIPS: [3000, 2500, 2000, 1500]
   GPUs: yes
}

machine class:
{
   Number of machines: 8
   CPU type: X86
   Number of cores: 8
   Memory: 16384
   S-States: [120, 100, 70, 40, 15, 5, 0]
   P-States: [10, 7, 5, 3]
   C-States: [10, 3, 1, 0]
   MIPS: [2200, 1800, 1400, 1000]
   GPUs: no
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
   MIPS: [1800, 1400, 1000, 700]
   GPUs: yes
}

machine class:
{
   Number of machines: 10
   CPU type: ARM
   Number of cores: 16
   Memory: 16384
   S-States: [80, 60, 40, 20, 10, 4, 0]
   P-States: [8, 5, 3, 2]
   C-States: [8, 2, 1, 0]
   MIPS: [1200, 900, 700, 500]
   GPUs: no
}

Number of workloads: 6

task class:
{
   Start time: 50000
   End time: 25000000
   Inter arrival: 800
   Expected runtime: 400000
   Memory: 8
   VM type: LINUX
   GPU enabled: no
   SLA type: SLA0
   CPU type: X86
   Task type: WEB
   Seed: 1111
}

task class:
{
   Start time: 100000
   End time: 28000000
   Inter arrival: 1500
   Expected runtime: 2500000
   Memory: 14
   VM type: LINUX
   GPU enabled: yes
   SLA type: SLA0
   CPU type: X86
   Task type: AI
   Seed: 2222
}

task class:
{
    Start time: 200000
    End time: 30000000
    Inter arrival: 5000
    Expected runtime: 3000000
    Memory: 12
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA1
    CPU type: ARM
    Task type: HPC
    Distribution: poisson
    Seed: 3333
}

task class:
{
    Start time: 400000
    End time: 32000000
    Inter arrival: 2500
    Expected runtime: 2000000
    Memory: 10
    VM type: LINUX
    GPU enabled: yes
    SLA type: SLA1
    CPU type: ARM
    Task type: AI
    Distribution: uniform
    Seed: 4444
}

task class:
{
    Start time: 600000
    End time: 34000000
    Inter arrival: 3000
    Expected runtime: 1000000
    Memory: 8
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA2
    CPU type: X86
    Task type: CRYPTO
    Distribution: normal
    Seed: 5555
}

task class:
{
    Start time: 1000000
    End time: 36000000
    Inter arrival: 20000
    Expected runtime: 8000000
    Memory: 16
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA3
    CPU type: ARM
    Task type: HPC
    Distribution: exponential
    Seed: 6666
}
