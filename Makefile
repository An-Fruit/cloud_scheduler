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

# Object files
OBJ = $(addprefix $(BUILD_DIR)/,$(SRC:.cpp=.o))
OBJ_GREEDY = $(addprefix $(BUILD_DIR)/,$(SRC_GREEDY:.cpp=.o))
OBJ_PMAPPER = $(addprefix $(BUILD_DIR)/,$(SRC_PMAPPER:.cpp=.o))

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


# Build target
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $(OBJ)

# Compile source files into object files
$(BUILD_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean up build files
clean:
	rm $(OBJ_GREEDY) $(OBJ_PMAPPER) scheduler simulator scheduler_greedy scheduler_pmapper