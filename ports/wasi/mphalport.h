/**
 * MicroPython 硬件抽象层头文件（WASI Preview2）
 * 
 * 定义平台特定的函数和宏：
 * - 时间函数（ticks_ms, delay_ms）
 * - 标准输入输出
 * - 系统调用重试宏
 * 
 * 许可证：MIT
 */

#ifndef MICROPY_MPHALPORT_H
#define MICROPY_MPHALPORT_H

#include <stdint.h>
#include <time.h>
#include <errno.h>
#include "py/mpconfig.h"
#include "py/obj.h"

int mp_hal_stdin_rx_chr(void);
mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len);

// Time functions required by extmod/modtime.c
struct _timeutils_struct_time_t;
void mp_time_localtime_get(struct _timeutils_struct_time_t *tm);
mp_obj_t mp_time_time_get(void);

static inline uint64_t mp_hal_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline mp_uint_t mp_hal_ticks_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (mp_uint_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static inline mp_uint_t mp_hal_ticks_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (mp_uint_t)(ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
}

static inline void mp_hal_set_interrupt_char(char c) {
    (void)c;
}

static inline void mp_hal_delay_ms(mp_uint_t ms) {
    struct timespec ts = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000
    };
    nanosleep(&ts, NULL);
}

static inline void mp_hal_delay_us(mp_uint_t us) {
    struct timespec ts = {
        .tv_sec = us / 1000000,
        .tv_nsec = (us % 1000000) * 1000
    };
    nanosleep(&ts, NULL);
}

static inline mp_uint_t mp_hal_ticks_cpu(void) {
    return 0;
}

// This macro is used to implement PEP 475 to retry specified syscalls on EINTR
// Simplified version for WASI (no threading)
#define MP_HAL_RETRY_SYSCALL(ret, syscall, raise) { \
        for (;;) { \
            ret = syscall; \
            if (ret == -1) { \
                int err = errno; \
                if (err == EINTR) { \
                    mp_handle_pending(true); \
                    continue; \
                } \
                raise; \
            } \
            break; \
        } \
}

#endif // MICROPY_MPHALPORT_H
