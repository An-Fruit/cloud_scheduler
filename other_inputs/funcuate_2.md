# Rate of workload fluncuate throughout the time window and run on different
# machine types

machine class:
{
        Number of machines: 8
        CPU type: X86
        Number of cores: 8
        Memory: 16000
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}
machine class:
{
        Number of machines: 8
        CPU type: X86
        Number of cores: 8
        Memory: 16000
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}
machine class:
{
        Number of machines: 8
        CPU type: X86
        Number of cores: 8
        Memory: 16000
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}
machine class:
{
        Number of machines: 8
        CPU type: X86
        Number of cores: 8
        Memory: 16000
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}
task class:
{
        Start time: 10000
        End time : 20000
        Inter arrival: 200
        Expected runtime: 10000
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
        Start time: 10000
        End time : 20000
        Inter arrival: 200
        Expected runtime: 10000
        Memory: 8
        VM type: AIX
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 520230
}
task class:
{
        Start time: 15000
        End time : 100000
        Inter arrival: 20000
        Expected runtime: 2000000
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
        Start time: 15000
        End time : 100000
        Inter arrival: 20000
        Expected runtime: 2000000
        Memory: 8
        VM type: WIN
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 520230
}
task class:
{
        Start time: 150000
        End time : 200000
        Inter arrival: 50000
        Expected runtime: 2000000
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
        Start time: 150000
        End time : 200000
        Inter arrival: 50000
        Expected runtime: 2000000
        Memory: 8
        VM type: LINUX_RT
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 520230
}
task class:
{
        Start time: 200000
        End time : 300000
        Inter arrival: 100000
        Expected runtime: 2000000
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
        Start time: 200000
        End time : 300000
        Inter arrival: 100000
        Expected runtime: 2000000
        Memory: 8
        VM type: WIN
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 520230
}