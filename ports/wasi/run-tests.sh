#!/bin/bash
# Run MicroPython test suite against the WASI port.
#
# Usage:
#   ./run-tests.sh                    # run all default tests
#   ./run-tests.sh -d basics          # run only basics tests
#   ./run-tests.sh basics/assign1.py  # run a single test
#
# Environment variables:
#   MICROPY_MICROPYTHON_WASM  - path to micropython.wasm (default: build/micropython.wasm)
#   WASMTIME                  - path to wasmtime binary (default: wasmtime)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WASM_DIR="$SCRIPT_DIR"

# Default wasm path if not set
export MICROPY_MICROPYTHON_WASM="${MICROPY_MICROPYTHON_WASM:-$WASM_DIR/build/micropython.wasm}"

if [ ! -f "$MICROPY_MICROPYTHON_WASM" ]; then
    echo "Error: $MICROPY_MICROPYTHON_WASM not found. Run 'make' first." >&2
    exit 1
fi

if ! command -v "${WASMTIME:-wasmtime}" >/dev/null 2>&1; then
    echo "Error: wasmtime not found. Install it or set WASMTIME env var." >&2
    exit 1
fi

cd "$SCRIPT_DIR/../../tests"
exec python3 run-tests.py -t wasi "$@"
