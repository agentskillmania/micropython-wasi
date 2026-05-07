# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MicroPython 1.27.0 with a custom **WASI Preview2 port** (`ports/wasi/`). The WASI port compiles MicroPython to a WebAssembly component that runs under `wasmtime`, supporting Python 3.5+ syntax (f-strings, async/await), POSIX file system via VFS, TCP/UDP sockets with TLS (mbedTLS), and frozen Python modules (asyncio, requests, ssl, shutil, tempfile, pathlib, gzip).

The WASI port also supports a **Component Model** mode where it exports `agentskillmania:subcommand/python` via WIT, allowing it to be composed into a host component (e.g., busybox-wasi) using `wac plug`.

## Build Commands

### Prerequisites

- WASI SDK 22+ at `~/wasi-sdk` (or set `WASI_SDK_PATH`)
- `wasmtime` for running the output
- `wit-bindgen 0.57+` for component builds
- Standard: `git`, `bash`, `python3`, `make`

### Build the WASI port

```bash
cd ports/wasi
make                                    # produces build/micropython.wasm (~1.3MB)
```

### Build the WASI Component (guest mode)

```bash
cd ports/wasi
./build_component.sh                    # produces build-component/micropython-guest.wasm (~1.4MB)
```

### Run

```bash
# Basic execution
wasmtime run -W exceptions=y build/micropython.wasm "print('hello')"

# With file system access
wasmtime run -W exceptions=y --dir .:/work build/micropython.wasm "import os; print(os.listdir('/work'))"

# With networking
wasmtime run -W exceptions=y -S tcp=y -S udp=y -S inherit-network=y -S allow-ip-name-lookup=y build/micropython.wasm "..."
```

### Test

```bash
# Quick smoke test (built into Makefile)
cd ports/wasi && make test

# Full MicroPython test suite (against unix port)
cd tests && ./run-tests.py

# Custom WASI tests
python3 ports/wasi/test_features.py
python3 ports/wasi/test_wasi_port.py
```

## Architecture

### Core MicroPython (shared across all ports)

- `py/` — Compiler, bytecode VM, runtime, GC, native emitters. Public C names use `mp_`/`MP_` prefix.
- `mpy-cross/` — Cross-compiler: `.py` → `.mpy` bytecode for freezing into firmware.
- `extmod/` — Additional C modules (asyncio, select, socket, json, re, hashlib, deflate, etc.).
- `shared/` — Shared utilities across ports (timeutils, readline, stdout_helpers).
- `lib/` — Git submodules (mbedtls, tinyusb, littlefs, etc.).
- `tests/` — Test suite driven by `tests/run-tests.py` (CPython-comparison, `.exp` output, unittest).

### WASI Port (`ports/wasi/`)

| File | Purpose |
|------|---------|
| `mpconfigport.h` | All feature macros — this is the primary configuration file. Currently enables BASIC_FEATURES level + 60+ extras (Level 1–4 from EXTRA_FEATURES_LIST.md). |
| `Makefile` | Build config: `wasm32-wasip2-clang`, exception handling flags (`-wasm-enable-sjlj`), links mbedTLS from sibling `mbedtls/` project. |
| `main.c` | Entry point, CPython-compatible CLI argument parsing. |
| `modsocket.c` | Socket module wrapping WASI POSIX sockets (TCP/UDP, DNS, settimeout). |
| `mphalport.c` / `mphalport.h` | HAL: stdout/stdin, millis ticks, sleep. |
| `manifest.py` | Frozen module manifest: asyncio, ssl, requests, shutil, tempfile, pathlib, gzip. |
| `build_component.sh` | Builds the WASI Component (guest) variant with WIT bindings. |
| `wit/subcommand.wit` | WIT interface: `agentskillmania:subcommand/python` with `execute(args: list<string>) -> s32`. |
| `component/guest_main.c` | Guest wrapper that bridges WIT `execute()` to MicroPython's main. |

### Build pipeline

```
mpy-cross (cross-compiler)
    ↓
py/ + extmod/ + shared/ → .o files
    ↓
ports/wasi/*.c → .o files
    ↓
Link → build/micropython.wasm (CLI mode)
    ↓
build_component.sh → build-component/micropython-guest.wasm (Component mode)
```

The Component build reuses the same `.o` files from the CLI build, adds WIT-generated bindings, and links a separate wasm.

### mbedTLS integration

mbedTLS is expected at `../../mbedtls/` (sibling to this repo in the monorepo). The Makefile overrides `MBEDTLS_DIR` to point there. Custom entropy source at `../../mbedtls/wasi_entropy.c` replaces `/dev/urandom` (unavailable in WASI).

## Code Conventions

### C code

- 4 spaces, no tabs. Opening braces on same line. `//` comments only.
- Public names in `py/` and `extmod/` start with `mp_` or `MP_`.
- Use `mp_int_t`/`mp_uint_t` for machine-word-sized integers.
- Use `m_new`/`m_renew`/`m_del` macros for heap allocation (defined in `py/misc.h`).
- Format with `tools/codeformat.py` (uses uncrustify v0.71/0.72 only — newer versions break).

### Python code

- Formatted with `ruff format` (line length 99, target Python 3.8+).
- Module names: short lowercase. Classes: `CamelCase` (acronyms uppercase: `I2C`). Functions: `snake_case`. Constants: `UPPER_CASE`.

### Git commits

- Prefix with directory/file path: `ports/wasi:`, `py/objstr:`, `extmod:` etc.
- Subject: grammatical sentence with full stop, max 72 chars.
- Must include `Signed-off-by:` line (`git commit -s`).

## Key Configuration

- `MICROPY_HEAP_SIZE` — Set to 256KB in `mpconfigport.h`.
- `MICROPY_CONFIG_ROM_LEVEL` — Set to `BASIC_FEATURES`, then 60+ individual feature macros enabled on top.
- `MICROPY_PY_SSL`/`MICROPY_SSL_MBEDTLS` — Set via Makefile (not mpconfigport.h) to avoid redefinition warnings with extmod.mk.
- `MICROPY_HELPER_REPL` — Disabled (REPL not needed in sandbox environment).

## Feature Expansion

`EXTRA_FEATURES_LIST.md` tracks 77 potential features organized by risk level (Level 1: pure macros → Level 5: platform-dependent). Current status: ~60+ features enabled, ~10 skipped (REPL-related, stack check that hangs in WASI, stdfiles qstr conflict).
