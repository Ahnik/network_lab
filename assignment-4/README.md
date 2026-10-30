# CDMA with Walsh Code: Implementation & Demonstration Guide

**Course:** CSE/PC/B/S/314 Computer Networks Lab  
**Assignment:** Assignment 4 — Implement Code Division Multiple Access (CDMA) with Walsh Code  
**Target Language:** C++ (C++17) & C (C11) with POSIX TCP Sockets  
**Reference Document:** `Assignment#4_CDMA.pdf` (converted text available in `assignment4_text.txt`)  

---

## 1. Executive Summary & Problem Statement

### 1.1 Problem Statement
> **Assignment 4: Implement CDMA with Walsh code.**  
> *Use the same sender-receiver design as previous assignments.*  
> In this assignment you have to implement CDMA for multiple access of a common channel by $n$ stations. Each sender uses a unique code word, given by the Walsh set, to encode its data, send it across the channel, and then perfectly reconstruct the data at $n$ stations.

### 1.2 Core Objectives
1. **Walsh-Hadamard Code Generation:** Generate orthogonal chip sequences for $n$ stations using Sylvester's recursive construction.
2. **Mutual Orthogonality Verification:** Mathematically prove that any pair of distinct station codes yields an inner product of 0 ($C_i \cdot C_j = 0$ for $i \neq j$) and self-inner product of $W$ ($C_i \cdot C_i = W$).
3. **Simultaneous Multiple Access:** Emulate the shared physical channel where multiple stations transmit concurrently on the exact same frequency band without collisions.
4. **Linear Channel Superposition:** Sum chip vectors linearly on the physical medium:
   $$S_{\text{channel}} = \sum_{i=1}^{n} S_i = \sum_{i=1}^{n} d_i \cdot C_i$$
5. **Exact Data Reconstruction:** At any receiver, correlate the composite channel signal with the sender's Walsh code to achieve 100% bit-exact reconstruction (Bit Error Rate = 0.00%):
   $$R_i = \frac{1}{W} (S_{\text{channel}} \cdot C_i) = d_i$$
6. **Sender-Receiver Networked Emulation:** Reusing the architecture from Assignment 3, implement a standalone shared physical medium (`channel_emulator`) and concurrent independent station processes (`station`) communicating over TCP sockets with CRC-32 Frame Check Sequences.

---

## 2. Mathematical Foundations of CDMA

### 2.1 Walsh-Hadamard Matrix Construction
Walsh codes are mutually orthogonal binary sequences. For $n$ stations, the spreading factor $W$ is the smallest power of 2 such that $W \ge n$ (e.g., $n=4 \implies W=4$, $n=5..8 \implies W=8$).

Using Sylvester's construction, starting from $H_1 = [+1]$:
$$H_{2k} = \begin{bmatrix} H_k & H_k \\ H_k & -H_k \end{bmatrix}$$

For $W = 2$:
$$H_2 = \begin{bmatrix} +1 & +1 \\ +1 & -1 \end{bmatrix}$$

For $W = 4$:
$$H_4 = \begin{bmatrix}
+1 & +1 & +1 & +1 \\
+1 & -1 & +1 & -1 \\
+1 & +1 & -1 & -1 \\
+1 & -1 & -1 & +1
\end{bmatrix}$$

For $W = 8$:
$$H_8 = \begin{bmatrix}
+1 & +1 & +1 & +1 & +1 & +1 & +1 & +1 \\
+1 & -1 & +1 & -1 & +1 & -1 & +1 & -1 \\
+1 & +1 & -1 & -1 & +1 & +1 & -1 & -1 \\
+1 & -1 & -1 & +1 & +1 & -1 & -1 & +1 \\
+1 & +1 & +1 & +1 & -1 & -1 & -1 & -1 \\
+1 & -1 & +1 & -1 & -1 & +1 & -1 & +1 \\
+1 & +1 & -1 & -1 & -1 & -1 & +1 & +1 \\
+1 & -1 & -1 & +1 & -1 & +1 & +1 & -1
\end{bmatrix}$$

Each station $i \in \{0, \dots, n-1\}$ is assigned row $i$ of the matrix as its unique chip sequence $C_i$.

### 2.2 Mathematical Orthogonality Property
The inner product (dot product) of two discrete chip sequences $C_i$ and $C_j$ of length $W$ is defined as:
$$C_i \cdot C_j = \sum_{k=0}^{W-1} C_i[k] \cdot C_j[k]$$

Because the rows of a Hadamard matrix are orthogonal:
$$C_i \cdot C_j = \begin{cases} W & \text{if } i = j \quad \text{(Autocorrelation)} \\ 0 & \text{if } i \neq j \quad \text{(Cross-correlation)} \end{cases}$$

### 2.3 Modulation & Spreading
Data bits are mapped to bipolar NRZ symbols:
- Bit `1` $\longrightarrow d_i = +1$
- Bit `0` $\longrightarrow d_i = -1$
- Silent (no data) $\longrightarrow d_i = 0$

Each data bit $d_i$ is spread across $W$ chips:
$$S_i = d_i \cdot C_i = [d_i \cdot C_i[0],\, d_i \cdot C_i[1],\, \dots,\, d_i \cdot C_i[W-1]]$$

### 2.4 Channel Superposition
When all $n$ stations transmit simultaneously on the common channel, their signals sum linearly in the physical medium:
$$S_{\text{channel}} = \sum_{j=0}^{n-1} S_j = \sum_{j=0}^{n-1} d_j \cdot C_j$$

Each element of $S_{\text{channel}}$ is an integer in the range $[-n, +n]$.

### 2.5 Despreading & Perfect Reconstruction Proof
To decode the data transmitted by Station $i$, the receiver correlates the received channel signal $S_{\text{channel}}$ with Station $i$'s known Walsh code $C_i$:

$$\text{Correlation} = S_{\text{channel}} \cdot C_i = \left( \sum_{j=0}^{n-1} d_j \cdot C_j \right) \cdot C_i$$

Distributing the dot product:
$$S_{\text{channel}} \cdot C_i = d_i (C_i \cdot C_i) + \sum_{j \neq i} d_j (C_j \cdot C_i)$$

Since $C_j \cdot C_i = 0$ for all $j \neq i$, and $C_i \cdot C_i = W$:
$$S_{\text{channel}} \cdot C_i = d_i \cdot W + 0 = d_i \cdot W$$

Normalizing by the code length $W$:
$$d_i = \frac{S_{\text{channel}} \cdot C_i}{W}$$

- If $d_i = +1 \implies$ Decoded bit is **`1`**
- If $d_i = -1 \implies$ Decoded bit is **`0`**
- If $d_i = 0 \implies$ Station $i$ was **`Silent`**

Because interference from all other stations evaluates identically to zero ($\sum_{j \neq i} 0 = 0$), multiple access interference (MAI) is completely eliminated, yielding **100% perfect reconstruction**.

---

## 3. Architecture & Code Reuse from Assignment 3

The implementation reuses and builds upon the robust sender-receiver design from `../assignment-3`:

```
                           +------------------------+
                           |  channel_emulator      |
                           |  (Shared Physical Bus) |
                           +-----------+------------+
                                       ^
                 TCP Synchronized      | Composite Broadcast
                 Chip Streams          | S_channel = Sum(S_i)
                 (S_i = d_i * C_i)     v
        +------------------+-----------+----------+------------------+
        |                  |                      |                  |
        v                  v                      v                  v
+---------------+  +---------------+      +---------------+  +---------------+
|   Station 0   |  |   Station 1   |      |   Station 2   |  |   Station 3   |
| (Walsh Code 0)|  | (Walsh Code 1)|      | (Walsh Code 2)|  | (Walsh Code 3)|
| S0 = d0 * C0  |  | S1 = d1 * C1  |      | S2 = d2 * C2  |  | S3 = d3 * C3  |
+---------------+  +---------------+      +---------------+  +---------------+
```

### 3.1 Reused Components from Assignment 3:
- **Frame Structure (`include/common.h`):** 64-byte frame standard with Header (MAC addresses, payload length, sequence number, station ID), Payload (44 bytes), and Trailer (4-byte CRC-32 FCS).
- **CRC-32 Engine (`src/common.c`):** Bit-exact generator (`0x04C11DB7`) and 256-entry lookup table for data integrity verification.
- **File Chunking Engine (`src/common.c`):** `chunk_file()` segments arbitrary binary files into transmission frames.
- **RAII Socket Wrapper (`include/classes.h`):** Safe resource management for POSIX sockets with move semantics and automated closure.

### 3.2 New CDMA Modules:
- **`WalshCodeSet` (`include/walsh.h`, `src/walsh.cpp`):**
  - Generates Walsh-Hadamard matrices of arbitrary power-of-2 dimensions.
  - Verifies mutual orthogonality and autocorrelation.
  - Encodes individual bits, bytes, and buffers into bipolar chip streams ($+1, -1, 0$).
  - Decodes composite channel chips back into original bytes.
- **`CdmaMultiplexer` (`include/cdma.h`, `src/cdma.cpp`):**
  - Superimposes simultaneous chip streams ($S_{\text{channel}} = \sum S_i$).
  - Optional Gaussian noise injector for channel robustness and SNR experiments.
- **`CdmaReceiver` (`include/cdma.h`, `src/cdma.cpp`):**
  - Extracts frames from composite channel signals using Walsh despreading.
  - Validates CRC-32 Frame Check Sequences.
- **`CdmaNet` Wire Protocol (`include/cdma.h`, `src/cdma.cpp`):**
  - Serializes and deserializes synchronous chip packets (`ChipPacketHeader`) over TCP sockets.
- **`channel_emulator` (`src/channel_emulator.cpp`):**
  - Multi-threaded/multiplexed server coordinating transmission rounds for $n$ stations.
  - Broadcasts composite channel vector to all connected stations.
- **`station` (`src/station.cpp`):**
  - Concurrent network client representing an independent station.
  - Supports synthetic test frames, custom strings (`--msg`), or real files (`--file`).
  - Capable of simultaneously decoding and verifying all active stations.
- **`cdma_demo` (`src/cdma_demo.cpp`):**
  - Standalone educational CLI tool demonstrating every mathematical step, multi-station text transfer, and noise tolerance.

---

## 4. Project Directory Layout

```
assignment-4/
├── include/
│   ├── classes.h           # RAII SocketWrapper and Stopwatch timer
│   ├── cdma.h              # CDMA Multiplexer, Receiver, and Network Wire Protocol
│   ├── common.h            # Frame formats, CRC-32 definitions, and socket helpers
│   └── walsh.h             # Walsh-Hadamard code generation and bit/byte codecs
├── src/
│   ├── cdma.cpp            # CDMA multiplexer and packet serialization implementation
│   ├── cdma_demo.cpp       # Standalone interactive & benchmark demonstration binary
│   ├── channel_emulator.cpp# Central shared medium emulator process
│   ├── common.c            # CRC-32 table, MAC addressing, and file chunking
│   ├── station.cpp         # Independent station client process
│   └── walsh.cpp           # Walsh matrix generation, inner products, and codecs
├── bin/                    # Compiled ELF binaries
│   ├── cdma_demo           # Standalone demonstration binary
│   ├── channel_emulator    # Central emulator daemon
│   └── station             # Contending station node
├── charts/                 # Visual plots generated by plot_cdma.py
│   ├── walsh_hadamard_matrix.png
│   ├── cdma_waveforms_superposition.png
│   ├── ber_vs_noise.png
│   └── cdma_vs_csma_throughput.png
├── logs/                   # Execution logs for stations and emulator
├── tests/                  # Test payloads and reconstructed verification files
│   ├── test_file.txt       # Sample multi-frame text file
│   └── reconstructed_file.txt # Byte-exact file recovered from CDMA channel
├── Makefile                # Automated GNU build system
├── plot_cdma.py            # Matplotlib visualizer for CDMA waveforms and curves
├── run_demo.sh             # Helper script to launch standalone demonstrations
└── run_simulation.sh       # Multi-process networked simulation runner
```

---

## 5. Compilation & Build Instructions

The project uses a standard `Makefile` with strict flags (`-Wall -Wextra -Wpedantic -std=c++17 -O2`).

### 5.1 Build All Binaries
```bash
make clean && make -j4
```
This compiles and links the three executables into `bin/`:
- `bin/channel_emulator`
- `bin/station`
- `bin/cdma_demo`

### 5.2 Clean Build Artifacts
```bash
make clean
```

---

## 6. How to Run & Demonstrate the Project

### Demonstration Method 1: Standalone Visual Demonstration (`cdma_demo`)
The standalone tool demonstrates each stage of CDMA with full mathematical transparency. It requires no network setup and is ideal for lab evaluations and viva questions.

#### 1. Full Demonstration (All 4 Modules)
```bash
./run_demo.sh all 4
# or directly:
./bin/cdma_demo --stations 4
```

#### 2. Walsh Matrix & Orthogonality Verification Only
Displays $H_N$ and computes the full correlation matrix $(C_i \cdot C_j)$:
```bash
./bin/cdma_demo --mode walsh --stations 4
./bin/cdma_demo --mode walsh --stations 8
```

#### 3. Step-by-Step Bit Transmission & Channel Superposition
Walks through 4 stations sending bits (+1, -1, 0, +1), shows chip vectors $S_i$, the composite channel vector $S_{\text{channel}}$, and the receiver inner products:
```bash
./bin/cdma_demo --mode step
```

#### 4. Simultaneous Multi-Station Text Stream Multiplexing
Encodes 4 (or 8) unique text strings simultaneously, superimposes them onto the channel, and reconstructs each string at separate receivers with 100% accuracy:
```bash
./bin/cdma_demo --mode text --stations 4
./bin/cdma_demo --mode text --stations 8
```

#### 5. Channel Noise Robustness & Processing Gain ($10 \log_{10} W$)
Evaluates bit error rates across increasing noise levels ($\sigma = 0.0$ to $3.0$) to prove the interference suppression capability of Walsh spreading:
```bash
./bin/cdma_demo --mode noise --stations 4
./bin/cdma_demo --mode noise --stations 8
```

---

### Demonstration Method 2: Multi-Process Networked Emulation (`run_simulation.sh`)
This runs the full distributed architecture across independent Linux processes communicating over TCP sockets.

Run an automated simulation with 4 stations executing 5 rounds of simultaneous CDMA transmission:
```bash
./run_simulation.sh 4 5 8989 0.0
```

**Parameters:**
- `Param 1`: Number of stations $N$ (e.g., `4` or `8`)
- `Param 2`: Number of transmission rounds (e.g., `5` or `10`)
- `Param 3`: TCP port (default `8989`)
- `Param 4`: Channel noise standard deviation (default `0.0`)

#### Sample Console Output:
```text
======================================================================
                  STARTING MULTI-PROCESS CDMA SIMULATION              
======================================================================
  Contending Stations (N): 4
  Transmission Rounds:     5
  Port:                    8989
  Channel Noise StdDev:    0.0
  Logs Directory:          /home/ahnik/college/network_lab/assignment-4/logs
======================================================================
  [+] Launched Station 0 (PID: 128822)
  [+] Launched Station 1 (PID: 128823)
  [+] Launched Station 2 (PID: 128824)
  [+] Launched Station 3 (PID: 128825)
Simultaneous CDMA transmission in progress across shared medium...
All 4 stations completed transmission and reconstruction!

======================================================================
                  MULTI-STATION CDMA SIMULATION RESULTS               
======================================================================
Station    Tx Frames    Rx Frames    CRC Errors   Success (%)  BER (%)   
----------------------------------------------------------------------
Station 0  5            20           0            100%         0.00%     
Station 1  5            20           0            100%         0.00%     
Station 2  5            20           0            100%         0.00%     
Station 3  5            20           0            100%         0.00%     
======================================================================
Zero collisions observed. Orthogonal CDMA codes guarantee concurrent
transmission and exact reconstruction without packet loss.
Detailed logs saved in: /home/ahnik/college/network_lab/assignment-4/logs/
======================================================================
```

#### Inspecting Logs:
- Shared Channel: `cat logs/emulator_N4.log`
- Station 0: `cat logs/station_0_N4.log`
- Station 1: `cat logs/station_1_N4.log`

---

### Demonstration Method 3: Multi-Process Real File Transfer & Integrity Verification
Demonstrates transferring a multi-frame file across the CDMA channel while other stations generate simultaneous cross-traffic, followed by a byte-for-byte `diff` verification.

```bash
# 1. Clean previous socket bindings
fuser -k -9 8990/tcp 2>/dev/null || true

# 2. Start the Channel Emulator in background
./bin/channel_emulator --port 8990 --stations 4 --rounds 10 > logs/file_emulator.log 2>&1 &
EM_PID=$!
sleep 0.3

# 3. Launch Station 0 (transmitting file), Stations 1 & 2 (concurrent text), Station 3 (receiver)
./bin/station --port 8990 --id 0 --file tests/test_file.txt --frames 10 > logs/file_tx_0.log 2>&1 &
P0=$!
./bin/station --port 8990 --id 1 --msg "Greetings from Station 1!" --frames 10 > logs/file_tx_1.log 2>&1 &
P1=$!
./bin/station --port 8990 --id 2 --msg "Concurrent CDMA traffic" --frames 10 > logs/file_tx_2.log 2>&1 &
P2=$!
./bin/station --port 8990 --id 3 --silent --target 0 --frames 10 --save-received tests/reconstructed_file.txt > logs/file_tx_3.log 2>&1 &
P3=$!

# 4. Wait for completion
wait $P0 $P1 $P2 $P3
wait $EM_PID

# 5. Verify 100% bit-exact reconstruction
diff -s tests/test_file.txt tests/reconstructed_file.txt
```

**Output:**
```text
Files tests/test_file.txt and tests/reconstructed_file.txt are identical
```

---

### Demonstration Method 4: Visualizing Performance Charts
Generate publication-quality figures illustrating CDMA principles:

```bash
python3 plot_cdma.py
```
Generated figures in `charts/`:
1. **`charts/walsh_hadamard_matrix.png`**: Heatmap of $H_4$ and $H_8$ showing orthogonal chip structure.
2. **`charts/cdma_waveforms_superposition.png`**: Signal waveforms showing individual chips $S_i$, composite channel sum $S_{\text{channel}}$, and receiver correlation bars.
3. **`charts/ber_vs_noise.png`**: Empirical Bit Error Rate vs. channel noise for $W \in \{2, 4, 8, 16\}$, demonstrating processing gain.
4. **`charts/cdma_vs_csma_throughput.png`**: Comparison between CDMA (constant collision-free throughput) and CSMA (contention breakdown).

---

## 7. CDMA vs. CSMA: Key Takeaways & Lab Analysis

| Metric / Dimension | CSMA (Assignment 3) | CDMA with Walsh Code (Assignment 4) |
| :--- | :--- | :--- |
| **Medium Access Paradigm** | Contention-based (Listen before talk) | Code-based (Orthogonal spreading) |
| **Simultaneous Transmission** | Causes destructive packet collisions | Supported natively via linear superposition |
| **Vulnerable Window** | $1\tau$ (CSMA) to $2\tau$ (CSMA/CD) | Zero (all stations transmit concurrently) |
| **Throughput under Heavy Load** | Saturates and degrades due to backoffs | Linear scaling: $N \times R_{\text{station}}$ |
| **Collision Detection / Retries** | Requires jam signals & Binary Exponential Backoff | Not needed; 0 collisions occurred |
| **Bandwidth Expansion** | Narrowband ($W = 1$) | Wideband spread spectrum ($W \ge N$) |
| **Reconstruction Mechanism** | Standard frame deserialization | Despreading via inner product: $\frac{1}{W} (S \cdot C_i)$ |
| **Frame Integrity Verification** | CRC-32 Frame Check Sequence | CRC-32 Frame Check Sequence |

### Summary of Results
- **Collision Rate:** 0.00% across all station counts ($N = 2, 4, 8$).
- **Frame CRC Pass Rate:** 100.00% in all non-noisy transmission rounds.
- **Bit Error Rate (BER):** Exactly 0.00% under ideal channel superposition.
- **Processing Gain:** A spreading factor of $W=8$ provides $10 \log_{10}(8) \approx 9.0\text{ dB}$ of processing gain, providing significant noise margin over unspread signals.
