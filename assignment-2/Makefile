# Config
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Iinclude -g
SRC_DIR = src
BIN_DIR = bin

# Core source files
COMMON				   = $(SRC_DIR)/common.c
STOP_AND_WAIT_SENDER   = $(SRC_DIR)/stop_and_wait_sender.c
STOP_AND_WAIT_RECEIVER = $(SRC_DIR)/stop_and_wait_receiver.c

# Main source files
STOP_AND_WAIT_SENDER_SRC   = $(COMMON) $(STOP_AND_WAIT_SENDER)
STOP_AND_WAIT_RECEIVER_SRC = $(COMMON) $(STOP_AND_WAIT_RECEIVER)

# Targets
.PHONY: all clean build

all: build

# At the beginning, make sure that the bin directory exists
start:
	mkdir -p $(BIN_DIR)

# For cleaning up
rm -rf $(BIN_DIR)