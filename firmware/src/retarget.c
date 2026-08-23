/*
 * retarget.c — sends printf() to the USB CDC console.
 *
 * Built against newlib-nano (--specs=nano.specs), so only _write matters;
 * the remaining syscalls come from nosys.specs. Output is dropped rather
 * than blocked when no host is listening, so a board sitting on a bench
 * with no cable behaves exactly like one with a terminal attached.
 */

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
        return len;             /* pretend success: nothing is listening */

    int written = 0;
    while (written < len) {
        size_t n = cdc_write(buf + written, (size_t)(len - written));
        if (n == 0u) {
            /*
             * Ring is full. Give the USB interrupt a chance to drain it,
             * but never spin forever — if the host stopped reading we drop
             * the rest instead of hanging the application.
             */
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

/* Line-buffering would hold output back until a newline; a console wants
 * bytes to appear as they are produced. */
void retarget_init(void)
{
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);
}

/* ------------------------------------------------------------------------ */
/* Minimal syscall set                                                       */
/* ------------------------------------------------------------------------ */
/*
 * Supplied here instead of linking libnosys, which pulls in stubs that emit
 * "not implemented and will always fail" warnings at link time for calls
 * this firmware never makes.
 */

int _close(int fd)                       { (void)fd; return -1; }
int _lseek(int fd, int off, int whence)  { (void)fd; (void)off; (void)whence; return 0; }
int _isatty(int fd)                      { (void)fd; return 1; }
int _getpid(void)                        { return 1; }
int _kill(int pid, int sig)              { (void)pid; (void)sig; errno = EINVAL; return -1; }

int _fstat(int fd, struct stat *st)
{
    (void)fd;
    st->st_mode = S_IFCHR;      /* character device: unbuffered, no seeking */
    return 0;
}

void _exit(int status)
{
    (void)status;
    __asm volatile ("cpsid i");
    for (;;)
        __asm volatile ("wfi");
}

/*
 * Heap allocator. printf's float formatting can call malloc, so a working
 * _sbrk is required — but it must refuse to grow into the stack rather than
 * silently corrupt it.
 */
void *_sbrk(ptrdiff_t incr)
{
    extern char _end;           /* start of the heap, from the linker script */
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
