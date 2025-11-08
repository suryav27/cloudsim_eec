machine class:
{
        Number of machines: 12
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [100, 80, 60, 40, 20, 10, 0]
        P-States: [10, 8, 6, 4]
        C-States: [10, 4, 2, 0]
        MIPS: [3200, 2600, 2000, 1500]
        GPUs: no
}

machine class:
{
        Number of machines: 12
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [150, 120, 100, 80, 60, 40, 0]
        P-States: [40, 32, 24, 12]
        C-States: [40, 16, 8, 0]
        MIPS: [3200, 2600, 2000, 1500]
        GPUs: yes
}

machine class:
{
        Number of machines: 8
        CPU type: ARM
        Number of cores: 6
        Memory: 12288
        S-States: [60, 45, 30, 20, 10, 5, 0]
        P-States: [6, 4, 3, 1]
        C-States: [6, 3, 1, 0]
        MIPS: [2000, 1600, 1200, 800]
        GPUs: no
}

task class:
{
        Start time: 80000
        End time : 15000000
        Inter arrival: 120000
        Expected runtime: 1200000
        Memory: 8
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA1
        CPU type: X86
        Task type: WEB
        Seed: 12345
}

task class:
{
        Start time: 4000000
        End time :  9000000
        Inter arrival: 1200
        Expected runtime: 4000000
        Memory: 8
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA0
        CPU type: X86
        Task type: HPC
        Seed: 7777
}

task class:
{
        Start time: 9500000
        End time :  13500000
        Inter arrival: 2000
        Expected runtime: 5000000
        Memory: 6
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: ARM
        Task type: WEB
        Seed: 9900
}

task class:
{
        Start time: 11500000
        End time : 14500000
        Inter arrival: 1000
        Expected runtime: 4000000
        Memory: 8
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA0
        CPU type: X86
        Task type: HPC
        Seed: 5555
}
