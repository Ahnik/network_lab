# Config
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Iinclude -g
SRC_DIR = src
BIN_DIR = bin

# Core source files
COMMON					= $(SRC_DIR)/common.c
ERROR_INJECTOR			= $(SRC_DIR)/error_injector.c
STOP_AND_WAIT_SENDER	= $(SRC_DIR)/stop_and_wait_sender.c
STOP_AND_WAIT_RECEIVER	= $(SRC_DIR)/stop_and_wait_receiver.c

# Main source files
STOP_AND_WAIT_SENDER_SRC   = $(COMMON) $(ERROR_INJECTOR) $(STOP_AND_WAIT_SENDER)
STOP_AND_WAIT_RECEIVER_SRC = $(COMMON) $(STOP_AND_WAIT_RECEIVER)

# Binary files
STOP_AND_WAIT_SENDER_BIN	= $(BIN_DIR)/stop_and_wait_sender.out
STOP_AND_WAIT_RECEIVER_BIN	= $(BIN_DIR)/stop_and_wait_receiver.out

# Targets
.PHONY: all clean build

all: build

# At the beginning, make sure that the bin directory exists
start:
	mkdir -p $(BIN_DIR)

# For building the binaries
$(STOP_AND_WAIT_SENDER_BIN): $(STOP_AND_WAIT_SENDER_SRC) | start
	$(CC) $(CFLAGS) $^ -o $@

$(STOP_AND_WAIT_RECEIVER_BIN): $(STOP_AND_WAIT_RECEIVER_SRC) | start
	$(CC) $(CFLAGS) $^ -o $@

build: $(STOP_AND_WAIT_RECEIVER_BIN) $(STOP_AND_WAIT_SENDER_BIN)

# For cleaning up
clean:
	rm -rf $(BIN_DIR)