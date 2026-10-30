#!/usr/bin/env bash
# ==============================================================================
# run_receiver.sh
# Runs receiver test suites for Stop-and-Wait, Go-Back-N, and Selective-Repeat.
# This script should be started first, waiting for connections from the sender.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${SCRIPT_DIR}/bin"
LOGS_DIR="${SCRIPT_DIR}/logs"
PID_FILE="${SCRIPT_DIR}/.receiver.pid"
PORT=8989

mkdir -p "${LOGS_DIR}"

# Common test parameters
MAX_DELAY_MS=120
SEQ_NO_BITS=4
ERROR_PROBS=("0.0" "0.2" "0.4")
TEST_FILES=("tests/test1.bin" "tests/test2.bin" "tests/test3.bin")

# Schemes to run: scheme_name receiver_binary_name log_prefix
# Note: gobackn_receiver with m=1 is used for Stop-and-Wait
SCHEMES=(
    "stop_and_wait:gobackn_receiver:stop_and_wait"
    "gobackn:gobackn_receiver:gobackn"
    "selective_repeat:sr_receiver:sr"
)

wait_for_port_free() {
    local timeout=15
    local elapsed=0
    while (exec 3<>/dev/tcp/127.0.0.1/${PORT}) 2>/dev/null || nc -z 127.0.0.1 ${PORT} 2>/dev/null; do
        exec 3<&- 3>&- 2>/dev/null || true
        fuser -k ${PORT}/tcp 2>/dev/null || true
        pkill -9 -f "_receiver" 2>/dev/null || true
        sleep 0.1
        elapsed=$((elapsed + 1))
        if [[ $elapsed -ge $((timeout * 10)) ]]; then
            echo "Warning: Port ${PORT} busy after ${timeout}s, force killing..." >&2
            fuser -k -9 ${PORT}/tcp 2>/dev/null || true
            sleep 0.5
            break
        fi
    done
}

cleanup() {
    if [[ -f "${PID_FILE}" ]]; then
        local pid
        pid=$(cat "${PID_FILE}" 2>/dev/null || true)
        if [[ -n "${pid}" ]]; then
            kill -INT "${pid}" 2>/dev/null || kill -TERM "${pid}" 2>/dev/null || true
        fi
        rm -f "${PID_FILE}"
    fi
    fuser -k ${PORT}/tcp 2>/dev/null || true
    exit 0
}

trap cleanup SIGINT SIGTERM EXIT

# Kill any leftover receiver instances before starting
wait_for_port_free

echo "=== Receiver Test Runner Started ==="
echo "Logs directory: ${LOGS_DIR}"
echo ""

for scheme_entry in "${SCHEMES[@]}"; do
    IFS=":" read -r SCHEME_NAME RECV_BIN_NAME LOG_PREFIX <<< "${scheme_entry}"
    RECV_BIN="${BIN_DIR}/${RECV_BIN_NAME}"
    LOG_FILE="${LOGS_DIR}/${LOG_PREFIX}_recv.txt"

    if [[ ! -x "${RECV_BIN}" ]]; then
        echo "Error: Binary ${RECV_BIN} not found or not executable. Run 'make' first." >&2
        exit 1
    fi

    # Ensure log file exists without truncating
    touch "${LOG_FILE}"

    echo "=================================================="
    echo " Starting Scheme: ${SCHEME_NAME} (Receiver)"
    echo " Binary: ${RECV_BIN_NAME} | Log: ${LOG_FILE}"
    echo "=================================================="

    test_num=1
    for file in "${TEST_FILES[@]}"; do
        for error_prob in "${ERROR_PROBS[@]}"; do
            echo -e "--- TEST #${test_num} ---\n" >> "${LOG_FILE}"
            echo "[${SCHEME_NAME}] Waiting for Sender to run TEST #${test_num} (${file}, error=${error_prob})..."

            # Ensure port 8989 is completely unbound before launching next receiver
            wait_for_port_free

            # Prepare receiver arguments (m=1 for Stop-and-Wait)
            if [[ "${SCHEME_NAME}" == "stop_and_wait" ]]; then
                ARGS=("${MAX_DELAY_MS}" "${error_prob}" "1")
            else
                ARGS=("${MAX_DELAY_MS}" "${error_prob}" "${SEQ_NO_BITS}")
            fi

            # Launch receiver in background; stdout goes to terminal, stderr appends to log file
            "${RECV_BIN}" "${ARGS[@]}" 2>> "${LOG_FILE}" &
            RECV_PID=$!
            echo "${RECV_PID}" > "${PID_FILE}"

            # Wait for sender to finish transmission and kill this receiver instance
            wait "${RECV_PID}" 2>/dev/null || true
            rm -f "${PID_FILE}"

            echo "[${SCHEME_NAME}] TEST #${test_num} completed."
            test_num=$((test_num + 1))
            sleep 0.2
        done
    done
done

# Create alias symlink if needed for selective_repeat
if [[ -f "${LOGS_DIR}/sr_recv.txt" && ! -f "${LOGS_DIR}/selective_repeat_recv.txt" ]]; then
    ln -s "sr_recv.txt" "${LOGS_DIR}/selective_repeat_recv.txt" 2>/dev/null || true
fi

echo ""
echo "=== All Receiver Tests Completed Successfully! ==="
