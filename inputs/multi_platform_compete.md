# this is meant to test the ability for the scheduler to function when
# it must schedule tasks that require a certain platform, but we only
# have relatively few machines that fulfill the requirements. In this case,
# we have a few
machine class:
{
        Number of machines: 8
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}
machine class:
{
        Number of machines: 2
        CPU type: POWER
        Number of cores: 16
        Memory: 65536
        S-States: [200, 160, 120, 80, 40, 10, 0]
        P-States: [20, 16, 12, 8]
        C-States: [20, 6, 3, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}

# most standard tasks can utilize the x86 machines
# these start at 50k microseconds and end at 60 seconds.
task class:
{
        Start time: 50000
        End time: 60000000
        Inter arrival: 100000 
        Expected runtime: 500000
        Memory: 200
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA1
        CPU type: X86
        Task type: AI
        Seed: 123456
}

# however, a few HPC tasks will instead require to be run on the POWER
# architecture using an AIX VM. These tasks will take a long time and many
# resources, and will probably not be able to run if the POWER machines are
# already being used for the other tasks. This will try to test if the 
# scheduler deals with this case.
task class:
{
        Start time: 1000000
        End time: 5000000
        Inter arrival: 2000000
        Expected runtime: 10000000
        Memory: 23000
        VM type: AIX
        GPU enabled: yes
        SLA type: SLA0
        CPU type: POWER
        Task type: HPC
        Seed: 123456
}