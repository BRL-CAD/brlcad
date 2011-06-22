/*                      N M G _ J U N K . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file primitives/nmg/nmg_junk.c
 *
 * This module is a resting place for unfinished subroutines that are
 * NOT a part of the current NMG library, but which were sufficiently
 * far along as to be worth saving.
 *
 * THESE ROUTINES ARE ALL MARKED STATIC AS THEY ARE EXPERIMENTAL AND
 * NOT YET INTENDED TO BE USED.  ASK A BRL-CAD DEVELOPER IF YOU NEED
 * SOMETHING IN HERE WHAT IT WILL TAKE TO ENABLE THE ROUTINE.
 *
 * NOTE: THIS FILE SHOULD BE ENABLED FOR COMPILATION SO THAT IT CAN
 * CONTINUE TO BE MAINTAINED UNTIL MIGRATION TO LIBNMG.
 *
 */
/** @} */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "bu.h"
#include "bn.h"
#include "vmath.h"
#include "raytrace.h"


/**
 * N M G _ P O L Y T O N M G
 *
 * Read a polygon file and convert it to an NMG shell
 *
 * A polygon file consists of the following:
 *
 * The first line consists of two integer numbers: the number of
 * points (vertices) in the file, followed by the number of polygons
 * in the file.  This line is followed by lines for each of the
 * verticies.  Each vertex is listed on its own line, as the 3tuple "X
 * Y Z".  After the list of verticies comes the list of polygons.
 * each polygon is represented by a line containing 1) the number of
 * verticies in the polygon, followed by 2) the indicies of the
 * verticies that make up the polygon.
 *
 * Implicitly returns r->s_p which is a new shell containing all the
 * faces from the polygon file.
 *
 * XXX This is a horrible way to do this.  Lee violates his own rules
 * about not creating fundamental structures on his own...  :-)
 * Retired in favor of more modern tessellation strategies.
 */
struct shell *
nmg_polytonmg(FILE *fp, struct nmgregion *r, const struct bn_tol *tol)
{
    int i, j, num_pts, num_facets, pts_this_face, facet;
    int vl_len;
    struct vertex **v;  /* list of all vertices */
    struct vertex **vl; /* list of vertices for this polygon*/
    point_t p;
    struct shell *s;
    struct faceuse *fu;
    struct loopuse *lu;
    struct edgeuse *eu;
    plane_t plane;
    struct model *m;

    s = nmg_msv(r);
    m = s->r_p->m_p;
    nmg_kvu(s->vu_p);

    /* get number of points & number of facets in file */
    if (fscanf(fp, "%d %d", &num_pts, &num_facets) != 2)
	bu_bomb("polytonmg() Error in first line of poly file\n");
    else
	if (rt_g.NMG_debug & DEBUG_POLYTO)
	    bu_log("points: %d facets: %d\n",
		   num_pts, num_facets);


    v = (struct vertex **) bu_calloc(num_pts, sizeof (struct vertex *),
				     "vertices");

    /* build the vertices */
    for (i = 0; i < num_pts; ++i) {
	GET_VERTEX(v[i], m);
	v[i]->magic = NMG_VERTEX_MAGIC;
    }

    /* read in the coordinates of the vertices */
    for (i=0; i < num_pts; ++i) {
	if (fscanf(fp, "%lg %lg %lg", &p[0], &p[1], &p[2]) != 3)
	    bu_bomb("polytonmg() Error reading point");
	else
	    if (rt_g.NMG_debug & DEBUG_POLYTO)
		bu_log("read vertex #%d (%g %g %g)\n",
		       i, p[0], p[1], p[2]);

	nmg_vertex_gv(v[i], p);
    }

    vl = (struct vertex **)bu_calloc(vl_len=8, sizeof (struct vertex *),
				     "vertex parameter list");

    for (facet = 0; facet < num_facets; ++facet) {
	if (fscanf(fp, "%d", &pts_this_face) != 1)
	    bu_bomb("polytonmg() error getting pt count for this face");

	if (rt_g.NMG_debug & DEBUG_POLYTO)
	    bu_log("facet %d pts in face %d\n",
		   facet, pts_this_face);

	if (pts_this_face > vl_len) {
	    while (vl_len < pts_this_face) vl_len *= 2;
	    vl = (struct vertex **)bu_realloc((char *)vl,
					      vl_len*sizeof(struct vertex *),
					      "vertex parameter list (realloc)");
	}

	for (i=0; i < pts_this_face; ++i) {
	    if (fscanf(fp, "%d", &j) != 1)
		bu_bomb("polytonmg() error getting point index for v in f");
	    vl[i] = v[j-1];
	}

	fu = nmg_cface(s, vl, pts_this_face);
	lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	/* XXX should check for vertex-loop */
	eu = BU_LIST_FIRST(edgeuse, &lu->down_hd);
	NMG_CK_EDGEUSE(eu);
	if (bn_mk_plane_3pts(plane, eu->vu_p->v_p->vg_p->coord,
			     BU_LIST_PNEXT(edgeuse, eu)->vu_p->v_p->vg_p->coord,
			     BU_LIST_PLAST(edgeuse, eu)->vu_p->v_p->vg_p->coord,
			     tol)) {
	    bu_log("At %d in %s\n", __LINE__, __FILE__);
	    bu_bomb("polytonmg() cannot make plane equation\n");
	} else nmg_face_g(fu, plane);
    }

    for (i=0; i < num_pts; ++i) {
	if (BU_LIST_IS_EMPTY(&v[i]->vu_hd)) continue;
	FREE_VERTEX(v[i]);
    }
    bu_free((char *)v, "vertex array");
    return s;
}


/**
 * N M G _ I S E C T _ F A C E 3 P _ S H E L L _ I N T
 *
 * Intersect all the edges in fu1 that don't lie on any of the faces
 * of shell s2 with s2, i.e. "interior" edges, where the endpoints lie
 * on s2, but the edge is not shared with a face of s2.  Such edges
 * wouldn't have been processed by the NEWLINE version of
 * nmg_isect_two_generic_faces(), so intersections need to be looked
 * for here.  Fortunately, it's easy to reject everything except edges
 * that need processing using only the topology structures.
 *
 * The "_int" at the end of the name is to signify that this routine
 * does only "interior" edges, and is not a general face/shell
 * intersector.
 */
void
nmg_isect_face3p_shell_int(struct nmg_inter_struct *is, struct faceuse *fu1, struct shell *s2)
{
    struct shell *s1;
    struct loopuse *lu1;
    struct edgeuse *eu1;

    NMG_CK_INTER_STRUCT(is);
    NMG_CK_FACEUSE(fu1);
    NMG_CK_SHELL(s2);
    s1 = fu1->s_p;
    NMG_CK_SHELL(s1);

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_face3p_shell_int(, fu1=x%x, s2=x%x) START\n", fu1, s2);

    for (BU_LIST_FOR (lu1, loopuse, &fu1->lu_hd)) {
	NMG_CK_LOOPUSE(lu1);
	if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC)
	    continue;
	for (BU_LIST_FOR (eu1, edgeuse, &lu1->down_hd)) {
	    struct edgeuse *eu2;

	    eu2 = nmg_find_matching_eu_in_s(eu1, s2);
	    if (eu2) {
		bu_log("nmg_isect_face3p_shell_int() eu1=x%x, e1=x%x, eu2=x%x, e2=x%x (nothing to do)\n", eu1, eu1->e_p, eu2, eu2->e_p);
		/* Whether the edgeuse is in a face, or a wire
		 * edgeuse, the other guys will isect it.
		 */
		continue;
	    }
	    /* vu2a and vu2b are in shell s2, but there is no edge
	     * running between them in shell s2.  Create a line of
	     * intersection, and go to it!.
	     */
	    bu_log("nmg_isect_face3p_shell_int(, s2=x%x) eu1=x%x, no eu2\n", s2, eu1);
/* XXX eso no existe todavia */
/* 	    nmg_isect_edge3p_shell(is, eu1, s2); */
	}
    }

    if (rt_g.NMG_debug & DEBUG_POLYSECT)
	bu_log("nmg_isect_face3p_shell_int(, fu1=x%x, s2=x%x) END\n", fu1, s2);
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
