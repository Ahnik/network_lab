# Compiler configuration
CXX      = g++
CXXFLAGS = -Wall -Wextra -Wpedantic -std=c++17 -Iinclude -pthread -g -O2
CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -Iinclude -g -O2

SRC_DIR  = src
BIN_DIR  = bin
LOG_DIR  = logs

# Common objects
COMMON_C_OBJ   = $(SRC_DIR)/common.o
WALSH_OBJ      = $(SRC_DIR)/walsh.o
CDMA_OBJ       = $(SRC_DIR)/cdma.o
EMULATOR_OBJ   = $(SRC_DIR)/channel_emulator.o
STATION_OBJ    = $(SRC_DIR)/station.o
DEMO_OBJ       = $(SRC_DIR)/cdma_demo.o

# Output Binaries
EMULATOR_BIN   = $(BIN_DIR)/channel_emulator
STATION_BIN    = $(BIN_DIR)/station
DEMO_BIN       = $(BIN_DIR)/cdma_demo

.PHONY: all directories clean demo test

all: directories $(EMULATOR_BIN) $(STATION_BIN) $(DEMO_BIN)

directories:
	@mkdir -p $(BIN_DIR) $(LOG_DIR)

# Compile C files
$(SRC_DIR)/common.o: $(SRC_DIR)/common.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C++ files
$(SRC_DIR)/walsh.o: $(SRC_DIR)/walsh.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC_DIR)/cdma.o: $(SRC_DIR)/cdma.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC_DIR)/channel_emulator.o: $(SRC_DIR)/channel_emulator.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC_DIR)/station.o: $(SRC_DIR)/station.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC_DIR)/cdma_demo.o: $(SRC_DIR)/cdma_demo.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link Binaries
$(EMULATOR_BIN): $(EMULATOR_OBJ) $(WALSH_OBJ) $(CDMA_OBJ) $(COMMON_C_OBJ) | directories
	$(CXX) $(CXXFLAGS) $^ -o $@

$(STATION_BIN): $(STATION_OBJ) $(WALSH_OBJ) $(CDMA_OBJ) $(COMMON_C_OBJ) | directories
	$(CXX) $(CXXFLAGS) $^ -o $@

$(DEMO_BIN): $(DEMO_OBJ) $(WALSH_OBJ) $(CDMA_OBJ) $(COMMON_C_OBJ) | directories
	$(CXX) $(CXXFLAGS) $^ -o $@

demo: all
	@echo "Running CDMA Standalone Demonstration..."
	@./$(DEMO_BIN) --stations 4

clean:
	rm -rf $(BIN_DIR) $(SRC_DIR)/*.o
