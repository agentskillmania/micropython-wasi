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
 * MicroPython CLI 主逻辑
 * 
 * 初始化运行时并执行 Python 代码，返回进程退出码。
 * 可被 component guest wrapper 直接调用。
 */
int mpy_cli_main(int argc, char **argv) {
    // 获取栈地址用于 GC
    int stack_dummy;
    stack_top = (char *)&stack_dummy;

    // 初始化垃圾回收器
    gc_init(heap, heap + sizeof(heap));
    
    // 初始化 MicroPython
    mp_init();

    // 设置 sys.argv (参考 unix port 的 set_sys_argv)
    mp_obj_list_init(MP_OBJ_TO_PTR(mp_sys_argv), 0);
    for (int i = 0; i < argc; i++) {
        mp_obj_list_append(mp_sys_argv, MP_OBJ_NEW_QSTR(qstr_from_str(argv[i])));
    }

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

    // Read MICROPYPATH environment variable to populate sys.path.
    // This is needed for run-tests.py which sets MICROPYPATH to include
    // extmod and unittest directories. Without this, sys.path only has
    // ["", ".frozen"] and import unittest fails.
    {
        char *path = getenv("MICROPYPATH");
        if (path != NULL) {
            // sys.path already has ["", ".frozen"] from py/runtime.c
            // Split MICROPYPATH by ':' and append each entry
            while (*path) {
                if (*path == ':') {
                    path++;
                    continue;
                }
                char *entry_end = strchr(path, ':');
                size_t entry_len;
                if (entry_end != NULL) {
                    entry_len = entry_end - path;
                } else {
                    entry_len = strlen(path);
                }
                if (entry_len > 0) {
                    mp_obj_list_append(
                        mp_sys_path,
                        mp_obj_new_str_via_qstr(path, entry_len)
                    );
                }
                path += entry_len;
                if (*path == ':') {
                    path++;
                }
            }
        }
    }

    int ret = 0;

    // Handle command-line arguments (CPython-compatible)
    while (argc > 1) {
        if (strcmp(argv[1], "-X") == 0) {
            // Ignore -X options (run-tests.py compatibility)
            if (argc > 2) {
                argc -= 2;
                argv += 2;
                continue;
            } else {
                fprintf(stderr, "python: -X requires an argument\n");
                ret = 1;
                break;
            }
        }
        if (strcmp(argv[1], "-c") == 0) {
            // python -c "code" — execute code string
            if (argc > 2) {
                ret = exec_python_code(argv[2]);
            } else {
                fprintf(stderr, "python: -c requires an argument\n");
                ret = 1;
            }
        } else if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0) {
            printf("MicroPython %s\n", MICROPY_VERSION_STRING);
        } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: python [option] ... [-c cmd | file] [arg] ...\n");
            printf("Options:\n");
            printf("  -c cmd     Program passed in as string\n");
            printf("  -h, --help Show this help message and exit\n");
            printf("  -V, --version Print the MicroPython version number and exit\n");
        } else if (argv[1][0] == '-') {
            // Unknown option
            fprintf(stderr, "python: unknown option '%s'\n", argv[1]);
            fprintf(stderr, "Use python --help for usage information.\n");
            ret = 1;
        } else {
            // python script.py — execute file
            nlr_buf_t nlr;
            if (nlr_push(&nlr) == 0) {
                mp_lexer_t *lex = mp_lexer_new_from_file(qstr_from_str(argv[1]));
                mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
                mp_obj_t module_fun = mp_compile(&parse_tree, lex->source_name, false);
                mp_call_function_0(module_fun);
                nlr_pop();
            } else {
                mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
                ret = 1;
            }
        }
        break;
    }
    if (argc <= 1 && ret == 0) {
        // python — no arguments, print version info
        printf("MicroPython %s\n", MICROPY_VERSION_STRING);
    }

    // Clean up MicroPython
    mp_deinit();
    return ret;
}

/**
 * 主函数 - 程序入口
 */
int main(int argc, char **argv) {
    return mpy_cli_main(argc, argv);
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
