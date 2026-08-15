/*                    M O V E _ A R B . H
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */
/** @file libged/move_arb.h
 *
 * Private shared validation for ARB edge and face editing commands.
 */

#ifndef LIBGED_MOVE_ARB_H
#define LIBGED_MOVE_ARB_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "bu/vls.h"


static int
ged_arb_vector3_parse(const char *arg, double values[3])
{
    const char *cursor = arg;

    if (!cursor)
	return -1;
    for (size_t i = 0; i < 3; i++) {
	char *end = NULL;
	double value;

	while (*cursor && isspace((unsigned char)*cursor))
	    cursor++;
	if (!*cursor)
	    return -1;
	errno = 0;
	value = strtod(cursor, &end);
	if (end == cursor || errno == ERANGE || !isfinite(value))
	    return -1;
	values[i] = value;
	cursor = end;
    }
    while (*cursor && isspace((unsigned char)*cursor))
	cursor++;
    return *cursor ? -1 : 0;
}


static int
ged_arb_vector3_validate(struct bu_vls *msg, const char *arg)
{
    double values[3];

    if (ged_arb_vector3_parse(arg, values) == 0)
	return 0;
    if (msg)
	bu_vls_printf(msg, "expected exactly three finite coordinates");
    return -1;
}


static int
ged_arb_positive_integer_validate(struct bu_vls *msg, const char *arg)
{
    char *end = NULL;
    long value;

    if (!arg)
	return -1;
    errno = 0;
    value = strtol(arg, &end, 0);
    if (end && end != arg && *end == '\0' && errno != ERANGE &&
	value > 0 && value <= INT_MAX)
	return 0;
    if (msg)
	bu_vls_printf(msg, "expected a positive one-based index");
    return -1;
}

#endif /* LIBGED_MOVE_ARB_H */
