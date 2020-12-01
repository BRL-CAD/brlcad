#ifndef __SC_STDIO_H
#define __SC_STDIO_H

/*
 * https://stackoverflow.com/questions/2915672/snprintf-and-visual-studio-2010
 * (NOTE: MSVC defines va_list as a char*, so this should be safe)
 */
#if defined(_MSC_VER) && _MSC_VER < 1900

#include <stdarg.h>

static __inline int
c99_vsnprintf(char *buffer, size_t sz, const char *format, va_list ap) {
    int count = -1;

    if (sz != 0)
        count = _vsnprintf_s(buffer, sz, _TRUNCATE, format, ap);
    if (count == -1)
        count = _vscprintf(format, ap);

    return count;
}

static __inline int
c99_snprintf(char *buffer, size_t sz, const char *format, ...) {
    int count;
    va_list ap;

    va_start(ap, format);
    count = c99_vsnprintf(buffer, sz, format, ap);
    va_end(ap);

    return count;
}

#endif

#endif /* __SC_STDIO_H */

