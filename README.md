# Project Structure
- ```build/``` directory contains all object files (including the ones for the schedulers when built)
- ```given_inputs/``` directory contains input files from canvas
- ```inputs/``` and ```other_inputs/``` directories contain our own input files
- ```SchedulerGreedy.cpp``` source code for Greedy Algo
- ```SchedulerPMapper.cpp``` source code for PMapper Algo
- ```SchedulerEEco.cpp``` source code for E-Eco Algo

# Building
To build the the scheduler executable:
```make algorithm_name```
- for the Greedy Algorithm: ```make greedy```
- for the PMapper Algorithm: ```make pmapper```
- for the E-Eco Algorithm: ```make eco```

To clean object files and executables:
```make clean```