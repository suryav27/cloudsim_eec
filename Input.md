Number of machine classes: 2

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
   GPUs: yes
}

machine class:
{
   Number of machines: 12
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
   Start time: 60000
   End time: 700000
   Inter arrival: 5000
   Expected runtime: 120000
   Memory: 8
   VM type: LINUX
   GPU enabled: no
   SLA type: SLA0
   CPU type: X86
   Task type: WEB
   Seed: 520230
}

task class:
{
   Start time: 100000
   End time: 1300000
   Inter arrival: 20000
   Expected runtime: 3000000
   Memory: 12
   VM type: LINUX
   GPU enabled: yes
   SLA type: SLA1
   CPU type: X86
   Task type: AI
   Seed: 952023
}

task class: 
{
    Start time: 200000
    End time: 1500000
    Inter arrival: 9000
    Expected runtime: 1500000
    Memory: 16
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA1
    CPU type: ARM
    Task type: ANALYTICS
    Distribution: poisson
    Seed: 320230
}

task class: 
{
    Start time: 300000
    End time: 1800000
    Inter arrival: 25000
    Expected runtime: 400000
    Memory: 4
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA2
    CPU type: ARM
    Task type: BACKGROUND
    Distribution: uniform
    Seed: 820230
}

task class: 
{
    Start time: 1300000
    End time: 2000000
    Inter arrival: 8000
    Expected runtime: 600000
    Memory: 6
    VM type: WINDOWS
    GPU enabled: no
    SLA type: SLA1
    CPU type: X86
    Task type: SURGE_TEST
    Distribution: normal
    Seed: 620230
}