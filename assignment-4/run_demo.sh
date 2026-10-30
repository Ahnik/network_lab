#!/usr/bin/env bash
# ==============================================================================
# run_demo.sh
# Runs visual, interactive and benchmark demonstrations of CDMA with Walsh codes.
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${SCRIPT_DIR}/bin"

if [[ ! -x "${BIN_DIR}/cdma_demo" ]]; then
    echo "Binary ${BIN_DIR}/cdma_demo not found. Compiling..."
    make -C "${SCRIPT_DIR}" all
fi

MODE=${1:-"all"}
STATIONS=${2:-4}

echo "Running CDMA Demonstration (Mode: ${MODE}, Stations: ${STATIONS})..."
"${BIN_DIR}/cdma_demo" --mode "${MODE}" --stations "${STATIONS}"
