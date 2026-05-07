# WASI Test Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable `run-tests.py -t wasi` to run MicroPython's upstream test suite against the WASI port.

**Architecture:** Add a `WasiRuntimeRunner` class to `tests/run-tests.py` (modeled after `PyboardNodeRunner`) that invokes `wasmtime run` to execute tests. Patch `ports/wasi/main.c` to read the `MICROPYPATH` env var (needed for `import unittest`) and ignore `-X` flags (compatibility with test runner). After the code changes, run the test suite in phases to collect results and populate the skip list.

**Tech Stack:** C (main.c), Python (run-tests.py), wasmtime CLI, bash

**Design spec:** `.vibe/2026-05-07-wasi-test-integration-design.md`

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `ports/wasi/main.c` | Modify | Add MICROPYPATH reading + `-X` flag handling |
| `tests/run-tests.py` | Modify | Add `WasiRuntimeRunner` class, `wasi` target registration, skip list |
| `ports/wasi/run-tests.sh` | Create | Convenience script to run tests |

---

### Task 1: Add `-X` flag handling to `ports/wasi/main.c`

`run-tests.py` passes `-X emit=bytecode` on the command line for non-remote targets, and `-X heapsize=...` is used by `PyboardNodeRunner`. The WASI port currently rejects `-X` as an unknown option, causing every test to fail.

**Files:**
- Modify: `ports/wasi/main.c:150-155` (the `argv[1][0] == '-'` else branch)

- [ ] **Step 1: Edit `ports/wasi/main.c` to handle `-X` flag**

In `mpy_cli_main()`, find the `else if (argv[1][0] == '-')` block (around line 150). Insert a new branch **before** it that handles `-X` by skipping the flag and its argument:

```c
        } else if (strcmp(argv[1], "-X") == 0) {
            // Ignore -X options (run-tests.py compatibility)
            if (argc > 2) {
                // Skip both -X and its argument, then reparse
                int ret = mpy_cli_main(argc - 2, argv + 2);
                return ret;
            } else {
                fprintf(stderr, "python: -X requires an argument\n");
                return 1;
            }
        } else if (argv[1][0] == '-') {
```

The key detail: this uses recursive call to `mpy_cli_main(argc - 2, argv + 2)` instead of mutating argv in-place, because `mpy_cli_main` reads argv from the beginning. This preserves the rest of the arguments unchanged.

- [ ] **Step 2: Build and verify `-X` is ignored**

Run:
```bash
cd ports/wasi && make
```

Then verify:
```bash
wasmtime run -W exceptions=y --dir /::/ build/micropython.wasm -X emit=bytecode -c "print('hello')"
```

Expected output: `hello`

Also verify unknown flags still error:
```bash
wasmtime run -W exceptions=y --dir /::/ build/micropython.wasm --unknown -c "print('hello')"
```

Expected: exit code 1, `python: unknown option '--unknown'`

- [ ] **Step 3: Commit**

```bash
git add ports/wasi/main.c
git commit -m "ports/wasi: Handle -X flag for run-tests.py compatibility.

The test runner passes -X emit=bytecode and -X heapsize=... on the
command line. Ignore these flags rather than treating them as errors."
```

---

### Task 2: Add MICROPYPATH env var reading to `ports/wasi/main.c`

Without this, `sys.path` only contains `['', '.frozen']`. The test runner sets `MICROPYPATH` to include `unittest` and `extmod` directories. The WASI port needs to read this env var and populate `sys.path`, identical to how the unix port does it.

**Files:**
- Modify: `ports/wasi/main.c:96-128` (the `mpy_cli_main` function, after VFS mount)

**Reference:** `ports/unix/main.c:524-564` contains the canonical MICROPYPATH parsing logic.

- [ ] **Step 1: Add MICROPYPATH reading after VFS mount in `mpy_cli_main()`**

Find the VFS mount block ending with `#endif` (around line 128). Insert the MICROPYPATH parsing code **after** the VFS mount, **before** `int ret = 0;`:

```c
    // Read MICROPYPATH environment variable to populate sys.path.
    // This is needed for run-tests.py which sets MICROPYPATH to include
    // extmod and unittest directories. Without this, sys.path only has
    // ["", ".frozen"] and import unittest fails.
    {
        char *path = getenv("MICROPYPATH");
        if (path != NULL) {
            // sys.path already has ["", ".frozen"] from py/runtime.c
            // Split MICROPYPATH by ':' and append each entry
            while (*path) {
                if (*path == ':') {
                    path++;
                    continue;
                }
                char *entry_end = strchr(path, ':');
                size_t entry_len;
                if (entry_end != NULL) {
                    entry_len = entry_end - path;
                } else {
                    entry_len = strlen(path);
                }
                if (entry_len > 0) {
                    mp_obj_list_append(
                        mp_sys_path,
                        mp_obj_new_str_via_qstr(path, entry_len)
                    );
                }
                path += entry_len;
                if (*path == ':') {
                    path++;
                }
            }
        }
    }
```

Notes:
- Uses `mp_obj_new_str_via_qstr` (same as unix port) to create string entries for sys.path
- Uses `mp_sys_path` directly — this is the `sys.path` list, already initialized by `mp_init()` with `['', '.frozen']`
- The `#include <stdlib.h>` already exists at the top of main.c, so `getenv()` is available
- Does NOT do `~` expansion (WASI has no HOME) — simplifies from unix port

- [ ] **Step 2: Build and verify MICROPYPATH is read**

Run:
```bash
cd ports/wasi && make
```

Verify MICROPYPATH changes sys.path:
```bash
wasmtime run -W exceptions=y --dir /::/ --env MICROPYPATH=/test_dir build/micropython.wasm -c "import sys; print(sys.path)"
```

Expected output contains `/test_dir`:
```
['', '.frozen', '/test_dir']
```

Verify without MICROPYPATH, behavior is unchanged:
```bash
wasmtime run -W exceptions=y --dir /::/ build/micropython.wasm -c "import sys; print(sys.path)"
```

Expected:
```
['', '.frozen']
```

- [ ] **Step 3: Commit**

```bash
git add ports/wasi/main.c
git commit -m "ports/wasi: Read MICROPYPATH env var to populate sys.path.

Needed for run-tests.py which sets MICROPYPATH to include extmod and
unittest directories. Without this, import unittest fails."
```

---

### Task 3: Add `WasiRuntimeRunner` class to `tests/run-tests.py`

This is the core change — a new runner class that invokes `wasmtime run` instead of running micropython directly. Modeled after `PyboardNodeRunner` (line 812-854).

**Files:**
- Modify: `tests/run-tests.py`

- [ ] **Step 1: Add `WasiRuntimeRunner` class after `PyboardNodeRunner`**

Insert after line 854 (after the `PyboardNodeRunner` class):

```python


class WasiRuntimeRunner:
    def __init__(self):
        wasm = os.getenv("MICROPY_MICROPYTHON_WASM")
        if wasm is None:
            wasm = base_path("../ports/wasi/build/micropython.wasm")
        else:
            wasm = os.path.abspath(wasm)
        self.micropython_wasm = wasm
        self.wasmtime = os.getenv("WASMTIME", "wasmtime")

    def close(self):
        pass

    def run_script_on_remote_target(self, args, test_file, is_special):
        cwd = os.path.dirname(test_file)
        micropypath = os.environ.get("MICROPYPATH", "")

        cmdlist = [
            self.wasmtime, "run",
            "-W", "exceptions=y",
            "--dir", "/::/",
        ]
        if micropypath:
            cmdlist.extend(["--env", "MICROPYPATH=" + micropypath])
        cmdlist.append(self.micropython_wasm)
        cmdlist.append(test_file)

        try:
            had_crash = False
            output_mupy = subprocess.check_output(
                cmdlist, stderr=subprocess.STDOUT, timeout=TEST_TIMEOUT, cwd=cwd
            )
        except subprocess.CalledProcessError as er:
            had_crash = True
            output_mupy = er.output + b"CRASH"
        except subprocess.TimeoutExpired as er:
            had_crash = True
            output_mupy = (er.output or b"") + b"TIMEOUT"

        return had_crash, output_mupy
```

Notes:
- `self.micropython_wasm`: path to wasm file, from `MICROPY_MICROPYTHON_WASM` env var or default
- `self.wasmtime`: path to wasmtime binary, from `WASMTIME` env var or default `wasmtime`
- `--dir /::/`: maps host root `/` to guest root `/`, so test files at absolute paths are accessible
- `--env MICROPYPATH=...`: passes the MICROPYPATH env var through to the WASI guest
- Uses `TEST_TIMEOUT` (the global 30-second timeout) consistent with unix/webassembly runners
- Uses string concatenation (`"MICROPYPATH=" + micropypath`) instead of f-string to be consistent with the rest of `run-tests.py` (Python 3.3+ compat)

- [ ] **Step 2: Register `wasi` in `get_test_instance()`**

In `get_test_instance()` (line 378), add a `wasi` branch after the `webassembly` branch:

Find (line 381-382):
```python
    elif test_instance == "webassembly":
        return PyboardNodeRunner()
```

Insert after it:
```python
    elif test_instance == "wasi":
        return WasiRuntimeRunner()
```

- [ ] **Step 3: Register `wasi` in `platform_to_port_map`**

Find (line 127):
```python
platform_to_port_map = {"pyboard": "stm32", "WiPy": "cc3200"}
```

Change to:
```python
platform_to_port_map = {"pyboard": "stm32", "WiPy": "cc3200", "wasi": "wasi"}
```

- [ ] **Step 4: Add `wasi` entry to `platform_tests_to_skip`**

Find the `"webassembly"` entry in `platform_tests_to_skip` (line 213-241). Insert a `wasi` entry **after** the webassembly tuple (after line 241's closing `),`):

```python
    "wasi": (
        # Populated after initial test run and analysis
    ),
```

This starts empty. Task 6 will populate it with actual test results.

- [ ] **Step 5: Add wasi to help text**

Find in `test_instance_epilog` (around line 1339-1341):
```
- webassembly - use the webassembly port of MicroPython, specified by the
  MICROPY_MICROPYTHON_MJS environment variable (which defaults to the standard
  variant of the webassembly port)
```

Insert after the webassembly lines:
```
- wasi - use the wasi port of MicroPython, specified by the
  MICROPY_MICROPYTHON_WASM environment variable (which defaults to the
  wasi port build), requires wasmtime to be installed
```

- [ ] **Step 6: Verify basic invocation works**

Run:
```bash
cd tests
MICROPY_MICROPYTHON_WASM=../ports/wasi/build/micropython.wasm \
  python3 run-tests.py -t wasi basics/assign1.py
```

Expected: single test passes (or shows the result without errors about unknown target).

- [ ] **Step 7: Commit**

```bash
git add tests/run-tests.py
git commit -m "tests: Add wasi target support to run-tests.py.

Add WasiRuntimeRunner class that invokes wasmtime to run tests against
the WASI port. Register 'wasi' as a native test target with platform
detection and skip list support."
```

---

### Task 4: Create convenience test runner script

A simple script so developers don't need to remember the full command.

**Files:**
- Create: `ports/wasi/run-tests.sh`

- [ ] **Step 1: Create the script**

Create `ports/wasi/run-tests.sh`:

```bash
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

cd "$SCRIPT_DIR/../../tests"
exec python3 run-tests.py -t wasi "$@"
```

- [ ] **Step 2: Make executable and test**

```bash
chmod +x ports/wasi/run-tests.sh
./ports/wasi/run-tests.sh basics/bool1.py
```

Expected: single test passes.

- [ ] **Step 3: Commit**

```bash
git add ports/wasi/run-tests.sh
git commit -m "ports/wasi: Add run-tests.sh convenience script."
```

---

### Task 5: Run basics test suite and collect results

Run the basics test suite (~552 tests) to identify which tests pass and which fail. This produces a baseline for the skip list.

**Files:** None (data collection only)

- [ ] **Step 1: Run basics tests**

```bash
cd tests
MICROPY_MICROPYTHON_WASM=../ports/wasi/build/micropython.wasm \
  python3 run-tests.py -t wasi -d basics -r results/wasi-basics 2>&1 | tee /tmp/wasi-basics-output.txt
```

This will take several minutes (~552 tests, each spawning a wasmtime process).

- [ ] **Step 2: Extract pass/fail counts**

```bash
grep -c "PASS\|FAIL\|SKIP" /tmp/wasi-basics-output.txt || true
tail -5 /tmp/wasi-basics-output.txt
```

Record the summary line (e.g., "530 tests, 480 passed, 30 failed, 20 skipped").

- [ ] **Step 3: Extract failed test names**

```bash
grep "FAIL:" /tmp/wasi-basics-output.txt | awk '{print $2}' | sort > /tmp/wasi-basics-fails.txt
cat /tmp/wasi-basics-fails.txt
```

This list will be used in Task 6 to populate the skip list or identify bugs to fix.

---

### Task 6: Populate the wasi skip list

Based on the results from Task 5, analyze each failure and add permanently-unsupported tests to `platform_tests_to_skip["wasi"]`.

**Files:**
- Modify: `tests/run-tests.py:241` (the `wasi` entry in `platform_tests_to_skip`)

- [ ] **Step 1: Categorize each failed test**

For each test in the failure list from Task 5, determine:

| Category | Action | Example |
|----------|--------|---------|
| WASI limitation (no threads, no REPL) | Add to skip list | `thread/` tests, REPL tests |
| Bug to fix in code | Fix in separate commit, don't skip | Wrong output, missing feature |
| Heap too small | Consider skipping or increasing heap | Tests allocating >256KB |
| Flaky (timing, network) | Add to skip list | `time_res.py` |

- [ ] **Step 2: Update `platform_tests_to_skip["wasi"]`**

Edit the wasi entry in `platform_tests_to_skip` with the actual skip list. Each entry should have a comment explaining why:

```python
    "wasi": (
        # Example (fill with actual results from Task 5):
        # "basics/some_test.py",  # reason
    ),
```

- [ ] **Step 3: Verify skip list works**

```bash
cd tests
MICROPY_MICROPYTHON_WASM=../ports/wasi/build/micropython.wasm \
  python3 run-tests.py -t wasi -d basics
```

Expected: all remaining tests should pass (no unexpected failures).

- [ ] **Step 4: Commit**

```bash
git add tests/run-tests.py
git commit -m "tests: Add wasi platform skip list for basics tests."
```

---

### Task 7: Run remaining test directories

Extend testing beyond basics to extmod, micropython, misc, float, and stress.

**Files:**
- Modify: `tests/run-tests.py` (update skip list as needed)

- [ ] **Step 1: Run extmod tests (~202 tests)**

```bash
cd tests
MICROPY_MICROPYTHON_WASM=../ports/wasi/build/micropython.wasm \
  python3 run-tests.py -t wasi -d extmod -r results/wasi-extmod 2>&1 | tee /tmp/wasi-extmod-output.txt
```

- [ ] **Step 2: Run micropython, misc, float, stress tests**

```bash
cd tests
for dir in micropython misc float stress; do
  MICROPY_MICROPYTHON_WASM=../ports/wasi/build/micropython.wasm \
    python3 run-tests.py -t wasi -d $dir -r results/wasi-$dir 2>&1 | tee /tmp/wasi-$dir-output.txt
done
```

- [ ] **Step 3: Update skip list with new failures**

Same process as Task 6 Step 1-2: categorize failures and update `platform_tests_to_skip["wasi"]`.

- [ ] **Step 4: Commit updated skip list**

```bash
git add tests/run-tests.py
git commit -m "tests: Expand wasi skip list with extmod and misc test results."
```

---

## Summary

| Task | What | Output |
|------|------|--------|
| 1 | `-X` flag handling in main.c | `wasmtime run ... micropython.wasm -X emit=bytecode -c "..."` works |
| 2 | MICROPYPATH reading in main.c | `sys.path` includes MICROPYPATH entries |
| 3 | WasiRuntimeRunner in run-tests.py | `run-tests.py -t wasi` works |
| 4 | Convenience script | `./run-tests.sh` one-liner |
| 5 | Run basics tests | Baseline pass/fail data |
| 6 | Populate skip list | Known failures documented and skipped |
| 7 | Run remaining tests | Full coverage baseline |
