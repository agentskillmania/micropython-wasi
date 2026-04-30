# WASI port manifest - includes asyncio for async/await support
include("$(MPY_DIR)/extmod/asyncio")

# Include ssl wrapper (maps ssl -> tls module)
module("ssl.py", base_path="$(MPY_DIR)/lib/micropython-lib/python-stdlib/ssl", opt=3)
