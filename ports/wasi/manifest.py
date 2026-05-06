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
