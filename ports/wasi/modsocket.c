/**
 * MicroPython WASI Preview2 Socket 模块
 * 
 * 本文件实现了 MicroPython 的 socket 模块，基于 POSIX socket API
 * 专为 WASI Preview2 平台适配
 * 
 * 【与 MicroPython Unix port 的关键差异总结】
 * 
 * 1. socket_connect() - 地址解析方式完全不同
 *    Unix:   使用 mp_get_buffer_raise() 获取 sockaddr 缓冲区，支持 getaddrinfo() 解析的地址
 *    WASI:   手动解析 (host, port) 元组，使用 inet_pton() 转换 IP 字符串
 *    原因:   WASI Preview2 不支持 getaddrinfo() DNS 解析
 * 
 * 2. socket_connect() - EINPROGRESS 处理方式
 *    Unix:   直接返回 MP_ETIMEDOUT 错误
 *    WASI:   使用 poll() 轮询等待连接完成（30秒超时）
 *    原因:   WASI socket 默认非阻塞，connect() 立即返回 EINPROGRESS
 * 
 * 3. socket_bind() - 地址解析方式
 *    Unix:   使用 mp_get_buffer_raise() 获取 sockaddr 缓冲区
 *    WASI:   手动解析 (host, port) 元组，使用 inet_pton() 转换
 *    原因:   与 connect() 保持一致，避免 sockaddr 缓冲区问题
 * 
 * 4. 缺失的功能
 *    - getaddrinfo(): WASI Preview2 不支持 DNS 解析
 *    - Unix domain socket: AF_UNIX 可能不受支持
 * 
 * 5. 行为一致的函数
 *    - socket_read/write(): 使用 MP_HAL_RETRY_SYSCALL，与 Unix port 相同
 *    - socket_send/recv(): 基本与 Unix port 相同
 *    - socket_listen/accept(): 基本与 Unix port 相同
 *    - socket_ioctl(): 支持 MP_STREAM_POLL，用于 asyncio
 * 
 * 【使用方法】
 *   import socket
 *   s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
 *   s.connect(("192.168.1.1", 80))  # 必须使用 IP 地址
 *   s.send(b"GET / HTTP/1.0\r\n\r\n")
 *   data = s.recv(1024)
 *   s.close()
 * 
 * 【运行要求】
 *   wasmtime run -W exceptions=y -S tcp=y -S inherit-network=y micropython.wasm
 * 
 * 原始版权：
 * Copyright (c) 2014-2018 Paul Sokolovsky
 * Copyright (c) 2014-2019 Damien P. George
 * Copyright (c) 2026 WASI Port Contributors
 * 许可证：MIT
 */

#include "py/mpconfig.h"

#if MICROPY_PY_SOCKET

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <math.h>

#include "py/objtuple.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/builtin.h"
#include "py/mphal.h"
#include "extmod/vfs.h"
#include <poll.h>

#define RAISE_ERRNO(err_flag, error_val) \
    { if (err_flag == -1) mp_raise_OSError(error_val); }

// This type must "inherit" from mp_obj_fdfile_t, i.e. matching subset of
// fields should have the same layout.
typedef struct _mp_obj_socket_t {
    mp_obj_base_t base;
    int fd;
    bool blocking;
} mp_obj_socket_t;

const mp_obj_type_t mp_type_socket;

static mp_obj_socket_t *socket_new(int fd) {
    mp_obj_socket_t *o = mp_obj_malloc(mp_obj_socket_t, &mp_type_socket);
    o->fd = fd;
    o->blocking = true;
    return o;
}


static void socket_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<_socket %d>", self->fd);
}

static mp_uint_t socket_read(mp_obj_t o_in, void *buf, mp_uint_t size, int *errcode) {
    mp_obj_socket_t *o = MP_OBJ_TO_PTR(o_in);
    ssize_t r;
    MP_HAL_RETRY_SYSCALL(r, recv(o->fd, buf, size, 0), {
        // On blocking socket, we get EAGAIN in case SO_RCVTIMEO/SO_SNDTIMEO
        // timed out, and need to convert that to ETIMEDOUT.
        if (err == EAGAIN && o->blocking) {
            err = MP_ETIMEDOUT;
        }

        *errcode = err;
        return MP_STREAM_ERROR;
    });
    return (mp_uint_t)r;
}

static mp_uint_t socket_write(mp_obj_t o_in, const void *buf, mp_uint_t size, int *errcode) {
    mp_obj_socket_t *o = MP_OBJ_TO_PTR(o_in);
    ssize_t r;
    MP_HAL_RETRY_SYSCALL(r, send(o->fd, buf, size, 0), {
        // On blocking socket, we get EAGAIN in case SO_RCVTIMEO/SO_SNDTIMEO
        // timed out, and need to convert that to ETIMEDOUT.
        if (err == EAGAIN && o->blocking) {
            err = MP_ETIMEDOUT;
        }

        *errcode = err;
        return MP_STREAM_ERROR;
    });
    return (mp_uint_t)r;
}

static mp_uint_t socket_ioctl(mp_obj_t o_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(o_in);
    (void)arg;
    switch (request) {
        case MP_STREAM_CLOSE:
            // There's a POSIX drama regarding return value of close in general,
            // and EINTR error in particular. See e.g.
            // http://lwn.net/Articles/576478/
            // http://austingroupbugs.net/view.php?id=529
            // The rationale MicroPython follows is that close() just releases
            // file descriptor. If you're interested to catch I/O errors before
            // closing fd, fsync() it.
            if (self->fd >= 0) {
                close(self->fd);
            }
            self->fd = -1;
            return 0;

        case MP_STREAM_GET_FILENO:
            return self->fd;

        #if MICROPY_PY_SELECT
        case MP_STREAM_POLL: {
            mp_uint_t ret = 0;
            uint8_t pollevents = 0;
            if (arg & MP_STREAM_POLL_RD) {
                pollevents |= POLLIN;
            }
            if (arg & MP_STREAM_POLL_WR) {
                pollevents |= POLLOUT;
            }
            struct pollfd pfd = { .fd = self->fd, .events = pollevents };
            if (poll(&pfd, 1, 0) > 0) {
                if (pfd.revents & POLLIN) {
                    ret |= MP_STREAM_POLL_RD;
                }
                if (pfd.revents & POLLOUT) {
                    ret |= MP_STREAM_POLL_WR;
                }
                if (pfd.revents & POLLERR) {
                    ret |= MP_STREAM_POLL_ERR;
                }
                if (pfd.revents & POLLHUP) {
                    ret |= MP_STREAM_POLL_HUP;
                }
                if (pfd.revents & POLLNVAL) {
                    ret |= MP_STREAM_POLL_NVAL;
                }
            }
            return ret;
        }
        #endif

        default:
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
    }
}

static mp_obj_t socket_fileno(mp_obj_t self_in) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);
    return MP_OBJ_NEW_SMALL_INT(self->fd);
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_fileno_obj, socket_fileno);

/**
 * socket.connect() 方法 - WASI Preview2 适配版本
 * 
 * 【与 Unix port 的关键差异】
 * 
 * Unix port 原版实现：
 *   1. 使用 mp_get_buffer_raise() 获取地址缓冲区
 *   2. 直接使用 connect() 系统调用
 *   3. EINPROGRESS 直接转为 ETIMEDOUT 错误
 * 
 * WASI Preview2 本版实现：
 *   1. 解析 Python 元组 (host, port) - 因为 WASI 不支持 sockaddr 缓冲区直接传递
 *   2. 使用 inet_pton() 或 getaddrinfo() 解析地址 - inet_pton 用于 IP，getaddrinfo 用于域名
 *   3. EINPROGRESS 使用 poll() 等待连接完成 - 因为 WASI socket 默认非阻塞
 * 
 * 这些修改是因为 WASI Preview2 的 socket 行为与标准 POSIX 有差异：
 * - 所有 socket 默认是非阻塞的
 * - connect() 立即返回 EINPROGRESS
 * - 必须通过 poll/select 等待连接完成
 */
static mp_obj_t socket_connect(mp_obj_t self_in, mp_obj_t addr_in) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);
    
    /* === 差异 1: 支持两种地址格式 === */
    // Unix port: mp_get_buffer_raise(addr_in, &bufinfo, MP_BUFFER_READ);
    // WASI 版本: 同时支持 (host, port) 元组 和 bytearray (sockaddr 缓冲区，asyncio 使用)
    if (mp_obj_is_type(addr_in, &mp_type_tuple) || mp_obj_is_type(addr_in, &mp_type_list)) {
        // (host, port) 元组格式
        mp_obj_t *addr_items;
        mp_obj_get_array_fixed_n(addr_in, 2, &addr_items);
        
        const char *host = mp_obj_str_get_str(addr_items[0]);
        mp_int_t port = mp_obj_get_int(addr_items[1]);
        
        /* === 差异 2: 使用 inet_pton + getaddrinfo 解析地址 === */
        // Unix port: 地址已经通过 getaddrinfo 解析为 sockaddr
        // WASI 版本: 先尝试 inet_pton 解析 IP，失败则用 getaddrinfo 解析域名 (需 -S allow-ip-name-lookup=y)
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        
        if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
            // inet_pton 失败，尝试用 getaddrinfo 解析域名
            struct addrinfo hints;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            
            char port_str[6];
            snprintf(port_str, sizeof(port_str), "%d", (int)port);
            
            struct addrinfo *addr_list;
            int res = getaddrinfo(host, port_str, &hints, &addr_list);
            if (res != 0 || addr_list == NULL) {
                mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("[addrinfo error %d]"), res);
            }
            
            // 使用第一个结果
            memcpy(&addr, addr_list->ai_addr, sizeof(addr));
            freeaddrinfo(addr_list);
        }
        
        /* === 差异 3: EINPROGRESS 使用 poll 等待 === */
        // Unix port: EINPROGRESS -> MP_ETIMEDOUT (直接报错)
        // WASI 版本: EINPROGRESS -> poll() 等待连接完成
        for (;;) {
            int r = connect(self->fd, (const struct sockaddr *)&addr, sizeof(addr));
            if (r == -1) {
                int err = errno;
                if (self->blocking) {
                    if (err == EINTR) {
                        mp_handle_pending(true);
                        continue;
                    }
                    // WASI Preview2 特殊处理：
                    // 阻塞 socket 也可能返回 EINPROGRESS，必须使用 poll 等待
                    if (err == EINPROGRESS) {
                        struct pollfd pfd = { .fd = self->fd, .events = POLLOUT };
                        int poll_ret;
                        // 使用 30 秒超时轮询
                        MP_HAL_RETRY_SYSCALL(poll_ret, poll(&pfd, 1, 30000), {
                            mp_raise_OSError(err);
                        });
                        if (poll_ret == 0) {
                            // 超时
                            mp_raise_OSError(MP_ETIMEDOUT);
                        }
                        // 检查连接是否成功
                        int so_error = 0;
                        socklen_t so_error_len = sizeof(so_error);
                        r = getsockopt(self->fd, SOL_SOCKET, SO_ERROR, &so_error, &so_error_len);
                        if (r == -1) {
                            mp_raise_OSError(errno);
                        }
                        if (so_error != 0) {
                            mp_raise_OSError(so_error);
                        }
                        return mp_const_none;
                    }
                }
                mp_raise_OSError(err);
            }
            return mp_const_none;
        }
    } else {
        // bytearray 格式 (asyncio 使用 getaddrinfo 返回的 sockaddr 缓冲区)
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(addr_in, &bufinfo, MP_BUFFER_READ);
        
        for (;;) {
            int r = connect(self->fd, (const struct sockaddr *)bufinfo.buf, bufinfo.len);
            if (r == -1) {
                int err = errno;
                if (self->blocking) {
                    if (err == EINTR) {
                        mp_handle_pending(true);
                        continue;
                    }
                    if (err == EINPROGRESS) {
                        struct pollfd pfd = { .fd = self->fd, .events = POLLOUT };
                        int poll_ret;
                        MP_HAL_RETRY_SYSCALL(poll_ret, poll(&pfd, 1, 30000), {
                            mp_raise_OSError(err);
                        });
                        if (poll_ret == 0) {
                            mp_raise_OSError(MP_ETIMEDOUT);
                        }
                        int so_error = 0;
                        socklen_t so_error_len = sizeof(so_error);
                        r = getsockopt(self->fd, SOL_SOCKET, SO_ERROR, &so_error, &so_error_len);
                        if (r == -1) {
                            mp_raise_OSError(errno);
                        }
                        if (so_error != 0) {
                            mp_raise_OSError(so_error);
                        }
                        return mp_const_none;
                    }
                }
                mp_raise_OSError(err);
            }
            return mp_const_none;
        }
    }
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_connect_obj, socket_connect);

/**
 * socket.bind() 方法 - WASI Preview2 版本
 * 
 * 【与 Unix port 的差异】
 * Unix port: 使用 mp_get_buffer_raise() 获取 sockaddr 缓冲区
 * WASI 版本: 同样使用缓冲区方式，但为了与 connect() 保持一致，
 *            实际通过 Python 层传入的地址会被转换为 sockaddr
 * 
 * 注意：WASI Preview2 的 bind 行为与标准 POSIX 基本一致
 */
static mp_obj_t socket_bind(mp_obj_t self_in, mp_obj_t addr_in) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);
    
    if (mp_obj_is_type(addr_in, &mp_type_tuple) || mp_obj_is_type(addr_in, &mp_type_list)) {
        // (host, port) 元组格式
        mp_obj_t *addr_items;
        mp_obj_get_array_fixed_n(addr_in, 2, &addr_items);
        
        const char *host = mp_obj_str_get_str(addr_items[0]);
        mp_int_t port = mp_obj_get_int(addr_items[1]);
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        
        if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
            mp_raise_ValueError(MP_ERROR_TEXT("invalid IP address"));
        }
        
        int r = bind(self->fd, (const struct sockaddr *)&addr, sizeof(addr));
        RAISE_ERRNO(r, errno);
    } else {
        // bytearray 格式 (asyncio 使用 getaddrinfo 返回的 sockaddr 缓冲区)
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(addr_in, &bufinfo, MP_BUFFER_READ);
        int r = bind(self->fd, (const struct sockaddr *)bufinfo.buf, bufinfo.len);
        RAISE_ERRNO(r, errno);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_bind_obj, socket_bind);

// method socket.listen([backlog])
static mp_obj_t socket_listen(size_t n_args, const mp_obj_t *args) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(args[0]);

    int backlog = MICROPY_PY_SOCKET_LISTEN_BACKLOG_DEFAULT;
    if (n_args > 1) {
        backlog = (int)mp_obj_get_int(args[1]);
        backlog = (backlog < 0) ? 0 : backlog;
    }

    int r = listen(self->fd, backlog);
    RAISE_ERRNO(r, errno);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_listen_obj, 1, 2, socket_listen);

/**
 * socket.accept() 方法 - WASI Preview2 版本
 * 
 * 【与 Unix port 的差异】
 * Unix port: 返回 (socket, sockaddr_buffer)
 * WASI 版本: 同样返回 (socket, address_buffer)
 * 
 * 行为基本一致，但注意 WASI 的 accept 也可能返回 EAGAIN/EWOULDBLOCK
 */
static mp_obj_t socket_accept(mp_obj_t self_in) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);
    // 使用固定大小的缓冲区代替 sockaddr_storage（节省栈空间）
    byte addr[32];
    socklen_t addr_len = sizeof(addr);
    int fd;
    MP_HAL_RETRY_SYSCALL(fd, accept(self->fd, (struct sockaddr *)&addr, &addr_len), {
        // EAGAIN on a blocking socket means the operation timed out
        if (self->blocking && err == EAGAIN) {
            err = MP_ETIMEDOUT;
        }
        mp_raise_OSError(err);
    });

    // 返回 (socket, address) 元组
    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(2, NULL));
    t->items[0] = MP_OBJ_FROM_PTR(socket_new(fd));
    t->items[1] = mp_obj_new_bytearray(addr_len, &addr);

    return MP_OBJ_FROM_PTR(t);
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_accept_obj, socket_accept);

// Note: besides flag param, this differs from read() in that
// this does not swallow blocking errors (EAGAIN, EWOULDBLOCK) -
// these would be thrown as exceptions.
static mp_obj_t socket_recv(size_t n_args, const mp_obj_t *args) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(args[0]);
    int sz = mp_obj_get_int(args[1]);
    int flags = 0;

    if (n_args > 2) {
        flags = mp_obj_get_int(args[2]);
    }

    byte *buf = m_new(byte, sz);
    ssize_t out_sz;
    MP_HAL_RETRY_SYSCALL(out_sz, recv(self->fd, buf, sz, flags), mp_raise_OSError(err));
    mp_obj_t ret = mp_obj_new_str_of_type(&mp_type_bytes, buf, out_sz);
    m_del(char, buf, sz);
    return ret;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_recv_obj, 2, 3, socket_recv);

/**
 * socket.recvfrom() 方法 - WASI Preview2 版本
 *
 * 【与 Unix port 的差异】
 * Unix port: 返回 (data, sockaddr_bytearray)
 * WASI 版本: 返回 (data, (host, port)) 元组
 * 原因: WASI Preview2 的 sockaddr 缓冲区在 Python 层不友好
 */
static mp_obj_t socket_recvfrom(size_t n_args, const mp_obj_t *args) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(args[0]);
    int sz = mp_obj_get_int(args[1]);
    int flags = 0;

    if (n_args > 2) {
        flags = mp_obj_get_int(args[2]);
    }

    byte *buf = m_new(byte, sz);
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    ssize_t out_sz;
    MP_HAL_RETRY_SYSCALL(out_sz, recvfrom(self->fd, buf, sz, flags, (struct sockaddr *)&addr, &addr_len), mp_raise_OSError(err));

    // 解析 sockaddr 为 (host, port) 元组
    mp_obj_t addr_tuple;
    if (addr.ss_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)&addr;
        char host[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin->sin_addr, host, sizeof(host));
        mp_obj_t items[2] = {
            mp_obj_new_str(host, strlen(host)),
            mp_obj_new_int(ntohs(sin->sin_port))
        };
        addr_tuple = mp_obj_new_tuple(2, items);
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&addr;
        char host[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &sin6->sin6_addr, host, sizeof(host));
        mp_obj_t items[2] = {
            mp_obj_new_str(host, strlen(host)),
            mp_obj_new_int(ntohs(sin6->sin6_port))
        };
        addr_tuple = mp_obj_new_tuple(2, items);
    } else {
        addr_tuple = mp_const_none;
    }

    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(2, NULL));
    t->items[0] = mp_obj_new_str_of_type(&mp_type_bytes, buf, out_sz);
    t->items[1] = addr_tuple;
    m_del(char, buf, sz);
    return MP_OBJ_FROM_PTR(t);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_recvfrom_obj, 2, 3, socket_recvfrom);

static mp_obj_t socket_send(mp_obj_t self_in, mp_obj_t data_in) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);

    ssize_t out_sz;
    MP_HAL_RETRY_SYSCALL(out_sz, send(self->fd, bufinfo.buf, bufinfo.len, 0), mp_raise_OSError(err));
    return mp_obj_new_int(out_sz);
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_send_obj, socket_send);

/**
 * socket.sendto() 方法 - WASI Preview2 版本
 *
 * 【与 Unix port 的差异】
 * Unix port: 使用 mp_get_buffer_raise() 获取 sockaddr 缓冲区
 * WASI 版本: 手动解析 (host, port) 元组，使用 inet_pton() 转换
 * 原因: WASI Preview2 的 sockaddr 缓冲区传递不可靠
 */
static mp_obj_t socket_sendto(mp_obj_t self_in, mp_obj_t data_in, mp_obj_t addr_in) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if (mp_obj_is_type(addr_in, &mp_type_tuple) || mp_obj_is_type(addr_in, &mp_type_list)) {
        // (host, port) 元组格式
        mp_obj_t *addr_items;
        mp_obj_get_array_fixed_n(addr_in, 2, &addr_items);

        const char *host = mp_obj_str_get_str(addr_items[0]);
        mp_int_t port = mp_obj_get_int(addr_items[1]);

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
            // inet_pton 失败，尝试用 getaddrinfo 解析域名
            struct addrinfo hints;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;

            char port_str[6];
            snprintf(port_str, sizeof(port_str), "%d", (int)port);

            struct addrinfo *addr_list;
            int res = getaddrinfo(host, port_str, &hints, &addr_list);
            if (res != 0 || addr_list == NULL) {
                mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("[addrinfo error %d]"), res);
            }

            // 使用第一个结果
            memcpy(&addr, addr_list->ai_addr, sizeof(addr));
            freeaddrinfo(addr_list);
        }
    } else {
        // bytearray 格式
        mp_buffer_info_t addr_bufinfo;
        mp_get_buffer_raise(addr_in, &addr_bufinfo, MP_BUFFER_READ);
        memcpy(&addr, addr_bufinfo.buf, addr_bufinfo.len);
    }

    ssize_t out_sz;
    MP_HAL_RETRY_SYSCALL(out_sz, sendto(self->fd, bufinfo.buf, bufinfo.len, 0, (struct sockaddr *)&addr, addr_len), mp_raise_OSError(err));
    return mp_obj_new_int(out_sz);
}
static MP_DEFINE_CONST_FUN_OBJ_3(socket_sendto_obj, socket_sendto);

static mp_obj_t socket_setsockopt(size_t n_args, const mp_obj_t *args) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(args[0]);
    int level = mp_obj_get_int(args[1]);
    int option = mp_obj_get_int(args[2]);

    int r;
    if (mp_obj_is_integer(args[3])) {
        int val = mp_obj_get_int(args[3]);
        r = setsockopt(self->fd, level, option, &val, sizeof(val));
    } else {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[3], &bufinfo, MP_BUFFER_READ);
        r = setsockopt(self->fd, level, option, bufinfo.buf, bufinfo.len);
    }
    RAISE_ERRNO(r, errno);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_setsockopt_obj, 4, 4, socket_setsockopt);

static mp_obj_t socket_setblocking(mp_obj_t self_in, mp_obj_t flag_in) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);
    int val = mp_obj_is_true(flag_in);
    int r = fcntl(self->fd, F_GETFL, 0);
    RAISE_ERRNO(r, errno);
    if (val) {
        r = fcntl(self->fd, F_SETFL, r & ~O_NONBLOCK);
    } else {
        r = fcntl(self->fd, F_SETFL, r | O_NONBLOCK);
    }
    RAISE_ERRNO(r, errno);
    self->blocking = val;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_setblocking_obj, socket_setblocking);

static mp_obj_t socket_settimeout(mp_obj_t self_in, mp_obj_t timeout_in) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);

    struct timeval tv;

    if (timeout_in == mp_const_none) {
        // None = restore blocking mode with no timeout
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        int r = setsockopt(self->fd, SOL_SOCKET, SO_RCVTIMEO, NULL, 0);
        RAISE_ERRNO(r, errno);
        r = setsockopt(self->fd, SOL_SOCKET, SO_SNDTIMEO, NULL, 0);
        RAISE_ERRNO(r, errno);
        // Restore blocking mode if it was changed
        if (!self->blocking) {
            socket_setblocking(self_in, mp_obj_new_bool(true));
        }
    } else {
        mp_float_t val = mp_obj_get_float(timeout_in);
        if (val < 0) {
            mp_raise_ValueError(MP_ERROR_TEXT("negative timeout"));
        }
        val = val * 1000; // float to ms (secs)
        int ms = (int)val;
        tv.tv_sec = ms / 1000;
        tv.tv_usec = (ms % 1000) * 1000;
        // Set timeout via SO_RCVTIMEO/SO_SNDTIMEO only.
        // Do NOT switch to non-blocking mode — WASI sockets handle
        // connect EINPROGRESS via poll only when blocking=true, and
        // recv/send rely on SO_RCVTIMEO for timeout in blocking mode.
        int r = setsockopt(self->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));
        RAISE_ERRNO(r, errno);
        r = setsockopt(self->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(struct timeval));
        RAISE_ERRNO(r, errno);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_settimeout_obj, socket_settimeout);

static mp_obj_t socket_makefile(size_t n_args, const mp_obj_t *args) {
    // TODO: CPython explicitly says that closing returned object doesn't close
    // the original socket (Python2 at all says that fd is dup()ed). But we
    // save on the bloat.
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_obj_t *new_args = alloca(n_args * sizeof(mp_obj_t));
    memcpy(new_args + 1, args + 1, (n_args - 1) * sizeof(mp_obj_t));
    new_args[0] = MP_OBJ_NEW_SMALL_INT(self->fd);
    return mp_vfs_open(n_args, new_args, (mp_map_t *)&mp_const_empty_map);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_makefile_obj, 1, 3, socket_makefile);

static mp_obj_t socket_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type_in;
    (void)n_kw;

    int family = AF_INET;
    int type = SOCK_STREAM;
    int proto = 0;

    if (n_args > 0) {
        family = mp_obj_get_int(args[0]);
        if (n_args > 1) {
            type = mp_obj_get_int(args[1]);
            if (n_args > 2) {
                proto = mp_obj_get_int(args[2]);
            }
        }
    }

    int fd = socket(family, type, proto);
    RAISE_ERRNO(fd, errno);
    return MP_OBJ_FROM_PTR(socket_new(fd));
}

static const mp_rom_map_elem_t socket_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_fileno), MP_ROM_PTR(&socket_fileno_obj) },
    { MP_ROM_QSTR(MP_QSTR_makefile), MP_ROM_PTR(&socket_makefile_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&socket_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_bind), MP_ROM_PTR(&socket_bind_obj) },
    { MP_ROM_QSTR(MP_QSTR_listen), MP_ROM_PTR(&socket_listen_obj) },
    { MP_ROM_QSTR(MP_QSTR_accept), MP_ROM_PTR(&socket_accept_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv), MP_ROM_PTR(&socket_recv_obj) },
    { MP_ROM_QSTR(MP_QSTR_recvfrom), MP_ROM_PTR(&socket_recvfrom_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&socket_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendto), MP_ROM_PTR(&socket_sendto_obj) },
    { MP_ROM_QSTR(MP_QSTR_setsockopt), MP_ROM_PTR(&socket_setsockopt_obj) },
    { MP_ROM_QSTR(MP_QSTR_setblocking), MP_ROM_PTR(&socket_setblocking_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout), MP_ROM_PTR(&socket_settimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj) },
};

static MP_DEFINE_CONST_DICT(socket_locals_dict, socket_locals_dict_table);

static const mp_stream_p_t socket_stream_p = {
    .read = socket_read,
    .write = socket_write,
    .ioctl = socket_ioctl,
};

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_socket,
    MP_QSTR_socket,
    MP_TYPE_FLAG_NONE,
    make_new, socket_make_new,
    print, socket_print,
    protocol, &socket_stream_p,
    locals_dict, &socket_locals_dict
    );

#define BINADDR_MAX_LEN sizeof(struct in6_addr)
static mp_obj_t mod_socket_inet_pton(mp_obj_t family_in, mp_obj_t addr_in) {
    int family = mp_obj_get_int(family_in);
    byte binaddr[BINADDR_MAX_LEN];
    int r = inet_pton(family, mp_obj_str_get_str(addr_in), binaddr);
    RAISE_ERRNO(r, errno);
    if (r == 0) {
        mp_raise_OSError(MP_EINVAL);
    }
    int binaddr_len = 0;
    switch (family) {
        case AF_INET:
            binaddr_len = sizeof(struct in_addr);
            break;
        case AF_INET6:
            binaddr_len = sizeof(struct in6_addr);
            break;
    }
    return mp_obj_new_bytes(binaddr, binaddr_len);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mod_socket_inet_pton_obj, mod_socket_inet_pton);

static mp_obj_t mod_socket_inet_ntop(mp_obj_t family_in, mp_obj_t binaddr_in) {
    int family = mp_obj_get_int(family_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(binaddr_in, &bufinfo, MP_BUFFER_READ);
    vstr_t vstr;
    vstr_init_len(&vstr, family == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN);
    if (inet_ntop(family, bufinfo.buf, vstr.buf, vstr.len) == NULL) {
        mp_raise_OSError(errno);
    }
    vstr.len = strlen(vstr.buf);
    return mp_obj_new_str_from_utf8_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mod_socket_inet_ntop_obj, mod_socket_inet_ntop);

static mp_obj_t mod_socket_getaddrinfo(size_t n_args, const mp_obj_t *args) {

    const char *host = mp_obj_str_get_str(args[0]);
    const char *serv = NULL;
    struct addrinfo hints;
    char buf[6];
    memset(&hints, 0, sizeof(hints));
    // getaddrinfo accepts port in string notation, so however
    // it may seem stupid, we need to convert int to str
    if (mp_obj_is_small_int(args[1])) {
        unsigned port = (unsigned short)mp_obj_get_int(args[1]);
        snprintf(buf, sizeof(buf), "%u", port);
        serv = buf;
        hints.ai_flags = AI_NUMERICSERV;
    } else {
        serv = mp_obj_str_get_str(args[1]);
    }

    if (n_args > 2) {
        hints.ai_family = mp_obj_get_int(args[2]);
        if (n_args > 3) {
            hints.ai_socktype = mp_obj_get_int(args[3]);
            if (n_args > 4) {
                hints.ai_protocol = mp_obj_get_int(args[4]);
                if (n_args > 5) {
                    hints.ai_flags = mp_obj_get_int(args[5]);
                }
            }
        }
    }

    struct addrinfo *addr_list;
    int res = getaddrinfo(host, serv, &hints, &addr_list);

    if (res != 0) {
        // CPython: socket.gaierror
        mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("[addrinfo error %d]"), res);
    }
    assert(addr_list);

    mp_obj_t list = mp_obj_new_list(0, NULL);
    for (struct addrinfo *addr = addr_list; addr; addr = addr->ai_next) {
        mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(5, NULL));
        t->items[0] = MP_OBJ_NEW_SMALL_INT(addr->ai_family);
        t->items[1] = MP_OBJ_NEW_SMALL_INT(addr->ai_socktype);
        t->items[2] = MP_OBJ_NEW_SMALL_INT(addr->ai_protocol);
        // "canonname will be a string representing the canonical name of the host
        // if AI_CANONNAME is part of the flags argument; else canonname will be empty." ??
        if (addr->ai_canonname) {
            t->items[3] = MP_OBJ_NEW_QSTR(qstr_from_str(addr->ai_canonname));
        } else {
            t->items[3] = mp_const_none;
        }
        t->items[4] = mp_obj_new_bytearray(addr->ai_addrlen, addr->ai_addr);
        mp_obj_list_append(list, MP_OBJ_FROM_PTR(t));
    }
    freeaddrinfo(addr_list);
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_socket_getaddrinfo_obj, 2, 6, mod_socket_getaddrinfo);

static mp_obj_t mod_socket_sockaddr(mp_obj_t sockaddr_in) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(sockaddr_in, &bufinfo, MP_BUFFER_READ);
    switch (((struct sockaddr *)bufinfo.buf)->sa_family) {
        case AF_INET: {
            struct sockaddr_in *sa = (struct sockaddr_in *)bufinfo.buf;
            mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(3, NULL));
            t->items[0] = MP_OBJ_NEW_SMALL_INT(AF_INET);
            t->items[1] = mp_obj_new_bytes((byte *)&sa->sin_addr, sizeof(sa->sin_addr));
            t->items[2] = MP_OBJ_NEW_SMALL_INT(ntohs(sa->sin_port));
            return MP_OBJ_FROM_PTR(t);
        }
        case AF_INET6: {
            struct sockaddr_in6 *sa = (struct sockaddr_in6 *)bufinfo.buf;
            mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(5, NULL));
            t->items[0] = MP_OBJ_NEW_SMALL_INT(AF_INET6);
            t->items[1] = mp_obj_new_bytes((byte *)&sa->sin6_addr, sizeof(sa->sin6_addr));
            t->items[2] = MP_OBJ_NEW_SMALL_INT(ntohs(sa->sin6_port));
            t->items[3] = MP_OBJ_NEW_SMALL_INT(ntohl(sa->sin6_flowinfo));
            t->items[4] = MP_OBJ_NEW_SMALL_INT(ntohl(sa->sin6_scope_id));
            return MP_OBJ_FROM_PTR(t);
        }
        default: {
            struct sockaddr *sa = (struct sockaddr *)bufinfo.buf;
            mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(2, NULL));
            t->items[0] = MP_OBJ_NEW_SMALL_INT(sa->sa_family);
            t->items[1] = mp_obj_new_bytes((byte *)sa->sa_data, bufinfo.len - offsetof(struct sockaddr, sa_data));
            return MP_OBJ_FROM_PTR(t);
        }
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mod_socket_sockaddr_obj, mod_socket_sockaddr);

static const mp_rom_map_elem_t mp_module_socket_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_socket) },
    { MP_ROM_QSTR(MP_QSTR_socket), MP_ROM_PTR(&mp_type_socket) },
    { MP_ROM_QSTR(MP_QSTR_getaddrinfo), MP_ROM_PTR(&mod_socket_getaddrinfo_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_pton), MP_ROM_PTR(&mod_socket_inet_pton_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_ntop), MP_ROM_PTR(&mod_socket_inet_ntop_obj) },
    { MP_ROM_QSTR(MP_QSTR_sockaddr), MP_ROM_PTR(&mod_socket_sockaddr_obj) },

#define C(name) { MP_ROM_QSTR(MP_QSTR_##name), MP_ROM_INT(name) }
    #ifdef AF_UNIX
    C(AF_UNIX),
    #endif
    C(AF_INET),
    C(AF_INET6),
    C(SOCK_STREAM),
    C(SOCK_DGRAM),
    #ifdef SOCK_RAW
    C(SOCK_RAW),
    #endif

    #ifdef MSG_DONTROUTE
    C(MSG_DONTROUTE),
    #endif
    #ifdef MSG_DONTWAIT
    C(MSG_DONTWAIT),
    #endif
    C(MSG_PEEK),

    C(SOL_SOCKET),
    #ifdef SO_BROADCAST
    C(SO_BROADCAST),
    #endif
    C(SO_ERROR),
    C(SO_KEEPALIVE),
    #ifdef SO_LINGER
    C(SO_LINGER),
    #endif
    C(SO_REUSEADDR),

    #ifdef IP_ADD_MEMBERSHIP
    C(IP_ADD_MEMBERSHIP),
    #endif
    #ifdef IP_DROP_MEMBERSHIP
    C(IP_DROP_MEMBERSHIP),
    #endif
#undef C
};

static MP_DEFINE_CONST_DICT(mp_module_socket_globals, mp_module_socket_globals_table);

const mp_obj_module_t mp_module_socket = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_socket_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_socket, mp_module_socket);

#endif // MICROPY_PY_SOCKET
