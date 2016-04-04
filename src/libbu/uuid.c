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


/* interface header */
#include "bu/uuid.h"

/* system headers */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


int
bu_uuid_create(uint8_t uuid[STATIC_ARRAY(16)], size_t nbytes, uint8_t *bytes)
{
    int type = 4; /* random */

    if (nbytes > 0 && bytes)
	type = 5;

    memset(uuid, 0, sizeof(uint8_t) * 16);

    switch (type) {
	case 4: {
	    size_t i;
#ifdef HAVE_UUID_GENERATE
	    uuid_t generated_uuid;
	    uuid_generate(generated_uuid);
	    for (i=0; i<16; i++)
		uuid[i] = (uint8_t)generated_uuid[i];
#else
	    for (i=0; i< 16; i++) {
		uuid[i] = drand48() * 0xFF + 0.5;
	    }
	    /* set the UUIDv4 reserved bits */
	    uuid[6] = (uuid[6] & 0x0F) | 0x40;
	    uuid[7] = (uuid[7] & 0xFF) | 0x00;
	    uuid[8] = (uuid[8] & 0x3F) | 0x80;
#endif /* HAVE_UUID_GENERATE */
	    break;
	}
    }
    /* FIXME: create the UUID */

    return type;
}


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


int
bu_uuid_decode(const char *cp, uint8_t uuid[STATIC_ARRAY(16)])
{
    if (!cp)
	return 1;

    while (*cp) {
	const char *cpp = cp;
	cp++;

	switch (*cpp) {
	    case '{':
	    case '}':
	    case '-':
		continue;
	    default:
		/* FIXME: decode properly here */
		uuid[0] = *cpp;
		break;
	}
    }

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
