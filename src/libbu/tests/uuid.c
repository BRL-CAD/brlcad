/*                       U U I D . C
 * BRL-CAD
 *
 * Copyright (c) 2016-2021 United States Government as represented by
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

#include <stdio.h>

#include "bu/app.h"
#include "bu/uuid.h"
#include "bu/str.h"
#include "bu/exit.h"


int
main(int ac, char *av[])
{
    int test = 0;
    uint8_t uuid[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
			0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    char uuidstr[39] = {0};

    bu_setprogname(av[0]);

    if (ac > 1 && av)
	test = atoi(av[1]);

    switch(test) {
	case 0:
	    (void)bu_uuid_encode(uuid, (uint8_t *)(uuidstr+1));
	    uuidstr[0] = '{';
	    uuidstr[sizeof(uuidstr)-2] = '}';
	    uuidstr[sizeof(uuidstr)-1] = '\0';

	    if (!bu_strcmp(uuidstr, "{00112233-4455-6677-8899-00AABBCCDDEEFF}"))
		return 1;
	    return 0;
	default:
	    bu_exit(1, "ERROR: unrecognized test number\n");
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
