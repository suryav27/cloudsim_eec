machine class:
{
        Number of machines: 10
        CPU type: X86
        Number of cores: 4
        Memory: 8192
        S-States: [120, 100, 80, 60, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [2500, 2000, 1600, 1000]
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
        End time : 10000000
        Inter arrival: 20000
        Expected runtime: 700000
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
        Start time: 10000000
        End time : 16000000
        Inter arrival: 15000
        Expected runtime: 1000000
        Memory: 8
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA2
        CPU type: X86
        Task type: STREAM
        Seed: 333
}
