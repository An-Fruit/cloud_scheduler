# Rate of workload spikes at different points in time
# This is trying to see if we can handle a sudden spike in the amount of 
# requests in a mixed web and streaming environment. The sudden increase in
# requests in this case will be due to streaming (i.e. a new show is released, 
# and everyone is trying to watch it).

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
# baseline web task that will run throughout and require relatively
# little resources. The tasks will start arriving at time 0, and stop at 5
# minutes.
# These tasks don't require good performance, so SLA will be set to 2
task class:
{
    Start time: 0
    End time: 300000000
    Inter arrival: 5000
    Expected runtime: 50000
    Memory: 5
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA2
    CPU type: X86
    Task type: WEB
    Seed: 123456
}

# first spike: this will be streaming tasks that are compute intensive
# and occurs in short bursts. This one will start 1 minute in, and last
# for around 25 seconds
task class:
{
        Start time: 60000000
        End time: 85000000
        Inter arrival: 100000
        Expected runtime: 500000
        Memory: 250
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: STREAM
        Seed: 123456
}

# second spike. This will start around 3 minutes, and last 20 seconds
task class:
{
        Start time: 180000000
        End time: 200000000
        Inter arrival: 100000
        Expected runtime: 500000
        Memory: 250
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: STREAM
        Seed: 123456
}