#!/usr/bin/env python3
"""
plot_cdma.py
Visualizes CDMA principles with Walsh-Hadamard codes:
1. Walsh Matrix Heatmap & Orthogonality
2. Signal Multiplexing & Channel Superposition Waveforms
3. Bit Error Rate (BER) vs Channel Noise for Spreading Factors W=2,4,8,16
4. CDMA vs CSMA Throughput Comparison
"""

import os
import numpy as np
import matplotlib.pyplot as plt

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "charts")
os.makedirs(OUTPUT_DIR, exist_ok=True)

def generate_walsh_matrix(n):
    """Generate Hadamard/Walsh matrix of order n (power of 2)"""
    h = np.array([[1]])
    while h.shape[0] < n:
        h = np.block([[h, h], [h, -h]])
    return h

def plot_walsh_matrix():
    fig, axes = plt.subplots(1, 2, figsize=(13, 6))

    for idx, w in enumerate([4, 8]):
        h = generate_walsh_matrix(w)
        ax = axes[idx]
        im = ax.imshow(h, cmap="coolwarm", vmin=-1.2, vmax=1.2)
        ax.set_title(f"Walsh-Hadamard Matrix $H_{w}$ ($W={w}$)", fontsize=13, fontweight='bold', pad=12)
        ax.set_xticks(range(w))
        ax.set_yticks(range(w))
        ax.set_xticklabels([f"Chip {i}" for i in range(w)], fontsize=9)
        ax.set_yticklabels([f"Station {i}" for i in range(w)], fontsize=9)

        # Annotate cell values
        for r in range(w):
            for c in range(w):
                val = h[r, c]
                text = "+1" if val > 0 else "-1"
                color = "white" if val > 0 else "black"
                ax.text(c, r, text, ha="center", va="center", color=color, fontweight="bold", fontsize=10)

    fig.suptitle("Walsh Code Orthogonal Spreading Sequences ($C_i \\cdot C_j = 0$ for $i \\neq j$)", fontsize=15, fontweight='bold')
    plt.tight_layout()
    out_path = os.path.join(OUTPUT_DIR, "walsh_hadamard_matrix.png")
    plt.savefig(out_path, dpi=300)
    plt.close()
    print(f"[+] Saved: {out_path}")

def plot_waveforms():
    walsh = generate_walsh_matrix(4)
    # Scenario: 4 stations, transmitting bits: S0: 1, S1: 0, S2: silent, S3: 1
    data_symbols = [1, -1, 0, 1] # +1 for bit 1, -1 for bit 0, 0 for silent
    chips = []
    for i in range(4):
        chips.append(data_symbols[i] * walsh[i])
    chips = np.array(chips)
    composite = np.sum(chips, axis=0)

    fig, axes = plt.subplots(6, 1, figsize=(11, 10), sharex=True)
    t = np.arange(5) # for step plotting

    station_names = [
        "Station 0 ($d_0 = +1$, bit 1)",
        "Station 1 ($d_1 = -1$, bit 0)",
        "Station 2 ($d_2 =  0$, silent)",
        "Station 3 ($d_3 = +1$, bit 1)"
    ]
    colors = ['#1f77b4', '#ff7f0e', '#7f7f7f', '#2ca02c']

    for i in range(4):
        signal = np.append(chips[i], chips[i][-1])
        axes[i].step(t, signal, where='post', color=colors[i], linewidth=2.2)
        axes[i].set_ylabel(f"Station {i}\nAmplitude", fontsize=9, fontweight='bold')
        axes[i].set_ylim(-1.5, 1.5)
        axes[i].grid(True, linestyle="--", alpha=0.6)
        axes[i].set_title(f"{station_names[i]} - Chip Sequence $S_{i} = d_{i} \\times C_{i}$", fontsize=10, loc='left')

    # Composite channel
    comp_signal = np.append(composite, composite[-1])
    axes[4].step(t, comp_signal, where='post', color='#d62728', linewidth=2.5)
    axes[4].set_ylabel("Channel\n$S_{chan}$", fontsize=10, fontweight='bold')
    axes[4].set_ylim(-3.5, 3.5)
    axes[4].grid(True, linestyle="--", alpha=0.6)
    axes[4].set_title("Composite Shared Physical Channel Signal: $S_{channel} = \\sum_{i=0}^3 S_i$", fontsize=11, fontweight='bold', loc='left')

    # Correlation at receiver
    correlations = [np.dot(composite, walsh[i]) / 4.0 for i in range(4)]
    x_pos = np.arange(4) + 0.5
    axes[5].bar(x_pos, correlations, width=0.4, color=colors, edgecolor='black', alpha=0.85)
    axes[5].axhline(0, color='black', linewidth=1)
    axes[5].set_ylabel("Decision\n$(S \\cdot C_i)/W$", fontsize=9, fontweight='bold')
    axes[5].set_ylim(-1.5, 1.5)
    axes[5].set_xticks(x_pos)
    axes[5].set_xticklabels([f"Receiver {i}\n(Decoded: {data_symbols[i]})" for i in range(4)], fontsize=10, fontweight='bold')
    axes[5].grid(True, linestyle="--", alpha=0.6)
    axes[5].set_title("Receiver Output: Normalized Inner Product $(S_{channel} \\cdot C_i) / W$ -> Perfect Detection", fontsize=10, loc='left')

    axes[4].set_xticks(range(4))
    axes[4].set_xticklabels([f"Chip {k}" for k in range(4)], fontsize=10)

    fig.suptitle("CDMA Chip Spreading, Linear Channel Superposition, and Orthogonal Despreading", fontsize=14, fontweight='bold')
    plt.tight_layout()
    out_path = os.path.join(OUTPUT_DIR, "cdma_waveforms_superposition.png")
    plt.savefig(out_path, dpi=300)
    plt.close()
    print(f"[+] Saved: {out_path}")

def plot_ber_vs_noise():
    noise_sigmas = np.linspace(0.0, 3.0, 31)
    num_trials = 10000

    plt.figure(figsize=(9, 6))

    for w in [2, 4, 8, 16]:
        walsh = generate_walsh_matrix(w)
        code0 = walsh[0] # Station 0's code
        bers = []

        for sigma in noise_sigmas:
            # Generate random bits (0 or 1)
            bits = np.random.randint(0, 2, size=num_trials)
            symbols = np.where(bits == 1, 1, -1) # +1 or -1

            # Spread into chips
            chips = np.outer(symbols, code0) # shape: (num_trials, w)

            # Add Gaussian noise
            if sigma > 0:
                noise = np.random.normal(0, sigma, size=chips.shape)
                received_chips = chips + noise
            else:
                received_chips = chips

            # Correlate at receiver
            decision_values = np.dot(received_chips, code0) / w
            detected_bits = np.where(decision_values >= 0, 1, 0)

            errors = np.sum(bits != detected_bits)
            bers.append(errors / num_trials)

        label = f"Walsh $W={w}$ ($10\\log_{{10}}(W) = {10*np.log10(w):.1f}$ dB)"
        plt.semilogy(noise_sigmas, np.maximum(bers, 1e-4), marker='o', markersize=4, label=label, linewidth=1.8)

    plt.title("CDMA Bit Error Rate (BER) vs Channel Noise StdDev (Processing Gain Effect)", fontsize=13, fontweight='bold', pad=12)
    plt.xlabel("Channel Noise Standard Deviation $\\sigma$", fontsize=11, fontweight='bold')
    plt.ylabel("Bit Error Rate (BER) [Log Scale]", fontsize=11, fontweight='bold')
    plt.ylim(1e-4, 1.0)
    plt.xlim(0, 3.0)
    plt.grid(True, which="both", linestyle="--", alpha=0.5)
    plt.legend(fontsize=10, loc="lower right")
    plt.tight_layout()
    out_path = os.path.join(OUTPUT_DIR, "ber_vs_noise.png")
    plt.savefig(out_path, dpi=300)
    plt.close()
    print(f"[+] Saved: {out_path}")

def plot_cdma_vs_csma():
    stations = np.arange(1, 17)

    # In CSMA (Assignment 3), throughput degrades as stations increase due to collisions
    # CSMA/CD throughput model: S = 1 / (1 + 2*a*e*(N-1)/N) or empirical curve
    csma_efficiency = 1.0 / (1.0 + 0.25 * (stations - 1))
    csma_aggregate_throughput = stations * csma_efficiency * 0.45 # normalized

    # In CDMA (Assignment 4), as long as N <= W (e.g. W=16), all stations transmit simultaneously
    # with 0 collisions, giving linear aggregate capacity!
    cdma_efficiency = np.ones_like(stations, dtype=float)
    cdma_aggregate_throughput = stations * cdma_efficiency * 0.50 # normalized bit rate

    plt.figure(figsize=(9, 5.5))
    plt.plot(stations, cdma_aggregate_throughput, 'g-o', linewidth=2.5, markersize=7, label="CDMA (Walsh Orthogonal: 0 Collisions)")
    plt.plot(stations, csma_aggregate_throughput, 'r--s', linewidth=2.2, markersize=6, label="CSMA / CSMA-CD (Contention & Collisions)")

    plt.title("Multiple Access Throughput Comparison: CDMA vs CSMA", fontsize=13, fontweight='bold', pad=12)
    plt.xlabel("Number of Contending Stations ($N$)", fontsize=11, fontweight='bold')
    plt.ylabel("Aggregate Channel Throughput (Normalized)", fontsize=11, fontweight='bold')
    plt.xticks(stations)
    plt.grid(True, linestyle="--", alpha=0.6)
    plt.legend(fontsize=11, loc="upper left")

    plt.annotate("CDMA: Linear scaling without collisions\n(Orthogonal Walsh Codes)", 
                 xy=(12, 6.0), xytext=(7, 6.5),
                 arrowprops=dict(facecolor='green', shrink=0.05, width=1.5, headwidth=8),
                 fontsize=10, fontweight='bold', color='darkgreen')

    plt.annotate("CSMA: Throughput saturates & degrades\ndue to packet collisions & backoffs", 
                 xy=(12, 1.4), xytext=(6, 2.5),
                 arrowprops=dict(facecolor='red', shrink=0.05, width=1.5, headwidth=8),
                 fontsize=10, fontweight='bold', color='darkred')

    plt.tight_layout()
    out_path = os.path.join(OUTPUT_DIR, "cdma_vs_csma_throughput.png")
    plt.savefig(out_path, dpi=300)
    plt.close()
    print(f"[+] Saved: {out_path}")

if __name__ == "__main__":
    print("Generating CDMA Evaluation Charts...")
    plot_walsh_matrix()
    plot_waveforms()
    plot_ber_vs_noise()
    plot_cdma_vs_csma()
    print("All charts generated successfully in 'charts/' directory.")
