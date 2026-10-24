#!/usr/bin/env bash
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# run_experiments.sh
# Automated parameter sweep and performance evaluation for CSMA schemes:
#   Case (i):  p-Persistent CSMA vs probability p (fixed N)
#   Case (ii): Comparative MAC schemes vs contending stations N
# Automatically runs simulations, logs CSV datasets, and invokes plot_metrics.py.
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${SCRIPT_DIR}/bin"
LOGS_DIR="${SCRIPT_DIR}/logs"
CHARTS_DIR="${SCRIPT_DIR}/charts"

mkdir -p "${LOGS_DIR}" "${CHARTS_DIR}"

MODE=${1:-"all"} # options: all, case1, case2, plot

# Experiment Parameters
CASE1_N=4
CASE1_FRAMES=15
CASE1_TX_MS=25
CASE1_P_VALUES=(0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9)

CASE2_N_VALUES=(2 3 4 5 6 8)
CASE2_FRAMES=15
CASE2_TX_MS=25
CASE2_SCHEMES=("non-persistent" "1-persistent" "p-persistent" "csma-cd")

# Helper function to run a single simulation instance and extract metrics
run_single_sim() {
    local strat="$1"
    local n="$2"
    local frames="$3"
    local p="$4"
    local tx_ms="$5"
    local port="${6:-8989}"

    # Kill any lingering process on this port
    fuser -k -9 "${port}/tcp" 2>/dev/null || true
    sleep 0.1

    local output
    output=$("${SCRIPT_DIR}/run_simulation.sh" "${strat}" "${n}" "${frames}" "${p}" "${tx_ms}" "${port}")
    local summary_line
    summary_line=$(echo "${output}" | grep "^SUMMARY_METRICS" | tail -n 1 || true)
    
    if [[ -z "${summary_line}" ]]; then
        echo "ERROR: Simulation failed for ${strat} (N=${n}, p=${p})" >&2
        return 1
    fi
    echo "${summary_line}"
}

run_case_i() {
    echo ""
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    echo "  RUNNING CASE (i): p-Persistent CSMA Performance vs Probability p"
    echo "  Fixed Stations (N): ${CASE1_N} | Frames/Station: ${CASE1_FRAMES} | Airtime: ${CASE1_TX_MS} ms"
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

    local csv_file="${LOGS_DIR}/case_i_p_sweep.csv"
    echo "p,N,FramesPerStation,Delivered,TotalAttempted,Collisions,AvgDelayMs,ThroughputBps,Efficiency" > "${csv_file}"

    for p in "${CASE1_P_VALUES[@]}"; do
        echo -n " [*] Testing p = ${p} ... "
        local res
        res=$(run_single_sim "p-persistent" "${CASE1_N}" "${CASE1_FRAMES}" "${p}" "${CASE1_TX_MS}")
        
        IFS=',' read -r _ strat n p_val att del coll delay tp eff <<< "${res}"
        echo "${p},${n},${CASE1_FRAMES},${del},${att},${coll},${delay},${tp},${eff}" >> "${csv_file}"
        echo "Delivered=${del}/${att}, Collisions=${coll}, Delay=${delay}ms, Throughput=${tp}bps, Eff=${eff}%"
        sleep 0.2
    done

    echo " [✓] Case (i) sweep complete. Logged to ${csv_file}"
}

run_case_ii() {
    echo ""
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    echo "  RUNNING CASE (ii): Comparative MAC Schemes vs Contending Stations (N)"
    echo "  Schemes: ${CASE2_SCHEMES[*]}"
    echo "  Stations N: ${CASE2_N_VALUES[*]} | Frames/Station: ${CASE2_FRAMES} | Airtime: ${CASE2_TX_MS} ms"
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

    local csv_file="${LOGS_DIR}/case_ii_n_sweep.csv"
    echo "Scheme,N,FramesPerStation,Delivered,TotalAttempted,Collisions,AvgDelayMs,ThroughputBps,Efficiency" > "${csv_file}"

    for n in "${CASE2_N_VALUES[@]}"; do
        echo ""
        echo ">>> Contending Stations N = ${n} <<<"
        local p_opt
        p_opt=$(echo "scale=3; 1.0 / ${n}" | bc -l)

        for strat in "${CASE2_SCHEMES[@]}"; do
            local p_val="0.5"
            if [[ "${strat}" == "p-persistent" ]]; then
                p_val="${p_opt}"
            fi

            echo -n " [*] Scheme: ${strat} (p=${p_val}) ... "
            local res
            res=$(run_single_sim "${strat}" "${n}" "${CASE2_FRAMES}" "${p_val}" "${CASE2_TX_MS}")

            IFS=',' read -r _ s stations p_v att del coll delay tp eff <<< "${res}"
            echo "${strat},${stations},${CASE2_FRAMES},${del},${att},${coll},${delay},${tp},${eff}" >> "${csv_file}"
            echo "Delivered=${del}/${att}, Collisions=${coll}, Delay=${delay}ms, Throughput=${tp}bps, Eff=${eff}%"
            sleep 0.2
        done
    done

    echo " [✓] Case (ii) sweep complete. Logged to ${csv_file}"
}

# Main Execution Flow
case "${MODE}" in
    "all")
        run_case_i
        run_case_ii
        echo ""
        python3 "${SCRIPT_DIR}/plot_metrics.py"
        ;;
    "case1")
        run_case_i
        echo ""
        python3 "${SCRIPT_DIR}/plot_metrics.py"
        ;;
    "case2")
        run_case_ii
        echo ""
        python3 "${SCRIPT_DIR}/plot_metrics.py"
        ;;
    "plot")
        python3 "${SCRIPT_DIR}/plot_metrics.py"
        ;;
    *)
        echo "Usage: $0 [all|case1|case2|plot]"
        exit 1
        ;;
esac

echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo "  All experiments and plotting routines completed successfully!"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
