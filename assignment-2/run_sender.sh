#!/usr/bin/env bash
# ==============================================================================
# run_sender.sh
# Runs sender test suites for Stop-and-Wait, Go-Back-N, and Selective-Repeat.
# Automatically detects the running receiver, executes each test, and terminates
# the receiver instance after each test completes.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${SCRIPT_DIR}/bin"
LOGS_DIR="${SCRIPT_DIR}/logs"
PID_FILE="${SCRIPT_DIR}/.receiver.pid"

mkdir -p "${LOGS_DIR}"

# Common test parameters
SERVER_IP="127.0.0.1"
PORT=8989
MAX_DELAY_MS=120
TIMEOUT_MS=300
SEQ_NO_BITS=4
ERROR_PROBS=("0.0" "0.2" "0.4")
TEST_FILES=("tests/test1.bin" "tests/test2.bin" "tests/test3.bin")

# Schemes to run: scheme_name sender_binary_name receiver_binary_name log_prefix
# Note: gobackn_receiver with m=1 is used for Stop-and-Wait
SCHEMES=(
    "stop_and_wait:stop_and_wait_sender:gobackn_receiver:stop_and_wait"
    "gobackn:gobackn_sender:gobackn_receiver:gobackn"
    "selective_repeat:sr_sender:sr_receiver:sr"
)

wait_for_receiver_port() {
    local timeout=60
    local elapsed=0
    echo -n "Waiting for Receiver on port ${PORT}..."
    while ! (exec 3<>/dev/tcp/127.0.0.1/${PORT}) 2>/dev/null && ! nc -z 127.0.0.1 ${PORT} 2>/dev/null; do
        sleep 0.2
        elapsed=$((elapsed + 1))
        if [[ $elapsed -ge $((timeout * 5)) ]]; then
            echo " Timeout waiting for receiver!" >&2
            return 1
        fi
    done
    exec 3<&- 3>&- 2>/dev/null || true
    echo " Ready."
    return 0
}

wait_for_port_closed() {
    local timeout=15
    local elapsed=0
    while (exec 3<>/dev/tcp/127.0.0.1/${PORT}) 2>/dev/null || nc -z 127.0.0.1 ${PORT} 2>/dev/null; do
        exec 3<&- 3>&- 2>/dev/null || true
        sleep 0.1
        elapsed=$((elapsed + 1))
        if [[ $elapsed -ge $((timeout * 10)) ]]; then
            fuser -k -9 ${PORT}/tcp 2>/dev/null || true
            sleep 0.2
            break
        fi
    done
}

kill_receiver_instance() {
    local recv_bin_name="$1"
    # Kill via PID file if present
    if [[ -f "${PID_FILE}" ]]; then
        local pid
        pid=$(cat "${PID_FILE}" 2>/dev/null || true)
        if [[ -n "${pid}" ]]; then
            kill -INT "${pid}" 2>/dev/null || kill -TERM "${pid}" 2>/dev/null || true
        fi
        rm -f "${PID_FILE}"
    fi

    # Fallback cleanup by binary name
    pkill -INT -x "${recv_bin_name}" 2>/dev/null || true
    pkill -INT -f "bin/${recv_bin_name}" 2>/dev/null || true
    wait_for_port_closed
}

cleanup() {
    for scheme_entry in "${SCHEMES[@]}"; do
        IFS=":" read -r _ _ RECV_BIN_NAME _ <<< "${scheme_entry}"
        kill_receiver_instance "${RECV_BIN_NAME}"
    done
    exit 0
}

trap cleanup SIGINT SIGTERM

echo "=== Sender Test Runner Started ==="
echo "Logs directory: ${LOGS_DIR}"
echo ""

for scheme_entry in "${SCHEMES[@]}"; do
    IFS=":" read -r SCHEME_NAME SENDER_BIN_NAME RECV_BIN_NAME LOG_PREFIX <<< "${scheme_entry}"
    SENDER_BIN="${BIN_DIR}/${SENDER_BIN_NAME}"
    LOG_FILE="${LOGS_DIR}/${LOG_PREFIX}_send.txt"

    if [[ ! -x "${SENDER_BIN}" ]]; then
        echo "Error: Binary ${SENDER_BIN} not found or not executable. Run 'make' first." >&2
        exit 1
    fi

    # Ensure log file exists without truncating
    touch "${LOG_FILE}"

    echo "=================================================="
    echo " Starting Scheme: ${SCHEME_NAME} (Sender)"
    echo " Binary: ${SENDER_BIN_NAME} | Log: ${LOG_FILE}"
    echo "=================================================="

    test_num=1
    for file in "${TEST_FILES[@]}"; do
        for error_prob in "${ERROR_PROBS[@]}"; do
            echo "--- [${SCHEME_NAME}] TEST #${test_num} (File: ${file}, Error: ${error_prob}) ---"

            # Wait until receiver is actively listening
            wait_for_receiver_port

            # Record test header to log file using append redirection
            echo -e "--- TEST #${test_num} ---\n" >> "${LOG_FILE}"

            # Prepare sender arguments
            if [[ "${SCHEME_NAME}" == "stop_and_wait" ]]; then
                ARGS=("${SERVER_IP}" "${file}" "${MAX_DELAY_MS}" "${TIMEOUT_MS}" "${error_prob}")
            else
                ARGS=("${SERVER_IP}" "${file}" "${MAX_DELAY_MS}" "${TIMEOUT_MS}" "${error_prob}" "${SEQ_NO_BITS}")
            fi

            # Execute sender synchronously: stdout displays on terminal, stderr appends to log file
            "${SENDER_BIN}" "${ARGS[@]}" 2>> "${LOG_FILE}" || {
                echo "Warning: Sender exited with error code $?" >&2
            }

            # Brief pause to ensure all final ACKs are sent and flushed
            sleep 0.3

            # Kill the receiver program when the sender returns
            kill_receiver_instance "${RECV_BIN_NAME}"

            echo "[${SCHEME_NAME}] TEST #${test_num} finished."
            test_num=$((test_num + 1))
            sleep 0.2
        done
    done
done

# Create alias symlink if needed for selective_repeat
if [[ -f "${LOGS_DIR}/sr_send.txt" && ! -f "${LOGS_DIR}/selective_repeat_send.txt" ]]; then
    ln -s "sr_send.txt" "${LOGS_DIR}/selective_repeat_send.txt" 2>/dev/null || true
fi

echo ""
echo "=== All Sender Tests Completed Successfully! ==="
