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

int soup_add_face(struct soup_s *s, point_t a, point_t b, point_t c, const struct bn_tol *tol);
int split_face_single(struct soup_s *s, unsigned long int fid, point_t isectpt[2], struct face_s *opp_face, const struct bn_tol *tol);

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
    int count = 0;
#define TRY(suc, t00x,t00y,t00z, t01x,t01y,t01z, t02x,t02y,t02z, t10x,t10y,t10z, t11x,t11y,t11z, t12x,t12y,t12z, p0x,p0y,p0z, p1x,p1y,p1z) { \
    point_t t0[3], t1[3], p0, p1; \
    VSET(t0[0],t00x,t00y,t00z); VSET(t0[1],t01x,t01y,t01z); VSET(t0[2],t02x,t02y,t02z); \
    VSET(t1[0],t10x,t10y,t10z); VSET(t1[1],t11x,t11y,t11z); VSET(t1[2],t12x,t12y,t12z); \
    VSET(p0,p0x,p0y,p0z); VSET(p1,p1x,p1y,p1z) \
    count += test_intersection(suc, t0, t1, p0, p1); }

    TRY(1,0,0,0,0,1,0,0,0,1,-1,0,0.5,0,1,0.5,1,0,0.5,0,0,.5,0,.5,.5);	/* ep ef */
    TRY(0,0,0,0,0,1,0,0,0,1,-1,2,0.5,0,3,0.5,1,2,0.5,0,0,.5,0,.5,.5);	/* no intersect */

    return count;
}

static int
find_tri(struct soup_s *s, struct face_s *f, struct bn_tol *t) {
    unsigned int i, j, k;
    for(i=0;i<s->nfaces;i++) {
	int found[3] = {0,0,0};
	struct face_s *wf = s->faces+i;

	for(j=0;j<3;j++) for(k=0;k<3;k++) if(VNEAR_EQUAL( wf->vert[j], f->vert[k], t->dist)) found[j] = 1;
	if(found[0] == 1 && found[1] == 1 && found[2] == 1) return i;
    }
    return -1;
}

int
test_face_split_single()
{
    unsigned long int i, count = 0, nsplt;
    int urf[2];
    struct soup_s s;
    point_t isectpt[2];
    struct face_s f;
    struct bn_tol t;

    BN_TOL_INIT(&t);
    t.dist = 0.005;
    t.dist_sq = t.dist * t.dist;

    s.magic = SOUP_MAGIC;
    s.faces = NULL;
    s.maxfaces = 0;
    s.nfaces = 0;
    VSET(f.vert[0], -1, 0, 0); VSET(f.vert[1], 0, 1, 0); VSET(f.vert[2], 1, 0, 0);
    soup_add_face(&s, V3ARGS(f.vert), &t);
    VSET(f.plane, 0, 0, 1);
    VSET(isectpt[0], 0, 0, 0);
    VSET(isectpt[1], 0, 1, 0);
    nsplt = split_face_single(&s, 0, isectpt, &f, &t);
    if(nsplt != s.nfaces) { printf("Errr, nsplit %lu != s.nfaces %lu ?\n", nsplt, s.nfaces); }
    VSET(f.vert[0], 0, 0, 0); VSET(f.vert[1], 0, 1, 0); VSET(f.vert[2], -1, 0, 0); urf[0] = find_tri(&s, &f, &t);
    VSET(f.vert[0], 0, 0, 0); VSET(f.vert[1], 0, 1, 0); VSET(f.vert[2], 1, 0, 0); urf[1] = find_tri(&s, &f, &t);
    if(nsplt != 2 && urf[0] == -1 && urf[1] == -1) {
	printf("\033[1;31mFAILURE\033[m\n");
	printf("%lu faces now\n", s.nfaces);
	for(i=0;i<s.nfaces;i++)
	    printf("%03lu: % 2g,% 2g,% 2g | % 2g,% 2g,% 2g | % 2g,% 2g,% 2g\n", i, V3ARGS(s.faces[i].vert[0]), V3ARGS(s.faces[i].vert[1]), V3ARGS(s.faces[i].vert[2]));
	count++;
    }

    return (int)count;
}

int
test_face_splits()
{
    int count = 0;

    return count;
}

int
main(void)
{
    printf("TRI INTERSECTION: %d failures\n", test_tri_intersections());
    printf("SINGL FACE SPLIT: %d failures\n", test_face_split_single());
    printf("FACE SPLITTING  : %d failures\n", test_face_splits());
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
