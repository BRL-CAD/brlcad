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
 * Triangulate the faces of a polygonal NMG using the marching cubes
 * algorithm.
 *
 */
/** @} */

/* rough game plan
 *   [ ] develop/proof metaball primitive tesselation using MC
 *       [ ] asc-g/g-asc of metaballs for regression/comparison testing
 *           [X] asc-g
 *           [X] g-asc
 *	     [ ] regression shtuff
 *       [ ] edge solve cubes
 *       [ ] write compiled table shtuff (use existing table, the hex/index one?)
 *       [ ] produce NMG mesh
 *       [ ] experiment with decimating
 *   [ ] hoist code where final cube edge intersections is known
 *   [ ] implement ray-firing to find edge intersects
 *   [ ] hoise table stuff
 *   [ ] hoist NMG mesh writer.
 *   [ ] try decimation?
 *   [ ] ???
 *   [ ] profit!
 *   [ ] explore optimizations if time left?
 *   [ ] compare old nmg tesselator for accuracy/performance/robustness?
 */

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
