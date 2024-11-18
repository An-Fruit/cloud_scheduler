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
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}

# most standard tasks can utilize the x86 machines
# these will 
task class:
{
        Start time: 50000
        End time: 60000000
}

# however, some 