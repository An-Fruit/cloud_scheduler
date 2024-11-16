# Format for the machine and task class used for copy and paste

machine class:
{
        Number of machines: #
        CPU type: ARM POWER RISCV X86
        Number of cores: #
        Memory: # (in MB)
        S-States: [] Sleep Mode
        P-States: [] Performance States (DVFS)
        C-States: [] CPU States
        MIPS: []
        GPUs: yes/no
}
task class:
{
        Start time: # (μs)
        End time : # (μs)
        Inter arrival: 6000
        Expected runtime: 2000000
        Memory: 8
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 520230
}


