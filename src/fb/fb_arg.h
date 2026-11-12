/*                        F B _ A R G . H
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

#ifndef FB_ARG_H
#define FB_ARG_H

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/log.h"

static inline int
fb_arg_report(int valid, struct bu_vls *msg, const char *label)
{
    if (!valid)
	bu_log("%s: %s: %s\n", bu_getprogname(), label, bu_vls_cstr(msg));
    bu_vls_free(msg);
    return valid;
}

static inline int
fb_arg_int(const char *arg, int *value, const char *label)
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    return fb_arg_report(bu_arg_integer_from_str(&msg, value, arg, 10), &msg, label);
}

static inline int
fb_arg_int_range(const char *arg, int *value, int minimum, int maximum, const char *label)
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    return fb_arg_report(bu_arg_integer_range_from_str(&msg, value, arg, 10,
	minimum, maximum), &msg, label);
}

static inline int
fb_arg_size_t_range(const char *arg, size_t *value, size_t minimum, size_t maximum,
    const char *label)
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    return fb_arg_report(bu_arg_size_t_range_from_str(&msg, value, arg, 10,
	minimum, maximum), &msg, label);
}

static inline int
fb_arg_uchar(const char *arg, unsigned char *value, const char *label)
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    return fb_arg_report(bu_arg_unsigned_char_from_str(&msg, value, arg, 10),
	&msg, label);
}

static inline int
fb_arg_double(const char *arg, double *value, const char *label)
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    return fb_arg_report(bu_arg_double_from_str(&msg, value, arg), &msg, label);
}

#endif /* FB_ARG_H */
