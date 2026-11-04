/**
 *                 S E C U R E _ C R T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CREO_BRL_SECURE_CRT_H
#define CREO_BRL_SECURE_CRT_H

#ifndef _WIN32

#  include <stdarg.h>
#  include <stdio.h>

static inline int
vsprintf_s(char *buffer, size_t buffer_size, const char *format, va_list arguments)
{
    int result;

    if (!buffer || buffer_size == 0 || !format)
        return -1;

    result = vsnprintf(buffer, buffer_size, format, arguments);
    if (result < 0 || (size_t)result >= buffer_size) {
        buffer[0] = '\0';
        return -1;
    }

    return result;
}


static inline int
sprintf_s(char *buffer, size_t buffer_size, const char *format, ...)
{
    int result;
    va_list arguments;

    va_start(arguments, format);
    result = vsprintf_s(buffer, buffer_size, format, arguments);
    va_end(arguments);

    return result;
}

#endif

#endif /* CREO_BRL_SECURE_CRT_H */
