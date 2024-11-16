# Rate of workload is lpow-intensive for long period of time

machine class:
{
        Number of machines: 16
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
        End time : 200000
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