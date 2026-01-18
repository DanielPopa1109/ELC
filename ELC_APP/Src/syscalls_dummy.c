#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

int _write(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    return len;          // report "success" to printf/newlib
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    errno = ENOSYS;      // no input
    return -1;
}

int _close(int file)
{
    (void)file;
    return 0;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

// Optional but good to have for completeness if something drags these in:

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}
