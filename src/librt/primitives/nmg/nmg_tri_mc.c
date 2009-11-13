/*                     N M G _ T R I _ M C . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2009 United States Government as represented by
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
/** @addtogroup nmg */
/** @{ */
/** @file nmg_tri_mc.c
 *
 *  Triangulate the faces of a polygonal NMG using the marching cubes
 *  algorithm.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "plot3.h"

void
nmg_triangulate_model_mc(struct model *m, const struct bn_tol *tol)
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;

    BN_CK_TOL(tol);
    NMG_CK_MODEL(m);
    nmg_vmodel(m);

    if (rt_g.NMG_debug & DEBUG_TRI)
	bu_log("Triangulating NMG\n");


    if (rt_g.NMG_debug & DEBUG_TRI)
	bu_log("Triangulation completed\n");
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
