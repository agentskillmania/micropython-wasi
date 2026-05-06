#!/bin/bash
# Build micropython-guest.wasm — a WASI Component that exports the subcommand interface.
#
# Output: build-component/micropython-guest.wasm
#
# Prerequisites:
#   - wasi-sdk 22+ at $HOME/wasi-sdk
#   - wit-bindgen 0.57+ in PATH
#   - MicroPython WASI build completed (run `make` first)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WASI_SDK="${WASI_SDK:-$HOME/wasi-sdk}"
CC="$WASI_SDK/bin/wasm32-wasip2-clang"
WIT_BINDGEN="${WIT_BINDGEN:-wit-bindgen}"
BUILD_DIR="$SCRIPT_DIR/build-component"
WIT_DIR="$SCRIPT_DIR/wit"
MICROPYTHON_BUILD="$SCRIPT_DIR/build"

# --- Prerequisite checks ---

if [ ! -f "$WASI_SDK/bin/wasm32-wasip2-clang" ]; then
    echo "Error: wasi-sdk not found at $WASI_SDK" >&2
    exit 1
fi

if ! command -v "$WIT_BINDGEN" &>/dev/null; then
    echo "Error: wit-bindgen not found in PATH" >&2
    exit 1
fi

# Ensure core objects are built
if [ ! -f "$MICROPYTHON_BUILD/micropython.wasm" ]; then
    echo "=== Building micropython objects ==="
    make -C "$SCRIPT_DIR"
fi

echo "=== Building micropython-guest component ==="
echo "WASI_SDK:     $WASI_SDK"
echo "WIT:          $WIT_DIR"
echo "Build dir:    $BUILD_DIR"
echo

# --- Prepare build directory ---

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/bindings" "$BUILD_DIR/obj"

# --- Step 1: Generate guest bindings from WIT ---

echo "--- Generating guest bindings ---"
$WIT_BINDGEN c "$WIT_DIR" --world guest-python --out-dir "$BUILD_DIR/bindings"

# --- Step 2: Compiler flags ---

CFLAGS="-O2"
CFLAGS+=" -I$BUILD_DIR/bindings"

# --- Step 3: Compile guest wrapper ---

echo "--- Compiling guest wrapper ---"
$CC $CFLAGS \
    -c "$SCRIPT_DIR/component/guest_main.c" \
    -o "$BUILD_DIR/obj/guest_main.o"

# --- Step 4: Compile WIT bindings ---

echo "--- Compiling WIT bindings ---"
$CC $CFLAGS \
    -c "$BUILD_DIR/bindings/guest_python.c" \
    -o "$BUILD_DIR/obj/guest_python.o"

# --- Step 5: Collect all MicroPython object files ---

echo "--- Collecting object files ---"
OBJECTS=$(find "$MICROPYTHON_BUILD" -name "*.o" | sort | tr '\n' ' ')
OBJ_COUNT=$(echo "$OBJECTS" | wc -w | tr -d ' ')
echo "Found $OBJ_COUNT object files"

# --- Step 6: Link micropython-guest.wasm ---

echo "--- Linking micropython-guest.wasm ---"

$CC -O2 \
    -o "$BUILD_DIR/micropython-guest.wasm" \
    $OBJECTS \
    "$BUILD_DIR/obj/guest_main.o" \
    "$BUILD_DIR/obj/guest_python.o" \
    "$BUILD_DIR/bindings/guest_python_component_type.o" \
    -Wl,--gc-sections \
    -Wl,--allow-undefined \
    -lsetjmp \
    -lm

echo
echo "=== Output ==="
ls -lh "$BUILD_DIR/micropython-guest.wasm"

# --- Step 7: Verify ---

echo
echo "=== Verify ==="
WASMTIME="${WASMTIME:-$HOME/bin/wasmtime}"
if [ -x "$WASMTIME" ]; then
    echo "--- WIT interface ---"
    wasm-tools component wit "$BUILD_DIR/micropython-guest.wasm" 2>/dev/null | grep -E "export|agentskillmania" || true
    echo

    echo "--- Test 1: basic print ---"
    $WASMTIME run -W exceptions=y "$BUILD_DIR/micropython-guest.wasm" \
        "print('hello from component')" 2>&1

    echo
    echo "--- Test 2: sys.argv ---"
    $WASMTIME run -W exceptions=y "$BUILD_DIR/micropython-guest.wasm" \
        "import sys; print('argv:', sys.argv)" 2>&1

    echo
    echo "--- Test 3: frozen modules (asyncio, requests, ssl) ---"
    $WASMTIME run -W exceptions=y "$BUILD_DIR/micropython-guest.wasm" \
        "import asyncio, ssl; print('asyncio ok:', hasattr(asyncio, 'run')); print('ssl ok:', hasattr(ssl, 'wrap_socket'))" 2>&1

    echo
    echo "--- Test 4: socket + DNS ---"
    $WASMTIME run -W exceptions=y -S tcp=y -S inherit-network=y -S allow-ip-name-lookup=y \
        "$BUILD_DIR/micropython-guest.wasm" \
        "import socket; print('DNS ok:', socket.getaddrinfo('example.com', 80)[0][4])" 2>&1
else
    echo "(wasmtime not found at $WASMTIME, skipping verification)"
fi

echo
echo "Build succeeded."
