# This input file is meant to simulate a heavy amount of requests
# at the beginning, which steadily drops off over the course of the run.
# This is meant to see if we are able to deallocate machines properly
# as less of them are needed to service fewer requests.
# All workloads start within the first 500,000 microseconds, and the last
# workload should end at 10,000,000 microseconds.
# All workloads start one after another spaced out by 50k microseconds.
# In this case, there is only one type of machine to help eliminate other
# factors that we are not testing for.
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
# Longest running task
task class:
{
        Start time: 50000
        End time: 10000000
        Inter arrival: 50000
        Expected runtime: 100000
        Memory: 8
        VM Type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: x86
        Task Type: WEB
        Seed: 123456
}
# Second longest running
task class:
{
        Start time: 100000
        End time: 9000000
        Inter arrival: 50000
        Expected runtime: 100000
        Memory: 8
        VM Type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: x86
        Task Type: WEB
        Seed: 123456
}
# Third longest running
task class:
{
        Start time: 150000
        End time: 8000000
        Inter arrival: 50000
        Expected runtime: 100000
        Memory: 8
        VM Type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: x86
        Task Type: WEB
        Seed: 123456
}
# Fourth longest running
task class:
{
        Start time: 200000
        End time: 7000000
        Inter arrival: 50000
        Expected runtime: 100000
        Memory: 8
        VM Type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: x86
        Task Type: WEB
        Seed: 123456
}
# Fifth longest running
task class:
{
        Start time: 250000
        End time: 6000000
        Inter arrival: 50000
        Expected runtime: 100000
        Memory: 8
        VM Type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: x86
        Task Type: WEB
        Seed: 123456
}
# Sixth longest running
task class:
{
        Start time: 300000
        End time: 5000000
        Inter arrival: 50000
        Expected runtime: 100000
        Memory: 8
        VM Type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: x86
        Task Type: WEB
        Seed: 123456
}
# Seventh longest running
task class:
{
        Start time: 350000
        End time: 4000000
        Inter arrival: 50000
        Expected runtime: 100000
        Memory: 8
        VM Type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: x86
        Task Type: WEB
        Seed: 123456
}
# Eigth longest running
task class:
{
        Start time: 400000
        End time: 3000000
        Inter arrival: 50000
        Expected runtime: 100000
        Memory: 8
        VM Type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: x86
        Task Type: WEB
        Seed: 123456
}
# Ninth longest running
task class:
{
        Start time: 450000
        End time: 2000000
        Inter arrival: 50000
        Expected runtime: 100000
        Memory: 8
        VM Type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: x86
        Task Type: WEB
        Seed: 123456
}
# Tenth longest running
task class:
{
        Start time: 500000
        End time: 1000000
        Inter arrival: 50000
        Expected runtime: 100000
        Memory: 8
        VM Type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: x86
        Task Type: WEB
        Seed: 123456
}