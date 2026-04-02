# MicroPython WASI Preview2 Port

将 MicroPython 编译为 WASI Preview2 组件，支持直接执行 Python 代码和文件系统操作。

## Python 语法版本

**支持 Python 3.5+ 的大部分语法：**

| 特性 | Python 版本 | 支持状态 |
|-----|------------|---------|
| `yield from` | 3.3 | ✅ |
| `async` / `await` | 3.5 | ✅ |
| f-strings (`f"hello {name}"`) | 3.6 | ✅ |
| `print()` 函数 | 3.0 | ✅ |
| 类型注解 (基础) | 3.5+ | ✅ |
| 海象运算符 `:=` | 3.8 | ❌ |
| match/case | 3.10 | ❌ |

**注意：** 虽然 `sys.version` 显示 "3.4.0"，但实际支持的语法已扩展到 3.6+（f-strings）。

## 功能特性

### ✅ 核心 Python 功能
- 数据类型：int, float, str, bool, bytes, list, dict, set, tuple
- 控制流：if/elif/else, for, while, try/except/finally
- 函数、类、Lambda、列表推导式、生成器
- f-strings 字符串格式化
- async/await 协程
- 垃圾回收

### ✅ 标准库模块
```python
import sys      # platform, version, path, modules, argv, exit
import math     # sqrt, sin, cos, tan, log, exp, pi, e
import gc       # mem_free, mem_alloc, collect
import os       # 文件系统操作
import io       # 文件 I/O
import socket   # TCP/UDP sockets, getaddrinfo
```

### ✅ 文件系统（WASI）
通过 `wasmtime --dir` 挂载宿主目录：

```bash
# 挂载单个目录
wasmtime run -W exceptions=y --dir /host/path:/guest/path micropython.wasm

# 挂载多个目录
wasmtime run -W exceptions=y \
  --dir /home/user/data:/data \
  --dir /tmp:/tmp \
  micropython.wasm
```

Python 中使用：
```python
# 读写文件
with open('/data/file.txt', 'w') as f:
    f.write('Hello!')

with open('/data/file.txt', 'r') as f:
    content = f.read()

# 目录操作
import os
os.listdir('/data')
os.mkdir('/data/newdir')
os.remove('/data/file.txt')
```

## 依赖

- [WASI SDK 22+](https://github.com/WebAssembly/wasi-sdk) (LLVM 22+)
- [wasmtime](https://wasmtime.dev/)

## 构建

```bash
cd ports/wasi
make
```

输出：`build/micropython.wasm`（约 925KB，包含 socket 支持，完全自包含）

## 运行示例

### 基础用法
```bash
# 执行代码
wasmtime run -W exceptions=y build/micropython.wasm "print('Hello World')"

# f-strings
wasmtime run -W exceptions=y build/micropython.wasm "
name = 'WASI'
version = 3.6
print(f'Running on {name}, Python {version}+ syntax')
"

# async/await
wasmtime run -W exceptions=y build/micropython.wasm "
async def greet():
    return 'Hello from async!'
print(await greet())
"
```

### 文件操作
```bash
# 挂载当前目录到 /work
wasmtime run -W exceptions=y --dir .:/work build/micropython.wasm "
import os

# 创建文件
with open('/work/output.txt', 'w') as f:
    f.write('Line 1\n')
    f.write('Line 2\n')

# 读取文件
with open('/work/output.txt', 'r') as f:
    print(f.read())

# 目录操作
os.mkdir('/work/subdir')
print('Files:', os.listdir('/work'))
"
```

### 数据处理
```bash
wasmtime run -W exceptions=y --dir .:/data build/micropython.wasm "
import math

# 读取数据文件
with open('/data/numbers.txt', 'r') as f:
    lines = f.readlines()

# 处理
numbers = [float(line.strip()) for line in lines if line.strip()]
avg = sum(numbers) / len(numbers)

# 输出结果
with open('/data/result.txt', 'w') as f:
    f.write(f'Count: {len(numbers)}\n')
    f.write(f'Average: {avg:.2f}\n')
    f.write(f'Sum: {sum(numbers)}\n')

print('Processing complete!')
"
```

## 技术细节

### 关于 VFS（虚拟文件系统）

你可能会问：为什么要有 VFS？不能直接用 libc 的 `fopen` 吗？

**答案：** WASI 确实支持 libc → 宿主文件系统映射，但 MicroPython **强制**使用 VFS 架构：

```
Python open() → MicroPython VFS → VFS-POSIX后端 → libc open() → WASI → 宿主文件系统
```

MicroPython 使用 VFS 的原因：
1. 嵌入式设备需要支持多种存储（SD卡、Flash、POSIX等）
2. 统一的文件操作接口，无需为每种存储重写代码
3. 支持挂载点、路径解析等高级功能

在我们的 WASI 场景中，`MICROPY_VFS_POSIX` 就是 VFS 的 POSIX 后端，内部实际调用 libc。

### WASI Preview2 组件
- 导出 `wasi:cli/run@0.2.0` 接口
- 导入 WASI 接口：filesystem, stdin/stdout/stderr, clocks, environment
- 文件大小：约 925KB（含 socket 支持）
- 堆内存：256KB

### 关键技术参数
```bash
# 编译器
CC = wasm32-wasip2-clang
CFLAGS = -mllvm -wasm-enable-sjlj -mllvm -wasm-use-legacy-eh=0
LDFLAGS = -mexec-model=command -lsetjmp

# 运行
wasmtime run -W exceptions=y --dir host:guest micropython.wasm
```

### ✅ 网络 Socket（WASI Preview2）
```bash
# 运行时需要启用网络权限
wasmtime run -W exceptions=y -S tcp=y -S udp=y -S inherit-network=y \
  -S allow-ip-name-lookup=y micropython.wasm
```

Python 中使用：
```python
import socket

# 创建 TCP socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# 解析域名
addr = socket.getaddrinfo('example.com', 80)[0]

# 连接服务器
s.connect(addr[4])
s.send(b'GET / HTTP/1.0\r\nHost: example.com\r\n\r\n')

# 接收响应
data = s.recv(1024)
print(data.decode())

s.close()
```

支持的 socket 功能：
- ✅ TCP/UDP sockets (`socket.AF_INET`, `socket.SOCK_STREAM`, `socket.SOCK_DGRAM`)
- ✅ 域名解析 (`socket.getaddrinfo`)
- ✅ 连接、发送、接收 (`connect`, `send`, `recv`)
- ✅ 服务器端 (`bind`, `listen`, `accept`)
- ✅ 选项设置 (`setsockopt`, `settimeout`, `setblocking`)
- ✅ 地址转换 (`inet_pton`, `inet_ntop`)

## 限制

- 不支持 `:=` 海象运算符（3.8+）
- 不支持 `match/case` 模式匹配（3.10+）
- 多线程不支持

## 部署

只需要拷贝 **`micropython.wasm`** 这一个文件（约 925KB）：

```bash
# 复制到任意位置
cp build/micropython.wasm /usr/local/bin/

# 运行
wasmtime run -W exceptions=y /usr/local/bin/micropython.wasm "print('Hello')"
```

## 参考

- [MicroPython](https://micropython.org/)
- [WebAssembly Component Model](https://component-model.bytecodealliance.org/)
- [WASI Preview2](https://github.com/WebAssembly/WASI/tree/main/preview2)
