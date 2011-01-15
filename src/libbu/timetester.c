/*                       T I M E T E S T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"

int
main(int argc, char **argv)
{
    int64_t time1, time2;
    int i = 0;
    unsigned long counter = 1;

    if (argc > 1)
	bu_exit(1, "ERROR: Unexpected parameter [%s]\n", argv[0]);

    time1 = bu_gettime();
    while (i < 1.0e6) {
	counter++;
	time2 = bu_gettime();
	i = time2 - time1;
    }
    bu_log("Called bu_gettime() %lu times\n", counter);
    bu_log("Time delta: %d\n", i);
    bu_log("time1: %lu\n", (unsigned long)time1);
    bu_log("time2: %lu\n", (unsigned long)time2);

    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
