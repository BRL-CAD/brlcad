/*                  T E S T _ B O T T E S S . C
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
/** @file libgcv/bottess_tester.c
 *
 * Test things in the bottess routines...
 */

#include "common.h"

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "nmg.h"
#include "gcv.h"

#include "./soup.h"
#include "./tri_intersect.h"

int
test_intersection(int should, point_t *t1, point_t *t2, point_t p1, point_t p2)
{
    int coplanar = 0;
    point_t i[2] = {{0,0,0},{0,0,0}};
    vect_t tmp[2] = {{0,0,0},{0,0,0}};
    struct face_s f[2];
    struct bn_tol tol;

    BN_TOL_INIT(&tol);
    tol.dist = 0.005;
    tol.dist_sq = tol.dist*tol.dist;

    VMOVE(f[0].vert[0], t1[0]);
    VMOVE(f[0].vert[1], t1[1]);
    VMOVE(f[0].vert[2], t1[2]);
    VSUB2(tmp[0], t1[1], t1[0]);
    VSUB2(tmp[1], t1[2], t1[0]);
    VCROSS(f[0].plane,tmp[0], tmp[1]); 
    f[0].foo = 0;

    VMOVE(f[1].vert[0], t2[0]);
    VMOVE(f[1].vert[1], t2[1]);
    VMOVE(f[1].vert[2], t2[2]);
    VSUB2(tmp[0], t2[1], t2[0]);
    VSUB2(tmp[1], t2[2], t2[0]);
    VCROSS(f[1].plane,tmp[0], tmp[1]); 
    f[1].foo = 0;

    if( gcv_tri_tri_intersect_with_isectline(NULL,NULL, f, f+1, &coplanar, (point_t *)&i[0], &tol) == 0 && should == 0)
	return 0;
    return !(VNEAR_EQUAL(i[0], p1, tol.dist) && VNEAR_EQUAL(i[1], p2, tol.dist));
}

int
test_tri_intersections()
{
    int i = 0;
    point_t t1[3], t2[3], p1, p2;

    VSET(t1[0], 0, 0, 0);
    VSET(t1[1], 0, 1, 0);
    VSET(t1[2], 0, 0, 1);
    VSET(t2[0],-1, 0, 0.5);
    VSET(t2[1], 0, 1, 0.5);
    VSET(t2[2], 1, 0, 0.5);
    VSET(p1, 0, 0, .5)
    VSET(p2, 0, .5, .5)
    i += test_intersection(1, t1, t2, p1, p2);

    VSET(t1[0], 0, 0, 0);
    VSET(t1[1], 0, 1, 0);
    VSET(t1[2], 0, 0, 1);
    VSET(t2[0],-1, 2, 0.5);
    VSET(t2[1], 0, 3, 0.5);
    VSET(t2[2], 1, 2, 0.5);
    VSET(p1, 0, 0, .5)
    VSET(p2, 0, .5, .5)
    i += test_intersection(0, t1, t2, p1, p2);

    return i;
}

int
main(void)
{
    printf("%d failures\n", test_tri_intersections());
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
