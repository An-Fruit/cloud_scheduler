# Compiler
CXX = g++
# Compiler flags
CXXFLAGS = -Wall -std=c++17
# Include directories
INCLUDES = -I.
# Build directory
BUILD_DIR = build

# Source files
# If you want to restore: add Scheduler.cpp to SRC again
# and rename the source file you want to compile to Scheduler.cpp
SRC = Init.cpp Machine.cpp main.cpp Simulator.cpp Task.cpp VM.cpp
SRC_GREEDY = SchedulerGreedy.cpp
SRC_PMAPPER = SchedulerPMapper.cpp
SRC_ECO = SchedulerEEco.cpp

# Object files for the simulator
OBJ = $(addprefix $(BUILD_DIR)/,$(SRC:.cpp=.o))
# Object files specific to a scheduler policy
OBJ_GREEDY = $(addprefix $(BUILD_DIR)/,$(SRC_GREEDY:.cpp=.o))
OBJ_PMAPPER = $(addprefix $(BUILD_DIR)/,$(SRC_PMAPPER:.cpp=.o))
OBJ_ECO = $(addprefix $(BUILD_DIR)/,$(SRC_ECO:.cpp=.o))

# Executable
TARGET = simulator

# Default target
all: $(TARGET)

# Default target
scheduler: $(OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o scheduler $(OBJ)

# Greedy Scheduler
greedy: $(OBJ) $(OBJ_GREEDY)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o scheduler_greedy $(OBJ) $(OBJ_GREEDY)
	
# PMapper Scheduler
pmapper: $(OBJ) $(OBJ_PMAPPER)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o scheduler_pmapper $(OBJ) $(OBJ_PMAPPER)

# E-Eco Scheduler
eco: $(OBJ) $(OBJ_ECO)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o scheduler_e_eco $(OBJ) $(OBJ_ECO)


# Build target
# ignore this
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $(OBJ)

# Compile source files into object files
$(BUILD_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean up build files
clean:
	rm $(OBJ_GREEDY) $(OBJ_PMAPPER) $(OBJ_ECO) scheduler simulator scheduler_greedy scheduler_pmapper scheduler_e_eco