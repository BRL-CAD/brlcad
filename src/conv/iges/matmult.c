/*                       M A T M U L T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
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

#include "vmath.h"

void Matmult(mat_t a, mat_t b, mat_t c)
{
    mat_t tmp;
    int i, j, k;

    for (i=0; i<4; i++)
	for (j=0; j<4; j++) {
	    tmp[i*4+j] = 0.0;
	    for (k=0; k<4; k++)
		tmp[i*4+j] += a[i*4+k] * b[k*4+j];
	}
    for (i=0; i<4; i++)
	for (j=0; j<4; j++)
	    c[i*4+j] = tmp[i*4+j];
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
