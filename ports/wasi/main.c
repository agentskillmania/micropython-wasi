/**
 * MicroPython WASI Preview2 Port - 主程序入口
 * 
 * 本文件是 WASI Preview2 平台的主程序入口，负责：
 * 1. 初始化 MicroPython 运行时（GC、解释器等）
 * 2. 挂载 POSIX 文件系统（VFS）
 * 3. 执行 Python 代码（命令行参数或内置测试）
 * 
 * WASI Preview2 特点：
 * - 基于 WebAssembly System Interface Preview2
 * - 支持 POSIX socket API（通过 wasmtime -S tcp=y）
 * - 支持异常处理（通过 wasm32-wasip2-clang 的 sjlj 支持）
 * 
 * 编译命令：
 *   wasm32-wasip2-clang -mllvm -wasm-enable-sjlj -mllvm -wasm-use-legacy-eh=0
 * 
 * 运行命令：
 *   wasmtime run -W exceptions=y -S tcp=y -S inherit-network=y micropython.wasm
 * 
 * 许可证: MIT
 * Copyright (c) 2014-2019 Paul Sokolovsky, Damien P. George
 * Copyright (c) 2026 @agentskillmania
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

/* MicroPython 核心头文件 */
#include "py/builtin.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/mphal.h"

/* 虚拟文件系统支持 */
#include "extmod/vfs.h"
#include "extmod/vfs_posix.h"

/* REPL 已禁用 - 沙箱环境不需要交互式界面 */

/* 栈顶指针，用于 GC 扫描栈 */
static char *stack_top;

/* 堆内存，用于 MicroPython 的内存分配 */
static char heap[MICROPY_HEAP_SIZE];

/**
 * 执行 Python 代码字符串
 * 
 * @param code - 要执行的 Python 代码
 * @return 0 成功，1 失败
 */
int exec_python_code(const char *code) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        // 创建词法分析器
        mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, code, strlen(code), 0);
        qstr source_name = lex->source_name;
        
        // 解析为 AST
        mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
        
        // 编译为字节码
        mp_obj_t module_fun = mp_compile(&parse_tree, source_name, false);
        
        // 执行编译后的函数
        mp_obj_t result = mp_call_function_0(module_fun);
        
        nlr_pop();
        
        // 如果返回值不是 None，打印它
        if (result != mp_const_none) {
            mp_obj_print_helper(MP_PYTHON_PRINTER, result, PRINT_REPR);
            mp_hal_stdout_tx_strn("\n", 1);
        }
        return 0;
    } else {
        // 发生异常，打印异常信息
        mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
        return 1;
    }
}

/**
 * 主函数 - 程序入口
 */
int main(int argc, char **argv) {
    // 获取栈地址用于 GC
    int stack_dummy;
    stack_top = (char *)&stack_dummy;

    // 初始化垃圾回收器
    gc_init(heap, heap + sizeof(heap));
    
    // 初始化 MicroPython
    mp_init();
    
    /* 
     * 挂载 POSIX 文件系统
     * 这使得 Python 代码可以访问宿主机的文件系统
     */
    #if MICROPY_VFS_POSIX
    {
        // 创建 VFS POSIX 对象
        mp_obj_t args[2] = {
            MP_OBJ_TYPE_GET_SLOT(&mp_type_vfs_posix, make_new)(&mp_type_vfs_posix, 0, 0, NULL),
            MP_OBJ_NEW_QSTR(MP_QSTR__slash_),  // 挂载到根目录 /
        };
        // 执行挂载
        mp_vfs_mount(2, args, (mp_map_t *)&mp_const_empty_map);
        MP_STATE_VM(vfs_cur) = MP_STATE_VM(vfs_mount_table);
    }
    #endif
    
    int ret = 0;
    
    // 处理命令行参数
    if (argc > 1) {
        // 如果有参数，将每个参数作为 Python 代码执行
        for (int i = 1; i < argc; i++) {
            ret = exec_python_code(argv[i]);
            if (ret != 0) break;
        }
    } else {
        // 没有参数时，执行内置测试代码
        const char *test_code = 
            "import sys\n"
            "print('=== MicroPython on WASI Preview2 ===')\n"
            "print('Platform:', sys.platform)\n"
            "print('Version:', sys.version)\n"
            "print()\n"
            "print('=== Testing socket module ===')\n"
            "try:\n"
            "    import socket\n"
            "    print('socket module imported successfully')\n"
            "    print('Available attributes:', [x for x in dir(socket) if not x.startswith(\"_\")])\n"
            "    print()\n"
            "    print('=== Creating a socket ===')\n"
            "    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)\n"
            "    print('Socket created:', s)\n"
            "    print('File descriptor:', s.fileno())\n"
            "    s.close()\n"
            "    print('Socket closed successfully')\n"
            "except Exception as e:\n"
            "    print('Error:', e)\n"
            "print()\n"
            "print('Done!')";
        ret = exec_python_code(test_code);
    }

    // 清理 MicroPython
    mp_deinit();
    return ret;
}

/**
 * 垃圾回收 - 扫描栈和堆
 * 
 * GC 需要知道哪些内存地址在栈上，哪些是堆上的对象
 * 这是 MicroPython GC 的回调函数
 */
void gc_collect(void) {
    void *dummy;
    gc_collect_start();
    // 扫描从 stack_top 到当前栈指针的所有地址
    gc_collect_root(&dummy, ((mp_uint_t)stack_top - (mp_uint_t)&dummy) / sizeof(mp_uint_t));
    gc_collect_end();
}

/**
 * nlr（非局部返回）跳转失败处理
 * 正常情况下不应到达这里
 */
void nlr_jump_fail(void *val) {
    while (1) {
        ;
    }
}

/**
 * 致命错误处理
 */
void MP_NORETURN __fatal_error(const char *msg) {
    while (1) {
        ;
    }
}

#ifndef NDEBUG
/**
 * 断言失败处理（仅在调试模式下）
 */
void MP_WEAK __assert_func(const char *file, int line, const char *func, const char *expr) {
    printf("Assertion '%s' failed, at file %s:%d\n", expr, file, line);
    __fatal_error("Assertion failed");
}
#endif
