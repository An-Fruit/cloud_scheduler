# This input file checks if the scheduling algorithm correctly gives tasks 
# with a certain CPU/VM requirement to the correct machine. For this reason, 
# the tasks are not intensive and do not require much memory or time. If the 
# assignment is correct, there should be no difficulty with passing this task.

# X86 machine
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

# ARM  machine
machine class:
{
        Number of machines: 16
        CPU type: ARM
        Number of cores: 8
        Memory: 16384
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}

# POWER machine
machine class:
{
        Number of machines: 16
        CPU type: POWER
        Number of cores: 8
        Memory: 16384
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}

# RISCV machine
machine class:
{
        Number of machines: 16
        CPU type: RISCV
        Number of cores: 8
        Memory: 16384
        S-States: [120, 100, 100, 80, 40, 10, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: yes
}

# simple task that makes sure tasks needing AIX gets a POWER machine
task class:
{
        Start time: 0
        End time : 10000
        Inter arrival: 5000
        Expected runtime: 10000
        Memory: 2
        VM type: AIX
        GPU enabled: yes
        SLA type: SLA0
        CPU type: POWER
        Task type: WEB
        Seed: 123456
}

# these 2 tasks make sure that tasks requiring a Windows VM get to either a
# X86 or ARM  machine
task class:
{
        Start time: 20000
        End time : 30000
        Inter arrival: 5000
        Expected runtime: 10000
        Memory: 2
        VM type: WIN
        GPU enabled: yes
        SLA type: SLA0
        CPU type: ARM
        Task type: WEB
        Seed: 123456
}

task class:
{
        Start time: 40000
        End time : 50000
        Inter arrival: 5000
        Expected runtime: 10000
        Memory: 2
        VM type: WIN
        GPU enabled: yes
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 123456
}

# these 4 tasks make sure that tasks requiring a LINUX VM can routed to 
# any machines

task class:
{
        Start time: 60000
        End time : 70000
        Inter arrival: 5000
        Expected runtime: 10000
        Memory: 2
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA0
        CPU type: ARM
        Task type: WEB
        Seed: 123456
}

task class:
{
        Start time: 80000
        End time : 90000
        Inter arrival: 5000
        Expected runtime: 10000
        Memory: 2
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 123456
}

task class:
{
        Start time: 100000
        End time : 110000
        Inter arrival: 5000
        Expected runtime: 10000
        Memory: 2
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA0
        CPU type: POWER
        Task type: WEB
        Seed: 123456
}

task class:
{
        Start time: 120000
        End time : 130000
        Inter arrival: 5000
        Expected runtime: 10000
        Memory: 2
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA0
        CPU type: RISCV
        Task type: WEB
        Seed: 123456
}

# these 4 tasks make sure that tasks requiring a LINUX_RT VM can routed to 
# any machines

task class:
{
        Start time: 140000
        End time : 150000
        Inter arrival: 5000
        Expected runtime: 10000
        Memory: 2
        VM type: LINUX_RT
        GPU enabled: yes
        SLA type: SLA0
        CPU type: ARM
        Task type: WEB
        Seed: 123456
}

task class:
{
        Start time: 160000
        End time : 170000
        Inter arrival: 5000
        Expected runtime: 10000
        Memory: 2
        VM type: LINUX_RT
        GPU enabled: yes
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 123456
}

task class:
{
        Start time: 1800000
        End time : 190000
        Inter arrival: 5000
        Expected runtime: 10000
        Memory: 2
        VM type: LINUX_RT
        GPU enabled: yes
        SLA type: SLA0
        CPU type: POWER
        Task type: WEB
        Seed: 123456
}

task class:
{
        Start time: 200000
        End time : 210000
        Inter arrival: 5000
        Expected runtime: 10000
        Memory: 2
        VM type: LINUX_RT
        GPU enabled: yes
        SLA type: SLA0
        CPU type: RISCV
        Task type: WEB
        Seed: 123456
}