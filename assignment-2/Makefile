# Config
CC		= gcc
CFLAGS  = -Wall -Wextra -Wpedantic -Iinclude -g
SRC_DIR = src
BIN_DIR = bin

# Core source files
COMMON					= $(SRC_DIR)/common.c
ERROR_INJECTOR			= $(SRC_DIR)/error_injector.c
RECEIVER				= $(SRC_DIR)/receiver.c
RING_BUFFER				= $(SRC_DIR)/ring_buffer.c
STOP_AND_WAIT_SENDER	= $(SRC_DIR)/stop_and_wait_sender.c
STOP_AND_WAIT_RECEIVER	= $(SRC_DIR)/stop_and_wait_receiver.c
GO_BACK_N_SENDER		= $(SRC_DIR)/gobackn_sender.c
GO_BACK_N_RECEIVER		= $(SRC_DIR)/gobackn_receiver.c
SR_SENDER				= $(SRC_DIR)/sr_sender.c
SR_RECEIVER				= $(SRC_DIR)/sr_receiver.c

# Main source files
STOP_AND_WAIT_SENDER_SRC   = $(COMMON) $(ERROR_INJECTOR) $(STOP_AND_WAIT_SENDER)
STOP_AND_WAIT_RECEIVER_SRC = $(COMMON) $(ERROR_INJECTOR) $(STOP_AND_WAIT_RECEIVER)
GO_BACK_N_SENDER_SRC	   = $(COMMON) $(ERROR_INJECTOR) $(RECEIVER) $(GO_BACK_N_SENDER) $(RING_BUFFER)
GO_BACK_N_RECEIVER_SRC	   = $(COMMON) $(ERROR_INJECTOR) $(GO_BACK_N_RECEIVER)
SR_SENDER_SRC			   = $(COMMON) $(ERROR_INJECTOR) $(RECEIVER) $(SR_SENDER) $(RING_BUFFER)
SR_RECEIVER_SRC			   = $(COMMON) $(ERROR_INJECTOR) $(SR_RECEIVER)

# Binary files
STOP_AND_WAIT_SENDER_BIN	= $(BIN_DIR)/stop_and_wait_sender
STOP_AND_WAIT_RECEIVER_BIN	= $(BIN_DIR)/stop_and_wait_receiver
GO_BACK_N_SENDER_BIN		= $(BIN_DIR)/gobackn_sender
GO_BACK_N_RECEIVER_BIN		= $(BIN_DIR)/gobackn_receiver
SR_SENDER_BIN				= $(BIN_DIR)/sr_sender
SR_RECEIVER_BIN				= $(BIN_DIR)/sr_receiver

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

$(GO_BACK_N_SENDER_BIN): $(GO_BACK_N_SENDER_SRC) | start
	$(CC) $(CFLAGS) $^ -o $@

$(GO_BACK_N_RECEIVER_BIN): $(GO_BACK_N_RECEIVER_SRC) | start
	$(CC) $(CFLAGS) $^ -o $@

$(SR_SENDER_BIN): $(SR_SENDER_SRC) | start
	$(CC) $(CFLAGS) $^ -o $@

$(SR_RECEIVER_BIN): $(SR_RECEIVER_SRC) | start
	$(CC) $(CFLAGS) $^ -o $@

build: $(STOP_AND_WAIT_RECEIVER_BIN) $(STOP_AND_WAIT_SENDER_BIN) $(GO_BACK_N_RECEIVER_BIN) $(GO_BACK_N_SENDER_BIN) $(SR_SENDER_BIN) $(SR_RECEIVER_BIN)

# For cleaning up
clean:
	rm -rf $(BIN_DIR)