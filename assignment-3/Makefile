# Compiler configuration
CXX      = g++
CXXFLAGS = -Wall -Wextra -Wpedantic -std=c++17 -Iinclude -pthread -g
CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -Iinclude -g

SRC_DIR  = src
BIN_DIR  = bin

# Common objects
COMMON_C_OBJ   = $(SRC_DIR)/common.o
CLASSES_OBJ    = $(SRC_DIR)/classes.o
CSMA_OBJ       = $(SRC_DIR)/csma.o
STATION_OBJ    = $(SRC_DIR)/station.o
EMULATOR_OBJ   = $(SRC_DIR)/channel_emulator.o

# Output Binaries
STATION_BIN    = $(BIN_DIR)/station
EMULATOR_BIN   = $(BIN_DIR)/channel_emulator

.PHONY: all directories clean

all: directories $(STATION_BIN) $(EMULATOR_BIN)

directories:
	@mkdir -p $(BIN_DIR)

# Compile C files
$(SRC_DIR)/common.o: $(SRC_DIR)/common.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C++ files
$(SRC_DIR)/classes.o: $(SRC_DIR)/classes.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC_DIR)/csma.o: $(SRC_DIR)/csma.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC_DIR)/station.o: $(SRC_DIR)/station.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC_DIR)/channel_emulator.o: $(SRC_DIR)/channel_emulator.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link Binaries
$(STATION_BIN): $(STATION_OBJ) $(CSMA_OBJ) $(CLASSES_OBJ) $(COMMON_C_OBJ) | directories
	$(CXX) $(CXXFLAGS) $^ -o $@

$(EMULATOR_BIN): $(EMULATOR_OBJ) $(COMMON_C_OBJ) | directories
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf $(BIN_DIR) $(SRC_DIR)/*.o
