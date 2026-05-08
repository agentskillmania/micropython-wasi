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
#define MICROPY_HELPER_REPL         (0)  // 关闭REPL，沙箱环境不需要交互式界面
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

// ==== Level 1 EXTRA Features: 纯宏开启，零依赖 (13项) ====
// 内置类型增强
#define MICROPY_PY_BUILTINS_FROZENSET       (1)  // 01: frozenset不可变集合
#define MICROPY_PY_BUILTINS_MEMORYVIEW      (1)  // 02: memoryview内存视图

// slice需要先开基础支持，再开额外功能
#define MICROPY_PY_BUILTINS_SLICE           (1)  // 基础slice类型（前置依赖）
#define MICROPY_PY_BUILTINS_SLICE_ATTRS     (1)  // 03: slice.start/stop/step属性
#define MICROPY_PY_BUILTINS_SLICE_INDICES   (1)  // 04: slice.indices()方法
#define MICROPY_PY_BUILTINS_ROUND_INT       (1)  // 05: round()整数舍入
#define MICROPY_PY_BUILTINS_NOTIMPLEMENTED  (1)  // 06: NotImplemented对象
#define MICROPY_PY_BUILTINS_POW3            (1)  // 07: pow(x,y,mod)三参数
#define MICROPY_PY_ALL_SPECIAL_METHODS      (1)  // 08: 所有特殊方法(__add__等)
#define MICROPY_PY_REVERSE_SPECIAL_METHODS  (1)  // 09: 反向特殊方法(__radd__等)

// 字符串方法增强
#define MICROPY_PY_BUILTINS_STR_CENTER      (1)  // 10: str.center()方法
#define MICROPY_PY_BUILTINS_STR_PARTITION   (1)  // 11: str.partition()/rpartition()
#define MICROPY_PY_BUILTINS_STR_SPLITLINES  (1)  // 12: str.splitlines()
// 注意: MICROPY_PY_BUILTINS_STR_UNICODE 不控制isascii()，isascii()在MicroPython中未实现

// 字节方法增强
#define MICROPY_PY_BUILTINS_BYTES_HEX       (1)  // 13: bytes.hex()/bytearray.hex()

// ==== Level 1 续：语言特性与优化 (逐个添加验证) ====
#define MICROPY_PY_DESCRIPTORS              (1)  // 15: 描述符协议
#define MICROPY_PY_DELATTR_SETATTR          (1)  // 16: del/setattr支持
#define MICROPY_PY_FUNCTION_ATTRS           (1)  // 17: 函数__name__等属性
#define MICROPY_PY_CAN_OVERRIDE_BUILTINS    (1)  // 18: 覆盖内置函数

// ==== 编译器优化 (6项) ====
#define MICROPY_COMP_MODULE_CONST           (1)  // 19: 模块常量优化
#define MICROPY_COMP_TRIPLE_TUPLE_ASSIGN    (1)  // 20: 三元组解包优化
#define MICROPY_COMP_RETURN_IF_EXPR         (1)  // 21: return-if表达式优化
#define MICROPY_OPT_LOAD_ATTR_FAST_PATH     (1)  // 22: 属性访问快速路径
#define MICROPY_OPT_MAP_LOOKUP_CACHE        (1)  // 23: 字典查找缓存
#define MICROPY_OPT_MPZ_BITWISE             (1)  // 24: 大整数位运算优化

// ==== 运行时优化 ====
#undef MICROPY_QSTR_BYTES_IN_HASH
#define MICROPY_QSTR_BYTES_IN_HASH          (2)  // 25: qstr哈希2字节（默认1字节）
#define MICROPY_MODULE_BUILTIN_INIT         (1)  // 26: 模块__init__支持

// ==== 调试工具 ====
#define MICROPY_PY_MICROPYTHON_MEM_INFO     (1)  // 27: micropython.mem_info()
#define MICROPY_PY_MICROPYTHON_RINGIO       (1)  // 28: micropython.ringio()

// ==== Level 2: 非REPL功能 (逐个添加) ====
// 跳过的REPL功能: 29-34 (EMACS_KEYS, AUTO_INDENT, INPUT, HELP, HELP_MODULES, PS1_PS2)
#define MICROPY_PY_SYS_STDFILES          (1)  // 35: sys.stdin/stdout/stderr as file objects
// 跳过 36: SYS_STDIO_BUFFER (暂不需要)
#define MICROPY_PY_SYS_MAXSIZE              (1)  // 37: sys.maxsize
#define MICROPY_ENABLE_SCHEDULER            (1)  // 38: 调度器支持
// 跳过 39: STACK_CHECK (WASI环境下卡死)
#define MICROPY_ENABLE_SOURCE_LINE          (1)  // 40: 异常源代码行号
#define MICROPY_STREAMS_NON_BLOCK           (1)  // 41: 非阻塞流
#define MICROPY_PY_BUILTINS_COMPILE         (1)  // 42: compile()函数
#define MICROPY_PY_BUILTINS_EXECFILE        (1)  // 43: execfile()支持 (Python3已移除)
#define MICROPY_MODULE_ATTR_DELEGATION      (1)  // 44: 模块属性委托
#define MICROPY_PY_ARRAY_SLICE_ASSIGN       (1)  // 45: array切片赋值
#define MICROPY_PY_MATH_CONSTANTS           (1)  // 46: math.pi/e/tau常量

// ==== Level 3: collections和math增强 (逐个添加验证) ====
#define MICROPY_PY_COLLECTIONS_DEQUE        (1)  // 47: collections.deque双端队列
#define MICROPY_PY_COLLECTIONS_DEQUE_ITER   (1)  // 48: deque迭代器
#define MICROPY_PY_COLLECTIONS_DEQUE_SUBSCR (1)  // 49: deque下标访问
#define MICROPY_PY_COLLECTIONS_ORDEREDDICT  (1)  // 50: OrderedDict有序字典

// ==== math增强 ====
#define MICROPY_PY_MATH_FACTORIAL           (1)  // 51: math.factorial()
#define MICROPY_PY_MATH_ISCLOSE             (1)  // 52: math.isclose()
#define MICROPY_PY_MATH_SPECIAL_FUNCTIONS   (1)  // 53: gamma/erf/lgamma等特殊函数
#define MICROPY_OPT_MATH_FACTORIAL          (1)  // 54: factorial优化
#define MICROPY_PY_CMATH                    (1)  // 55: cmath复数数学

// ==== Level 4: extmod外部模块 (逐个添加验证) ====
#define MICROPY_PY_JSON                     (1)  // 56: json模块
#define MICROPY_PY_BINASCII                 (1)  // 57: binascii(hex/base64)
#define MICROPY_PY_BINASCII_CRC32           (1)  // 58: binascii.crc32() (现在deflate已添加)
#define MICROPY_PY_HEAPQ                    (1)  // 61: heapq堆队列
#define MICROPY_PY_RANDOM                   (1)  // 62: random随机数
#define MICROPY_PY_RANDOM_EXTRA_FUNCS       (1)  // 63: random额外函数(random/randint/choice/shuffle)

// ==== re 正则表达式 ====
#define MICROPY_PY_RE                       (1)  // 64: re正则表达式
#define MICROPY_PY_RE_SUB                   (1)  // 65: re.sub()替换
#define MICROPY_PY_HASHLIB                  (1)  // 66: hashlib(md5/sha)
#define MICROPY_PY_DEFLATE                  (1)  // 67: deflate/zlib压缩
#define MICROPY_PY_UCTYPES                  (1)  // 59: ctypes结构体
#define MICROPY_PY_PLATFORM                 (1)  // 60: platform模块

// Enable socket support (WASI Preview2 has POSIX sockets)
#define MICROPY_PY_SOCKET           (1)
#define MICROPY_PY_SOCKET_LISTEN_BACKLOG_DEFAULT (128)

// Enable select module for asyncio support
#define MICROPY_PY_SELECT           (1)
#define MICROPY_PY_SELECT_SELECT    (0)  // Use poll instead of select
#define MICROPY_PY_SELECT_POSIX_OPTIMISATIONS (0)  // WASI poll constants don't match

// Enable errno module
#define MICROPY_PY_ERRNO            (1)

// Enable time functions (WASI libc supports these)
#define MICROPY_PY_TIME_GMTIME_LOCALTIME_MKTIME (1)
#define MICROPY_PY_TIME_TIME_TIME_NS (1)

// Enable sys features
#define MICROPY_PY_SYS_EXC_INFO     (1)
#define MICROPY_PY_SYS_ATEXIT       (1)

// Note: MICROPY_PY_BUILTINS_HELP and INPUT are disabled because they
// require readline which is not available in WASI Preview2

// Enable asyncio (required for async/await with sockets)
#define MICROPY_PY_ASYNCIO          (1)

// Time ticks period for asyncio
#define MICROPY_PY_TIME_TICKS_PERIOD (65536)

// Note: MICROPY_PY_SSL and MICROPY_SSL_MBEDTLS are defined in Makefile
// (via CFLAGS_EXTMOD from extmod.mk) to avoid macro redefinition warnings

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
