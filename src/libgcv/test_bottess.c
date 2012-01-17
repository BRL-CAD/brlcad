/*                  T E S T _ B O T T E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2012 United States Government as represented by
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
    point_t i[2] = {VINIT_ZERO, VINIT_ZERO};
    vect_t tmp[2] = {VINIT_ZERO, VINIT_ZERO};
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

    if( gcv_tri_tri_intersect_with_isectline(NULL,NULL, f, f+1, &coplanar, i, &tol) == 0 && should == 0)
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

#undef TRY
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
    unsigned long int i, tcount = 0, count = 0, nsplt;
    struct soup_s s;
    struct face_s f;
    struct bn_tol t;

    BN_TOL_INIT(&t);
    t.dist = 0.005;
    t.dist_sq = t.dist * t.dist;

    /* evil macros to do a lot of scaffolding, setup, execution, etc... */
    /* prep take: the triangle to split (9 fastf_t), the intersection line (6
     * fastf_t), and the number of expected resulting triangles. */

    /* NOTE THE "PREP" AND "POST" MACROS MUST BE USED IN PAIRS DUE TO THE SPLIT ENCLOSING CURLY BRACES */
#define PREP(t00,t01,t02,t10,t11,t12,t20,t21,t22,                       \
             ispt00,ispt01,ispt02,ispt10,ispt11,ispt12,_nsplt)          \
          {                                                             \
            point_t isectpt[2];                                         \
            int urf[_nsplt+1];                                          \
            for (i = 0; i < _nsplt + 1; ++i) urf[i] = 0;                \
            unsigned long int failure = 0, numsplit = _nsplt;           \
            tcount++;                                                   \
            VSET(isectpt[0],ispt00,ispt01,ispt02);                      \
            VSET(isectpt[1],ispt10,ispt11,ispt12);                      \
            s.magic = SOUP_MAGIC;                                       \
            s.faces = NULL;                                             \
            s.maxfaces = 0;                                             \
            s.nfaces = 0;                                               \
            VSET(f.vert[0],t00,t01,t02);                                \
            VSET(f.vert[1],t10,t11,t12);                                \
            VSET(f.vert[2],t20,t21,t22);                                \
            soup_add_face(&s,V3ARGS(f.vert),&t);                        \
            VSET(f.plane,0,0,1);                                        \
            nsplt = split_face_single(&s,0,isectpt,&f,&t);              \
            if (nsplt != s.nfaces) {                                    \
              printf("Errr, nsplit %lu != s.nfaces %lu ?\n", numsplit, s.nfaces); \
            }

    /* the _splits is an array of expected triangles, as 9 fastf_t tuples */
    /* fastf_t _splits[nsplt][9] = {{...},{...}} */
    /* post tests to see if the resulting triangles match and cleans up */
#define POST(name)                                                      \
    for (i = 0; i < numsplit; i++) {                                    \
      VSET(f.vert[0],_splits[i][0],_splits[i][1],_splits[i][2]);        \
      VSET(f.vert[1],_splits[i][3],_splits[i][4],_splits[i][5]);        \
      VSET(f.vert[2],_splits[i][6],_splits[i][7],_splits[i][8]);        \
      urf[i] = find_tri(&s,&f,&t);                                      \
      if (urf[i] == -1) failure++;                                      \
    }                                                                   \
    if (nsplt != 2 && urf[0] == -1 && urf[1] == -1) {                   \
      printf("\033[1;31mFAILURE "name"\033[m\n");                       \
      printf("%lu faces now\n",s.nfaces);                               \
      for (i = 0; i < s.nfaces; i++)                                    \
        printf("%03lu: % 2g,% 2g,% 2g | % 2g,% 2g,% 2g | % 2g,% 2g,% 2g\n", \
               i, V3ARGS(s.faces[i].vert[0]), V3ARGS(s.faces[i].vert[1]), V3ARGS(s.faces[i].vert[2])); \
      count++;                                                          \
    }                                                                   \
    free(s.faces);                                                      \
} /* NOTE THIS IS THE CLOSING BRACE FROM THE OPENING BRACE IN THE PREVIOUS MACRO!! */


    /* VERT/VERT */
    PREP(0,0,0, 0,1,0, 1,0,0, 0,0,0, 0,1,0, 1); fastf_t _splits[1][9] = {{0,0,0, 0,1,0, 1,0,0}}; POST("VERT/VERT");

    /* VERT/EDGE */
    PREP(-1,0,0, 0,1,0, 1,0,0, 0,0,0, 0,1,0, 2); fastf_t _splits[2][9] = {{0,0,0,0,1,0,-1,0,0},{0,0,0,0,1,0,1,0,0}}; POST("VERT/EDGE");

#if 0 /* known to be broken right now.  */
    /* EDGE/EDGE */
    PREP(-2,0,0, 0,2,0, 2,0,0, 1,1,0, -1,1,0, 3); fastf_t _splits[3][9] = { { 1,1,0,  0,2,0, -1,1,0}, { 1,1,0, -2,0,0,  2,0,0}, {-1,1,0,  1,1,0, -2,0,0}}; POST("EDGE/EDGE");
#endif

    /* VERT/FACE */
    PREP(-2,0,0, 0,2,0, 2,0,0, 0,2,0, 0,1,0, 3); fastf_t _splits[4][9]={ {-2,0,0,2,0,0,0,1,0}, {-2,0,0,0,2,0,0,1,0}, {2,0,0,0,2,0,0,1,0}}; POST("VERT/FACE");

    /* EDGE/FACE */
    PREP(-2,0,0, 0,2,0, 2,0,0, 0,0,0, 0,1,0, 4); fastf_t _splits[4][9]={{0,0,0,-2,0,0,0,1,0},{0,0,0,2,0,0,0,1,0},{-2,0,0,0,2,0,0,1,0},{2,0,0,0,2,0,0,1,0}}; POST("EDGE/FACE");

#if 0 /* known to be broken right now */
    /* FACE/FACE */
    PREP(-2,0,0, 0,2,0, 2,0,0, 0,0,0, 0,1,0, 4); fastf_t _splits[4][9]={{0,0,0,-2,0,0,0,1,0},{0,0,0,2,0,0,0,1,0},{-2,0,0,0,2,0,0,1,0},{2,0,0,0,2,0,0,1,0}}; POST("EDGE/FACE");
#endif

#undef PREP
#undef POST

    return count;
}

int
test_face_splits()
{
    int count = 0;

    int rval;
    struct soup_s l, r;
    struct bn_tol t;
    point_t p[3];


    BN_TOL_INIT(&t);
    t.dist = 0.005;
    t.dist_sq = t.dist * t.dist;

    l.magic = r.magic = SOUP_MAGIC;
    l.faces = r.faces = NULL;
    l.maxfaces = l.nfaces = r.maxfaces = r.nfaces = 0;
    VSET(p[0], -1, 0, 0); VSET(p[1], 0, 1, 0); VSET(p[2], 1, 0, 0); soup_add_face(&l, p[0], p[1], p[2], &t);
    VSET(p[0], 0, 0, -1); VSET(p[1], 0, 1, 0); VSET(p[2], 0, 0, 1); soup_add_face(&r, p[0], p[1], p[2], &t);

    rval = split_face(&l, 0, &r, 0, &t);
    if(rval != 3 || l.nfaces != 2 || r.nfaces != 2) {
	printf("\033[1;31mFAILURE\033[m\n");
	count++;
    }
    {
	struct face_s f;
	int tri;
	VSET(f.vert[0], 0,0,0); VSET(f.vert[1], 0,1,0); VSET(f.vert[2], 1,0,0); tri = find_tri(&l, &f, &t); if(tri==-1) { count++; printf("\033[1;31mFAILURE\033[m\n"); } else printf("tri.fu: %x\n", l.faces[tri].foo);
	VSET(f.vert[0], 0,0,0); VSET(f.vert[1], 0,1,0); VSET(f.vert[2], -1,0,0); tri = find_tri(&l, &f, &t); if(tri==-1) { count++; printf("\033[1;31mFAILURE\033[m\n"); } else printf("tri.fu: %x\n", l.faces[tri].foo);

	VSET(f.vert[0], 0,0,0); VSET(f.vert[1], 0,1,0); VSET(f.vert[2], 0,0,-1); tri = find_tri(&r, &f, &t); if(tri==-1) { count++; printf("\033[1;31mFAILURE\033[m\n"); } else printf("tri.fu: %x\n", l.faces[tri].foo);
	VSET(f.vert[0], 0,0,0); VSET(f.vert[1], 0,1,0); VSET(f.vert[2], 0,0,1); tri = find_tri(&r, &f, &t); if(tri==-1) { count++; printf("\033[1;31mFAILURE\033[m\n"); } else printf("tri.fu: %x\n", l.faces[tri].foo);
    }

    return count;
}

int
main(void)
{
    printf("RESULT: TRI INTERSECTION: %d failures\n", test_tri_intersections());
    printf("RESULT: SINGL FACE SPLIT: %d failures\n", test_face_split_single());
    printf("RESULT: FACE SPLITTING  : %d failures\n", test_face_splits());
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
