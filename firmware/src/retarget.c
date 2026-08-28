#include "usb_cdc.h"
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stddef.h>

int _write(int fd, const char *buf, int len)
{
    if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        errno = EBADF;
        return -1;
    }

    if (!cdc_is_configured())
        return len;

    int written = 0;
    while (written < len) {
        size_t n = cdc_write(buf + written, (size_t)(len - written));
        if (n == 0u) {
            cdc_flush();
            for (volatile int spin = 0; spin < 20000; spin++)
                __asm volatile ("nop");

            if (cdc_write(buf + written, 1u) == 0u)
                break;
            written++;
            continue;
        }
        written += (int)n;
    }

    cdc_flush();
    return written;
}

int _read(int fd, char *buf, int len)
{
    if (fd != STDIN_FILENO) {
        errno = EBADF;
        return -1;
    }
    return (int)cdc_read(buf, (size_t)len);
}

void retarget_init(void)
{
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);
}

int _close(int fd)                       { (void)fd; return -1; }
int _lseek(int fd, int off, int whence)  { (void)fd; (void)off; (void)whence; return 0; }
int _isatty(int fd)                      { (void)fd; return 1; }
int _getpid(void)                        { return 1; }
int _kill(int pid, int sig)              { (void)pid; (void)sig; errno = EINVAL; return -1; }

int _fstat(int fd, struct stat *st)
{
    (void)fd;
    st->st_mode = S_IFCHR;
    return 0;
}

void _exit(int status)
{
    (void)status;
    __asm volatile ("cpsid i");
    for (;;)
        __asm volatile ("wfi");
}

void *_sbrk(ptrdiff_t incr)
{
    extern char _end;
    extern char _estack;
    extern char _stack_size;

    static char *brk;
    if (brk == 0)
        brk = &_end;

    char *limit = &_estack - (ptrdiff_t)&_stack_size;

    if (brk + incr > limit) {
        errno = ENOMEM;
        return (void *)-1;
    }

    char *prev = brk;
    brk += incr;
    return prev;
}
