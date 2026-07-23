#include "common.h"
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

void exit_with_error(const char *fmt, ...) {
    int errno_save = errno;
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);

    if (errno_save != 0) {
        fprintf(stderr, "(errno = %d) : %s\n", errno_save, strerror(errno_save));
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    va_end(ap);

    exit(1);
}