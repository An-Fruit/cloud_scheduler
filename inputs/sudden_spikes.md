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
# little resources
# start at beginning, end at 5 minutes

task class: {
    Start time: 0
    End time: 300000000
    Inter arrival: 5000
    Expected runtime: 1500000
    Memory: 4096
    VM type: LINUX
    GPU enabled: no
    SLA type: SLA0
    CPU type: X86
    Task type: WEB
    Seed: 123456
}