/*                          U U I D . C
 * BRL-CAD
 *
 * Copyright (c) 2016 United States Government as represented by
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

#include "common.h"

#include "string.h" /* memcmp */

/* interface header */
#include "bu/uuid.h"

/* system headers */
#include <stdio.h>
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif


int
bu_uuid_compare(const void *uuid_left, const void *uuid_right)
{
    uint8_t *left = (uint8_t *)uuid_left;
    uint8_t *right = (uint8_t *)uuid_right;
    if (!left)
	return (right ? -1 : 0);
    if (!right)
	return 1;

    return memcmp(left, right, sizeof(uint8_t) * 16);
}


int
bu_uuid_encode(const uint8_t uuid[STATIC_ARRAY(16)], uint8_t cp[STATIC_ARRAY(37)])
{
    snprintf((char *)cp, 37, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
	     uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
	     uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
