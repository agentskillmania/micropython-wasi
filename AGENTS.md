# MicroPython Agent Guide

This document provides the essential context an AI coding agent needs to work effectively in the MicroPython repository.

## Project Overview

MicroPython is a lean and efficient implementation of Python 3.x designed to run on microcontrollers and small embedded systems. It implements Python 3.4 syntax plus selected features from later versions (including `async`/`await`). The project is written primarily in C, with build tooling in Python and GNU Make.

- **Version**: 1.27.0
- **License**: MIT
- **Repository structure**: Monorepo containing the core VM, cross-compiler, many hardware ports, tests, and documentation.

## Directory Structure

| Directory | Purpose |
|-----------|---------|
| `py/` | Core Python implementation: compiler, bytecode VM, runtime, garbage collector, and native code emitters. |
| `mpy-cross/` | The MicroPython cross-compiler. Pre-compiles `.py` files into `.mpy` bytecode for freezing into firmware. |
| `ports/` | Platform-specific code. Each subdirectory is a port (e.g., `unix`, `stm32`, `esp32`, `rp2`, `webassembly`). |
| `extmod/` | Additional non-core modules implemented in C (e.g., `machine`, `bluetooth`, `asyncio`, `btree`, `deflate`). |
| `shared/` | Shared utility code used across multiple ports (e.g., `runtime/`, `readline/`, `timeutils/`). |
| `lib/` | Git submodules for external dependencies (e.g., `pico-sdk`, `mbedtls`, `tinyusb`, `littlefs`, `btstack`). |
| `tests/` | Comprehensive test suite including CPython-comparison tests, `.exp` output tests, and `unittest` tests. |
| `tools/` | Build and maintenance scripts (e.g., `ci.sh`, `codeformat.py`, `mpy-tool.py`, `mpy_ld.py`, `makemanifest.py`). |
| `docs/` | User documentation written in Sphinx reStructuredText. |
| `drivers/` | Hardware driver collections. |
| `examples/` | Example Python scripts and native module examples. |

## Technology Stack

- **C99 / GNU C**: Core implementation.
- **Python 3.3+**: Build scripts and test runner (CPython 3.8.2+ is required to run the full test suite).
- **GNU Make**: Primary build system.
- **CMake**: Used by some ports (`rp2`, `esp32`, `zephyr`).
- **Emscripten**: Required to build the `webassembly` port.
- **Sphinx**: Documentation generation.

## Build System

### General prerequisites
- `git`, `bash`, `gcc` (or `clang` on macOS), `python3`, `make`.
- Some ports need additional toolchains or SDKs (e.g., ARM GCC, ESP-IDF, Emscripten).

### Typical build steps

1. **Build `mpy-cross`** (required by most ports):
   ```bash
   cd mpy-cross
   make
   ```

2. **Fetch submodules** (if building from git):
   ```bash
   cd ports/<port_name>
   make submodules
   ```

3. **Build a port**:
   ```bash
   cd ports/<port_name>
   make
   ```

Many ports support **variants** (e.g., `standard`, `minimal`, `coverage`). Set them with:
```bash
make VARIANT=<variant>
```

### Key ports for development and CI
- `ports/unix` -- used for host-based testing and development.
- `ports/webassembly` -- Emscripten port producing `micropython.mjs` and `micropython.wasm`.
- `ports/stm32`, `ports/esp32`, `ports/rp2` -- major microcontroller ports.

## Testing

The test suite lives in `tests/` and is driven by `tests/run-tests.py`.

### Running tests

- **Against the Unix port** (default):
  ```bash
  cd tests
  ./run-tests.py
  ```

- **Against a bare-metal board via serial**:
  ```bash
  ./run-tests.py -t /dev/ttyACM0
  ```

- **Print failures from the last run**:
  ```bash
  ./run-tests.py --print-failures
  ```

### Test types
1. **CPython-comparison tests** (no `.exp` file): Run under CPython to capture expected output, then under MicroPython and compare.
2. **Expected-output tests** (with `.exp` file): Run only under MicroPython; output must match the `.exp` file exactly.
3. **`unittest` tests**: Run only under MicroPython and pass if the `unittest` runner prints `OK`.

### Skipping unsupported features
Tests that rely on features not present on all platforms should detect the missing capability, print `SKIP`, and call `sys.exit()`. The runner also uses `tests/feature_check/` scripts to conditionally skip tests.

### Additional test runners
- `run-perfbench.py` -- performance benchmarks.
- `run-multitests.py` -- multi-device / network tests.
- `run-natmodtests.py` -- native `.mpy` module tests.
- `run-internalbench.py` -- internal benchmarks.

## Code Style and Formatting

### C code
- Formatted with `uncrustify` **v0.71 or v0.72 only** (newer versions are incompatible).
- Run `tools/codeformat.py` on changed C files before committing.
- Conventions:
  - 4 spaces, no tabs.
  - Opening braces on the same line.
  - `// ` for comments (not `/* */`).
  - Public names in `py/` and `extmod/` should start with `mp_` or `MP_`.
  - Use `mp_int_t` / `mp_uint_t` for machine-word-sized integers.
  - Use `m_new`, `m_renew`, `m_del` macros for heap allocation.

### Python code
- Formatted with `ruff format` (line length 99, target Python 3.8+).
- Linted with `ruff`.
- Naming:
  - Modules: short, lowercase (e.g., `pyb`).
  - Classes: `CamelCase` with acronyms all uppercase (e.g., `I2C`).
  - Functions: `snake_case`.
  - Constants: `UPPER_CASE`.

### Spell checking
- `codespell` is used in CI; configured in `pyproject.toml`.

### Pre-commit hooks
The repository provides a `.pre-commit-config.yaml` that checks:
- C code formatting (`tools/codeformat.py`)
- Python linting and formatting (`ruff`)
- Commit message format (`tools/verifygitlog.py`)
- Spell checking (`codespell`)

Install with:
```bash
pre-commit install --hook-type pre-commit --hook-type commit-msg
```

## Git Commit Conventions

- **Prefix**: Start the subject with a directory or file path prefix (e.g., `py/objstr:`, `ports/unix:`, `docs:`).
- **Subject line**: A grammatical sentence ending with a full stop, max 72 characters.
- **Body**: Add a blank line after the subject, then detailed description if needed (wrap at 75 characters).
- **Sign-off**: Commits must include a `Signed-off-by:` line (`git commit -s`).
- Good examples:
  - `py/objstr: Add splitlines() method.`
  - `ports/unix: Fix socket timeout handling.`

## Configuration Files

| File | Purpose |
|------|---------|
| `pyproject.toml` | Configures `ruff` (linting/formatting) and `codespell` (spell checking). |
| `.pre-commit-config.yaml` | Pre-commit hooks for code formatting, commit message validation, and spell checking. |
| `tools/uncrustify.cfg` | uncrustify configuration for C code formatting. |
| `py/mpconfig.h` | Default core feature/configuration macros. |
| `ports/<port>/mpconfigport.h` | Port-specific configuration overrides. |

## CI / Automation

- GitHub Actions workflows are in `.github/workflows/` (one per port or cross-cutting concern).
- `tools/ci.sh` is the central script used by CI; it defines `ci_<port>_*` helper functions for setup, build, and test steps.
- CI runs formatting checks (`codeformat`, `ruff`, `codespell`, `verifygitlog`) and builds/tests many port variants.

## Working with the WebAssembly Port

Given the workspace context, the `ports/webassembly` port is particularly relevant:

- **Dependencies**: Emscripten SDK, optionally `terser` for minification.
- **Build**:
  ```bash
  cd ports/webassembly
  make              # produces build-<variant>/micropython.mjs and .wasm
  make min          # produces minified output
  make repl         # run REPL via Node.js
  ```
- **Variants**: `standard` (default) and `pyscript`.
- **Key files**: `main.c`, `proxy_c.c`, `objjsproxy.c`, `api.js`, `library.js`.

## Security and Contribution Notes

- All contributions are under the MIT license.
- Contributors must follow the [Contributor Guidelines](https://github.com/micropython/micropython/wiki/ContributorGuidelines) and `CODECONVENTIONS.md`.
- Do not run `git commit`, `git push`, `git rebase`, etc., unless explicitly asked.
