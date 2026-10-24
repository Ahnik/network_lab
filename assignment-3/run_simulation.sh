#!/usr/bin/env bash
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# run_simulation.sh
# Orchestrates multi-station CSMA and CSMA/CD simulations.
# Launches the Channel Emulator and N contending stations concurrently.
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${SCRIPT_DIR}/bin"
LOGS_DIR="${SCRIPT_DIR}/logs"

mkdir -p "${LOGS_DIR}"

PORT=${PORT:-${6:-8989}}
STRATEGY=${1:-"csma-cd"}          # Options: 1-persistent, non-persistent, p-persistent, csma-cd
NUM_STATIONS=${2:-4}             # Contending stations (N)
FRAMES_PER_STATION=${3:-20}      # Frames each station attempts to send
P_VAL=${4:-0.5}                  # p value for p-persistent
TX_DURATION_MS=${5:-25}          # Simulated frame transmission airtime

echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo " Starting CSMA Simulation"
echo " Strategy:           ${STRATEGY}"
echo " Stations (N):       ${NUM_STATIONS}"
echo " Frames/Station:     ${FRAMES_PER_STATION}"
echo " p-value:            ${P_VAL}"
echo " Frame Tx Airtime:   ${TX_DURATION_MS} ms"
echo " Port:               ${PORT}"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"

# Ensure previous runs are cleaned up
fuser -k -9 "${PORT}/tcp" 2>/dev/null || true
sleep 0.2

# 1. Start the Channel Emulator in the background
EMULATOR_LOG="${LOGS_DIR}/emulator_${STRATEGY}_N${NUM_STATIONS}.log"
"${BIN_DIR}/channel_emulator" --port "${PORT}" --tx-ms "${TX_DURATION_MS}" > "${EMULATOR_LOG}" 2>&1 &
EMULATOR_PID=$!

cleanup() {
    if [[ -n "${EMULATOR_PID:-}" ]] && kill -0 "${EMULATOR_PID}" 2>/dev/null; then
        kill -INT "${EMULATOR_PID}" 2>/dev/null || true
        wait "${EMULATOR_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

# Wait briefly for emulator to bind to port
sleep 0.3

# 2. Launch N stations concurrently in the background
STATION_PIDS=()
for i in $(seq 1 "${NUM_STATIONS}"); do
    STATION_LOG="${LOGS_DIR}/station_${i}_${STRATEGY}_N${NUM_STATIONS}.log"
    "${BIN_DIR}/station" \
        --strategy "${STRATEGY}" \
        --id "${i}" \
        --prob "${P_VAL}" \
        --frames "${FRAMES_PER_STATION}" \
        --port "${PORT}" \
        --slot-ms 10 > "${STATION_LOG}" 2>&1 &
    STATION_PIDS+=($!)
    echo " Launched Station ${i} (PID: ${STATION_PIDS[-1]})"
done

echo "Contention in progress... Waiting for stations to finish."

# 3. Wait for all stations to complete
for pid in "${STATION_PIDS[@]}"; do
    wait "${pid}"
done

echo "All stations completed!"
sleep 0.2

# Stop emulator cleanly so its summary is flushed
if [[ -n "${EMULATOR_PID:-}" ]] && kill -0 "${EMULATOR_PID}" 2>/dev/null; then
    kill -INT "${EMULATOR_PID}" 2>/dev/null || true
    wait "${EMULATOR_PID}" 2>/dev/null || true
fi

# 4. Show summary results
echo ""
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo " INDIVIDUAL STATION RESULTS"
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
printf "%-8s %-15s %-10s %-10s %-11s %-12s %-14s %-16s %-14s\n" \
       "Station" "Strategy" "Attempted" "Delivered" "TxAttempts" "Collisions" "Avg Delay(ms)" "Throughput(bps)" "Efficiency(%)"
echo "-------------------------------------------------------------------------------------------------------------------"

SUM_ATT=0
SUM_DEL=0
SUM_TX_ATT=0
SUM_COLL=0
SUM_DELAY=0
SUM_TP=0
VALID_STATIONS=0

for i in $(seq 1 "${NUM_STATIONS}"); do
    STATION_LOG="${LOGS_DIR}/station_${i}_${STRATEGY}_N${NUM_STATIONS}.log"
    CSV_LINE=$(grep "^CSV_METRICS" "${STATION_LOG}" | tail -n 1 || true)
    if [[ -n "${CSV_LINE}" ]]; then
        IFS=',' read -r _ id strat p att del drop coll tx_att delay tp eff <<< "${CSV_LINE}"
        printf "%-8s %-15s %-10s %-10s %-11s %-12s %-14s %-16s %-14s\n" \
               "${id}" "${strat}" "${att}" "${del}" "${tx_att}" "${coll}" "${delay}" "${tp}" "${eff}%"
        
        SUM_ATT=$((SUM_ATT + att))
        SUM_DEL=$((SUM_DEL + del))
        SUM_TX_ATT=$((SUM_TX_ATT + tx_att))
        SUM_COLL=$((SUM_COLL + coll))
        SUM_DELAY=$(echo "${SUM_DELAY} + ${delay}" | bc -l)
        SUM_TP=$(echo "${SUM_TP} + ${tp}" | bc -l)
        VALID_STATIONS=$((VALID_STATIONS + 1))
    else
        echo " Station ${i}: Failed or error in log: ${STATION_LOG}"
    fi
done

if [[ ${VALID_STATIONS} -gt 0 ]]; then
    NET_AVG_DELAY=$(echo "scale=2; ${SUM_DELAY} / ${VALID_STATIONS}" | bc -l)
    NET_TP=$(echo "scale=2; ${SUM_TP}" | bc -l)
    if [[ ${SUM_TX_ATT} -gt 0 ]]; then
        NET_EFF=$(echo "scale=2; (${SUM_DEL} * 100.0) / ${SUM_TX_ATT}" | bc -l)
    else
        NET_EFF="0.00"
    fi
    echo "-------------------------------------------------------------------------------------------------------------------"
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    echo " AGGREGATE NETWORK PERFORMANCE"
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    echo " Contending Stations (N):    ${NUM_STATIONS}"
    echo " Total Frames Delivered:     ${SUM_DEL} / ${SUM_ATT}"
    echo " Total Network Collisions:   ${SUM_COLL}"
    echo " Network Avg Delay:          ${NET_AVG_DELAY} ms"
    echo " Aggregate Throughput:       ${NET_TP} bps"
    echo " Overall Scheme Efficiency:  ${NET_EFF} %"
    echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
    # Print machine-readable summary line
    echo "SUMMARY_METRICS,${STRATEGY},${NUM_STATIONS},${P_VAL},${SUM_ATT},${SUM_DEL},${SUM_COLL},${NET_AVG_DELAY},${NET_TP},${NET_EFF}"
fi

echo ""
echo "Simulation finished cleanly. Emulator log saved to ${EMULATOR_LOG}"
