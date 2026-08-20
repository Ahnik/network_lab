# Config
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Iinclude -g
SRC_DIR = src
BIN_DIR = bin

# Core source files
COMMON			= $(SRC_DIR)/common.c
CRC				= $(SRC_DIR)/crc.c
ERROR_INJECTOR 	= $(SRC_DIR)/error_injector.c
RECEIVER		= $(SRC_DIR)/receiver.c
SENDER			= $(SRC_DIR)/sender.c
EVALUATOR 		= $(SRC_DIR)/evaluator.c

# Main source files
RECEIVER_SRC 	= $(RECEIVER) $(COMMON) $(CRC)
SENDER_SRC		= $(SENDER) $(COMMON) $(CRC) $(ERROR_INJECTOR)
EVALUATOR_SRC 	= $(EVALUATOR) $(COMMON) $(CRC) $(ERROR_INJECTOR)

# Binary files
RECEIVER_BIN	= $(BIN_DIR)/receiver.out
EVALUATOR_BIN	= $(BIN_DIR)/evaluator.out
SENDER_BIN		= $(BIN_DIR)/sender.out

# Targets
.PHONY: all clean build receiver sender

all: build

# At the beginning to ensure that the bin directory exists
start:
	mkdir -p $(BIN_DIR)

# For building the receiver and sender binaries
$(RECEIVER_BIN): $(RECEIVER_SRC) | start
	$(CC) $(CFLAGS) $^ -o $@

$(SENDER_BIN): $(SENDER_SRC) | start
	$(CC) $(CFLAGS) $^ -o $@

$(EVALUATOR_BIN): $(EVALUATOR_SRC) | start
	$(CC) $(CFLAGS) $^ -o $@

build: $(RECEIVER_BIN) $(SENDER_BIN) $(EVALUATOR_BIN)

# For running the receiver
receiver: $(RECEIVER_BIN)
	@$(RECEIVER_BIN)

# For running the sender
sender: $(SENDER_BIN)
	@$(SENDER_BIN) $(ARGS)

# For running the test sender
evaluator: $(EVALUATOR_BIN)
	@$(EVALUATOR_BIN) $(ARGS)

# For cleaning up
clean:
	rm -rf $(BIN_DIR)