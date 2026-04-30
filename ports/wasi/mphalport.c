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
#include <time.h>
#include "py/runtime.h"
#include "shared/timeutils/timeutils.h"

// Platform-specific time functions for time module
void mp_time_localtime_get(timeutils_struct_time_t *tm) {
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    tm->tm_year = lt->tm_year + 1900;
    tm->tm_mon = lt->tm_mon + 1;
    tm->tm_mday = lt->tm_mday;
    tm->tm_hour = lt->tm_hour;
    tm->tm_min = lt->tm_min;
    tm->tm_sec = lt->tm_sec;
    tm->tm_wday = lt->tm_wday;
    tm->tm_yday = lt->tm_yday + 1;
}

mp_obj_t mp_time_time_get(void) {
    #if MICROPY_PY_BUILTINS_FLOAT && MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_DOUBLE
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    mp_float_t val = ts.tv_sec + (mp_float_t)ts.tv_nsec / 1000000000;
    return mp_obj_new_float(val);
    #else
    return mp_obj_new_int((mp_int_t)time(NULL));
    #endif
}

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
