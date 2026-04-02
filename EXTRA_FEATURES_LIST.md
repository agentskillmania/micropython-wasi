# MicroPython EXTRA_FEATURES 77项功能清单

**按复杂程度排序**：从最简单（纯宏开启）到最复杂（需外部库）

---

## 🔹 Level 1: 纯宏开启，零依赖，零风险 (28项)

**只需在 mpconfigport.h 加一行 `#define MACRO (1)`，不涉及任何文件操作，不出问题**

| 编号 | 宏名称 | 功能描述 | 类别 |
|------|--------|----------|------|
| 01 | MICROPY_PY_BUILTINS_FROZENSET | frozenset不可变集合 | 内置类型 |
| 02 | MICROPY_PY_BUILTINS_MEMORYVIEW | memoryview内存视图 | 内置类型 |
| 03 | MICROPY_PY_BUILTINS_SLICE_ATTRS | slice.start/stop/step属性 | 内置类型 |
| 04 | MICROPY_PY_BUILTINS_SLICE_INDICES | slice.indices()方法 | 内置类型 |
| 05 | MICROPY_PY_BUILTINS_ROUND_INT | round()整数舍入 | 内置类型 |
| 06 | MICROPY_PY_BUILTINS_NOTIMPLEMENTED | NotImplemented对象 | 内置类型 |
| 07 | MICROPY_PY_BUILTINS_POW3 | pow(x,y,mod)三参数 | 内置类型 |
| 08 | MICROPY_PY_ALL_SPECIAL_METHODS | 所有特殊方法(__add__等) | 内置类型 |
| 09 | MICROPY_PY_REVERSE_SPECIAL_METHODS | 反向特殊方法(__radd__等) | 内置类型 |
| 10 | MICROPY_PY_BUILTINS_STR_CENTER | str.center()方法 | 字符串方法 |
| 11 | MICROPY_PY_BUILTINS_STR_PARTITION | str.partition()/rpartition() | 字符串方法 |
| 12 | MICROPY_PY_BUILTINS_STR_SPLITLINES | str.splitlines() | 字符串方法 |
| 13 | MICROPY_PY_BUILTINS_BYTES_HEX | bytes.hex()/bytearray.hex() | 字节方法 |
| 14 | MICROPY_PY_BUILTINS_STR_UNICODE | 完整Unicode字符串方法 | 字符串方法 |
| 15 | MICROPY_PY_DESCRIPTORS | 描述符协议 | 语言特性 |
| 16 | MICROPY_PY_DELATTR_SETATTR | del/setattr支持 | 语言特性 |
| 17 | MICROPY_PY_FUNCTION_ATTRS | 函数__name__等属性 | 语言特性 |
| 18 | MICROPY_PY_CAN_OVERRIDE_BUILTINS | 覆盖内置函数 | 语言特性 |
| 19 | MICROPY_COMP_MODULE_CONST | 模块常量优化 | 编译器优化 |
| 20 | MICROPY_COMP_TRIPLE_TUPLE_ASSIGN | 三元组解包优化 | 编译器优化 |
| 21 | MICROPY_COMP_RETURN_IF_EXPR | return-if表达式优化 | 编译器优化 |
| 22 | MICROPY_OPT_LOAD_ATTR_FAST_PATH | 属性访问快速路径 | 编译器优化 |
| 23 | MICROPY_OPT_MAP_LOOKUP_CACHE | 字典查找缓存 | 编译器优化 |
| 24 | MICROPY_OPT_MPZ_BITWISE | 大整数位运算优化 | 编译器优化 |
| 25 | MICROPY_QSTR_BYTES_IN_HASH=2 | qstr哈希2字节 | 运行时优化 |
| 26 | MICROPY_MODULE_BUILTIN_INIT | 模块__init__支持 | 模块系统 |
| 27 | MICROPY_PY_MICROPYTHON_MEM_INFO | micropython.mem_info() | 调试工具 |
| 28 | MICROPY_PY_MICROPYTHON_RINGIO | micropython.ringio() | 调试工具 |

---

## 🔹 Level 2: 基础设施功能，低风险 (18项)

**需要基础REPL或sys支持，WASI已有这些基础设施**

| 编号 | 宏名称 | 功能描述 | 类别 |
|------|--------|----------|------|
| 29 | MICROPY_REPL_EMACS_KEYS | Emacs快捷键(Ctrl+A/E等) | REPL |
| 30 | MICROPY_REPL_AUTO_INDENT | 自动缩进 | REPL |
| 31 | MICROPY_PY_BUILTINS_INPUT | input()函数 | REPL |
| 32 | MICROPY_PY_BUILTINS_HELP | help()函数 | REPL |
| 33 | MICROPY_PY_BUILTINS_HELP_MODULES | help('modules') | REPL |
| 34 | MICROPY_PY_SYS_PS1_PS2 | sys.ps1/ps2提示符 | sys模块 |
| 35 | MICROPY_PY_SYS_STDFILES | sys.stdin/stdout/stderr文件接口 | sys模块 |
| 36 | MICROPY_PY_SYS_STDIO_BUFFER | sys.stdin.buffer二进制缓冲 | sys模块 |
| 37 | MICROPY_PY_SYS_MAXSIZE | sys.maxsize | sys模块 |
| 38 | MICROPY_ENABLE_SCHEDULER | 调度器支持 | 运行时 |
| 39 | MICROPY_STACK_CHECK | 栈溢出检查 | 运行时 |
| 40 | MICROPY_ENABLE_SOURCE_LINE | 异常源代码行号 | 调试 |
| 41 | MICROPY_STREAMS_NON_BLOCK | 非阻塞流 | IO优化 |
| 42 | MICROPY_PY_BUILTINS_COMPILE | compile()函数 | 动态编译 |
| 43 | MICROPY_PY_BUILTINS_EXECFILE | execfile()支持 | 动态执行 |
| 44 | MICROPY_MODULE_ATTR_DELEGATION | 模块属性委托 | 模块系统 |
| 45 | MICROPY_PY_ARRAY_SLICE_ASSIGN | array切片赋值 | array模块 |
| 46 | MICROPY_PY_MATH_CONSTANTS | math.pi/e/tau常量 | math模块 |

---

## 🔹 Level 3: collections和math增强，低风险 (10项)

**需要相应模块支持，但代码已在内核中**

| 编号 | 宏名称 | 功能描述 | 类别 |
|------|--------|----------|------|
| 47 | MICROPY_PY_COLLECTIONS_DEQUE | collections.deque双端队列 | collections |
| 48 | MICROPY_PY_COLLECTIONS_DEQUE_ITER | deque迭代器 | collections |
| 49 | MICROPY_PY_COLLECTIONS_DEQUE_SUBSCR | deque下标访问 | collections |
| 50 | MICROPY_PY_COLLECTIONS_ORDEREDDICT | OrderedDict有序字典 | collections |
| 51 | MICROPY_PY_MATH_FACTORIAL | math.factorial() | math |
| 52 | MICROPY_PY_MATH_ISCLOSE | math.isclose() | math |
| 53 | MICROPY_PY_MATH_SPECIAL_FUNCTIONS | gamma/erf/lgamma等特殊函数 | math |
| 54 | MICROPY_OPT_MATH_FACTORIAL | factorial优化 | math |
| 55 | MICROPY_PY_CMATH | cmath复数数学 | math |

---

## 🔹 Level 4: extmod外部模块，中等风险 (14项)

**需要链接 extmod/ 目录下的独立.o文件，可能涉及文件读取**

| 编号 | 宏名称 | 功能描述 | 依赖 |
|------|--------|----------|------|
| 56 | MICROPY_PY_JSON | json模块 | 无 |
| 57 | MICROPY_PY_BINASCII | binascii(hex/base64) | 无 |
| 58 | MICROPY_PY_BINASCII_CRC32 | binascii.crc32() | 无 |
| 59 | MICROPY_PY_UCTYPES | ctypes结构体 | 无 |
| 60 | MICROPY_PY_PLATFORM | platform模块 | 无 |
| 61 | MICROPY_PY_HEAPQ | heapq堆队列 | 无 |
| 62 | MICROPY_PY_RANDOM | random随机数 | 无 |
| 63 | MICROPY_PY_RANDOM_EXTRA_FUNCS | random额外函数 | 无 |
| 64 | MICROPY_PY_RE | re正则表达式 | 可能需要re库 |
| 65 | MICROPY_PY_RE_SUB | re.sub()替换 | re依赖 |
| 66 | MICROPY_PY_HASHLIB | hashlib(md5/sha) | 可能需要crypto库 |
| 67 | MICROPY_PY_DEFLATE | deflate/zlib压缩 | 可能需要zlib |
| 68 | MICROPY_PY_FRAMEBUF | framebuf帧缓冲 | 无 |

---

## 🔹 Level 5: 可能有平台依赖，需谨慎 (5项)

**涉及信号处理或外部库，WASI可能不支持或需要特殊处理**

| 编号 | 宏名称 | 功能描述 | 风险说明 |
|------|--------|----------|----------|
| 69 | MICROPY_KBD_EXCEPTION | Ctrl+C键盘中断 | WASI可能不支持信号 |
| 70 | MICROPY_PY_SSL_DTLS | SSL/TLS DTLS | 需要mbedtls库 |

---

## 🔹 已手工开启（无需添加）

| 宏名称 | 功能描述 |
|--------|----------|
| MICROPY_PY_SOCKET | socket支持 |
| MICROPY_PY_SELECT | select/poll |
| MICROPY_PY_ASYNCIO | asyncio |
| MICROPY_PY_ERRNO | errno模块 |
| MICROPY_PY_OS | os模块 |
| MICROPY_PY_MATH | math模块 |
| MICROPY_PY_BUILTINS_FLOAT | float支持 |
| MICROPY_PY_FSTRINGS | f-string |
| MICROPY_VFS | 虚拟文件系统 |
| MICROPY_VFS_POSIX | POSIX文件系统 |
| MICROPY_HELPER_REPL | REPL |
| MICROPY_ENABLE_FINALISER | GC终结器 |

---

## 推荐添加顺序

### 阶段1：热身（Level 1，绝对安全）
```
07-frozenset → 08-memoryview → 01-SLICE_ATTRS → 02-SLICE_INDICES
```

### 阶段2：基础增强（Level 1剩余 + Level 2）
```
字符串方法(10-14) → 编译器优化(19-24) → INPUT/HELP(31-33) → math.pi/e(46)
```

### 阶段3：实用数据结构（Level 3）
```
deque(47-49) → OrderedDict(50) → math增强(51-55)
```

### 阶段4：常用模块（Level 4）
```
json(56) → binascii(57-58) → random(62-63) → platform(60)
```

### 阶段5：高级功能（Level 4剩余 + Level 5）
```
re(64-65) → hashlib(66) → deflate(67) → Ctrl+C(69)
```

---

**当前状态：** 已开14项，待开约63项  
**建议：** 从 Level 1 开始，逐个测试
