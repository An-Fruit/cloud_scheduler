# Typical ML workload with both training and inference.
# In this case, we will have both training and inference tasks being requested
# at the same time.
machine class:
{
        Number of machines: 16
        CPU type: X86
        Number of cores: 16
        Memory: 65536
        S-States: [200, 160, 120, 80, 40, 10, 0]
        P-States: [20, 16, 12, 8]
        C-States: [20, 10, 3, 0]
        MIPS: [2000, 1600, 1200, 800]
        GPUs: yes
}

# training a (relatively) large model should take place over a long period 
# of time, but since it is not that time sensitive we will not enforce 
# strict SLA. in this case, 2 minutes is enough.
# new tasks arrive roughly every 2 seconds.
task class:
{
        Start time: 0
        End time: 60000000
        Inter arrival: 2000000 
        Expected runtime: 120000000
        Memory: 4096
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA2
        CPU type: X86
        Task type: AI
        Seed: 123456
}

# example inferencing for a small model: GPU is not necessary here because
# the model is small enough for the overhead of data transfer to the
# GPU to outweigh any performance benefit
task class:
{
        Start time: 100000
        End time: 900000
        Inter arrival: 20000
        Expected runtime: 50000
        Memory: 100
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: AI
        Seed: 520989
}

# example inference for large model: GPU should improve performance greatly.
task class:
{
        Start time: 30000000
        End time: 35000000
        Inter arrival: 50000
        Expected runtime: 100000
        Memory: 500
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: AI
        Seed: 951825
}