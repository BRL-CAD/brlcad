/*                   L S E G _ L S E G . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2022 United States Government as represented by
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

#include "bu.h"
#include "bg.h"

int
main(int argc, char **argv)
{
    int test_num = 0;

    bu_setprogname(argv[0]);

    if (argc != 2)
	bu_exit(1, "ERROR: [%s] input format is: test_number\n", argv[0]);

    sscanf(argv[1], "%d", &test_num);

    if (test_num == 1) {
	point_t P0, P1, Q0, Q1;
	point_t c0, c1;
	double dist = -1.0;

	VSET(P0, 3.41819463201248075,7.64533159204993584,22.99994836139265786);
	VSET(P1, 3.52252929469979303,7.79661877760347455,22.99995349026097102);

	VSET(Q0, 3.44524589136147830,7.67444957869714628,22.91984784277452647);
	VSET(Q1, 3.56555225936148323,7.98564760063074353,23.37334866995422900);

	dist = sqrt(bg_distsq_lseg3_lseg3(&c0, &c1, P0, P1, Q0, Q1));

	if (dist < 0) {
	    bu_exit(-1, "Fatal error - mesh validity test failed\n");
	} else {
	    bu_log("Distance: %f\n", dist);
	    bu_log("c0      : %f %f %f\n", V3ARGS(c0));
	    bu_log("c1      : %f %f %f\n", V3ARGS(c1));
	}

	return 0;
    }

    bu_log("Error: unknown test number %d\n", test_num);
    return -1;
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
