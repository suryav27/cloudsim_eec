
machine class:
{
        Number of machines: 6
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [120, 100, 80, 60, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [3000, 2400, 2000, 1500]
        GPUs: no
}

machine class:
{
        Number of machines: 4
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [180, 150, 120, 80, 50, 20, 0]
        P-States: [48, 36, 30, 12]
        C-States: [48, 12, 8, 0]
        MIPS: [3000, 2400, 2000, 1500]
        GPUs: yes
}

machine class:
{
        Number of machines: 2
        CPU type: ARM
        Number of cores: 8
        Memory: 8192
        S-States: [80, 40, 28, 20, 12, 8, 0]
        P-States: [8, 4, 2, 1]
        C-States: [8, 2, 1, 0]
        MIPS: [2000, 1500, 1200, 800]
        GPUs: no
}

machine class:
{
        Number of machines: 2
        CPU type: POWER
        Number of cores: 32
        Memory: 65536
        S-States: [120, 60, 30, 15, 8, 4, 0]
        P-States: [8, 4, 2, 1]
        C-States: [8, 2, 1, 0]
        MIPS: [1500, 1200, 1000, 800]
        GPUs: no
}

task class:
{
        Start time: 10000
        End time : 6000000
        Inter arrival: 3000
        Expected runtime: 5000000
        Memory: 12
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA0
        CPU type: X86
        Task type: AI
        Seed: 111
}

task class:
{
        Start time: 6000000
        End time : 12000000
        Inter arrival: 40000
        Expected runtime: 800000
        Memory: 8
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA1
        CPU type: X86
        Task type: WEB
        Seed: 222
}

task class:
{
        Start time: 12000000
        End time : 20000000
        Inter arrival: 20000
        Expected runtime: 3000000
        Memory: 16
        VM type: AIX
        GPU enabled: no
        SLA type: SLA2
        CPU type: POWER
        Task type: HPC
        Seed: 333
}

task class:
{
        Start time: 20000000
        End time : 26000000
        Inter arrival: 120000
        Expected runtime: 600000
        Memory: 6
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA1
        CPU type: ARM
        Task type: WEB
        Seed: 444
}

task class:
{
        Start time: 26000000
        End time : 32000000
        Inter arrival: 5000
        Expected runtime: 4000000
        Memory: 10
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA0
        CPU type: X86
        Task type: AI
        Seed: 555
}
