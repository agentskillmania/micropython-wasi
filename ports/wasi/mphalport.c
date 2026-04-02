/**
 * MicroPython 硬件抽象层实现（WASI Preview2）
 * 
 * 实现平台特定的 I/O 函数：
 * - 标准输入读取（用于 REPL）
 * - 标准输出写入（用于 print）
 * 
 * 注意：时间相关函数在 mphalport.h 中以内联方式实现
 * 
 * 许可证：MIT
 */

#include <unistd.h>
#include "py/mpconfig.h"

// Receive single character
int mp_hal_stdin_rx_chr(void) {
    unsigned char c = 0;
    int r = read(STDIN_FILENO, &c, 1);
    if (r <= 0) {
        return 0;
    }
    return c;
}

// Send string of given length
mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
    int r = write(STDOUT_FILENO, str, len);
    if (r < 0) {
        return 0;
    }
    return r;
}
