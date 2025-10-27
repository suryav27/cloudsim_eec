
# Energy-Efficient Cloud Scheduler Project Input File

# This input file was designed to evaluate scheduler performance across heterogeneous resources and dynamic workloads. It includes both low-intensity and surge scenarios, GPU and CPU-bound tasks, and distinct SLA requirements, enabling balanced testing of energy savings and SLA compliance under varying conditions.

machine class: {
    Number of machines: 8
    CPU type: X86
    Number of cores: 8
    Memory: 16384
    # Sleep-state energy usage (Watts)
    S-States: [120, 100, 70, 40, 15, 5, 0]
    # Performance states (P-States: power-performance tradeoff)
    P-States: [12, 8, 6, 4]
    # Idle-state energy usage (C-States)
    C-States: [12, 3, 1, 0]
    # Millions of Instructions Per Second for each P-State
    MIPS: [1000, 800, 600, 400]
    GPUs: yes
}

machine class: {
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

# ==========================================================
# WORKLOAD DEFINITIONS
# ==========================================================

Number of workloads: 5

# Short, frequent web service requests (latency-sensitive)
task class: {
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
    Distribution: poisson
    Seed: 520230
}

# Long, GPU-heavy ML training jobs (high power draw)
task class: {
    Start time: 100000
    End time: 1300000
    Inter arrival: 20000
    Expected runtime: 3000000
    Memory: 12
    VM type: LINUX
    GPU enabled: yes
    SLA type: SLA1
    CPU type: X86
    Task type: ML_TRAINING
    Distribution: normal
    Seed: 952023
}

# Medium-length analytics tasks (CPU-heavy, no GPU)
task class: {
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

# Light background system maintenance (energy-aware)
task class: {
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

# Low-load burst arriving late (tests idle → active transitions)
task class: {
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
