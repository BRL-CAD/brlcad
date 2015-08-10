/*                      R E A D _ M A T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file nirt/read_mat.c
 *
 * This program is Natalie's Interactive Ray-Tracer
 *
 * Author:
 *   Natalie L. Barker
 *
 * Date:
 *   Jan 90
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "raytrace.h"

#include "./nirt.h"
#include "./usrfmt.h"


#define			RMAT_SAW_EYE	0x01
#define			RMAT_SAW_ORI	0x02
#define			RMAT_SAW_VR	0x02

extern outval			ValTab[];
extern int			nirt_debug;

void read_mat (struct rt_i *rtip)
{
    double scan[16] = MAT_INIT_ZERO;
    char	*buf;
    int		status = 0x0;
    mat_t	m;
    mat_t       q;

    while ((buf = rt_read_cmd(stdin)) != (char *) 0) {
	if (bu_strncmp(buf, "eye_pt", 6) == 0) {
	    if (sscanf(buf + 6, "%lf%lf%lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
		bu_exit(1, "nirt: read_mat(): Failed to read eye_pt\n");
	    }
	    target(X) = scan[X];
	    target(Y) = scan[Y];
	    target(Z) = scan[Z];
	    status |= RMAT_SAW_EYE;
	} else if (bu_strncmp(buf, "orientation", 11) == 0) {
	    if (sscanf(buf + 11,
		       "%lf%lf%lf%lf",
		       &scan[X], &scan[Y], &scan[Z], &scan[W]) != 4) {
		bu_exit(1, "nirt: read_mat(): Failed to read orientation\n");
	    }
	    MAT_COPY(q, scan);
	    quat_quat2mat(m, q);
	    if (nirt_debug & DEBUG_MAT)
		bn_mat_print("view matrix", m);
	    azimuth() = atan2(-m[0], m[1]) / DEG2RAD;
	    elevation() = atan2(m[10], m[6]) / DEG2RAD;
	    status |= RMAT_SAW_ORI;
	} else if (bu_strncmp(buf, "viewrot", 7) == 0) {
	    if (sscanf(buf + 7,
		       "%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf%lf",
		       &scan[0], &scan[1], &scan[2], &scan[3],
		       &scan[4], &scan[5], &scan[6], &scan[7],
		       &scan[8], &scan[9], &scan[10], &scan[11],
		       &scan[12], &scan[13], &scan[14], &scan[15]) != 16) {
		bu_exit(1, "nirt: read_mat(): Failed to read viewrot\n");
	    }
	    MAT_COPY(m, scan);
	    if (nirt_debug & DEBUG_MAT)
		bn_mat_print("view matrix", m);
	    azimuth() = atan2(-m[0], m[1]) / DEG2RAD;
	    elevation() = atan2(m[10], m[6]) / DEG2RAD;
	    status |= RMAT_SAW_VR;
	}
    }

    if ((status & RMAT_SAW_EYE) == 0) {
	bu_exit(1, "nirt: read_mat(): Was given no eye_pt\n");
    }
    if ((status & (RMAT_SAW_ORI | RMAT_SAW_VR)) == 0) {
	bu_exit(1, "nirt: read_mat(): Was given no orientation or viewrot\n");
    }

    direct(X) = -m[8];
    direct(Y) = -m[9];
    direct(Z) = -m[10];

    dir2ae();

    targ2grid();
    shoot("", 0, rtip);
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
