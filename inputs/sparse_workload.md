# This is meant to simulate a sparse workload to see if we can save
# energy by turning off machines that are not needed.
# Tasks will be short, require little memory, and have large intervals between
# arrival.
machine class:
{
        Number of machines: 16
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}

task class:
{
        Start time: 100000
        End time : 10000000
        Inter arrival: 1000000
        Expected runtime: 50000
        Memory: 5
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 520230
}
