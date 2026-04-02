# MicroPython WASI Preview2 Makefile
#
# 本 Makefile 配置 WASI Preview2 平台的构建
#
# 关键配置：
# - 使用 wasm32-wasip2-clang 编译器
# - 启用异常处理（-wasm-enable-sjlj）
# - 包含 extmod.mk 以获取 asyncio、select 等模块
#
# 环境变量：
# - WASI_SDK_PATH：WASI SDK 安装路径（默认 ~/wasi-sdk）
#
# 许可证：MIT

include ../../py/mkenv.mk

# WASI SDK path
WASI_SDK_PATH ?= $(HOME)/wasi-sdk

CC = $(WASI_SDK_PATH)/bin/wasm32-wasip2-clang
CXX = $(WASI_SDK_PATH)/bin/wasm32-wasip2-clang++
LD = $(WASI_SDK_PATH)/bin/wasm32-wasip2-clang
AR = $(WASI_SDK_PATH)/bin/llvm-ar
SIZE = true

# qstr definitions (must come before including py.mk)
QSTR_DEFS = qstrdefsport.h

# Include manifest for frozen modules (asyncio)
FROZEN_MANIFEST ?= $(PWD)/manifest.py

# Set a dummy micropython-lib dir to skip submodule check (asyncio is in extmod)
MPY_LIB_DIR = $(TOP)/lib

MICROPY_ROM_TEXT_COMPRESSION ?= 1

# include py core make definitions
include $(TOP)/py/py.mk
include $(TOP)/extmod/extmod.mk

INC += -I.
INC += -I$(TOP)
INC += -I$(BUILD)

CFLAGS += $(INC) -Wall -Werror -Wdouble-promotion -Wfloat-conversion -std=c99
CFLAGS += -Os -DNDEBUG
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -mllvm -wasm-enable-sjlj -mllvm -wasm-use-legacy-eh=0

LDFLAGS += -Wl,--gc-sections
LDFLAGS += -mexec-model=command
LDFLAGS += -lsetjmp
LDFLAGS += -Wl,--allow-undefined

LIBS = -lm

SRC_C = \
	main.c \
	mphalport.c \
	modsocket.c \
	shared/runtime/pyexec.c \
	shared/runtime/stdout_helpers.c \
	shared/readline/readline.c \

SRC_QSTR += $(SRC_C) $(SHARED_SRC_C)

OBJ += $(PY_CORE_O)
OBJ += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_EXTMOD_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_THIRDPARTY_C:.c=.o))

# Include frozen content if manifest is provided
ifneq ($(FROZEN_MANIFEST),)
OBJ += $(BUILD)/frozen_content.o
endif

all: $(BUILD)/micropython.wasm

$(BUILD)/micropython.wasm: $(OBJ)
	$(ECHO) "LINK $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(Q)$(SIZE) $@

run: $(BUILD)/micropython.wasm
	wasmtime run $< --mapdir /::$(PWD)

test: $(BUILD)/micropython.wasm
	/usr/bin/printf "print('hello world!', list(x+1 for x in range(10)), end='eol\\n')\r\n\004" | wasmtime run $< --mapdir /::$(PWD) | tail -n2 | grep "^hello world! \[1, 2, 3, 4, 5, 6, 7, 8, 9, 10\]eol"

include $(TOP)/py/mkrules.mk
