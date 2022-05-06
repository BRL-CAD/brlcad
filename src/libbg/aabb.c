/*                         A A B B . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file aabb.c
 *
 * Bounding box calculations
 *
 */

#include "common.h"
#include <math.h>
#include "vmath.h"
#include "bg/aabb.h"

int
bg_tor_bbox(point_t *min, point_t *max, struct bg_torus *t) {
    vect_t P, w1;       /* for RPP calculation */
    fastf_t f;

   /* Compute the bounding RPP planes for a circular torus.
    *
    * Given a circular torus with vertex V, vector N, and radii r1
    * and r2.  A bounding plane with direction vector P will touch
    * the surface of the torus at the points:
    *
    * V +/- [r2 + r1 * |N x P|] P
    */
   /* X */
   VSET(P, 1.0, 0, 0);          /* bounding plane normal */
   VCROSS(w1, t->h, P);       /* for sin(angle N P) */
   f = t->r_h + t->r_a * MAGNITUDE(w1);
   VSCALE(w1, P, f);
   f = fabs(w1[X]);
   (*min)[X] = t->v[X] - f;
   (*max)[X] = t->v[X] + f;

   /* Y */
   VSET(P, 0, 1.0, 0);          /* bounding plane normal */
   VCROSS(w1, t->h, P);       /* for sin(angle N P) */
   f = t->r_h + t->r_a * MAGNITUDE(w1);
   VSCALE(w1, P, f);
   f = fabs(w1[Y]);
   (*min)[Y] = t->v[Y] - f;
   (*max)[Y] = t->v[Y] + f;

   /* Z */
   VSET(P, 0, 0, 1.0);          /* bounding plane normal */
   VCROSS(w1, t->h, P);       /* for sin(angle N P) */
   f = t->r_h + t->r_a * MAGNITUDE(w1);
   VSCALE(w1, P, f);
   f = fabs(w1[Z]);
   (*min)[Z] = t->v[Z] - f;
   (*max)[Z] = t->v[Z] + f;

   return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
