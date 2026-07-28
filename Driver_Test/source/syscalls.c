/*
 * syscalls.c — newlib 系统调用桩 (弱符号, 可被外部重写)
 *
 * -lc 引入的函数需要这些底层实现, 裸机环境下全部提供最小实现防止链接失败.
 */

#include <sys/types.h>
#include <errno.h>

#undef errno
extern int errno;


void __attribute__((weak)) _exit(int status)
{
    (void)status;
    for (;;);
}

int __attribute__((weak)) _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int __attribute__((weak)) _getpid(void)
{
    return 1;
}

void *__attribute__((weak)) _sbrk(int incr)
{
    extern char _end;
    static unsigned long heap_end;
    unsigned long prev;

    if (heap_end == 0)
        heap_end = (unsigned long)&_end;

    prev = heap_end;
    heap_end += incr;
    return (void *)prev;
}

int __attribute__((weak)) _close(int fd)
{
    (void)fd;
    errno = EBADF;
    return -1;
}

int __attribute__((weak)) _lseek(int fd, int offset, int whence)
{
    (void)fd;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return -1;
}

int __attribute__((weak)) _read(int fd, void *buf, int count)
{
    (void)fd;
    (void)buf;
    (void)count;
    errno = ENOSYS;
    return -1;
}

int __attribute__((weak)) _write(int fd, const void *buf, int count)
{
    (void)fd;
    (void)buf;
    (void)count;
    errno = ENOSYS;
    return -1;
}

int __attribute__((weak)) _fstat(int fd, void *st)
{
    (void)fd;
    (void)st;
    errno = EBADF;
    return -1;
}

int __attribute__((weak)) _isatty(int fd)
{
    (void)fd;
    return 1;
}
