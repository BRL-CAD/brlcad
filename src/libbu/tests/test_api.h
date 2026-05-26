/*                   T E S T _ A P I . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef LIBBU_TESTS_TEST_API_H
#define LIBBU_TESTS_TEST_API_H

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"


#define ARRAY_LEN(_a) (sizeof(_a) / sizeof((_a)[0]))


struct test_api_log_capture {
    struct bu_vls text;
    struct bu_hook_list saved;
};


static void
test_api_fail(int *errors, const char *file, int line, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "%s:%d: ", file, line);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    (*errors)++;
}


#define TEST_API_CHECK(_cond, ...) \
    do { \
	if (!(_cond)) { \
	    test_api_fail(&errors, __FILE__, __LINE__, __VA_ARGS__); \
	} \
    } while (0)


static int
test_api_close_enough(double left, double right, double rel_tol, double abs_tol)
{
    double diff = fabs(left - right);
    double scale = fabs(left) > fabs(right) ? fabs(left) : fabs(right);

    if (scale < 1.0) {
	scale = 1.0;
    }

    return diff <= (abs_tol + rel_tol * scale);
}


static int
test_api_text_contains(const char *text, const char *needle)
{
    return text && needle && strstr(text, needle) != NULL;
}


static int
test_api_capture_log_hook(void *data, void *msg)
{
    struct bu_vls *v = (struct bu_vls *)data;

    if (msg) {
	bu_vls_strcat(v, (const char *)msg);
    }

    return 0;
}


static void
test_api_log_capture_begin(struct test_api_log_capture *capture)
{
    capture->saved.size = 0;
    capture->saved.capacity = 0;
    capture->saved.hooks = NULL;
    bu_vls_init(&capture->text);

    bu_log_hook_save_all(&capture->saved);
    bu_log_hook_delete_all();
    bu_log_add_hook(test_api_capture_log_hook, &capture->text);
}


static void
test_api_log_capture_end(struct test_api_log_capture *capture)
{
    bu_log_hook_delete_all();
    bu_log_hook_restore_all(&capture->saved);
    bu_hook_delete_all(&capture->saved);
}


static void
test_api_log_capture_free(struct test_api_log_capture *capture)
{
    bu_vls_free(&capture->text);
}

#endif
