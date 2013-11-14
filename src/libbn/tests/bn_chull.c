/*                      B N _ C H U L L . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
#include "vmath.h"
#include "bn.h"


int
main()
{
    /* FIXME: finish this */
    point_t pnts[4+1] = {{0}};
    int n = 4;
    point_t **hull = NULL;

    VSET(pnts[0], 1.5, 1.5, 0.0);
    VSET(pnts[1], 3.0, 2.0, 0.0);
    VSET(pnts[2], 2.0, 2.5, 0.0);
    VSET(pnts[3], 1.0, 2.0, 0.0);

    bn_2D_hull(hull, (const point_t *)pnts, n);

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

