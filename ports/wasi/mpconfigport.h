/**
 * MicroPython WASI Preview2 配置文件
 * 
 * 本文件定义 WASI Preview2 平台的编译配置
 * 启用以下功能：
 * - Socket 支持（MICROPY_PY_SOCKET）
 * - Asyncio 支持（MICROPY_PY_ASYNCIO）
 * - Select/poll 支持（MICROPY_PY_SELECT）
 * - VFS POSIX 文件系统（MICROPY_VFS_POSIX）
 * 
 * 编译目标：wasm32-wasip2
 * 许可证：MIT
 */

#include <stdint.h>
#include <stdlib.h>

// Enable POSIX features for nanosleep, clock_gettime
#define _POSIX_C_SOURCE 199309L

// Configuration for WASI preview2 port
#include <alloca.h>

// Use basic features level
#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_BASIC_FEATURES)

// Long integer implementation (needed for frozen modules)
#define MICROPY_LONGINT_IMPL        (MICROPY_LONGINT_IMPL_MPZ)

#define MICROPY_ENABLE_COMPILER     (1)
#define MICROPY_ENABLE_GC           (1)
#define MICROPY_HELPER_REPL         (1)
#define MICROPY_REPL_EVENT_DRIVEN   (0)
#define MICROPY_ENABLE_EXTERNAL_IMPORT (1)
#define MICROPY_ALLOC_PATH_MAX      (256)
#define MICROPY_ALLOC_PARSE_CHUNK_INIT    (16)

#define MICROPY_PY_SYS_PLATFORM     "wasi"

// Enable basic sys features
#define MICROPY_PY_SYS_MODULES      (1)
#define MICROPY_PY_SYS_EXIT         (1)
#define MICROPY_PY_SYS_PATH         (1)
#define MICROPY_PY_SYS_ARGV         (1)

// Enable modern Python features (available in BASIC level)
#define MICROPY_PY_ASYNC_AWAIT      (1)  // Python 3.5+

// Enable file system (VFS + POSIX backend for WASI)
#define MICROPY_ENABLE_FINALISER    (1)
#define MICROPY_VFS                 (1)
#define MICROPY_PY_VFS              (1)
#define MICROPY_VFS_POSIX           (1)
#define MICROPY_READER_VFS          (1)
#define MICROPY_PY_IO               (1)
#define MICROPY_PY_BUILTINS_OPEN    (1)
#define MICROPY_PY_OS               (1)
#define MICROPY_PY_UOS              (1)
#define MICROPY_PY_UOS_VFS          (1)

// Enable float support
#define MICROPY_FLOAT_IMPL          (MICROPY_FLOAT_IMPL_DOUBLE)
#define MICROPY_PY_MATH             (1)
#define MICROPY_PY_BUILTINS_FLOAT   (1)

// Enable f-strings (requires explicit enable even with BASIC level)
#define MICROPY_PY_FSTRINGS         (1)

// Enable socket support (WASI Preview2 has POSIX sockets)
#define MICROPY_PY_SOCKET           (1)
#define MICROPY_PY_SOCKET_LISTEN_BACKLOG_DEFAULT (128)

// Enable select module for asyncio support
#define MICROPY_PY_SELECT           (1)
#define MICROPY_PY_SELECT_SELECT    (0)  // Use poll instead of select
#define MICROPY_PY_SELECT_POSIX_OPTIMISATIONS (0)  // WASI poll constants don't match

// Enable errno module
#define MICROPY_PY_ERRNO            (1)

// Enable asyncio (required for async/await with sockets)
#define MICROPY_PY_ASYNCIO          (1)

// Time ticks period for asyncio
#define MICROPY_PY_TIME_TICKS_PERIOD (65536)

// Use standard C printf/stdout
#define MICROPY_USE_INTERNAL_PRINTF (0)

// Type definitions
#define MP_SSIZE_MAX (0x7fffffff)
typedef long mp_off_t;

#define MICROPY_HW_BOARD_NAME "WASI"
#define MICROPY_HW_MCU_NAME "wasm32"

#define MP_STATE_PORT MP_STATE_VM

// Stack/gc configuration
#define MICROPY_HEAP_SIZE (256 * 1024)
