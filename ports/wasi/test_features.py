#!/usr/bin/env python3
"""
测试 MicroPython WASI port 支持的功能
"""

import subprocess
import sys

def run_micropython(code):
    """运行 MicroPython 代码并返回输出"""
    # 创建测试用的 main.c
    test_c = f'''
#include <string.h>
#include "py/builtin.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "shared/runtime/pyexec.h"

static char heap[256 * 1024];
static char *stack_top;

int main(int argc, char **argv) {{
    int stack_dummy;
    stack_top = (char *)&stack_dummy;
    gc_init(heap, heap + sizeof(heap));
    mp_init();
    
    const char *code = {repr(code)};
    mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, code, strlen(code), 0);
    if (lex) {{
        mp_parse_tree_t pt = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t fun = mp_compile(&pt, lex->source_name, false);
        if (fun != MP_OBJ_NULL) {{
            mp_call_function_0(fun);
        }}
    }}
    
    mp_deinit();
    return 0;
}}

void gc_collect(void) {{}}
void nlr_jump_fail(void *val) {{ while(1); }}
mp_lexer_t *mp_lexer_new_from_file(qstr f) {{ return NULL; }}
int mp_import_stat(const char *p) {{ return 0; }}
void MP_NORETURN __fatal_error(const char *m) {{ while(1); }}
'''
    return "需要编译"

# 当前配置分析
print("=" * 60)
print("MicroPython WASI Preview2 Port - 功能分析")
print("=" * 60)

print("\n### 当前配置 (MICROPY_CONFIG_ROM_LEVEL_MINIMUM) ###\n")

features = {
    "核心功能": {
        "Python 编译器": "✅ 启用 (MICROPY_ENABLE_COMPILER=1)",
        "垃圾回收": "✅ 启用 (MICROPY_ENABLE_GC=1)",
        "REPL 交互": "✅ 启用 (MICROPY_HELPER_REPL=1)",
        "异常处理": "✅ 启用 (通过 setjmp/longjmp)",
        "堆大小": "256 KB",
    },
    "sys 模块": {
        "sys.platform": "'wasi'",
        "sys.exit()": "✅ 启用",
        "sys.modules": "✅ 启用",
        "sys.path": "✅ 启用",
        "sys.argv": "✅ 启用",
    },
    "最小配置默认禁用的功能": {
        "文件 I/O": "❌ 禁用 (open())",
        "import 模块": "❌ 禁用 (无法加载 .py/.mpy 文件)",
        "数学模块": "❌ 禁用 (math 模块)",
        "复杂数据结构": "⚠️ 部分可用 (list/dict 可用, 但某些方法可能缺失)",
    }
}

for category, items in features.items():
    print(f"\n{category}:")
    for name, status in items.items():
        print(f"  {name}: {status}")

print("\n" + "=" * 60)
print("可以启用的额外功能")
print("=" * 60)

extra_features = [
    ("MICROPY_PY_MATH", "数学模块 (math.sqrt, sin, cos 等)"),
    ("MICROPY_PY_CMATH", "复数数学模块"),
    ("MICROPY_PY_RANDOM", "随机数模块"),
    ("MICROPY_PY_UJSON", "JSON 支持"),
    ("MICROPY_PY_URE", "正则表达式模块 (ure)"),
    ("MICROPY_PY_UHASHLIB", "哈希库 (md5, sha1, sha256)"),
    ("MICROPY_PY_UHEAPQ", "堆队列/优先队列"),
    ("MICROPY_PY_UOS", "操作系统接口 (uos)"),
    ("MICROPY_PY_UTIME", "时间模块 (utime)"),
    ("MICROPY_PY_COLLECTIONS", "collections 模块 (deque, OrderedDict)"),
    ("MICROPY_PY_IO", "I/O 流支持"),
    ("MICROPY_PY_VFS", "虚拟文件系统"),
    ("MICROPY_PY_VFS_POSIX", "POSIX 文件系统访问"),
]

for macro, desc in extra_features:
    print(f"  {macro}")
    print(f"    └── {desc}")

print("\n" + "=" * 60)
print("标准库支持")
print("=" * 60)

stdlib = {
    "内置模块 (builtins)": [
        "abs, all, any, bin, bool, bytearray, bytes",
        "chr, dict, dir, divmod, enumerate, filter",
        "float, getattr, globals, hasattr, hash",
        "hex, id, int, isinstance, issubclass, iter",
        "len, list, locals, map, max, min, next",
        "object, oct, ord, pow, print, property",
        "range, repr, reversed, round, set, setattr",
        "slice, sorted, staticmethod, str, sum, super",
        "tuple, type, vars, zip",
    ],
    "sys 模块": [
        "sys.exit, sys.modules, sys.path, sys.platform",
        "sys.stdin, sys.stdout, sys.stderr",
        "sys.version, sys.version_info, sys.implementation",
    ],
    "gc 模块": [
        "gc.collect(), gc.disable(), gc.enable()",
        "gc.mem_alloc(), gc.mem_free()",
    ],
    "micropython 模块": [
        "micropython.mem_info()",
        "micropython.qstr_info()",
        "micropython.stack_use()",
    ]
}

for mod, funcs in stdlib.items():
    print(f"\n{mod}:")
    for f in funcs:
        print(f"  - {f}")

print("\n" + "=" * 60)
