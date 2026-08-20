# Computer Networks Lab Report

**Name:** Ahnik Sarkar
**Roll No:** 002410501021
**Class:** BCSE-III
**Assignment Number:** 1
**Problem Statement:** Design and implementation of an error detection module which has two schemes — Checksum and Cyclic Redundancy Check (CRC).

---

## 1. Design

### 1.1 Purpose

The purpose of this program is to implement and evaluate the error detection schemes - **16-bit Checksum** and **Cyclic Redundancy Check** on a simulated noisy network and to measure the performance and accuracy of the various error detection schemes. The system consists of :

- **Sender** - Reads an input file, chunks it into fixed-sized frames of 64 bytes, computes a Frame Check Sequence (FCS) using one of the error detection schemes, injects a randomly chosen error into each frame and transmits the frames to the receiver over a TCP socket.
- **Receiver** - Listens on a TCP port, accepts incoming connections from senders and receives frames from the senders. It then checks for errors in the frames using the FCS in the trailer of the frames and prints whether the frame is VALID or CORRUPTED.
- **Error Injector Module** - Module containing the logic for injecting one of the following types of errors - single-bit errors, two isolated single-bit errors, odd errors and burst errors.
- **CRC** - Module containing logic for calculating CRC-8, CRC-10, CRC-16 and CRC-32 of the payload and header of each frame, including logic for setting up CRC lookup tables for making CRC calculations faster.
- **Evaluator** - Another sender program which injects three specific types of errors alternatively - (i) error detected by both CRC and checksum, (ii) error detected by checksum but not by CRC, (iii) error detected by CRC but not by checksum.

### 1.2 Structure

The project is organized into header files (for function declarations) and source files (for implementations).

**Project Directory:**
```
assignment-1/
├── include/
│   ├── common.h            # Common macros, struct and function declarations
│   ├── crc.h               # CRC and CRC look-up table function declarations
│   └── error_injector.h    # Error-injection function declarations
├── src/
│   ├── sender.c            # Sender program
│   ├── receiver.c          # Receiver program
│   ├── evaluator.c         # Sender program for evaluation of errors
│   ├── common.c            # Implementation of common functions
│   ├── crc.c               # CRC-8, CRC-10, CRC-16, CRC-32 implementations along with CRC tables
│   └── error_injector.c    # Error injection function implementations
├── bin/                    # Compiled binaries and object files
├── logs/                   # Output logs from sender and receiver (for accuracy analysis)
├── tests/                  # All test files
├── Makefile                # Build system
├── run_tests.sh            # Bash script for running performance tests for a given error detection scheme
├── run_all_tests.sh        # Bash script for running performance tests for all error detection schemes
└── analyze_logs.py         # Python script to generate statistics on the accuracy of each of the schemes
```

**Data Flow:**

```mermaid
flowchart LR
    subgraph Sender [Sender]
        direction TB
        A["Input file"] --> B["Create CRC table"]
        B --> C["Divide data into 44-byte chunks"]
        C --> D["Compute FCS"]
        D --> E{"Which error to inject?"}
        E -- "SINGLE" --> F["inject_single_bit_error()"]
        E -- "TWO ISOLATED" --> G["inject_two_isolated_errors()"]
        E -- "ODD ERRORS" --> H["inject_odd_errors()"]
        E -- "BURST" --> I["inject_burst_error()"]
        E -- "NO ERROR" --> J["No error injected"]
        G --> K["Set up connection with receiver"]
        H --> K
        I --> K
        J --> K
        K --> L["Send the total number of frames to receiver as a 4-byte header"]
        L --> M["Send all frames to receiver one by one"]
    end

    subgraph Receiver [Receiver]
        direction TB
        N["Listen for incoming connection requests"] -> O["Receive 4-byte header from sender"]
        O --> P["Dynamically allocate memory for the data"]
        P --> Q["Receive all frames from sender"]
        Q --> R["Recompute FCS"]
        R --> S{"Result = 0?"}
        S -- "Yes" --> T["VALID"]
        S -- "No" --> U["CORRUPTED"]
    end

    M -- "Network" --> N
```

### 1.3 Frame Structure

The frame is a fixed-size 64-byte struct. Compiler-level struct packing is used to prevent any padding bytes, so the struct has the same binary layout on all platforms.

The frame consists of three parts: header, payload and trailer.
```c
#define FRAME_SIZE       64        // Size of frame in bytes
#define MAC_ADDRESS_SIZE  6        // Size of MAC address in bytes
#define HEADER_SIZE       4        // Size of the header containing length

#pragma pack(push, 1)
typedef struct {
    uint8_t  sender_addr[MAC_ADDRESS_SIZE];     // Sender address
    uint8_t  receiver_addr[MAC_ADDRESS_SIZE];   // Receiver address
    uint16_t length;                            // The length of the payload
    uint16_t type;                              // The type of error detection used
} Header;
#pragma pack(pop)

#pragma pack(push, 1)
typedef union {
    uint8_t crc[4];         // CRC bits
    uint16_t checksum;      // Checksum bits
} Trailer;
#pragma pack(pop)

#define ERROR_RANGE (FRAME_SIZE - sizeof(Trailer))
#define PAYLOAD_SIZE (FRAME_SIZE - sizeof(Header) - sizeof(Trailer))

#pragma pack(push, 1)
typedef struct {
    Header header;
    uint8_t payload[PAYLOAD_SIZE];  // The actual payload buffer
    Trailer trailer;
} Frame;
#pragma pack(pop)
```
