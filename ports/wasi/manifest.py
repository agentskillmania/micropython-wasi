# WASI port manifest - frozen modules

# asyncio: C辅助 + Python实现
include("$(MPY_DIR)/extmod/asyncio")

# SSL兼容层: import ssl → 映射到内置tls模块
module("ssl.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/ssl", opt=3)

# HTTP客户端 (package "requests")
include("$(MPY_DIR)/lib/micropython-lib/python-ecosys/requests")

# 实用工具
module("shutil.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/shutil", opt=3)
module("tempfile.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/tempfile", opt=3)
module("pathlib.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/pathlib", opt=3)
module("gzip.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/gzip", opt=3)

# ============================================================
# stdlib modules for AI agent sandbox (MICROPYPATH unavailable)
# ============================================================

# --- Tier 1: agent high-frequency imports ---

module("datetime.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/datetime", opt=3)
module("itertools.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/itertools", opt=3)
module("functools.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/functools", opt=3)
module("types.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/types", opt=3)
module("copy.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/copy", opt=3)
package("string", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/string", opt=3)
module("base64.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/base64", opt=3)
module("defaultdict.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/collections-defaultdict/collections", opt=3)

# --- Tier 2: commonly used ---

module("ucontextlib.py", base_path="$(MPY_DIR)/lib/micropython-lib/micropython/ucontextlib", opt=3)
module("contextlib.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/contextlib", opt=3)
module("logging.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/logging", opt=3)
module("traceback.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/traceback", opt=3)
package("unittest", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/unittest", opt=3)
module("pprint.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/pprint", opt=3)
module("pickle.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/pickle", opt=3)
module("stat.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/stat", opt=3)
module("operator.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/operator", opt=3)
module("hmac.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/hmac", opt=3)
module("zlib.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/zlib", opt=3)
module("warnings.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/warnings", opt=3)
module("abc.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/abc", opt=3)
module("bisect.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/bisect", opt=3)
module("fnmatch.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/fnmatch", opt=3)
