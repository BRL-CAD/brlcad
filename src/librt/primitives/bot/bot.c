/*                           B O T . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2011 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/bot/bot.c
 *
 * Intersect a ray with a bag o' triangles.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "bin.h"

#include "tcl.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "bot.h"

#define GLUE(_a, _b)      _a ## _b
#define XGLUE(_a, _b) GLUE(_a, _b)

#include "tie.h"
#include "btg.h"	/* for the bottie_ functions */

#define MAXHITS 128

#define BOT_MIN_DN 1.0e-9

#define BOT_UNORIENTED_NORM(_hitp, _out) { \
	if (_out) {	/* this is an exit */ \
		if ((_hitp)->hit_vpriv[X] < 0.0) { \
			VREVERSE((_hitp)->hit_normal, trip->tri_N); \
		} else { \
			VMOVE((_hitp)->hit_normal, trip->tri_N); \
		} \
	} else {	/* this is an entrance */ \
		if ((_hitp)->hit_vpriv[X] > 0.0) { \
			VREVERSE((_hitp)->hit_normal, trip->tri_N); \
		} else { \
			VMOVE((_hitp)->hit_normal, trip->tri_N); \
		} \
	} \
}


/* forward declarations needed for the included routines below */
int
rt_bot_makesegs(
    struct hit *hits,
    size_t nhits,
    struct soltab *stp,
    struct xray *rp,
    struct application *ap,
    struct seg *seghead,
    struct rt_piecestate *psp);

HIDDEN int
rt_bot_unoriented_segs(struct hit *hits,
		       size_t nhits,
		       struct soltab *stp,
		       struct xray *rp,
		       struct application *ap,
		       struct seg *seghead,
		       struct bot_specific *bot);

size_t
rt_botface_w_normals(struct soltab *stp,
		     struct bot_specific *bot,
		     fastf_t *ap,
		     fastf_t *bp,
		     fastf_t *cp,
		     fastf_t *vertex_normals, /* array of nine values (three unit normals vectors) */
		     size_t face_no,
		     const struct bn_tol *tol);


#define TRI_TYPE float
#define NORM_TYPE signed char
#define NORMAL_SCALE 127.0
#define ONE_OVER_SCALE (1.0/127.0)
#include "./g_bot_include.c"
#undef TRI_TYPE
#undef NORM_TYPE
#undef NORMAL_SCALE
#undef ONE_OVER_SCALE
#define TRI_TYPE double
#define NORM_TYPE fastf_t
#define NORMAL_SCALE 1.0
#define ONE_OVER_SCALE 1.0
#include "./g_bot_include.c"
#undef TRI_TYPE
#undef NORM_TYPE
#undef NORMAL_SCALE
#undef ONE_OVER_SCALE


/**
 * R T _ B O T F A C E
 *
 * This function is called with pointers to 3 points, and is used to
 * prepare BOT faces.  ap, bp, cp point to vect_t points.
 *
 * Return -
 * 0 if the 3 points didn't form a plane (eg, colinear, etc).
 * # pts (3) if a valid plane resulted.
 */
size_t
rt_botface_w_normals(struct soltab *stp,
		     struct bot_specific *bot,
		     fastf_t *ap,
		     fastf_t *bp,
		     fastf_t *cp,
		     fastf_t *vertex_normals, /* array of nine values (three unit normals vectors) */
		     size_t face_no,
		     const struct bn_tol *tol)
{

    if (bot->bot_flags & RT_BOT_USE_FLOATS) {
	return rt_botface_w_normals_float(stp, bot, ap, bp, cp,
					  vertex_normals, face_no, tol);
    } else {
	return rt_botface_w_normals_double(stp, bot, ap, bp, cp,
					   vertex_normals, face_no, tol);
    }
}


size_t
rt_botface(struct soltab *stp,
	   struct bot_specific *bot,
	   fastf_t *ap,
	   fastf_t *bp,
	   fastf_t *cp,
	   size_t face_no,
	   const struct bn_tol *tol)
{
    return rt_botface_w_normals(stp, bot, ap, bp, cp, NULL, face_no, tol);
}


/**
 * Do the prep to support pieces for a BOT/ARS
 */
void
rt_bot_prep_pieces(struct bot_specific *bot,
		   struct soltab *stp,
		   size_t ntri,
		   const struct bn_tol *tol)
{
    if (bot->bot_flags & RT_BOT_USE_FLOATS) {
	rt_bot_prep_pieces_float(bot, stp, ntri, tol);
    } else {
	rt_bot_prep_pieces_double(bot, stp, ntri, tol);
    }
}

/**
 * R T _ B O T _ B B O X
 *
 * Calculate an RPP for a BoT
 */
int
rt_bot_bbox(struct rt_db_internal *ip, point_t *min, point_t *max) {
    struct rt_bot_internal *bot_ip;
    size_t vert_index;

    RT_CK_DB_INTERNAL(ip);
    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);

    VSETALL((*min), MAX_FASTF);
    VSETALL((*max), -MAX_FASTF);
    for (vert_index=0; vert_index < bot_ip->num_vertices; vert_index++) {
	VMINMAX((*min), (*max), &bot_ip->vertices[vert_index]);
    }
    
    /* Prevent the RPP from being 0 thickness */
    if (NEAR_EQUAL((*min)[X], (*max)[X], SMALL_FASTF)) {
	(*min)[X] -= SMALL_FASTF;
	(*max)[X] += SMALL_FASTF;
    }
    if (NEAR_EQUAL((*min)[Y], (*max)[Y], SMALL_FASTF)) {
	(*min)[Y] -= SMALL_FASTF;
	(*max)[Y] += SMALL_FASTF;
    }
    if (NEAR_EQUAL((*min)[Z], (*max)[Z], SMALL_FASTF)) {
	(*min)[Z] -= SMALL_FASTF;
	(*max)[Z] += SMALL_FASTF;
    }
    return 0;
}


/**
 * R T _ B O T _ P R E P
 *
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid BOT, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 BOT is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct bot_specific is created, and its address is stored in
 * stp->st_specific for use by bot_shot().
 */
int
rt_bot_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_bot_internal *bot_ip;

    RT_CK_DB_INTERNAL(ip);
    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);

    if (rt_bot_bbox(ip, &(stp->st_min), &(stp->st_max))) return 1;

    if ( rt_bot_mintie > 0 && bot_ip->num_faces >= rt_bot_mintie /* FIXME: (necessary?) && (bot_ip->face_normals != NULL || bot_ip->orientation != RT_BOT_UNORIENTED) */ )
	return bottie_prep_double(stp, bot_ip, rtip);
    else if (bot_ip->bot_flags & RT_BOT_USE_FLOATS)
	return rt_bot_prep_float(stp, bot_ip, rtip);
    else
	return rt_bot_prep_double(stp, bot_ip, rtip);
}


/**
 * R T _ B O T _ P R I N T
 */
void
rt_bot_print(const struct soltab *stp)
{
    if (stp) RT_CK_SOLTAB(stp);
}


HIDDEN int
rt_bot_plate_segs(struct hit *hits,
		  size_t nhits,
		  struct soltab *stp,
		  struct xray *rp,
		  struct application *ap,
		  struct seg *seghead,
		  struct bot_specific *bot)
{
    if (bot->bot_flags & RT_BOT_USE_FLOATS) {
	return rt_bot_plate_segs_float(hits, nhits, stp, rp, ap, seghead, bot);
    } else {
	return rt_bot_plate_segs_double(hits, nhits, stp, rp, ap, seghead, bot);
    }
}


HIDDEN int
rt_bot_unoriented_segs(struct hit *hits,
		       size_t nhits,
		       struct soltab *stp,
		       struct xray *rp,
		       struct application *ap,
		       struct seg *seghead,
		       struct bot_specific *bot)
{
    if (bot->bot_flags & RT_BOT_USE_FLOATS) {
	return rt_bot_unoriented_segs_float(hits, nhits, stp, rp, ap, seghead);
    } else {
	return rt_bot_unoriented_segs_double(hits, nhits, stp, rp, ap, seghead);
    }
}


/**
 * R T _ B O T _ M A K E S E G S
 *
 * Given an array of hits, make segments out of them.  Exactly how
 * this is to be done depends on the mode of the BoT.
 */
int
rt_bot_makesegs(struct hit *hits, size_t nhits, struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead, struct rt_piecestate *psp)
{
    struct bot_specific *bot = (struct bot_specific *)stp->st_specific;

    if (bot->bot_mode == RT_BOT_PLATE ||
	bot->bot_mode == RT_BOT_PLATE_NOCOS) {
	return rt_bot_plate_segs(hits, nhits, stp, rp, ap, seghead, bot);
    }

    if (bot->bot_flags & RT_BOT_USE_FLOATS) {
	return rt_bot_makesegs_float(hits, nhits, stp, rp, ap, seghead, psp);
    } else {
	return rt_bot_makesegs_double(hits, nhits, stp, rp, ap, seghead, psp);
    }
}


/**
 * R T _ B O T _ S H O T
 *
 * Intersect a ray with a bot.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Notes for rt_bot_norm(): hit_private contains pointer to the
 * tri_specific structure.  hit_vpriv[X] contains dot product of ray
 * direction and unit normal from tri_specific.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_bot_shot(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct bot_specific *bot = (struct bot_specific *)stp->st_specific;

    if (bot->tie != NULL) {
	return bottie_shot_double(stp, rp, ap, seghead);
    } else if (bot->bot_flags & RT_BOT_USE_FLOATS) {
	return rt_bot_shot_float(stp, rp, ap, seghead);
    } else {
	return rt_bot_shot_double(stp, rp, ap, seghead);
    }
}


/**
 * R T _ B O T _ P I E C E _ S H O T
 *
 * Intersect a ray with a list of "pieces" of a BoT.
 *
 * This routine may be invoked many times for a single ray, as the ray
 * traverses from one space partitioning cell to the next.
 *
 * Plate-mode (2 hit) segments will be returned immediately in
 * seghead.
 *
 * Generally the hits are stashed between invocations in psp.
 */
int
rt_bot_piece_shot(struct rt_piecestate *psp, struct rt_piecelist *plp, double dist_corr, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct soltab *stp;
    struct bot_specific *bot;

    RT_CK_PIECELIST(plp);
    stp = plp->stp;
    RT_CK_SOLTAB(stp);
    bot = (struct bot_specific *)stp->st_specific;

    if (bot->bot_flags & RT_BOT_USE_FLOATS) {
	return rt_bot_piece_shot_float(psp, plp, dist_corr, rp, ap, seghead);
    } else {
	return rt_bot_piece_shot_double(psp, plp, dist_corr, rp, ap, seghead);
    }
}


/**
 * R T _ B O T _ P I E C E _ H I T S E G S
 */
void
rt_bot_piece_hitsegs(struct rt_piecestate *psp, struct seg *seghead, struct application *ap)
{
    RT_CK_PIECESTATE(psp);
    RT_CK_AP(ap);
    RT_CK_HTBL(&psp->htab);

    /* Sort hits, Near to Far */
    rt_hitsort(psp->htab.hits, psp->htab.end);

    /* build segments */
    (void)rt_bot_makesegs(psp->htab.hits, psp->htab.end, psp->stp, &ap->a_ray, ap, seghead, psp);
}


/**
 * R T _ B O T _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_bot_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    struct bot_specific *bot=(struct bot_specific *)stp->st_specific;

    if (bot->bot_flags & RT_BOT_USE_FLOATS) {
	rt_bot_norm_float(bot, hitp, stp, rp);
    } else {
	rt_bot_norm_double(bot, hitp, stp, rp);
    }
}


/**
 * R T _ B O T _ C U R V E
 *
 * Return the curvature of the bot.
 */
void
rt_bot_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp)
{
    if (stp) RT_CK_SOLTAB(stp);

    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
    cvp->crv_c1 = cvp->crv_c2 = 0;
}


/**
 * R T _ B O T _ U V
 *
 * For a hit on the surface of an bot, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 *
 * u = azimuth
 * v = elevation
 */
void
rt_bot_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);
    if (hitp) RT_CK_HIT(hitp);
    if (!uvp) return;
}


/**
 * R T _ B O T _ F R E E
 */
void
rt_bot_free(struct soltab *stp)
{
    struct bot_specific *bot =
	(struct bot_specific *)stp->st_specific;

    if (bot->tie != NULL) {
	bottie_free_double(bot->tie);
	bot->tie = NULL;
    }

    if (bot->bot_flags & RT_BOT_USE_FLOATS) {
	rt_bot_free_float(bot);
    } else {
	rt_bot_free_double(bot);
    }
}


/**
 * R T _ B O T _ C L A S S
 */
int
rt_bot_class(const struct soltab *stp, const fastf_t *min, const fastf_t *max, const struct bn_tol *tol)
{
    if (stp) RT_CK_SOLTAB(stp);
    if (tol) BN_CK_TOL(tol);
    if (!min) return 0;
    if (!max) return 0;

    return 0;
}


/**
 * R T _ B O T _ P L O T
 */
int
rt_bot_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
{
    struct rt_bot_internal *bot_ip;
    size_t i;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);

    if (bot_ip->num_vertices <= 0 || !bot_ip->vertices || bot_ip->num_faces <= 0 || !bot_ip->faces)
	return 0;

    for (i=0; i<bot_ip->num_faces; i++) {
	if (bot_ip->faces[i*3+2] < 0 || (size_t)bot_ip->faces[i*3+2] > bot_ip->num_vertices)
	    continue; /* sanity */

	RT_ADD_VLIST(vhead, &bot_ip->vertices[bot_ip->faces[i*3+0]*3], BN_VLIST_LINE_MOVE);
	RT_ADD_VLIST(vhead, &bot_ip->vertices[bot_ip->faces[i*3+1]*3], BN_VLIST_LINE_DRAW);
	RT_ADD_VLIST(vhead, &bot_ip->vertices[bot_ip->faces[i*3+2]*3], BN_VLIST_LINE_DRAW);
	RT_ADD_VLIST(vhead, &bot_ip->vertices[bot_ip->faces[i*3+0]*3], BN_VLIST_LINE_DRAW);
    }

    return 0;
}


/**
 * R T _ B O T _ P L O T _ P O L Y
 */
int
rt_bot_plot_poly(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol))
{
    struct rt_bot_internal *bot_ip;
    size_t i;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);

    if (bot_ip->num_vertices <= 0 || !bot_ip->vertices || bot_ip->num_faces <= 0 || !bot_ip->faces)
	return 0;

    /* XXX Should consider orientation here, flip if necessary. */
    for (i=0; i<bot_ip->num_faces; i++) {
	point_t aa, bb, cc;
	vect_t ab, ac;
	vect_t norm;

	if (bot_ip->faces[i*3+2] < 0 || (size_t)bot_ip->faces[i*3+2] > bot_ip->num_vertices)
	    continue; /* sanity */

	VMOVE(aa, &bot_ip->vertices[bot_ip->faces[i*3+0]*3]);
	VMOVE(bb, &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
	VMOVE(cc, &bot_ip->vertices[bot_ip->faces[i*3+2]*3]);

	VSUB2(ab, aa, bb);
	VSUB2(ac, aa, cc);
	VCROSS(norm, ab, ac);
	VUNITIZE(norm);
	RT_ADD_VLIST(vhead, norm, BN_VLIST_POLY_START);

	if ((bot_ip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) &&
	    (bot_ip->bot_flags & RT_BOT_USE_NORMALS)) {
	    vect_t na, nb, nc;

	    VMOVE(na, &bot_ip->normals[bot_ip->face_normals[i*3+0]*3]);
	    VMOVE(nb, &bot_ip->normals[bot_ip->face_normals[i*3+1]*3]);
	    VMOVE(nc, &bot_ip->normals[bot_ip->face_normals[i*3+2]*3]);
	    RT_ADD_VLIST(vhead, na, BN_VLIST_POLY_VERTNORM);
	    RT_ADD_VLIST(vhead, aa, BN_VLIST_POLY_MOVE);
	    RT_ADD_VLIST(vhead, nb, BN_VLIST_POLY_VERTNORM);
	    RT_ADD_VLIST(vhead, bb, BN_VLIST_POLY_DRAW);
	    RT_ADD_VLIST(vhead, nc, BN_VLIST_POLY_VERTNORM);
	    RT_ADD_VLIST(vhead, cc, BN_VLIST_POLY_DRAW);
	    RT_ADD_VLIST(vhead, aa, BN_VLIST_POLY_END);
	} else {
	    RT_ADD_VLIST(vhead, aa, BN_VLIST_POLY_MOVE);
	    RT_ADD_VLIST(vhead, bb, BN_VLIST_POLY_DRAW);
	    RT_ADD_VLIST(vhead, cc, BN_VLIST_POLY_DRAW);
	    RT_ADD_VLIST(vhead, aa, BN_VLIST_POLY_END);
	}
    }

    return 0;
}


/**
 * R T _ B O T _ T E S S
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_bot_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *tol)
{
    struct rt_bot_internal *bot_ip;
    struct shell *s;
    struct vertex **verts;
    point_t pt[3];
    point_t center;
    size_t i;

    RT_CK_DB_INTERNAL(ip);
    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);
#if 1
    if (bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS) {
#define RT_BOT_TESS_MAX_FACES 1024
        size_t faces[RT_BOT_TESS_MAX_FACES];
        plane_t planes[RT_BOT_TESS_MAX_FACES];
        fastf_t scale = 1.0;

        rt_bot_condense(bot_ip);
        VSETALL(center, 0.0);
        for (i=0 ; i<bot_ip->num_vertices ; i++) {
            VADD2(center, center, &bot_ip->vertices[i*3]);
        }
        scale = 1.0 / (fastf_t)bot_ip->num_vertices;
        VSCALE(center, center, scale);
        fprintf(stderr, "center pt = (%g %g %g)\n", V3ARGS(center));

        /* get the faces that use each vertex */
        for (i=0 ; i<bot_ip->num_vertices ; i++) {
            size_t faceCount = 0;
            size_t j;
            for (j=0 ; j<bot_ip->num_faces ; j++) {
                size_t k;
                for (k=0 ; k<3 ; k++) {
                    if ((size_t)bot_ip->faces[j*3+k] == i) {
                        /* this face uses vertex i */
                        faces[faceCount] = j;
                        faceCount++;
                        break;
                    }
                }
            }
            fprintf(stderr, "Vertex #%lu appears in %lu faces\n", (long unsigned)i, (long unsigned)faceCount);
            if (faceCount == 0) {
                continue;
            }
            if (bot_ip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS)
		for (i=0 ; i<faceCount ; i++) {
		    size_t k;
		    fastf_t *plane;
		    for (k=0 ; k<3 ; k++) {
                        size_t idx = faces[i] * 3 + k;
                        VMOVE(planes[i], &bot_ip->normals[bot_ip->face_normals[idx]*3]);
                        planes[i][3] = VDOT(planes[i], &bot_ip->vertices[bot_ip->faces[faces[i]*3]*3]);
		    }
		    plane = planes[i];
		    fprintf(stderr, "\tplane #%lu = (%g %g %g %g)\n", (long unsigned)i, V4ARGS(plane));
		}
        }
        return -1;
    }
#endif
    *r = nmg_mrsv(m);     /* Make region, empty shell, vertex */
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    verts = (struct vertex **)bu_calloc(bot_ip->num_vertices, sizeof(struct vertex *), "rt_bot_tess: *verts[]");

    for (i=0; i<bot_ip->num_faces; i++) {
	struct faceuse *fu;
	struct vertex **corners[3];

	if (bot_ip->faces[i*3+2] < 0 || (size_t)bot_ip->faces[i*3+2] > bot_ip->num_vertices)
	    continue; /* sanity */

	if (bot_ip->orientation == RT_BOT_CW) {
	    VMOVE(pt[2], &bot_ip->vertices[bot_ip->faces[i*3+0]*3]);
	    VMOVE(pt[1], &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
	    VMOVE(pt[0], &bot_ip->vertices[bot_ip->faces[i*3+2]*3]);
	    corners[2] = &verts[bot_ip->faces[i*3+0]];
	    corners[1] = &verts[bot_ip->faces[i*3+1]];
	    corners[0] = &verts[bot_ip->faces[i*3+2]];
	} else {
	    VMOVE(pt[0], &bot_ip->vertices[bot_ip->faces[i*3+0]*3]);
	    VMOVE(pt[1], &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
	    VMOVE(pt[2], &bot_ip->vertices[bot_ip->faces[i*3+2]*3]);
	    corners[0] = &verts[bot_ip->faces[i*3+0]];
	    corners[1] = &verts[bot_ip->faces[i*3+1]];
	    corners[2] = &verts[bot_ip->faces[i*3+2]];
	}

	if (!bn_3pts_distinct(pt[0], pt[1], pt[2], tol)
	    || bn_3pts_collinear(pt[0], pt[1], pt[2], tol))
	    continue;

	if ((fu=nmg_cmface(s, corners, 3)) == (struct faceuse *)NULL) {
	    bu_log("rt_bot_tess() nmg_cmface() failed for face #%zu\n", i);
	    continue;
	}

	if (!(*corners[0])->vg_p)
	    nmg_vertex_gv(*(corners[0]), pt[0]);
	if (!(*corners[1])->vg_p)
	    nmg_vertex_gv(*(corners[1]), pt[1]);
	if (!(*corners[2])->vg_p)
	    nmg_vertex_gv(*(corners[2]), pt[2]);

	if (nmg_calc_face_g(fu))
	    nmg_kfu(fu);
	else if (bot_ip->mode == RT_BOT_SURFACE) {
	    struct vertex **tmp;

	    tmp = corners[0];
	    corners[0] = corners[2];
	    corners[2] = tmp;
	    if ((fu=nmg_cmface(s, corners, 3)) == (struct faceuse *)NULL)
		bu_log("rt_bot_tess() nmg_cmface() failed for face #%zu\n", i);
	    else
		nmg_calc_face_g(fu);
	}
    }

    bu_free(verts, "rt_bot_tess *verts[]");

    nmg_mark_edges_real(&s->l.magic);

    nmg_region_a(*r, tol);

    if (bot_ip->mode == RT_BOT_SOLID && bot_ip->orientation == RT_BOT_UNORIENTED)
	nmg_fix_normals(s, tol);

    return 0;
}


/**
 * R T _ B O T _ I M P O R T
 *
 * Import an BOT from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_bot_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_bot_internal *bot_ip;
    union record *rp;
    size_t i;
    size_t chars_used;

    if (dbip) RT_CK_DBI(dbip);
    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    /* Check record type */
    if (rp->u_id != DBID_BOT) {
	bu_log("rt_bot_import4: defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_BOT;
    ip->idb_meth = &rt_functab[ID_BOT];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_bot_internal), "rt_bot_internal");
    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    bot_ip->magic = RT_BOT_INTERNAL_MAGIC;

    bot_ip->num_vertices = ntohl(*(uint32_t *)&rp->bot.bot_num_verts[0]);
    bot_ip->num_faces = ntohl(*(uint32_t *)&rp->bot.bot_num_triangles[0]);
    bot_ip->orientation = rp->bot.bot_orientation;
    bot_ip->mode = rp->bot.bot_mode;
    bot_ip->bot_flags = 0;

    bot_ip->vertices = (fastf_t *)bu_calloc(bot_ip->num_vertices * 3, sizeof(fastf_t), "Bot vertices");
    bot_ip->faces = (int *)bu_calloc(bot_ip->num_faces * 3, sizeof(int), "Bot faces");

    if (mat == NULL) mat = bn_mat_identity;
    for (i=0; i<bot_ip->num_vertices; i++) {
	point_t tmp;

	ntohd((unsigned char *)tmp, (const unsigned char *)(&rp->bot.bot_data[i*24]), 3);
	MAT4X3PNT(&(bot_ip->vertices[i*3]), mat, tmp);
    }

    chars_used = bot_ip->num_vertices * 3 * 8;

    for (i=0; i<bot_ip->num_faces; i++) {
	size_t idx=chars_used + i * 12;

	bot_ip->faces[i*3 + 0] = ntohl(*(uint32_t *)&rp->bot.bot_data[idx + 0]);
	bot_ip->faces[i*3 + 1] = ntohl(*(uint32_t *)&rp->bot.bot_data[idx + 4]);
	bot_ip->faces[i*3 + 2] = ntohl(*(uint32_t *)&rp->bot.bot_data[idx + 8]);
    }

    if (bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS) {
	chars_used = bot_ip->num_vertices * 3 * 8 + bot_ip->num_faces * 12;

	bot_ip->thickness = (fastf_t *)bu_calloc(bot_ip->num_faces, sizeof(fastf_t), "BOT thickness");
	for (i=0; i<bot_ip->num_faces; i++)
	    ntohd((unsigned char *)&(bot_ip->thickness[i]),
		  (const unsigned char *)(&rp->bot.bot_data[chars_used + i*8]), 1);
	bot_ip->face_mode = bu_hex_to_bitv((const char *)(&rp->bot.bot_data[chars_used + bot_ip->num_faces * 8]));
    } else {
	bot_ip->thickness = (fastf_t *)NULL;
	bot_ip->face_mode = (struct bu_bitv *)NULL;
    }

    return 0;			/* OK */
}


/**
 * R T _ B O T _ E X P O R T
 *
 * The name is added by the caller, in the usual place.
 */
int
rt_bot_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_bot_internal *bot_ip;
    union record *rec;
    size_t i;
    size_t chars_used;
    size_t num_recs;
    struct bu_vls face_mode;

    if (dbip) RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_BOT) return -1;
    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);

    if (bot_ip->num_normals > 0) {
	bu_log("BOT surface normals not supported in older database formats, normals not saved\n");
	bu_log("\tPlease update to current database format using \"dbupgrade\"\n");
    }

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(struct bot_rec) - 1 +
	bot_ip->num_vertices * 3 * 8 + bot_ip->num_faces * 3 * 4;
    if (bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS) {
	if (!bot_ip->face_mode) {
	    bot_ip->face_mode = bu_bitv_new(bot_ip->num_faces);
	}
	if (!bot_ip->thickness)
	    bot_ip->thickness = (fastf_t *)bu_calloc(bot_ip->num_faces, sizeof(fastf_t), "BOT thickness");
	bu_vls_init(&face_mode);
	bu_bitv_to_hex(&face_mode, bot_ip->face_mode);
	ep->ext_nbytes += bot_ip->num_faces * 8 + bu_vls_strlen(&face_mode) + 1;
    }

    /* round up to the nearest granule */
    if (ep->ext_nbytes % (sizeof(union record))) {
	ep->ext_nbytes += (sizeof(union record))
	    - ep->ext_nbytes % (sizeof(union record));
    }
    num_recs = ep->ext_nbytes / sizeof(union record) - 1;
    ep->ext_buf = (genptr_t)bu_calloc(1, ep->ext_nbytes, "bot external");
    rec = (union record *)ep->ext_buf;

    rec->bot.bot_id = DBID_BOT;

    *(uint32_t *)&rec->bot.bot_nrec[0] = htonl((uint32_t)num_recs);
    rec->bot.bot_orientation = bot_ip->orientation;
    rec->bot.bot_mode = bot_ip->mode;
    rec->bot.bot_err_mode = 0;
    *(uint32_t *)&rec->bot.bot_num_verts[0] = htonl(bot_ip->num_vertices);
    *(uint32_t *)&rec->bot.bot_num_triangles[0] = htonl(bot_ip->num_faces);

    /* Since libwdb users may want to operate in units other than mm,
     * we offer the opportunity to scale the solid (to get it into mm)
     * on the way out.
     */


    /* convert from local editing units to mm and export to database
     * record format
     */
    for (i=0; i<bot_ip->num_vertices; i++) {
	point_t tmp;

	VSCALE(tmp, &bot_ip->vertices[i*3], local2mm);
	htond((unsigned char *)&rec->bot.bot_data[i*24], (const unsigned char *)tmp, 3);
    }

    chars_used = bot_ip->num_vertices * 24;

    for (i=0; i<bot_ip->num_faces; i++) {
	size_t idx=chars_used + i * 12;

	*(uint32_t *)&rec->bot.bot_data[idx + 0] = htonl(bot_ip->faces[i*3+0]);
	*(uint32_t *)&rec->bot.bot_data[idx + 4] = htonl(bot_ip->faces[i*3+1]);
	*(uint32_t *)&rec->bot.bot_data[idx + 8] = htonl(bot_ip->faces[i*3+2]);
    }

    chars_used += bot_ip->num_faces * 12;

    if (bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS) {
	for (i=0; i<bot_ip->num_faces; i++) {
	    fastf_t tmp;
	    tmp = bot_ip->thickness[i] * local2mm;
	    htond((unsigned char *)&rec->bot.bot_data[chars_used], (const unsigned char *)&tmp, 1);
	    chars_used += 8;
	}
	bu_strlcpy((char *)&rec->bot.bot_data[chars_used], bu_vls_addr(&face_mode), ep->ext_nbytes - (sizeof(struct bot_rec)-1) - chars_used);
	bu_vls_free(&face_mode);
    }

    return 0;
}


/**
 * R T _ B O T _ I M P O R T 5
 */
int
rt_bot_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_bot_internal *bip;
    unsigned char *cp;
    size_t i;

    BU_CK_EXTERNAL(ep);
    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_BOT;
    ip->idb_meth = &rt_functab[ID_BOT];
    ip->idb_ptr = bu_calloc(1, sizeof(struct rt_bot_internal), "rt_bot_internal");

    bip = (struct rt_bot_internal *)ip->idb_ptr;
    bip->magic = RT_BOT_INTERNAL_MAGIC;

    cp = ep->ext_buf;
    bip->num_vertices = ntohl(*(uint32_t *)&cp[0]);
    cp += SIZEOF_NETWORK_LONG;
    bip->num_faces = ntohl(*(uint32_t *)&cp[0]);
    cp += SIZEOF_NETWORK_LONG;
    bip->orientation = *cp++;
    bip->mode = *cp++;
    bip->bot_flags = *cp++;

    if (bip->num_vertices > 0) {
	bip->vertices = (fastf_t *)bu_calloc(bip->num_vertices * 3, sizeof(fastf_t), "BOT vertices");
    } else {
	bip->vertices = (fastf_t *)NULL;
    }

    if (bip->num_faces > 0) {
	bip->faces = (int *)bu_calloc(bip->num_faces * 3, sizeof(int), "BOT faces");
    } else {
	bip->faces = (int *)NULL;
    }

    if (bip->vertices == NULL || bip->faces == NULL)
	bu_log("WARNING: BoT contains %zu vertices, %zu faces\n", bip->num_vertices, bip->num_faces);

    if (mat == NULL)
	mat = bn_mat_identity;

    for (i=0; i<bip->num_vertices; i++) {
	point_t tmp;

	ntohd((unsigned char *)tmp, (const unsigned char *)cp, 3);
	cp += SIZEOF_NETWORK_DOUBLE * 3;
	MAT4X3PNT(&(bip->vertices[i*3]), mat, tmp);
    }

    for (i=0; i<bip->num_faces; i++) {
	bip->faces[i*3 + 0] = ntohl(*(uint32_t *)&cp[0]);
	cp += SIZEOF_NETWORK_LONG;
	bip->faces[i*3 + 1] = ntohl(*(uint32_t *)&cp[0]);
	cp += SIZEOF_NETWORK_LONG;
	bip->faces[i*3 + 2] = ntohl(*(uint32_t *)&cp[0]);
	cp += SIZEOF_NETWORK_LONG;
    }

    if (bip->mode == RT_BOT_PLATE || bip->mode == RT_BOT_PLATE_NOCOS) {
	bip->thickness = (fastf_t *)bu_calloc(bip->num_faces, sizeof(fastf_t), "BOT thickness");
	for (i=0; i<bip->num_faces; i++) {
	    ntohd((unsigned char *)&(bip->thickness[i]), cp, 1);
	    cp += SIZEOF_NETWORK_DOUBLE;
	}
	bip->face_mode = bu_hex_to_bitv((const char *)cp);
	while (*(cp++) != '\0');
    } else {
	bip->thickness = (fastf_t *)NULL;
	bip->face_mode = (struct bu_bitv *)NULL;
    }

    if (bip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	vect_t tmp;

	bip->num_normals = ntohl(*(uint32_t *)&cp[0]);
	cp += SIZEOF_NETWORK_LONG;
	bip->num_face_normals = ntohl(*(uint32_t *)&cp[0]);
	cp += SIZEOF_NETWORK_LONG;

	if (bip->num_normals <= 0) {
	    bip->normals = (fastf_t *)NULL;
	}
	if (bip->num_face_normals <= 0) {
	    bip->face_normals = (int *)NULL;
	}
	if (bip->num_normals > 0) {
	    bip->normals = (fastf_t *)bu_calloc(bip->num_normals * 3, sizeof(fastf_t), "BOT normals");

	    for (i=0; i<bip->num_normals; i++) {
		ntohd((unsigned char *)tmp, (const unsigned char *)cp, 3);
		cp += SIZEOF_NETWORK_DOUBLE * 3;
		MAT4X3VEC(&(bip->normals[i*3]), mat, tmp);
	    }
	}
	if (bip->num_face_normals > 0) {
	    bip->face_normals = (int *)bu_calloc(bip->num_face_normals * 3, sizeof(int), "BOT face normals");

	    for (i=0; i<bip->num_face_normals; i++) {
		bip->face_normals[i*3 + 0] = ntohl(*(uint32_t *)&cp[0]);
		cp += SIZEOF_NETWORK_LONG;
		bip->face_normals[i*3 + 1] = ntohl(*(uint32_t *)&cp[0]);
		cp += SIZEOF_NETWORK_LONG;
		bip->face_normals[i*3 + 2] = ntohl(*(uint32_t *)&cp[0]);
		cp += SIZEOF_NETWORK_LONG;
	    }
	}
    } else {
	bip->normals = (fastf_t *)NULL;
	bip->face_normals = (int *)NULL;
	bip->num_normals = 0;
	bip->num_face_normals = 0;
    }

    bip->tie = NULL;

    return 0;			/* OK */
}


/**
 * R T _ B O T _ E X P O R T 5
 */
int
rt_bot_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_bot_internal *bip;
    struct bu_vls vls;
    unsigned char *cp;
    size_t i;
    size_t rem;

    RT_CK_DB_INTERNAL(ip);
    if (dbip) RT_CK_DBI(dbip);

    if (ip->idb_type != ID_BOT) return -1;
    bip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bip);

    BU_CK_EXTERNAL(ep);

    if (bip->mode == RT_BOT_PLATE || bip->mode == RT_BOT_PLATE_NOCOS) {
	/* build hex string for face mode */
	bu_vls_init(&vls);
	if (bip->face_mode)
	    bu_bitv_to_hex(&vls, bip->face_mode);
    }

    ep->ext_nbytes = 3				/* orientation, mode, bot_flags */
	+ SIZEOF_NETWORK_LONG * (bip->num_faces * 3 + 2) /* faces, num_faces, num_vertices */
	+ SIZEOF_NETWORK_DOUBLE * bip->num_vertices * 3; /* vertices */

    if (bip->mode == RT_BOT_PLATE || bip->mode == RT_BOT_PLATE_NOCOS) {
	ep->ext_nbytes += SIZEOF_NETWORK_DOUBLE * bip->num_faces /* face thicknesses */
	    + bu_vls_strlen(&vls) + 1;	/* face modes */
    }

    if (bip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	ep->ext_nbytes += SIZEOF_NETWORK_DOUBLE * bip->num_normals * 3 /* vertex normals */
	    + SIZEOF_NETWORK_LONG * (bip->num_face_normals * 3 + 2); /* indices into normals array, num_normals, num_face_normals */
    }

    ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes, "BOT external");

    cp = ep->ext_buf;
    rem = ep->ext_nbytes;

    *(uint32_t *)&cp[0] = htonl(bip->num_vertices);
    cp += SIZEOF_NETWORK_LONG;
    rem -= SIZEOF_NETWORK_LONG;

    *(uint32_t *)&cp[0] = htonl(bip->num_faces);
    cp += SIZEOF_NETWORK_LONG;
    rem -= SIZEOF_NETWORK_LONG;

    *cp++ = bip->orientation;
    *cp++ = bip->mode;
    *cp++ = bip->bot_flags;
    rem -= 3;

    for (i=0; i<bip->num_vertices; i++) {
	point_t tmp;

	VSCALE(tmp, &bip->vertices[i*3], local2mm);
	htond(cp, (unsigned char *)tmp, 3);
	cp += SIZEOF_NETWORK_DOUBLE * 3;
	rem -= SIZEOF_NETWORK_DOUBLE * 3;
    }

    for (i=0; i<bip->num_faces; i++) {
	*(uint32_t *)&cp[0] = htonl(bip->faces[i*3 + 0]);
	cp += SIZEOF_NETWORK_LONG;
	rem -= SIZEOF_NETWORK_LONG;

	*(uint32_t *)&cp[0] = htonl(bip->faces[i*3 + 1]);
	cp += SIZEOF_NETWORK_LONG;
	rem -= SIZEOF_NETWORK_LONG;

	*(uint32_t *)&cp[0] = htonl(bip->faces[i*3 + 2]);
	cp += SIZEOF_NETWORK_LONG;
	rem -= SIZEOF_NETWORK_LONG;
    }

    if (bip->mode == RT_BOT_PLATE || bip->mode == RT_BOT_PLATE_NOCOS) {
	for (i=0; i<bip->num_faces; i++) {
	    fastf_t tmp;

	    tmp = bip->thickness[i] * local2mm;
	    htond(cp, (const unsigned char *)&tmp, 1);
	    cp += SIZEOF_NETWORK_DOUBLE;
	    rem -= SIZEOF_NETWORK_DOUBLE;
	}
	bu_strlcpy((char *)cp, bu_vls_addr(&vls), rem);
	cp += bu_vls_strlen(&vls);
	rem -= bu_vls_strlen(&vls);
	*cp = '\0';
	cp++;
	rem--;
	bu_vls_free(&vls);
    }

    if (bip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	*(uint32_t *)&cp[0] = htonl(bip->num_normals);
	cp += SIZEOF_NETWORK_LONG;
	rem -= SIZEOF_NETWORK_LONG;

	*(uint32_t *)&cp[0] = htonl(bip->num_face_normals);
	cp += SIZEOF_NETWORK_LONG;
	rem -= SIZEOF_NETWORK_LONG;

	if (bip->num_normals > 0) {
	    htond(cp, (unsigned char*)bip->normals, bip->num_normals*3);
	    cp += SIZEOF_NETWORK_DOUBLE * 3 * bip->num_normals;
	    rem -= SIZEOF_NETWORK_DOUBLE * 3 * bip->num_normals;
	}
	if (bip->num_face_normals > 0) {
	    for (i=0; i<bip->num_face_normals; i++) {
		*(uint32_t *)&cp[0] = htonl(bip->face_normals[i*3 + 0]);
		cp += SIZEOF_NETWORK_LONG;
		rem -= SIZEOF_NETWORK_LONG;

		*(uint32_t *)&cp[0] = htonl(bip->face_normals[i*3 + 1]);
		cp += SIZEOF_NETWORK_LONG;
		rem -= SIZEOF_NETWORK_LONG;

		*(uint32_t *)&cp[0] = htonl(bip->face_normals[i*3 + 2]);
		cp += SIZEOF_NETWORK_LONG;
		rem -= SIZEOF_NETWORK_LONG;
	    }
	}
    }

    return 0;
}


static char *unoriented="unoriented";
static char *ccw="counter-clockwise";
static char *cw="clockwise";
static char *unknown_orientation="unknown orientation";
static char *surface="\tThis is a surface with no volume\n";
static char *solid="\tThis is a solid object (not just a surface)\n";
static char *plate="\tThis is a FASTGEN plate mode solid\n";
static char *nocos="\tThis is a plate mode solid with no obliquity angle effect\n";
static char *unknown_mode="\tunknown mode\n";

/**
 * R T _ B O T _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_bot_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    struct rt_bot_internal *bot_ip =
	(struct rt_bot_internal *)ip->idb_ptr;
    char buf[256];
    char *orientation, *mode;
    size_t i;

    size_t badVertexCount = 0;

    RT_BOT_CK_MAGIC(bot_ip);
    bu_vls_strcat(str, "Bag of triangles (BOT)\n");

    switch (bot_ip->orientation) {
	case RT_BOT_UNORIENTED:
	    orientation = unoriented;
	    break;
	case RT_BOT_CCW:
	    orientation = ccw;
	    break;
	case RT_BOT_CW:
	    orientation = cw;
	    break;
	default:
	    orientation = unknown_orientation;
	    break;
    }
    switch (bot_ip->mode) {
	case RT_BOT_SURFACE:
	    mode = surface;
	    break;
	case RT_BOT_SOLID:
	    mode = solid;
	    break;
	case RT_BOT_PLATE:
	    mode = plate;
	    break;
	case RT_BOT_PLATE_NOCOS:
	    mode = nocos;
	    break;
	default:
	    mode = unknown_mode;
	    break;
    }
    snprintf(buf, 256, "\t%lu vertices, %lu faces (%s)\n",
	     (long unsigned)bot_ip->num_vertices,
	     (long unsigned)bot_ip->num_faces,
	     orientation);
    bu_vls_strcat(str, buf);
    bu_vls_strcat(str, mode);
    if ((bot_ip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) && bot_ip->num_normals > 0) {
	bu_vls_strcat(str, "\twith surface normals");
	if (bot_ip->bot_flags & RT_BOT_USE_NORMALS) {
	    bu_vls_strcat(str, " (they will be used)\n");
	} else {
	    bu_vls_strcat(str, " (they will be ignored)\n");
	}
    }

    if (!verbose)
	return 0;
	
    for (i=0; i<bot_ip->num_faces; i++) {
	size_t j, k;
	point_t pt[3];

	snprintf(buf, 256, "\tface %lu:", (long unsigned)i);
	bu_vls_strcat(str, buf);

	for (j=0; j<3; j++) {
	    size_t ptnum;

	    ptnum = bot_ip->faces[i*3+j];
	    if (ptnum >= bot_ip->num_vertices) {
		bu_vls_strcat(str, " (\?\?\? \?\?\? \?\?\?)");
		badVertexCount++;
	    } else {
		VSCALE(pt[j], &bot_ip->vertices[ptnum*3], mm2local);
		snprintf(buf, 256, " (%g %g %g)", V3INTCLAMPARGS(pt[j]));
		bu_vls_strcat(str, buf);
	    }
	}
	if ((bot_ip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) && bot_ip->num_normals > 0) {
	    bu_vls_strcat(str, " normals: ");
	    for (k=0; k<3; k++) {
		size_t idx;

		idx = i*3 + k;
		if ((size_t)bot_ip->face_normals[idx] >= bot_ip->num_normals) {
		    bu_vls_strcat(str, "none ");
		} else {
		    snprintf(buf, 256, "(%g %g %g) ", V3INTCLAMPARGS(&bot_ip->normals[bot_ip->face_normals[idx]*3]));
		    bu_vls_strcat(str, buf);
		}
	    }
	    bu_vls_strcat(str, "\n");
	} else {
	    bu_vls_strcat(str, "\n");
	}
	if (bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS) {
	    char *face_mode;

	    if (BU_BITTEST(bot_ip->face_mode, i))
		face_mode = "appended to hit point";
	    else
		face_mode = "centered about hit point";
	    snprintf(buf, 256, "\t\tthickness = %g, %s\n", INTCLAMP(mm2local*bot_ip->thickness[i]), face_mode);
	    bu_vls_strcat(str, buf);
	}
    }
    if (badVertexCount > 0) {
	snprintf(buf, 256, "\tThis BOT has %lu invalid references to vertices\n", (long unsigned)badVertexCount);
	bu_vls_strcat(str, buf);
    }

    return 0;
}


void
rt_bot_ifree2(struct rt_bot_internal *bot_ip)
{
    RT_BOT_CK_MAGIC(bot_ip);
    bot_ip->magic = 0;			/* sanity */

    if (bot_ip->tie)
	bot_ip->tie = NULL;

    if (bot_ip->vertices) {
	bu_free(bot_ip->vertices, "BOT vertices");
	bot_ip->vertices = NULL;
	bot_ip->num_vertices = 0;
    }
    if (bot_ip->faces) {
	bu_free(bot_ip->faces, "BOT faces");
	bot_ip->faces = NULL;
	bot_ip->num_faces = 0;
    }

    if (bot_ip->mode == RT_BOT_PLATE || bot_ip->mode == RT_BOT_PLATE_NOCOS) {
	if (bot_ip->thickness) {
	    bu_free(bot_ip->thickness, "BOT thickness");
	    bot_ip->thickness = NULL;
	}
	if (bot_ip->face_mode) {
	    bu_free(bot_ip->face_mode, "BOT face_mode");
	    bot_ip->face_mode = NULL;
	}
    }

    bu_free(bot_ip, "bot ifree");
}


/**
 * R T _ B O T _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_bot_ifree(struct rt_db_internal *ip)
{
    struct rt_bot_internal *bot_ip;

    RT_CK_DB_INTERNAL(ip);

    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    rt_bot_ifree2(bot_ip);
    ip->idb_ptr = NULL; /* sanity */
}

int
rt_bot_xform(struct rt_db_internal *op, const fastf_t *mat, struct rt_db_internal *ip, const int release, struct db_i *dbip)
{
    struct rt_bot_internal *botip, *botop;
    size_t i;
    point_t pt;

    RT_CK_DB_INTERNAL(ip);
    botip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(botip);
    if (dbip) RT_CK_DBI(dbip);

    if (op != ip && !release) {
	RT_DB_INTERNAL_INIT(op);
	BU_GETSTRUCT(botop, rt_bot_internal);
	botop->magic = RT_BOT_INTERNAL_MAGIC;
	botop->mode = botip->mode;
	botop->orientation = botip->orientation;
	botop->bot_flags = botip->bot_flags;
	botop->num_vertices = botip->num_vertices;
	botop->num_faces = botip->num_faces;
	if (botop->num_vertices > 0) {
	    botop->vertices = (fastf_t *)bu_malloc(botip->num_vertices * 3 * sizeof(fastf_t), "botop->vertices");
	}
	if (botop->num_faces > 0) {
	    botop->faces = (int *)bu_malloc(botip->num_faces * 3 * sizeof(int), "botop->faces");
	    memcpy(botop->faces, botip->faces, botop->num_faces * 3 * sizeof(int));
	}
	if (botip->thickness)
	    botop->thickness = (fastf_t *)bu_malloc(botip->num_faces * sizeof(fastf_t), "botop->thickness");
	if (botip->face_mode)
	    botop->face_mode = bu_bitv_dup(botip->face_mode);
	if (botip->thickness) {
	    for (i=0; i<botip->num_faces; i++)
		botop->thickness[i] = botip->thickness[i];
	}

	if (botop->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	    botop->num_normals = botip->num_normals;
	    botop->normals = (fastf_t *)bu_calloc(botop->num_normals * 3, sizeof(fastf_t), "BOT normals");
	    botop->face_normals = (int *)bu_calloc(botop->num_faces * 3, sizeof(int), "BOT face normals");
	    memcpy(botop->face_normals, botip->face_normals, botop->num_faces * 3 * sizeof(int));
	}
	op->idb_ptr = (genptr_t)botop;
	op->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	op->idb_type = ID_BOT;
	op->idb_meth = &rt_functab[ID_BOT];
    } else
	botop = botip;

    if (ip != op) {
	if (ip->idb_avs.magic == BU_AVS_MAGIC) {
	    bu_avs_init(&op->idb_avs, ip->idb_avs.count, "avs");
	    bu_avs_merge(&op->idb_avs, &ip->idb_avs);
	}
    }

    for (i=0; i<botip->num_vertices; i++) {
	MAT4X3PNT(pt, mat, &botip->vertices[i*3]);
	VMOVE(&botop->vertices[i*3], pt);
    }

    for (i=0; i<botip->num_normals; i++) {
	MAT4X3VEC(pt, mat, &botip->normals[i*3]);
	VMOVE(&botop->normals[i*3], pt);
    }

    if (release && op != ip) {
	rt_bot_ifree(ip);
    }

    return 0;
}


int
rt_bot_find_v_nearest_pt2(
    const struct rt_bot_internal *bot,
    const point_t pt2,
    const mat_t mat)
{
    point_t v;
    size_t idx;
    fastf_t dist=MAX_FASTF;
    int closest=-1;

    RT_BOT_CK_MAGIC(bot);

    for (idx=0; idx < bot->num_vertices; idx++) {
	fastf_t tmp_dist;
	fastf_t tmpx, tmpy;

	MAT4X3PNT(v, mat, &bot->vertices[idx*3])
	    tmpx = v[X] - pt2[X];
	tmpy = v[Y] - pt2[Y];
	tmp_dist = tmpx * tmpx + tmpy * tmpy;
	if (tmp_dist < dist) {
	    dist = tmp_dist;
	    closest = idx;
	}
    }

    return closest;
}


int
rt_bot_edge_in_list(const int v1, const int v2, const int edge_list[], const size_t edge_count)
{
    size_t i;
    int ev1, ev2;

    for (i=0; i<edge_count; i++) {
	ev1 = edge_list[i*2 + 0];
	ev2 = edge_list[i*2 + 1];

	if (ev1 == v1 && ev2 == v2)
	    return 1;

	if (ev1 == v2 && ev2 == v1)
	    return 1;
    }

    return 0;
}


/**
 * This routine finds the edge closest to the 2D point "pt2", and
 * returns the edge as two vertex indices (vert1 and vert2). These
 * vertices are ordered (closest to pt2 is first)
 */
int
rt_bot_find_e_nearest_pt2(
    int *vert1,
    int *vert2,
    const struct rt_bot_internal *bot,
    const point_t pt2,
    const mat_t mat)
{
    size_t i;
    size_t v1, v2, v3;
    fastf_t dist=MAX_FASTF, tmp_dist;
    int *edge_list;
    size_t edge_count=0;
    struct bn_tol tol;

    RT_BOT_CK_MAGIC(bot);

    if (bot->num_faces < 1)
	return -1;

    /* first build a list of edges */
    edge_list = (int *)bu_calloc(bot->num_faces * 3 * 2, sizeof(int), "bot edge list");

    for (i=0; i<bot->num_faces; i++) {
	v1 = bot->faces[i*3 + 0];
	v2 = bot->faces[i*3 + 1];
	v3 = bot->faces[i*3 + 2];

	if (!rt_bot_edge_in_list(v1, v2, edge_list, edge_count)) {
	    edge_list[edge_count*2 + 0] = v1;
	    edge_list[edge_count*2 + 1] = v2;
	    edge_count++;
	}
	if (!rt_bot_edge_in_list(v3, v2, edge_list, edge_count)) {
	    edge_list[edge_count*2 + 0] = v3;
	    edge_list[edge_count*2 + 1] = v2;
	    edge_count++;
	}
	if (!rt_bot_edge_in_list(v1, v3, edge_list, edge_count)) {
	    edge_list[edge_count*2 + 0] = v1;
	    edge_list[edge_count*2 + 1] = v3;
	    edge_count++;
	}
    }

    /* build a tyolerance structure for the bn_dist routine */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0;
    tol.dist_sq = 0.0;
    tol.perp = 0.0;
    tol.para =  1.0;

    /* now look for the closest edge */
    for (i=0; i<edge_count; i++) {
	point_t p1, p2, pca;
	vect_t p1_to_pca, p1_to_p2;
	int ret;

	MAT4X3PNT(p1, mat, &bot->vertices[ edge_list[i*2+0]*3]);
	MAT4X3PNT(p2, mat, &bot->vertices[ edge_list[i*2+1]*3]);

	ret = bn_dist_pt2_lseg2(&tmp_dist, pca, p1, p2, pt2, &tol);

	if (ret < 3 || tmp_dist < dist) {
	    switch (ret) {
		case 0:
		    dist = 0.0;
		    if (tmp_dist < 0.5) {
			*vert1 = edge_list[i*2+0];
			*vert2 = edge_list[i*2+1];
		    } else {
			*vert1 = edge_list[i*2+1];
			*vert2 = edge_list[i*2+0];
		    }
		    break;
		case 1:
		    dist = 0.0;
		    *vert1 = edge_list[i*2+0];
		    *vert2 = edge_list[i*2+1];
		    break;
		case 2:
		    dist = 0.0;
		    *vert1 = edge_list[i*2+1];
		    *vert2 = edge_list[i*2+0];
		    break;
		case 3:
		    dist = tmp_dist;
		    *vert1 = edge_list[i*2+0];
		    *vert2 = edge_list[i*2+1];
		    break;
		case 4:
		    dist = tmp_dist;
		    *vert1 = edge_list[i*2+1];
		    *vert2 = edge_list[i*2+0];
		case 5:
		    dist = tmp_dist;
		    V2SUB2(p1_to_pca, pca, p1);
		    V2SUB2(p1_to_p2, p2, p1);
		    if (MAG2SQ(p1_to_pca) / MAG2SQ(p1_to_p2) < 0.25) {
			*vert1 = edge_list[i*2+0];
			*vert2 = edge_list[i*2+1];
		    } else {
			*vert1 = edge_list[i*2+1];
			*vert2 = edge_list[i*2+0];
		    }
		    break;
	    }
	}
    }

    bu_free(edge_list, "bot edge list");

    return 0;
}


static char *modes[]={
    "ERROR: Unrecognized mode",
    "surf",
    "volume",
    "plate",
    "plate_nocos"
};


static char *orientation[]={
    "ERROR: Unrecognized orientation",
    "no",
    "rh",
    "lh"
};


static char *los[]={
    "center",
    "append"
};


/**
 * R T _ B O T _ G E T
 *
 * Examples -
 * db get name fm	get los facemode bit vector
 * db get name fm#	get los face mode of face # (center, append)
 * db get name V	get coords for all vertices
 * db get name V#	get coords for vertex #
 * db get name F	get vertex indices for all faces
 * db get name F#	get vertex indices for face #
 * db get name f	get list of coords for all faces
 * db get name f#	get list of 3 3tuple coords for face #
 * db get name T	get thickness for all faces
 * db get name T#	get thickness for face #
 * db get name N	get list of normals
 * db get name N#	get coords for normal #
 * db get name fn	get list indices into normal vectors for all faces
 * db get name fn#	get list indices into normal vectors for face #
 * db get name nv	get num_vertices
 * db get name nt	get num_faces
 * db get name nn	get num_normals
 * db get name nfn	get num_face_normals
 * db get name mode	get mode (surf, volume, plate, plane_nocos)
 * db get name orient	get orientation (no, rh, lh)
 * db get name flags	get BOT flags
 */
int
rt_bot_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    struct rt_bot_internal *bot=(struct rt_bot_internal *)intern->idb_ptr;
    int status;
    size_t i;
    long li;

    RT_BOT_CK_MAGIC(bot);

    if (attr == (char *)NULL) {
	bu_vls_strcpy(logstr, "bot");
	bu_vls_printf(logstr, " mode %s orient %s",
		      modes[bot->mode], orientation[bot->orientation]);
	bu_vls_printf(logstr, " flags {");
	if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	    bu_vls_printf(logstr, " has_normals");
	}
	if (bot->bot_flags & RT_BOT_USE_NORMALS) {
	    bu_vls_printf(logstr, " use_normals");
	}
	if (bot->bot_flags & RT_BOT_USE_FLOATS) {
	    bu_vls_printf(logstr, " use_floats");
	}
	bu_vls_printf(logstr, "} V {");
	for (i=0; i<bot->num_vertices; i++)
	    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
			  V3ARGS(&bot->vertices[i*3]));
	bu_vls_strcat(logstr, "} F {");
	for (i=0; i<bot->num_faces; i++)
	    bu_vls_printf(logstr, " { %lu %lu %lu }",
			  V3ARGS(&bot->faces[i*3]));
	bu_vls_strcat(logstr, "}");
	if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	    bu_vls_strcat(logstr, " T {");
	    for (i=0; i<bot->num_faces; i++)
		bu_vls_printf(logstr, " %.25G", bot->thickness[i]);
	    bu_vls_strcat(logstr, "} fm ");
	    bu_bitv_to_hex(logstr, bot->face_mode);
	}
	if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	    bu_vls_printf(logstr, " N {");
	    for (i=0; i<bot->num_normals; i++) {
		bu_vls_printf(logstr, " { %.25G %.25G %.25G }", V3ARGS(&bot->normals[i*3]));
	    }
	    bu_vls_printf(logstr, "} fn {");
	    for (i=0; i<bot->num_faces; i++) {
		bu_vls_printf(logstr, " { %lu %lu %lu }", V3ARGS(&bot->face_normals[i*3]));
	    }
	    bu_vls_printf(logstr, "}");
	}
	status = BRLCAD_OK;
    } else {
	if (attr[0] == 'N') {
	    if (attr[1] == '\0') {
		if (!(bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) || bot->num_normals < 1) {
		    bu_vls_strcat(logstr, "{}");
		} else {
		    for (i=0; i<bot->num_normals; i++) {
			bu_vls_printf(logstr, " { %.25G %.25G %.25G }", V3ARGS(&bot->normals[i*3]));
		    }
		}
		status = BRLCAD_OK;
	    } else {
		li = atol(&attr[1]);
		if (li < 0 || (size_t)li >= bot->num_normals) {
		    bu_vls_printf(logstr, "Specified normal index [%ld] is out of range", li);
		    status = BRLCAD_ERROR;
		} else {
		    bu_vls_printf(logstr, "%.25G %.25G %.25G", V3ARGS(&bot->normals[li*3]));
		    status = BRLCAD_OK;
		}
	    }
	} else if (!strncmp(attr, "fn", 2)) {
	    if (attr[2] == '\0') {
		for (i=0; i<bot->num_faces; i++) {
		    bu_vls_printf(logstr, " { %lu %lu %lu }", V3ARGS(&bot->face_normals[i*3]));
		}
		status = BRLCAD_OK;
	    } else {
		li = atol(&attr[2]);
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "Specified face index [%ld] is out of range", li);
		    status = BRLCAD_ERROR;
		} else {
		    bu_vls_printf(logstr, "%lu %lu %lu", V3ARGS(&bot->face_normals[li*3]));
		    status = BRLCAD_OK;
		}
	    }
	} else if (BU_STR_EQUAL(attr, "nn")) {
	    if (!(bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) || bot->num_normals < 1) {
		bu_vls_strcat(logstr, "0");
	    } else {
		bu_vls_printf(logstr, "%lu", (long unsigned)bot->num_normals);
	    }
	    status = BRLCAD_OK;
	} else if (BU_STR_EQUAL(attr, "nfn")) {
	    if (!(bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) || bot->num_face_normals < 1) {
		bu_vls_strcat(logstr, "0");
	    } else {
		bu_vls_printf(logstr, "%lu", (long unsigned)bot->num_face_normals);
	    }
	    status = BRLCAD_OK;
	} else if (!strncmp(attr, "fm", 2)) {
	    if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
		bu_vls_strcat(logstr, "Only plate mode BOTs have face_modes");
		status = BRLCAD_ERROR;
	    } else {
		if (attr[2] == '\0') {
		    bu_bitv_to_hex(logstr, bot->face_mode);
		    status = BRLCAD_OK;
		} else {
		    li = atol(&attr[2]);
		    if (li < 0 || (size_t)li >= bot->num_faces) {
			bu_vls_printf(logstr, "face number [%ld] out of range (0..%zu)", li, bot->num_faces-1);
			status = BRLCAD_ERROR;
		    } else {
			bu_vls_printf(logstr, "%s",
				      los[BU_BITTEST(bot->face_mode, li)?1:0]);
			status = BRLCAD_OK;
		    }
		}
	    }
	} else if (attr[0] == 'V') {
	    if (attr[1] != '\0') {
		li = atol(&attr[1]);
		if (li < 0 || (size_t)li >= bot->num_vertices) {
		    bu_vls_printf(logstr, "vertex number [%ld] out of range (0..%zu)", li, bot->num_vertices-1);
		    status = BRLCAD_ERROR;
		} else {
		    bu_vls_printf(logstr, "%.25G %.25G %.25G",
				  V3ARGS(&bot->vertices[li*3]));
		    status = BRLCAD_OK;
		}
	    } else {
		for (i=0; i<bot->num_vertices; i++)
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  V3ARGS(&bot->vertices[i*3]));
		status = BRLCAD_OK;
	    }
	} else if (attr[0] == 'F') {
	    /* Retrieve one face, as vertex indices */
	    if (attr[1] == '\0') {
		for (i=0; i<bot->num_faces; i++)
		    bu_vls_printf(logstr, " { %lu %lu %lu }",
				  V3ARGS(&bot->faces[i*3]));
		status = BRLCAD_OK;
	    } else {
		li = atol(&attr[1]);
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "face number [%ld] out of range (0..%zu)", li, bot->num_faces-1);
		    status = BRLCAD_ERROR;
		} else {
		    bu_vls_printf(logstr, "%lu %lu %lu",
				  V3ARGS(&bot->faces[li*3]));
		    status = BRLCAD_OK;
		}
	    }
	} else if (BU_STR_EQUAL(attr, "flags")) {
	    bu_vls_printf(logstr, "{");
	    if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
		bu_vls_printf(logstr, " has_normals");
	    }
	    if (bot->bot_flags & RT_BOT_USE_NORMALS) {
		bu_vls_printf(logstr, " use_normals");
	    }
	    if (bot->bot_flags & RT_BOT_USE_FLOATS) {
		bu_vls_printf(logstr, " use_floats");
	    }
	    bu_vls_printf(logstr, "}");
	    status = BRLCAD_OK;
	} else if (attr[0] == 'f') {
	    size_t indx;
	    /* Retrieve one face, as list of 3 3tuple coordinates */
	    if (attr[1] == '\0') {
		for (i=0; i<bot->num_faces; i++) {
		    indx = bot->faces[i*3+0];
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  bot->vertices[indx*3+0],
				  bot->vertices[indx*3+1],
				  bot->vertices[indx*3+2]);
		    indx = bot->faces[i*3+1];
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  bot->vertices[indx*3+0],
				  bot->vertices[indx*3+1],
				  bot->vertices[indx*3+2]);
		    indx = bot->faces[i*3+2];
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  bot->vertices[indx*3+0],
				  bot->vertices[indx*3+1],
				  bot->vertices[indx*3+2]);
		}
		status = BRLCAD_OK;
	    } else {
		li = atol(&attr[1]);
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "face number [%ld] out of range (0..%zu)", li, bot->num_faces-1);
		    status = BRLCAD_ERROR;
		} else {
		    indx = bot->faces[li*3+0];
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  bot->vertices[indx*3+0],
				  bot->vertices[indx*3+1],
				  bot->vertices[indx*3+2]);
		    indx = bot->faces[li*3+1];
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  bot->vertices[indx*3+0],
				  bot->vertices[indx*3+1],
				  bot->vertices[indx*3+2]);
		    indx = bot->faces[li*3+2];
		    bu_vls_printf(logstr, " { %.25G %.25G %.25G }",
				  bot->vertices[indx*3+0],
				  bot->vertices[indx*3+1],
				  bot->vertices[indx*3+2]);
		    status = BRLCAD_OK;
		}
	    }
	} else if (attr[0] == 'T') {
	    if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
		bu_vls_strcat(logstr, "Only plate mode BOTs have thicknesses");
		status = BRLCAD_ERROR;
	    } else {
		if (attr[1] == '\0') {
		    for (i=0; i<bot->num_faces; i++)
			bu_vls_printf(logstr, " %.25G", bot->thickness[i]);
		    status = BRLCAD_OK;
		} else {
		    li = atol(&attr[1]);
		    if (li < 0 || (size_t)li >= bot->num_faces) {
			bu_vls_printf(logstr, "face number [%ld] out of range (0..%zu)", li, bot->num_faces-1);
			status = BRLCAD_ERROR;
		    } else {
			bu_vls_printf(logstr, " %.25G", bot->thickness[li]);
			status = BRLCAD_OK;
		    }
		}
	    }
	} else if (BU_STR_EQUAL(attr, "nv")) {
	    bu_vls_printf(logstr, "%zu", bot->num_vertices);
	    status = BRLCAD_OK;
	} else if (BU_STR_EQUAL(attr, "nt")) {
	    bu_vls_printf(logstr, "%zu", bot->num_faces);
	    status = BRLCAD_OK;
	} else if (BU_STR_EQUAL(attr, "mode")) {
	    bu_vls_printf(logstr, "%s", modes[bot->mode]);
	    status = BRLCAD_OK;
	} else if (BU_STR_EQUAL(attr, "orient")) {
	    bu_vls_printf(logstr, "%s", orientation[bot->orientation]);
	    status = BRLCAD_OK;
	} else {
	    bu_vls_printf(logstr, "BoT has no attribute '%s'", attr);
	    status = BRLCAD_ERROR;
	}
    }

    return status;
}


int
bot_check_vertex_indices(struct bu_vls *logstr, struct rt_bot_internal *bot)
{
    size_t badVertexCount = 0;
    size_t i;
    for (i=0 ; i<bot->num_faces ; i++) {
	int k;
	for (k=0 ; k<3 ; k++) {
	    int vertex_no = bot->faces[i*3+k];
	    if (vertex_no < 0 || (size_t)vertex_no >= bot->num_vertices) {
		bu_vls_printf(logstr, "WARNING: BOT has illegal vertex index (%d) in face #(%zu)\n", vertex_no, i);
		badVertexCount++;
	    }
	}
    }
    return badVertexCount;
}


/**
 * R T _ B O T _ A D J U S T
 *
 * Examples -
 * db adjust name fm		set los facemode bit vector
 * db adjust name fm#		set los face mode of face # (center, append)
 * db adjust name V		set coords for all vertices
 * db adjust name V#		set coords for vertex #
 * db adjust name F		set vertex indices for all faces
 * db adjust name F#		set vertex indices for face #
 * db adjust name T		set thickness for all faces
 * db adjust name T#		set thickness for face #
 * db adjust name N		set list of normals
 * db adjust name N#		set coords for normal #
 * db adjust name fn		set list indices into normal vectors for all faces
 * db adjust name fn#		set list indices into normal vectors for face #
 * db adjust name nn		set num_normals
 * db adjust name mode		set mode (surf, volume, plate, plane_nocos)
 * db adjust name orient	set orientation (no, rh, lh)
 * db adjust name flags		set flags
 */
int
rt_bot_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_bot_internal *bot;
    Tcl_Obj *obj, **obj_array;
    int len;
    size_t i;
    long li;

    RT_CK_DB_INTERNAL(intern);
    bot = (struct rt_bot_internal *)intern->idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    while (argc >= 2) {
	obj = Tcl_NewStringObj(argv[1], -1);

	if (!strncmp(argv[0], "fm", 2)) {
	    if (argv[0][2] == '\0') {
		if (bot->face_mode)
		    bu_free(bot->face_mode, "bot->face_mode");
		bot->face_mode = bu_hex_to_bitv(argv[1]);
	    } else {
		li = atol(&(argv[0][2]));
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "face number [%ld] out of range (0..%zu)", li, bot->num_faces-1);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}

		if (isdigit(*argv[1])) {
		    if (atol(argv[1]) == 0)
			BU_BITCLR(bot->face_mode, li);
		    else
			BU_BITSET(bot->face_mode, li);
		} else if (BU_STR_EQUAL(argv[1], "append"))
		    BU_BITSET(bot->face_mode, li);
		else
		    BU_BITCLR(bot->face_mode, li);
	    }
	} else if (BU_STR_EQUAL(argv[0], "nn")) {
	    long new_num=0;
	    size_t old_num = bot->num_normals;

	    new_num = atol(Tcl_GetStringFromObj(obj, NULL));
	    if (new_num < 0) {
		bu_vls_printf(logstr, "Number of normals [%ld] may not be less than 0", new_num);
		Tcl_DecrRefCount(obj);
		return BRLCAD_ERROR;
	    }

	    if (new_num == 0) {
		bot->num_normals = 0;
		if (bot->normals) {
		    bu_free(bot->normals, "BOT normals");
		    bot->normals = (fastf_t *)NULL;
		}
		bot->num_normals = 0;
	    } else {
		if ((size_t)new_num != old_num) {
		    bot->num_normals = (size_t)new_num;
		    if (bot->normals) {
			bot->normals = (fastf_t *)bu_realloc((char *)bot->normals, bot->num_normals * 3 * sizeof(fastf_t), "BOT normals");
		    } else {
			bot->normals = (fastf_t *)bu_calloc(bot->num_normals * 3, sizeof(fastf_t), "BOT normals");
		    }

		    if ((size_t)new_num > old_num) {
			for (i = old_num; i<(size_t)new_num; i++) {
			    VSET(&bot->normals[i*3], 0, 0, 0);
			}
		    }
		}

	    }

	} else if (!strncmp(argv[0], "fn", 2)) {
	    char *f_str;

	    if (argv[0][2] == '\0') {
		(void)Tcl_ListObjGetElements(brlcad_interp, obj, &len, &obj_array);
		if ((size_t)len != bot->num_faces || len <= 0) {
		    bu_vls_printf(logstr, "Only %ld face normals? Must provide normals for all faces!!!", len);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		if (bot->face_normals)
		    bu_free(bot->face_normals, "BOT face_normals");
		bot->face_normals = (int *)bu_calloc(len*3, sizeof(int), "BOT face_normals");
		bot->num_face_normals = len;
		for (i=0; i<(size_t)len; i++) {
		    f_str = Tcl_GetStringFromObj(obj_array[i], NULL);
		    while (isspace(*f_str)) f_str++;

		    if (*f_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of face_normals");
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }

		    /* normal [X,y,z] */
		    li = atol(f_str);
		    if (li < 0) {
			bu_vls_printf(logstr, "invalid face normal index [%ld]", li);
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }
		    bot->face_normals[i*3+0] = (size_t)li;

		    f_str = bu_next_token(f_str);
		    if (*f_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of face_normals");
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }

		    /* normal [x,Y,z] */
		    li = atol(f_str);
		    if (li < 0) {
			bu_vls_printf(logstr, "invalid face normal index [%ld]", li);
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }

		    bot->face_normals[i*3+1] = li;
		    f_str = bu_next_token(f_str);
		    if (*f_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of face_normals");
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }

		    /* normal [x,y,Z] */
		    li = atol(f_str);
		    if (li < 0) {
			bu_vls_printf(logstr, "invalid face normal index [%ld]", li);
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }
		    bot->face_normals[i*3+2] = li;
		}
		bot->bot_flags |= RT_BOT_HAS_SURFACE_NORMALS;
	    } else {
		li = atol(&argv[0][2]);
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "face_normal number [%ld] out of range!!!", li);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		i = (size_t)li;
		f_str = Tcl_GetStringFromObj(obj, NULL);
		while (isspace(*f_str)) f_str++;

		li = atol(f_str);
		if (li < 0) {
		    bu_vls_printf(logstr, "invalid face normal index [%ld]", li);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->face_normals[i*3+0] = li;
		f_str = bu_next_token(f_str);
		if (*f_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}

		li = atol(f_str);
		if (li < 0) {
		    bu_vls_printf(logstr, "invalid face normal index [%ld]", li);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->face_normals[i*3+1] = li;
		f_str = bu_next_token(f_str);
		if (*f_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}

		li = atol(f_str);
		if (li < 0) {
		    bu_vls_printf(logstr, "invalid face normal index [%ld]", li);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->face_normals[i*3+2] = li;
	    }
	} else if (argv[0][0] == 'N') {
	    char *v_str;

	    if (argv[0][1] == '\0') {
		(void)Tcl_ListObjGetElements(brlcad_interp, obj, &len, &obj_array);
		if (len <= 0) {
		    bu_vls_printf(logstr, "Must provide at least one normal!!!");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->num_normals = len;
		if (bot->normals) {
		    bu_free(bot->normals, "BOT normals");
		    bot->normals = (fastf_t *)NULL;
		}
		bot->num_normals = 0;
		bot->normals = (fastf_t *)bu_calloc(len*3, sizeof(fastf_t), "BOT normals");
		for (i=0; i<(size_t)len; i++) {
		    v_str = Tcl_GetStringFromObj(obj_array[i], NULL);
		    while (isspace(*v_str)) v_str++;
		    if (*v_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of normals");
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }
		    bot->normals[i*3+0] = atof(v_str);
		    v_str = bu_next_token(v_str);
		    if (*v_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of normals");
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }
		    bot->normals[i*3+1] = atof(v_str);
		    v_str = bu_next_token(v_str);
		    if (*v_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of normals");
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }
		    bot->normals[i*3+2] = atof(v_str);
		}
		bot->bot_flags |= RT_BOT_HAS_SURFACE_NORMALS;
	    } else {
		li = atol(&argv[0][1]);
		if (li < 0 || (size_t)li >= bot->num_normals) {
		    bu_vls_printf(logstr, "normal number [%ld] out of range!!!", li);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		i = (size_t)li;
		v_str = Tcl_GetStringFromObj(obj, NULL);
		while (isspace(*v_str)) v_str++;

		bot->normals[i*3+0] = atof(v_str);
		v_str = bu_next_token(v_str);
		if (*v_str == '\0') {
		    bu_vls_printf(logstr, "incomplete normal");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->normals[i*3+1] = atof(v_str);
		v_str = bu_next_token(v_str);
		if (*v_str == '\0') {
		    bu_vls_printf(logstr, "incomplete normal");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->normals[i*3+2] = atof(v_str);
	    }
	} else if (argv[0][0] == 'V') {
	    char *v_str;

	    if (argv[0][1] == '\0') {
		(void)Tcl_ListObjGetElements(brlcad_interp, obj, &len, &obj_array);
		if (len <= 0) {
		    bu_vls_printf(logstr, "Must provide at least one vertex!!!");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->num_vertices = len;
		if (bot->vertices)
		    bu_free(bot->vertices, "BOT vertices");
		bot->vertices = (fastf_t *)bu_calloc(len*3, sizeof(fastf_t), "BOT vertices");
		for (i=0; i<(size_t)len; i++) {
		    v_str = Tcl_GetStringFromObj(obj_array[i], NULL);
		    while (isspace(*v_str)) v_str++;
		    if (*v_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of vertices");
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }
		    bot->vertices[i*3+0] = atof(v_str);
		    v_str = bu_next_token(v_str);
		    if (*v_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of vertices");
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }
		    bot->vertices[i*3+1] = atof(v_str);
		    v_str = bu_next_token(v_str);
		    if (*v_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of vertices");
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }
		    bot->vertices[i*3+2] = atof(v_str);
		}
	    } else {
		li = atol(&argv[0][1]);
		if (li < 0 || (size_t)li >= bot->num_vertices) {
		    bu_vls_printf(logstr, "vertex number [%ld] out of range!!!", li);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		i = (size_t)li;
		v_str = Tcl_GetStringFromObj(obj, NULL);
		while (isspace(*v_str)) v_str++;

		bot->vertices[i*3+0] = atof(v_str);
		v_str = bu_next_token(v_str);
		if (*v_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->vertices[i*3+1] = atof(v_str);
		v_str = bu_next_token(v_str);
		if (*v_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->vertices[i*3+2] = atof(v_str);
	    }
	} else if (argv[0][0] == 'F') {
	    char *f_str;

	    if (argv[0][1] == '\0') {
		(void)Tcl_ListObjGetElements(brlcad_interp, obj, &len, &obj_array);
		if (len <= 0) {
		    bu_vls_printf(logstr, "Must provide at least one face!!!");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->num_faces = len;
		if (bot->faces)
		    bu_free(bot->faces, "BOT faces");
		bot->faces = (int *)bu_calloc(len*3, sizeof(int), "BOT faces");
		if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
		    if (!bot->face_normals) {
			bot->face_normals = (int *)bu_malloc(bot->num_faces * 3 * sizeof(int), "bot->face_normals");
			bot->num_face_normals = bot->num_faces;
			for (i=0; i<bot->num_face_normals; i++) {
			    VSETALL(&bot->face_normals[i*3], -1);
			}
		    } else if (bot->num_face_normals < bot->num_faces) {
			bot->face_normals = (int *)bu_realloc(bot->face_normals, bot->num_faces * 3 * sizeof(int), "bot->face_normals");
			for (i=bot->num_face_normals; i<bot->num_faces; i++) {
			    VSETALL(&bot->face_normals[i*3], -1);
			}
			bot->num_face_normals = bot->num_faces;
		    }
		}
		for (i=0; i<(size_t)len; i++) {
		    f_str = Tcl_GetStringFromObj(obj_array[i], NULL);
		    while (isspace(*f_str)) f_str++;

		    if (*f_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of faces");
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }

		    li = atol(f_str);
		    if (li < 0) {
			bu_vls_printf(logstr, "invalid face index [%ld]", li);
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }
		    bot->faces[i*3+0] = li;

		    f_str = bu_next_token(f_str);
		    if (*f_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of faces");
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }

		    li = atol(f_str);
		    if (li < 0) {
			bu_vls_printf(logstr, "invalid face index [%ld]", li);
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }
		    bot->faces[i*3+1] = li;

		    f_str = bu_next_token(f_str);
		    if (*f_str == '\0') {
			bu_vls_printf(logstr, "incomplete list of faces");
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }

		    li = atol(f_str);
		    if (li < 0) {
			bu_vls_printf(logstr, "invalid face index [%ld]", li);
			Tcl_DecrRefCount(obj);
			return BRLCAD_ERROR;
		    }
		    bot->faces[i*3+2] = li;
		}
	    } else {
		li = atol(&argv[0][1]);
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "face number [%ld] out of range!!!", li);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		i = (size_t)li;
		f_str = Tcl_GetStringFromObj(obj, NULL);
		while (isspace(*f_str)) f_str++;

		li = atol(f_str);
		if (li < 0) {
		    bu_vls_printf(logstr, "invalid face index [%ld]", li);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->faces[i*3+0] = li;

		f_str = bu_next_token(f_str);
		if (*f_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}

		li = atol(f_str);
		if (li < 0) {
		    bu_vls_printf(logstr, "invalid face index [%ld]", li);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->faces[i*3+1] = li;
		f_str = bu_next_token(f_str);
		if (*f_str == '\0') {
		    bu_vls_printf(logstr, "incomplete vertex");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}

		li = atol(f_str);
		if (li < 0) {
		    bu_vls_printf(logstr, "invalid face index [%ld]", li);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->faces[i*3+2] = li;
	    }
	    bot_check_vertex_indices(logstr, bot);
	} else if (argv[0][0] ==  'T') {
	    char *t_str;

	    if (argv[0][1] == '\0') {
		(void)Tcl_ListObjGetElements(brlcad_interp, obj, &len, &obj_array);
		if (len <= 0) {
		    bu_vls_printf(logstr, "Must provide at least one thickness!!!");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		if ((size_t)len > bot->num_faces) {
		    bu_vls_printf(logstr, "Too many thicknesses (there are not that many faces)!!!");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		if (!bot->thickness) {
		    bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t),
							  "bot->thickness");
		}
		for (i=0; i<(size_t)len; i++) {
		    bot->thickness[i] = atof(Tcl_GetStringFromObj(obj_array[i], NULL));
		}
	    } else {
		li = atol(&argv[0][1]);
		if (li < 0 || (size_t)li >= bot->num_faces) {
		    bu_vls_printf(logstr, "face number [%ld] out of range!!!", li);
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		if (!bot->thickness) {
		    bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t),
							  "bot->thickness");
		}
		t_str = Tcl_GetStringFromObj(obj, NULL);
		bot->thickness[li] = atof(t_str);
	    }
	} else if (BU_STR_EQUAL(argv[0], "mode")) {
	    char *m_str;

	    m_str = Tcl_GetStringFromObj(obj, NULL);
	    if (isdigit(*m_str)) {
		int mode;

		mode = atoi(m_str);
		if (mode < RT_BOT_SURFACE || mode > RT_BOT_PLATE_NOCOS) {
		    bu_vls_printf(logstr, "unrecognized mode!!!");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->mode = mode;
	    } else {
		if (!strncmp(m_str, modes[RT_BOT_SURFACE], 4))
		    bot->mode = RT_BOT_SURFACE;
		else if (BU_STR_EQUAL(m_str, modes[RT_BOT_SOLID]))
		    bot->mode = RT_BOT_SOLID;
		else if (BU_STR_EQUAL(m_str, modes[RT_BOT_PLATE]))
		    bot->mode = RT_BOT_PLATE;
		else if (BU_STR_EQUAL(m_str, modes[RT_BOT_PLATE_NOCOS]))
		    bot->mode = RT_BOT_PLATE_NOCOS;
		else {
		    bu_vls_printf(logstr, "unrecognized mode!!!");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
	    }
	} else if (!strncmp(argv[0], "orient", 6)) {
	    char *o_str;

	    o_str = Tcl_GetStringFromObj(obj, NULL);
	    if (isdigit(*o_str)) {
		int orient;

		orient = atoi(o_str);
		if (orient < RT_BOT_UNORIENTED || orient > RT_BOT_CW) {
		    bu_vls_printf(logstr, "unrecognized orientation!!!");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
		bot->orientation = orient;
	    } else {
		if (BU_STR_EQUAL(o_str, orientation[RT_BOT_UNORIENTED]))
		    bot->orientation = RT_BOT_UNORIENTED;
		else if (BU_STR_EQUAL(o_str, orientation[RT_BOT_CCW]))
		    bot->orientation = RT_BOT_CCW;
		else if (BU_STR_EQUAL(o_str, orientation[RT_BOT_CW]))
		    bot->orientation = RT_BOT_CW;
		else {
		    bu_vls_printf(logstr, "unrecognized orientation!!!");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
	    }
	} else if (BU_STR_EQUAL(argv[0], "flags")) {
	    (void)Tcl_ListObjGetElements(brlcad_interp, obj, &len, &obj_array);
	    bot->bot_flags = 0;
	    for (i=0; i<(size_t)len; i++) {
		char *str;

		str = Tcl_GetStringFromObj(obj_array[i], NULL);
		if (BU_STR_EQUAL(str, "has_normals")) {
		    bot->bot_flags |= RT_BOT_HAS_SURFACE_NORMALS;
		} else if (BU_STR_EQUAL(str, "use_normals")) {
		    bot->bot_flags |= RT_BOT_USE_NORMALS;
		} else if (BU_STR_EQUAL(str, "use_floats")) {
		    bot->bot_flags |= RT_BOT_USE_FLOATS;
		} else {
		    bu_vls_printf(logstr, "unrecognized flag (must be \"has_normals\", \"use_normals\", or \"use_floats\"!!!");
		    Tcl_DecrRefCount(obj);
		    return BRLCAD_ERROR;
		}
	    }
	}

	Tcl_DecrRefCount(obj);

	argc -= 2;
	argv += 2;
    }

    if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	if (!bot->thickness)
	    bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t), "BOT thickness");
	if (!bot->face_mode) {
	    bot->face_mode = bu_bitv_new(bot->num_faces);
	}
    } else {
	if (bot->thickness) {
	    bu_free(bot->thickness, "BOT thickness");
	    bot->thickness = (fastf_t *)NULL;
	}
	if (bot->face_mode) {
	    bu_free(bot->face_mode, "BOT facemode");
	    bot->face_mode = (struct bu_bitv *)NULL;
	}
    }

    return BRLCAD_OK;
}


int
rt_bot_form(struct bu_vls *logstr, const struct rt_functab *ftp)
{
    RT_CK_FUNCTAB(ftp);

    bu_vls_printf(logstr,
		  "mode {%%s} orient {%%s} V { {%%f %%f %%f} {%%f %%f %%f} ...} F { {%%lu %%lu %%lu} {%%lu %%lu %%lu} ...} T { %%f %%f %%f ... } fm %%s");

    return BRLCAD_OK;
}


/**
 * R T _B O T _ P A R A M S
 *
 */
int
rt_bot_params(struct pc_pc_set *ps, const struct rt_db_internal *ip)
{
    if (!ps) return 0;
    RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


/*************************************************************************
 *
 * BoT support routines used by MGED, converters, etc.
 *
 *************************************************************************/

struct bot_edge {
    int v;
    size_t use_count;
    struct bot_edge *next;
};

/* Builds a table of all unique edges in bot, storing the result in edges.
 * Returns number of edges.
 *
 * Edge i to j (i < j) is represented as the bot_edge struct with v = j,
 * stored in the list headed by (struct bot_edge*) edges[i].
 */
int
buildEdgeTable(struct rt_bot_internal *bot, struct bot_edge ***edges)
{ 
    size_t tmp, flen, currFace;
    int currVert, nextVert, from, to;
    size_t numVertices, numEdges = 0;
    int *faces;
    struct bot_edge *edge;

    RT_BOT_CK_MAGIC(bot);

    numVertices = bot->num_vertices;
    faces = bot->faces;

    /* allocate array with one index per vertex */
    *edges = (struct bot_edge**)bu_calloc(numVertices, sizeof(struct bot_edge*), "edges");

    /* for each face */
    flen = bot->num_faces * 3;
    for (currFace = 0; currFace < flen; currFace += 3) {

	for (currVert = 0; currVert < 3; currVert++) {

	    /* curr and next vertex form an edge of curr face */

	    /* get relative indices */
	    nextVert = currVert + 1;

	    if (nextVert > 2) {
		nextVert = 0;
	    }

	    /* get actual indices */
	    from = faces[currFace + currVert];
	    to = faces[currFace + nextVert];

	    /* make sure 'from' is the lower index */
	    if (to < from) {
		tmp = from;
		from = to;
		to = tmp;
	    }

	    /* get the list for this index */
	    edge = (*edges)[from];

	    /* make new list */
	    if (edge == (struct bot_edge*)NULL) {

		edge = (struct bot_edge*)bu_calloc(1, sizeof(struct bot_edge),
			"edges[from]");
		(*edges)[from] = edge;
	    }
	    
	    /* list already exists */
	    else {

		/* look for existing entry */
		while (edge->next && edge->v != to) {
		    edge = edge->next;
		}

		/* this edge found previously - update use count */
		if (edge->v == to) {

		    edge->use_count++;
		    continue;
		}

		/* this edge is new - append a new entry for it */
		edge->next = (struct bot_edge*)bu_calloc(1, sizeof(struct bot_edge),
			"edge->next");

		edge = edge->next;
	    }

	    /* initialize new entry */
	    edge->v = to;
	    edge->use_count = 1;
	    edge->next = (struct bot_edge*)NULL;

	    numEdges++;

	} /* indices loop */
    } /* faces loop */

    return numEdges;
}

/**
 * Routine for finding the smallest edge length in a BoT.
 */
float
minEdge(struct rt_bot_internal *bot)
{
    size_t i;
    size_t numVerts;
    fastf_t epsilon = 1e-10;
    fastf_t *vertices;
    fastf_t currMag, minMag = MAX_FASTF;
    struct bot_edge *edge, *tmp;
    struct bot_edge **edges = NULL;
    vect_t start, curr;

    RT_BOT_CK_MAGIC(bot);

    numVerts = bot->num_vertices;
    vertices = bot->vertices;

    /* build edge list */
    buildEdgeTable(bot, &edges);

    /* for each vertex */
    for (i = 0; i < numVerts; i++) {

	edge = edges[i];

	/* make sure this list exists */
	if (edge == (struct bot_edge*)NULL) {
	    continue;
	}

	/* starting vertex for these edges */
	VMOVE(start, &vertices[i]);

	/* for each edge beginning with this index */
	while (edge != (struct bot_edge*)NULL) {

	    /* calculate edge vector using current end vertex */
	    VSUB2(curr, start, &vertices[edge->v]);

	    /* see if it has the smallest magnitude so far */
	    currMag = MAGSQ(curr);

	    if (currMag < minMag && currMag > epsilon) {
		minMag = currMag;
	    }

	    edge = edge->next;
	}
    }

    /* free table memory */
    for (i = 0; i < numVerts; i++) {

	edge = edges[i];

	while (edge != (struct bot_edge*)NULL) {
	    tmp = edge;
	    edge = edge->next;
	    bu_free(tmp, "struct bot_edge");
	}
    }
    bu_free(edges, "bot edge table");
    edges = NULL;

    return sqrt(minMag);
}

/**
 * Routine for finding the largest edge length in a BoT.
 */
float
maxEdge(struct rt_bot_internal *bot)
{
    size_t i;
    size_t numVerts;
    fastf_t *vertices;
    fastf_t currMag, maxMag = 0.0;
    struct bot_edge *edge, *tmp;
    struct bot_edge **edges = NULL;
    vect_t start, curr;

    RT_BOT_CK_MAGIC(bot);

    numVerts = bot->num_vertices;
    vertices = bot->vertices;

    /* build edge list */
    buildEdgeTable(bot, &edges);

    /* for each vertex */
    for (i = 0; i < numVerts; i++) {

	edge = edges[i];

	/* make sure this list exists */
	if (edge == (struct bot_edge*)NULL) {
	    continue;
	}

	/* starting vertex for these edges */
	VMOVE(start, &vertices[i]);

	/* for each edge beginning with this index */
	while (edge != (struct bot_edge*)NULL) {

	    /* calculate edge vector using current end vertex */
	    VSUB2(curr, start, &vertices[edge->v]);

	    /* see if it has the largest magnitude so far */
	    currMag = MAGSQ(curr);

	    if (currMag > maxMag) {
		maxMag = currMag;
	    }

	    edge = edge->next;
	}
    }

    /* free table memory */
    for (i = 0; i < numVerts; i++) {

	edge = edges[i];

	while (edge != (struct bot_edge*)NULL) {
	    tmp = edge;
	    edge = edge->next;
	    bu_free(tmp, "struct bot_edge");
	}
    }
    bu_free(edges, "bot edge table");
    edges = NULL;

    return sqrt(maxMag);
}

/**
 * RT_BOT_PROPGET
 * 
 * Command used to query BoT property values. Returns parseable
 * values rather than formatted strings.
 *
 * Returns -1 on error.
 */
fastf_t
rt_bot_propget(struct rt_bot_internal *bot, const char *property)
{
    size_t len;

    RT_BOT_CK_MAGIC(bot);

    len = strlen(property);

    /* return value of requested property */
    if (strncmp(property, "faces", len) == 0) {
	return bot->num_faces;
    }
    else if (strncmp(property, "orientation", len) == 0) {
	return bot->orientation;
    }
    else if (strncmp(property, "type", len) == 0 ||
	     strncmp(property, "mode", len) == 0)
    {
	return bot->mode;
    }
    else if (strncmp(property, "vertices", len) == 0) {
	return bot->num_vertices;
    }
    else if (strncmp(property, "minEdge", len) == 0 ||
	     strncmp(property, "minedge", len) == 0)
    {
	return minEdge(bot);
    }
    else if (strncmp(property, "maxEdge", len) == 0 ||
	     strncmp(property, "maxedge", len) == 0)
    {
	return maxEdge(bot);
    }

    return -1;
}


/**
 * This routine adjusts the vertex pointers in each face so that
 * pointers to duplicate vertices end up pointing to the same vertex.
 * The unused vertices are removed and the resulting bot is condensed.
 * Returns the number of vertices fused.
 */
int
rt_bot_vertex_fuse(struct rt_bot_internal *bot)
{
#if 1
    /* THE OLD WAY .. possibly O(n^3) with the vertex shifting */

    size_t i, j, k;
    size_t count=0;

    RT_BOT_CK_MAGIC(bot);

    for (i=0; i<bot->num_vertices; i++) {
	j = i + 1;
	while (j < bot->num_vertices) {
	    /* specifically not using tolerances here (except underlying representation tolerance) */
	    if (VEQUAL(&bot->vertices[i*3], &bot->vertices[j*3])) {
		count++;

		/* update bot */
		bot->num_vertices--;

		/* shift vertices down */
		for (k=j; k<bot->num_vertices; k++)
		    VMOVE(&bot->vertices[k*3], &bot->vertices[(k+1)*3]);

		/* update face references */
		for (k=0; k<bot->num_faces*3; k++) {
		    if ((size_t)bot->faces[k] == j) {
			bot->faces[k] = i;
		    } else if ((size_t)bot->faces[k] > j)
			bot->faces[k]--;
		}
	    } else {
		j++;
	    }
	}
    }
#else

    /* THE NEW WAY .. possibly O(n) with basic bin sorting */

    size_t i, j, k;
    size_t count=0;

    long slot;
    size_t total = 0;
    long *bin[256];
    size_t bin_capacity[256];
    size_t bin_todonext[256];
    const int DEFAULT_CAPACITY = 32;
    fastf_t min_xval = (fastf_t)LONG_MAX;
    fastf_t max_xval = (fastf_t)LONG_MIN;
    fastf_t delta = (fastf_t)0.0;

    vect_t deleted;
    VSETALL(deleted, INFINITY);

    RT_BOT_CK_MAGIC(bot);

    /* initialize a simple 256-slot integer bin space partitioning */
    for (slot=0; slot<256; slot++) {
	bin_todonext[slot] = 0;
	bin_capacity[slot] = DEFAULT_CAPACITY;
	bin[slot] = bu_calloc(DEFAULT_CAPACITY, sizeof(long *), "vertices bin");
    }

    /* first pass to get the range of vertex values */
    for (i=0; i<bot->num_vertices; i++) {
	/* bins are assigned based on X value */
	if ((&bot->vertices[i*3])[X] < min_xval)
	    min_xval = (&bot->vertices[i*3])[X];
	if ((&bot->vertices[i*3])[X] > max_xval)
	    max_xval = (&bot->vertices[i*3])[X];

	/* sanity to make sure our book-keeping doesn't go haywire */
	if (ZERO((&bot->vertices[i*3])[X] - deleted[X])) {
	    bu_log("WARNING: Unable to fuse due to vertex with infinite value (idx=%ld)\n", i);
	    return 0;
	}
    }
    /* sanity swap */
    if (min_xval > max_xval) {
	fastf_t t;
	t = min_xval;
	min_xval = max_xval;
	max_xval = t;
    }
    /* range sanity */
    if (max_xval > (fastf_t)LONG_MAX) {
	max_xval = (fastf_t)LONG_MAX;
    }
    if (min_xval < (fastf_t)LONG_MIN) {
	min_xval = (fastf_t)LONG_MIN;
    }
    if (ZERO(max_xval - min_xval)) {
	if (ZERO(max_xval - (fastf_t)LONG_MAX)) {
	    max_xval += VDIVIDE_TOL;
	} else {
	    min_xval -= VDIVIDE_TOL;
	}
    }

    /* calculate the width of a bin */
    delta = fabs(max_xval - min_xval) / (fastf_t)256.0;
    if (ZERO(delta))
	delta = (fastf_t)1.0;

    /* second pass to sort the vertices into bins based on their X value */
    for (i=0; i<bot->num_vertices; i++) {
	if ((&bot->vertices[i*3])[X] > (fastf_t)LONG_MAX) {
	    /* exceeds our range, put in last bin */
	    slot = 255;
	} else if ((&bot->vertices[i*3])[X] < (fastf_t)LONG_MIN) {
	    /* exceeds our range, put in first bin */
	    slot = 0;
	} else {
	    /* bins are assigned based on X value */
	    slot = (long)(((&bot->vertices[i*3])[X] - min_xval) / delta);
	}

	/* extra sanity that we don't imagine non-existent bins */
	if (slot < 0) {
	    slot = 0;
	} else if (slot > 255) {
	    slot = 255;
	}

	if (bin_todonext[slot] + 1 > bin_capacity[slot]) {

/* bu_log("increasing %i from capacity %ld given next is %ld\n", slot, bin_capacity[slot], bin_todonext[slot]); */

	    BU_ASSERT_LONG(bin_capacity[slot], <, LONG_MAX / 2);

	    bin[slot] = bu_realloc(bin[slot], bin_capacity[slot] * 2 * sizeof(int), "increase vertices bin");
	    bin_capacity[slot] *= 2;

	    /* init to zero for sanity */
	    for (j=bin_todonext[slot]+1; j<bin_capacity[slot]; j++) {
		bin[slot][j] = 0;
	    }
	}

/* bu_log("setting bin[%lu][%lu] = %ld\n", slot, bin_todonext[slot], i); */

	bin[slot][bin_todonext[slot]++] = i;
    }

    /* third pass to check the vertices in each bin */
    for (slot=0; slot<256; slot++) {

	/* iterate over all vertices in this bin */
	for (i=0; i<bin_todonext[slot]; i++) {
	    
	    /* compare to the other vertices in this bin */
	    for (j=i+1; j<bin_todonext[slot]; j++) {

		/* specifically not using tolerances here (except underlying representation tolerance) */
		if (VEQUAL(&bot->vertices[bin[slot][i]*3], &bot->vertices[bin[slot][j]*3])) {
		    count++;

		    /*  update face references */
		    for (k=0; k<bot->num_faces*3; k++) {
			if (bot->faces[k] == bin[slot][j]) {
			    bot->faces[k] = bin[slot][i];
			}
		    }

		    /* wipe out the vertex marking it for cleanup later */
		    VMOVE(&bot->vertices[bin[slot][j]*3], deleted);
		}
	    }
	}
    }

    /* clean up and compress */
    k = rt_bot_condense(bot);
    if (k < count) {
	bu_log("WARNING: Condensed fewer vertices than expected (%ld < %ld)\n", k, count);
    }

    /* sanity check, there should be no deleted vertices */
    for (i=0; i < bot->num_vertices; i++) {
	if (VEQUAL(&bot->vertices[i*3], deleted)) {
	    bu_bomb("INTERNAL ERROR: encountered unexpected state during vertex fusing\n");
	}
    }

    /* clear and release the memory for our integer bin space partitioning */
    for (slot=0; slot<256; slot++) {
	total += bin_todonext[slot];
/*	bu_log("[%lu]: %ld (of %ld)\n", slot, bin_todonext[slot], bin_capacity[slot]); */
	bu_free(bin[slot], "vertices bin");
	bin_capacity[slot] = bin_todonext[slot] = 0;
    }
    memset(bin, 0, 256 * sizeof(long *));

/*    bu_log("sorted %lu of %lu vertices\n", total, bot->num_vertices); */
#endif

    return count;
}


int
rt_bot_same_orientation(const int *a, const int *b)
{
    int i;

    for (i=0; i<3; i++) {
	if (a[0] == b[i]) {
	    i++;
	    if (i == 3)
		i = 0;
	    if (a[1] == b[i])
		return 1;
	    else
		return 0;
	}
    }

    return 0;
}


int
rt_bot_face_fuse(struct rt_bot_internal *bot)
{
    size_t num_faces;
    size_t i, j, k, l;
    int count=0;

    RT_BOT_CK_MAGIC(bot);

    num_faces = bot->num_faces;
    for (i=0; i<num_faces; i++) {
	j = i+1;

	while (j<num_faces) {
	    /* each pass through this loop either increments j or
	     * decrements num_faces
	     */
	    int match=0;
	    int elim;

	    for (k=i*3; k<(i+1)*3; k++) {
		for (l=j*3; l<(j+1)*3; l++) {
		    if (bot->faces[k] == bot->faces[l]) {
			match++;
			break;
		    }
		}
	    }

	    if (match != 3) {
		j++;
		continue;
	    }

	    /* these two faces have the same vertices */
	    elim = -1;
	    switch (bot->mode) {
		case RT_BOT_PLATE:
		case RT_BOT_PLATE_NOCOS:
		    /* check the face thickness and face mode */
		    if (!ZERO(bot->thickness[i] - bot->thickness[j]) ||
			(BU_BITTEST(bot->face_mode, i)?1:0) != (BU_BITTEST(bot->face_mode, j)?1:0))
			break;
		case RT_BOT_SOLID:
		case RT_BOT_SURFACE:
		    if (bot->orientation == RT_BOT_UNORIENTED) {
			/* faces are identical, so eliminate one */
			elim = j;
		    } else {
			/* need to check orientation */
			if (rt_bot_same_orientation(&bot->faces[i*3], &bot->faces[j*3]))
			    elim = j;
		    }
		    break;
		default:
		    bu_bomb("bot_face_condense: Unrecognized BOT mode!!!\n");
		    break;
	    }

	    if (elim == -1) {
		j++;
		continue;
	    }

	    /* we are eliminating face number "elim" */
	    for (l=elim; l< num_faces-1; l++) {
		VMOVE(&bot->faces[l*3], &bot->faces[(l+1)*3]);
	    }

	    if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
		for (l=elim; l<num_faces-1; l++) {
		    bot->thickness[l] = bot->thickness[l+1];
		    if (BU_BITTEST(bot->face_mode, l+1))
			BU_BITSET(bot->face_mode, l);
		    else
			BU_BITCLR(bot->face_mode, l);
		}
	    }
	    num_faces--;
	}
    }

    count = bot->num_faces - num_faces;

    if (count) {
	bot->num_faces = num_faces;
	bot->faces = (int *)bu_realloc(bot->faces, num_faces*3*sizeof(int), "BOT faces realloc");
	if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	    struct bu_bitv *new_mode;

	    bot->thickness = bu_realloc(bot->thickness, num_faces*sizeof(fastf_t), "BOT thickness realloc");
	    new_mode = bu_bitv_new(num_faces);
	    for (l=0; l<num_faces; l++) {
		if (BU_BITTEST(bot->face_mode, l))
		    BU_BITSET(new_mode, l);
	    }
	    if (bot->face_mode)
		bu_free(bot->face_mode, "BOT face_mode");
	    bot->face_mode = new_mode;
	}
    }

    return count;
}


/**
 * Get rid of unused verticies
 */
int
rt_bot_condense(struct rt_bot_internal *bot)
{
    size_t i, j, k;
    size_t num_verts;
    size_t dead_verts=0;
    int *verts;

    RT_BOT_CK_MAGIC(bot);

    num_verts = bot->num_vertices;
    verts = (int *)bu_calloc(num_verts, sizeof(int), "VERTEX LIST");

    /* walk the list of verticies, and mark each one if it is used */

    for (i=0; i<bot->num_faces*3; i++) {
	j = bot->faces[i];
	if (j >= num_verts) {
	    bu_log("Illegal vertex number %zu, should be 0 through %zu\n", j, num_verts-1);
	    bu_bomb("Illegal vertex number\n");
	}
	verts[j] = 1;
    }

    /* Walk the list of verticies, eliminate each unused vertex by
     * copying the rest of the array downwards
     */
    i = 0;
    while (i < num_verts-dead_verts) {
	while (!verts[i] && i < num_verts-dead_verts) {
	    dead_verts++;
	    for (j=i; j<num_verts-dead_verts; j++) {
		k = j+1;
		VMOVE(&bot->vertices[j*3], &bot->vertices[k*3]);
		verts[j] = verts[k];
	    }
	    for (j=0; j<bot->num_faces*3; j++) {
		if ((size_t)bot->faces[j] >= i)
		    bot->faces[j]--;
	    }
	}
	i++;
    }

    if (!dead_verts)
	return 0;

    /* Reallocate the vertex array (which should free the space we are
     * no longer using)
     */
    bot->num_vertices -= dead_verts;
    bot->vertices = (fastf_t *)bu_realloc(bot->vertices, bot->num_vertices*3*sizeof(fastf_t), "bot verts realloc");

    return dead_verts;
}


int
find_closest_face(fastf_t **centers, int *piece, int *old_faces, size_t num_faces, fastf_t *vertices)
{
    pointp_t v0, v1, v2;
    point_t center;
    size_t i;
    fastf_t one_third = 1.0/3.0;
    fastf_t min_dist;
    size_t min_face=-1;

    if ((*centers) == NULL) {
	int count_centers=0;

	/* need to build the centers array */
	(*centers) = (fastf_t *)bu_malloc(num_faces * 3 * sizeof(fastf_t), "center");
	for (i=0; i<num_faces; i++) {
	    if (old_faces[i*3] == -1) {
		continue;
	    }
	    count_centers++;
	    v0 = &vertices[old_faces[i*3+0]*3];
	    v1 = &vertices[old_faces[i*3+1]*3];
	    v2 = &vertices[old_faces[i*3+2]*3];
	    VADD3(center, v0, v1, v2);
	    VSCALE(&(*centers)[i*3], center, one_third);
	}
    }

    v0 = &vertices[piece[0]*3];
    v1 = &vertices[piece[1]*3];
    v2 = &vertices[piece[2]*3];

    VADD3(center, v0, v1, v2);
    VSCALE(center, center, one_third);

    min_dist = MAX_FASTF;

    for (i=0; i<num_faces; i++) {
	vect_t diff;
	fastf_t dist;

	if (old_faces[i*3] == -1) {
	    continue;
	}

	VSUB2(diff, center, &(*centers)[i*3]);
	dist = MAGSQ(diff);
	if (dist < min_dist) {
	    min_dist = dist;
	    min_face = i;
	}
    }

    return min_face;
}


void
Add_unique_verts(int *piece_verts, int *v)
{
    int i, j;
    int *ptr=v;

    for (j=0; j<3; j++) {
	i = -1;
	while (piece_verts[++i] != -1) {
	    if (piece_verts[i] == (*ptr)) {
		break;
	    }
	}
	if (piece_verts[i] == -1) {
	    piece_verts[i] = (*ptr);
	}
	ptr++;
    }
}


/**
 * This routine sorts the faces of the BOT such that when they are
 * taken in groups of "tris_per_piece", * each group (piece) will
 * consist of adjacent faces
 */
int
rt_bot_sort_faces(struct rt_bot_internal *bot, size_t tris_per_piece)
{
    fastf_t *centers = (fastf_t *)NULL;	/* triangle centers, used when all else fails */
    int *new_faces = (int *)NULL;	/* the sorted list of faces to be attached to the BOT at the end of this routine */
    int *new_norms = (int *)NULL;	/* the sorted list of vertex normals corrsponding to the "new_faces" list */
    int *old_faces = (int *)NULL;	/* a copy of the original face list from the BOT */
    int *piece = (int *)NULL;		/* a small face list, for just the faces in the current piece */
    int *piece_norms = (int *)NULL;	/* vertex normals for faces in the current piece */
    int *piece_verts = (int *)NULL;	/* a list of vertices in the current piece (each vertex appears only once) */
    unsigned char *vert_count;		/* an array used to hold the number of piece vertices that appear in each BOT face */
    size_t new_face_count=0;		/* the current number of faces in the "new_faces" list */
    size_t faces_left;			/* the number of faces in the "old_faces" array that have not yet been used */
    size_t piece_len;			/* the current number of faces in the piece */
    size_t max_verts;			/* the maximum number of piece_verts found in a single unused face */
    size_t i;
    size_t j;

    RT_BOT_CK_MAGIC(bot);

    /* allocate memory for all the data */
    new_faces = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "new_faces");
    old_faces = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "old_faces");
    piece = (int *)bu_calloc(tris_per_piece * 3, sizeof(int), "piece");
    vert_count = (unsigned char *)bu_malloc(bot->num_faces * sizeof(unsigned char), "vert_count");
    piece_verts = (int *)bu_malloc((tris_per_piece * 3 + 1) * sizeof(int), "piece_verts");
    centers = (fastf_t *)NULL;

    if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	new_norms = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "new_norms");
	piece_norms = (int *)bu_calloc(tris_per_piece * 3, sizeof(int), "piece_norms");
    }

    /* make a copy of the faces list, this list will be modified
     * during the process.
     */
    for (i=0; i<bot->num_faces*3; i++) {
	old_faces[i] = bot->faces[i];
    }

    /* process until we have sorted all the faces */
    faces_left = bot->num_faces;
    while (faces_left) {
	size_t cur_face;
	int done_with_piece;

	/* initialize piece_verts */
	for (i=0; i<tris_per_piece*3+1; i++) {
	    piece_verts[i] = -1;
	}

	/* choose first unused face on the list */
	cur_face = 0;
	while (cur_face < bot->num_faces && old_faces[cur_face*3] == -1) {
	    cur_face++;
	}

	if (cur_face >= bot->num_faces) {
	    /* all faces used, we must be done */
	    break;
	}

	/* copy that face to start the piece */
	VMOVE(piece, &old_faces[cur_face*3]);
	if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	    VMOVE(piece_norms, &bot->face_normals[cur_face*3]);
	}

	/* also copy it to the piece vertex list */
	VMOVE(piece_verts, piece);

	/* mark this face as used */
	VSETALL(&old_faces[cur_face*3], -1);

	/* update counts */
	piece_len = 1;
	faces_left--;

	if (faces_left == 0) {
	    /* handle the case where the first face in a piece is the
	     * only face left.
	     */
	    for (j=0; j<piece_len; j++) {
		VMOVE(&new_faces[new_face_count*3], &piece[j*3]);
		if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
		    VMOVE(&new_norms[new_face_count*3], &piece_norms[j*3]);
		}
		new_face_count++;
	    }
	    piece_len = 0;
	    max_verts = 0;

	    /* set flag to skip the loop below */
	    done_with_piece = 1;
	} else {
	    done_with_piece = 0;
	}

	while (!done_with_piece) {
	    size_t max_verts_min;

	    /* count the number of times vertices from the current
	     * piece appear in the remaining faces.
	     */
	    (void)memset(vert_count, '\0', bot->num_faces);
	    max_verts = 0;
	    for (i=0; i<bot->num_faces; i++) {
		size_t vert_num;
		int v0, v1, v2;

		vert_num = i*3;
		if (old_faces[vert_num] == -1) {
		    continue;
		}
		v0 = old_faces[vert_num];
		v1 = old_faces[vert_num+1];
		v2 = old_faces[vert_num+2];

		j = (size_t)-1;
		while (piece_verts[++j] != -1) {
		    if (v0 == piece_verts[j]
			|| v1 == piece_verts[j]
			|| v2 == piece_verts[j])
		    {
			vert_count[i]++;
		    }
		}

		if (vert_count[i] > 1) {
		    /* add this face to the piece */
		    VMOVE(&piece[piece_len*3], &old_faces[i*3]);
		    if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
			VMOVE(&piece_norms[piece_len*3], &bot->face_normals[i*3]);
		    }

		    /* Add its vertices to the list of piece vertices */
		    Add_unique_verts(piece_verts, &old_faces[i*3]);

		    /* mark this face as used */
		    VSETALL(&old_faces[i*3], -1);

		    /* update counts */
		    piece_len++;
		    faces_left--;
		    vert_count[i] = 0;

		    /* check if this piece is done */
		    if (piece_len == tris_per_piece || faces_left == 0) {
			/* copy this piece to the "new_faces" list */
			for (j=0; j<piece_len; j++) {
			    VMOVE(&new_faces[new_face_count*3], &piece[j*3]);
			    if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
				VMOVE(&new_norms[new_face_count*3], &piece_norms[j*3]);
			    }
			    new_face_count++;
			}
			piece_len = 0;
			max_verts = 0;
			done_with_piece = 1;
			break;
		    }
		}
		if (vert_count[i] > max_verts) {
		    max_verts = vert_count[i];
		}
	    }

	    /* set this variable to 2, means look for faces with at
	     * least common edges.
	     */
	    max_verts_min = 2;

	    if (max_verts == 0 && !done_with_piece) {
		/* none of the remaining faces has any vertices in
		 * common with the current piece.
		 */
		size_t face_to_add;

		/* resort to using triangle centers. find the closest
		 * face to the first face in the piece
		 */
		face_to_add = find_closest_face(&centers, piece, old_faces, bot->num_faces, bot->vertices);

		/* Add this face to the current piece */
		VMOVE(&piece[piece_len*3], &old_faces[face_to_add*3]);
		if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
		    VMOVE(&piece_norms[piece_len*3], &bot->face_normals[face_to_add*3]);
		}

		/* Add its vertices to the list of piece vertices */
		Add_unique_verts(piece_verts, &old_faces[face_to_add*3]);

		/* mark this face as used */
		VSETALL(&old_faces[face_to_add*3], -1);

		/* update counts */
		piece_len++;
		faces_left--;

		/* check if this piece is done */
		if (piece_len == tris_per_piece || faces_left == 0) {
		    /* copy this piece to the "new_faces" list */
		    for (j=0; j<piece_len; j++) {
			VMOVE(&new_faces[new_face_count*3], &piece[j*3]);
			if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
			    VMOVE(&new_norms[new_face_count*3], &piece_norms[j*3]);
			}
			new_face_count++;
		    }
		    piece_len = 0;
		    max_verts = 0;
		    done_with_piece = 1;
		}
	    } else if (max_verts == 1 && !done_with_piece) {
		/* the best we can find is common vertices */
		max_verts_min = 1;
	    } else if (!done_with_piece) {
		/* there are some common edges, so ignore simple shared vertices */
		max_verts_min = 2;
	    }

	    /* now add the faces with the highest counts to the
	     * current piece do this in a loop that starts by only
	     * accepting the faces with the most vertices in common
	     * with the current piece
	     */
	    while (max_verts >= max_verts_min && !done_with_piece) {
		/* check every face */
		for (i=0; i<bot->num_faces; i++) {
		    /* if this face has enough vertices in common with
		     * the piece, add it to the piece
		     */
		    if (vert_count[i] == max_verts) {
			VMOVE(&piece[piece_len*3], &old_faces[i*3]);
			if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
			    VMOVE(&piece_norms[piece_len*3], &bot->face_normals[i*3]);
			}
			Add_unique_verts(piece_verts, &old_faces[i*3]);
			VSETALL(&old_faces[i*3], -1);

			piece_len++;
			faces_left--;

			/* Check if we are done */
			if (piece_len == tris_per_piece || faces_left == 0) {
			    /* copy this piece to the "new_faces" list */
			    for (j=0; j<piece_len; j++) {
				VMOVE(&new_faces[new_face_count*3], &piece[j*3]);
				if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
				    VMOVE(&new_norms[new_face_count*3], &piece_norms[j*3]);
				}
				new_face_count++;
			    }
			    piece_len = 0;
			    max_verts = 0;
			    done_with_piece = 1;
			    break;
			}
		    }
		}
		max_verts--;
	    }
	}
    }

    bu_free(old_faces, "old_faces");
    bu_free(piece, "piece");
    bu_free(vert_count, "vert_count");
    bu_free(piece_verts, "piece_verts");
    if (centers) {
	bu_free(centers, "centers");
	centers = NULL;
    }

    /* do some checking on the "new_faces" */
    if (new_face_count != bot->num_faces) {
	bu_log("new_face_count = %zu, should be %zu\n", new_face_count, bot->num_faces);
	bu_free(new_faces, "new_faces");
	return 1;
    }

    if (bot->faces)
	bu_free(bot->faces, "bot->faces");

    bot->faces = new_faces;

    if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	bu_free(piece_norms, "piece_norms");
	if (bot->face_normals)
	    bu_free(bot->face_normals, "bot->face_normals");
	bot->face_normals = new_norms;
    }

    return 0;
}

HIDDEN void
delete_edge(int v1, int v2, struct bot_edge **edges)
{
    struct bot_edge *edg, *prev=NULL;

    if (v1 < v2) {
	edg = edges[v1];
	while (edg) {
	    if (edg->v == v2) {
		edg->use_count--;
		if (edg->use_count < 1) {
		    if (prev) {
			prev->next = edg->next;
		    } else {
			edges[v1] = edg->next;
		    }
		    edg->v = -1;
		    edg->next = NULL;
		    bu_free(edg, "bot_edge");
		    return;
		}
	    }
	    prev = edg;
	    edg = edg->next;
	}
    } else {
	edg = edges[v2];
	while (edg) {
	    if (edg->v == v1) {
		edg->use_count--;
		if (edg->use_count < 1) {
		    if (prev) {
			prev->next = edg->next;
		    } else {
			edges[v2] = edg->next;
		    }
		    edg->v = -1;
		    edg->next = NULL;
		    bu_free(edg, "bot_edge");
		    return;
		}
	    }
	    prev = edg;
	    edg = edg->next;
	}
    }
}


/**
 * D E C I M A T E _ E D G E
 *
 * Routine to perform the actual edge decimation step The edge from v1
 * to v2 is eliminated by moving v1 to v2.  Faces that used this edge
 * are eliminated.  Faces that used v1 will have that reference
 * changed to v2.
 */
HIDDEN int
decimate_edge(int v1, int v2, struct bot_edge **edges, size_t num_edges, int *faces, size_t num_faces, int face_del1, int face_del2)
{
    size_t i;
    struct bot_edge *edg;

    /* first eliminate all the edges of the two deleted faces from the edge list */
    delete_edge(faces[face_del1*3 + 0], faces[face_del1*3 + 1], edges);
    delete_edge(faces[face_del1*3 + 1], faces[face_del1*3 + 2], edges);
    delete_edge(faces[face_del1*3 + 2], faces[face_del1*3 + 0], edges);
    delete_edge(faces[face_del2*3 + 0], faces[face_del2*3 + 1], edges);
    delete_edge(faces[face_del2*3 + 1], faces[face_del2*3 + 2], edges);
    delete_edge(faces[face_del2*3 + 2], faces[face_del2*3 + 0], edges);

    /* do the decimation */
    for (i=0; i<3; i++) {
	faces[face_del1*3 + i] = -1;
	faces[face_del2*3 + i] = -1;
    }
    for (i=0; i<num_faces*3; i++) {
	if (faces[i] == v1) {
	    faces[i] = v2;
	}
    }

    /* update the edge list; now move all the remaining edges at
     * edges[v1] to somewhere else.
     */
    edg = edges[v1];
    while (edg) {
	struct bot_edge *ptr;
	struct bot_edge *next;

	next = edg->next;

	if (edg->v < v2) {
	    ptr = edges[edg->v];
	    while (ptr) {
		if (ptr->v == v2) {
		    ptr->use_count++;
		    edg->v = -1;
		    edg->next = NULL;
		    bu_free(edg, "bot edge");
		    break;
		}
		ptr = ptr->next;
	    }
	    if (!ptr) {
		edg->next = edges[edg->v];
		edges[edg->v] = edg;
		edg->v = v2;
	    }
	} else if (edg->v > v2) {
	    ptr = edges[v2];
	    while (ptr) {
		if (ptr->v == edg->v) {
		    ptr->use_count++;
		    edg->v = -1;
		    edg->next = NULL;
		    bu_free(edg, "bot edge");
		    break;
		}
		ptr = ptr->next;
	    }
	    if (!ptr) {
		edg->next = edges[v2];
		edges[v2] = edg;
	    }
	} else {
	    edg->v = -1;
	    edg->next = NULL;
	    bu_free(edg, "bot edge");
	}

	edg = next;
    }
    edges[v1] = NULL;

    /* now change all remaining v1 references to v2 */
    for (i=0; i<num_edges; i++) {
	struct bot_edge *next, *prev, *ptr;

	prev = NULL;
	edg = edges[i];
	/* look at edges starting from vertex #i */
	while (edg) {
	    next = edg->next;

	    if (edg->v == v1) {
		/* this one is affected */
		edg->v = v2;	/* change v1 to v2 */
		if ((size_t)v2 < i) {
		    /* disconnect this edge from list #i */
		    if (prev) {
			prev->next = next;
		    } else {
			edges[i] = next;
		    }

		    /* this edge must move to the "v2" list */
		    ptr = edges[v2];
		    while (ptr) {
			if ((size_t)ptr->v == i) {
			    /* found another occurence of this edge
			     * increment use count
			     */
			    ptr->use_count++;

			    /* delete the original */
			    edg->v = -1;
			    edg->next = NULL;
			    bu_free(edg, "bot edge");
			    break;
			}
			ptr = ptr->next;
		    }
		    if (!ptr) {
			/* did not find another occurence, add to list */
			edg->next = edges[v2];
			edges[v2] = edg;
		    }
		    edg = next;
		} else if ((size_t)v2 > i) {
		    /* look for other occurences of this edge in this
		     * list if found, just increment use count
		     */
		    ptr = edges[i];
		    while (ptr) {
			if (ptr->v == v2 && ptr != edg) {
			    /* found another occurence */
			    /* increment use count */
			    ptr->use_count++;

			    /* disconnect original from list */
			    if (prev) {
				prev->next = next;
			    } else {
				edges[i] = next;
			    }

			    /* free it */
			    edg->v = -1;
			    edg->next = NULL;
			    bu_free(edg, "bot edge");

			    break;
			}
			ptr = ptr->next;
		    }
		    if (!ptr) {
			prev = edg;
		    }
		    edg = next;
		} else {
		    /* disconnect original from list */
		    if (prev) {
			prev->next = next;
		    } else {
			edges[i] = next;
		    } 

		    /* free it */
		    edg->v = -1;
		    edg->next = NULL;
		    bu_free(edg, "bot edge");
		}
	    } else {
		/* unaffected edge, just continue */
		edg = next;
	    }
	}
    }

    return 2;
}


/* for simplicity, only consider vertices that are shared with less
 * than MAX_AFFECTED_FACES */
#define MAX_AFFECTED_FACES 128


/**
 * E D G E _ C A N _ B E _ D E C I M A T E D
 *
 * Routine to determine if the specified edge can be eliminated within
 * the given constraints:
 *
 * "faces" is the current working version of the BOT face list.
 *
 * "v1" and "v2" are the indices into the BOT vertex list, they define
 * the edge.
 *
 * "max_chord_error" is the maximum distance allowed between the old
 * surface and new.
 *
 * "max_normal_error" is actually the minimum dot product allowed
 * between old and new surface normals (cosine).
 *
 * "min_edge_length_sq" is the square of the minimum allowed edge
 * length.
 *
 * any constraint value of -1.0 means ignore this constraint
 *
 * returns 1 if edge can be eliminated without breaking conatraints, 0
 * otherwise.
 */
HIDDEN int
edge_can_be_decimated(struct rt_bot_internal *bot,
		      int *faces,
		      struct bot_edge **edges,
		      int v1,
		      int v2,
		      int *face_del1,
		      int *face_del2,
		      fastf_t max_chord_error,
		      fastf_t max_normal_error,
		      fastf_t min_edge_length_sq)
{
    size_t i, j, k;
    size_t num_faces = bot->num_faces;
    size_t num_edges = bot->num_vertices;
    size_t count, v1_count;
    size_t affected_count=0;
    vect_t v01, v02, v12;
    fastf_t *vertices = bot->vertices;
    size_t faces_affected[MAX_AFFECTED_FACES];

    if (v1 == -1 || v2 == -1) {
	return 0;
    }

    /* find faces to be deleted or affected */
    *face_del1 = -1;
    *face_del2 = -1;
    for (i=0; i<num_faces*3; i += 3) {
	count = 0;
	v1_count = 0;
	for (j=0; j<3; j++) {
	    k = i + j;
	    if (faces[k] == v1) {
		/* found a reference to v1, count it */
		count++;
		v1_count++;
	    } else if (faces[k] == v2) {
		/* found a reference to v2, count it */
		count++;
	    }
	}
	if (count > 1) {
	    /* this face will get deleted */
	    if (*face_del1 != -1) {
		*face_del2 = i/3;
	    } else {
		*face_del1 = i/3;
	    }
	} else if (v1_count) {
	    /* this face will be affected */
	    faces_affected[affected_count] = i;
	    affected_count++;
	    if (affected_count >= MAX_AFFECTED_FACES) {
		return 0;
	    }
	}
    }

    /* if only one face will be deleted, do not decimate this may be a
     * free edge
     */
    if (*face_del2 == -1) {
	return 0;
    }

    /* another easy test to avoid moving free edges */
    if (affected_count < 1) {
	return 0;
    }

    /* for BOTs that are expected to have free edges, do a rigorous
     * check for free edges
     */
    if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_SURFACE) {
	struct bot_edge *edg;

	/* check if vertex v1 is on a free edge */
	for (i=0; i<num_edges; i++) {
	    edg = edges[i];
	    while (edg) {
		if ((i == (size_t)v1 || edg->v == v1) && edg->use_count < 2) {
		    return 0;
		}
		edg = edg->next;
	    }
	}
    }

    /* calculate edge vector */
    VSUB2(v12, &vertices[v1*3], &vertices[v2*3]);

    if (min_edge_length_sq > SMALL_FASTF) {
	if (MAGSQ(v12) > min_edge_length_sq) {
	    return 0;
	}
    }

    if (max_chord_error > -1.0 - SMALL_FASTF || max_normal_error > -1.0 - SMALL_FASTF) {
	/* check if surface is within max_chord_error of vertex to be
	 * eliminated; loop through all affected faces.
	 */
	for (i=0; i<affected_count; i++) {
	    fastf_t dist;
	    fastf_t dot;
	    plane_t pla, plb;
	    int va, vb, vc;

	    /* calculate plane of this face before and after
	     * adjustment if the normal changes too much, do not
	     * decimate
	     */

	    /* first calculate original face normal (use original BOT
	     * face list)
	     */
	    va = bot->faces[faces_affected[i]];
	    vb = bot->faces[faces_affected[i]+1];
	    vc = bot->faces[faces_affected[i]+2];
	    VSUB2(v01, &vertices[vb*3], &vertices[va*3]);
	    VSUB2(v02, &vertices[vc*3], &vertices[va*3]);
	    VCROSS(plb, v01, v02);
	    VUNITIZE(plb);
	    plb[W] = VDOT(&vertices[va*3], plb);

	    /* do the same using the working face list */
	    va = faces[faces_affected[i]];
	    vb = faces[faces_affected[i]+1];
	    vc = faces[faces_affected[i]+2];
	    /* make the proposed decimation changes */
	    if (va == v1) {
		va = v2;
	    } else if (vb == v1) {
		vb = v2;
	    } else if (vc == v1) {
		vc = v2;
	    }
	    VSUB2(v01, &vertices[vb*3], &vertices[va*3]);
	    VSUB2(v02, &vertices[vc*3], &vertices[va*3]);
	    VCROSS(pla, v01, v02);
	    VUNITIZE(pla);
	    pla[W] = VDOT(&vertices[va*3], pla);

	    /* max_normal_error is actually a minimum dot product */
	    dot = VDOT(pla, plb);
	    if (max_normal_error > -1.0 - SMALL_FASTF && dot < max_normal_error) {
		return 0;
	    }

	    /* check the distance between this new plane and vertex
	     * v1
	     */
	    dist = fabs(DIST_PT_PLANE(&vertices[v1*3], pla));
	    if (max_chord_error > -1.0 - SMALL_FASTF && dist > max_chord_error) {
		return 0;
	    }
	}
    }

    return 1;
}


/**
 * R T _ B O T _ D E C I M A T E
 *
 * routine to reduce the number of triangles in a BOT by edges
 * decimation.
 *
 * max_chord_error is the maximum error distance allowed
 * max_normal_error is the maximum change in surface normal allowed
 *
 * This and associated routines maintain a list of edges and their
 * "use counts" A "free edge" is one with a use count of 1, most edges
 * have a use count of 2 When a use count reaches zero, the edge is
 * removed from the list.  The list is used to direct the edge
 * decimation process and to avoid deforming the shape of a non-volume
 * enclosing BOT by keeping track of use counts (and thereby free
 * edges) If a free edge would be moved, that deciamtion is not
 * performed.
 */
int
rt_bot_decimate(struct rt_bot_internal *bot,	/* BOT to be decimated */
		fastf_t max_chord_error,	/* maximum allowable chord error (mm) */
		fastf_t max_normal_error,	/* maximum allowable normal error (degrees) */
		fastf_t min_edge_length)	/* minimum allowed edge length */
{
    int *faces = NULL;
    struct bot_edge **edges = NULL;
    fastf_t min_edge_length_sq = 0.0;
    size_t edges_deleted = 0;
    size_t edge_count = 0;
    size_t face_count = 0;
    size_t actual_count=0;
    size_t deleted = 0;
    size_t i = 0;
    int done;

    RT_BOT_CK_MAGIC(bot);

    /* convert normal error to something useful (a minimum dot product) */
    if (max_normal_error > -1.0 - SMALL_FASTF) {
	max_normal_error = cos(max_normal_error * DEG2RAD);
    }

    if (min_edge_length > SMALL_FASTF) {
	min_edge_length_sq = min_edge_length * min_edge_length;
    } else {
	min_edge_length_sq = min_edge_length;
    }

    /* make a working copy of the face list */
    faces = (int *)bu_malloc(sizeof(int) * bot->num_faces * 3, "faces");
    for (i=0; i<bot->num_faces*3; i++) {
	faces[i] = bot->faces[i];
    }
    face_count = bot->num_faces;

    /* make a list of edges in the BOT; each edge will be in the list
     * for its lower numbered vertex index
     */
    edge_count = buildEdgeTable(bot, &edges);

    /* the decimation loop */
    done = 0;
    while (!done) {
	done = 1;

	/* visit each edge */
	for (i=0; i<bot->num_vertices; i++) {
	    struct bot_edge *ptr;
	    int face_del1, face_del2;

	    ptr = edges[i];
	    while (ptr) {

		/* try to avoid making 2D objects */
		if (face_count < 5)
		    break;

		/* check if this edge can be eliminated (try both directions) */
		if (edge_can_be_decimated(bot, faces, edges, i, ptr->v,
					  &face_del1, &face_del2,
					  max_chord_error,
					  max_normal_error,
					  min_edge_length_sq)) {
		    face_count -= decimate_edge(i, ptr->v, edges, bot->num_vertices,
						faces, bot->num_faces,
						face_del1, face_del2);
		    edges_deleted++;
		    done = 0;
		    break;
		} else if (edge_can_be_decimated(bot, faces, edges, ptr->v, i,
						 &face_del1, &face_del2,
						 max_chord_error,
						 max_normal_error,
						 min_edge_length_sq)) {
		    face_count -= decimate_edge(ptr->v, i, edges, bot->num_vertices,
						faces, bot->num_faces,
						face_del1, face_del2);
		    edges_deleted++;
		    done = 0;
		    break;
		} else {
		    ptr = ptr->next;
		}
	    }
	}
    }

    /* free some memory */
    for (i=0; i<bot->num_vertices; i++) {
	struct bot_edge *ptr, *ptr2;

	ptr = edges[i];
	while (ptr) {
	    ptr2 = ptr;
	    ptr = ptr->next;
	    bu_free(ptr2, "ptr->edges");
	}
    }
    bu_free(edges, "edges");
    edges = NULL;

    /* condense the face list */
    actual_count = 0;
    deleted = 0;
    for (i=0; i<bot->num_faces*3; i++) {
	if (faces[i] == -1) {
	    deleted++;
	    continue;
	}
	if (deleted) {
	    faces[i-deleted] = faces[i];
	}
	actual_count++;
    }

    if (actual_count % 3) {
	bu_log("rt_bot_decimate: face vertices count is not a multilple of 3!!\n");
	bu_free(faces, "faces");
	return -1;
    }

    bu_log("original face count = %zu, edge count = %zu\n", bot->num_faces, edge_count);
    bu_log("\tedges deleted = %zu\n", edges_deleted);
    bu_log("\tnew face_count = %zu\n", face_count);

    actual_count /= 3;

    if (face_count != actual_count) {
	bu_log("rt_bot_decimate: Face count is confused!!\n");
	bu_free(faces, "faces");
	return -2;
    }

    if (bot->faces)
	bu_free(bot->faces, "bot->faces");
    bot->faces = (int *)bu_realloc(faces, sizeof(int) * face_count * 3, "bot->faces");
    bot->num_faces = face_count;

    /* removed unused vertices */
    (void)rt_bot_condense(bot);

    return edges_deleted;
}


HIDDEN int
bot_smooth_miss(struct application *ap)
{
    if (ap) RT_CK_APPLICATION(ap);
    return 0;
}


HIDDEN int
bot_smooth_hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(seg))
{
    struct partition *pp;
    struct soltab *stp;
    vect_t inormal, onormal;
    vect_t *normals=(vect_t *)ap->a_uptr;

    for (pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	stp = pp->pt_inseg->seg_stp;
	RT_HIT_NORMAL(inormal, pp->pt_inhit, stp, &(ap->a_ray), pp->pt_inflip);

	stp = pp->pt_outseg->seg_stp;
	RT_HIT_NORMAL(onormal, pp->pt_outhit, stp, &(ap->a_ray), pp->pt_outflip);
	if (pp->pt_inhit->hit_surfno == ap->a_user) {
	    VMOVE(normals[pp->pt_inhit->hit_surfno], inormal);
	    break;
	}
	if (pp->pt_outhit->hit_surfno == ap->a_user) {
	    VMOVE(normals[pp->pt_outhit->hit_surfno], onormal);
	    break;
	}
    }

    return 1;
}


int
rt_bot_smooth(struct rt_bot_internal *bot, char *bot_name, struct db_i *dbip, fastf_t norm_tol_angle)
{
    int vert_no;
    size_t i, j, k;
    struct rt_i *rtip;
    struct application ap;
    fastf_t normal_dot_tol=0.0;
    vect_t *normals;
    const double ONE_THIRD = 1.0 / 3.0;

    RT_BOT_CK_MAGIC(bot);

    if (norm_tol_angle < 0.0 || norm_tol_angle > M_PI) {
	bu_log("normal tolerance angle must be from 0 to Pi\n");
	return -2;
    }

    if ((bot->orientation == RT_BOT_UNORIENTED) && (bot->mode != RT_BOT_SOLID)) {
	bu_log("Cannot smooth unoriented BOT primitives unless they are solid objects\n");
	return -3;
    }

    normal_dot_tol = cos(norm_tol_angle);

    if (bot->normals) {
	bu_free(bot->normals, "bot->normals");
	bot->normals = NULL;
    }

    if (bot->face_normals) {
	bu_free(bot->face_normals, "bot->face_normals");
	bot->face_normals = NULL;
    }

    bot->bot_flags &= !(RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS);
    bot->num_normals = 0;
    bot->num_face_normals = 0;

    /* build an array of surface normals */
    normals = (vect_t *)bu_calloc(bot->num_faces, sizeof(vect_t), "normals");

    if (bot->orientation == RT_BOT_UNORIENTED) {
	/* need to do raytracing, do prepping */
	rtip = rt_new_rti(dbip);

	RT_APPLICATION_INIT(&ap);
	ap.a_rt_i = rtip;
	ap.a_hit = bot_smooth_hit;
	ap.a_miss = bot_smooth_miss;
	ap.a_uptr = (genptr_t)normals;
	if (rt_gettree(rtip, bot_name)) {
	    bu_log("rt_gettree failed for %s\n", bot_name);
	    return -1;
	}
	rt_prep(rtip);

	/* find the surface normal for each face */
	for (i=0; i<bot->num_faces; i++) {
	    vect_t a, b;
	    vect_t inv_dir;

	    if (bot->faces[i*3+2] < 0 || (size_t)bot->faces[i*3+2] > bot->num_vertices)
		continue; /* sanity */

	    VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
	    VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
	    VCROSS(ap.a_ray.r_dir, a, b);
	    VUNITIZE(ap.a_ray.r_dir);

	    /* calculate ray start point */
	    VADD3(ap.a_ray.r_pt, &bot->vertices[bot->faces[i*3]*3],
		  &bot->vertices[bot->faces[i*3+1]*3],
		  &bot->vertices[bot->faces[i*3+2]*3]);
	    VSCALE(ap.a_ray.r_pt, ap.a_ray.r_pt, ONE_THIRD);

	    /* back out to bounding box limits */

	    /* Compute the inverse of the direction cosines */
	    if (ap.a_ray.r_dir[X] < -SQRT_SMALL_FASTF) {
		inv_dir[X]=1.0/ap.a_ray.r_dir[X];
	    } else if (ap.a_ray.r_dir[X] > SQRT_SMALL_FASTF) {
		inv_dir[X]=1.0/ap.a_ray.r_dir[X];
	    } else {
		ap.a_ray.r_dir[X] = 0.0;
		inv_dir[X] = INFINITY;
	    }
	    if (ap.a_ray.r_dir[Y] < -SQRT_SMALL_FASTF) {
		inv_dir[Y]=1.0/ap.a_ray.r_dir[Y];
	    } else if (ap.a_ray.r_dir[Y] > SQRT_SMALL_FASTF) {
		inv_dir[Y]=1.0/ap.a_ray.r_dir[Y];
	    } else {
		ap.a_ray.r_dir[Y] = 0.0;
		inv_dir[Y] = INFINITY;
	    }
	    if (ap.a_ray.r_dir[Z] < -SQRT_SMALL_FASTF) {
		inv_dir[Z]=1.0/ap.a_ray.r_dir[Z];
	    } else if (ap.a_ray.r_dir[Z] > SQRT_SMALL_FASTF) {
		inv_dir[Z]=1.0/ap.a_ray.r_dir[Z];
	    } else {
		ap.a_ray.r_dir[Z] = 0.0;
		inv_dir[Z] = INFINITY;
	    }

	    if (!rt_in_rpp(&ap.a_ray, inv_dir, rtip->mdl_min, rtip->mdl_max)) {
		/* ray missed!!! */
		bu_log("ERROR: Ray missed target!!!!\n");
	    }
	    VJOIN1(ap.a_ray.r_pt, ap.a_ray.r_pt, ap.a_ray.r_min, ap.a_ray.r_dir);
	    ap.a_user = i;
	    (void) rt_shootray(&ap);
	}
	rt_free_rti(rtip);
    } else {
	/* calculate normals */
	for (i=0; i<bot->num_faces; i++) {
	    vect_t a, b;

	    if (bot->faces[i*3+2] < 0 || (size_t)bot->faces[i*3+2] > bot->num_vertices)
		continue; /* sanity */

	    VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
	    VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
	    VCROSS(normals[i], a, b);
	    VUNITIZE(normals[i]);
	    if (bot->orientation == RT_BOT_CW) {
		VREVERSE(normals[i], normals[i]);
	    }
	}
    }

    bot->num_normals = bot->num_faces * 3;
    bot->num_face_normals = bot->num_faces;

    bot->normals = (fastf_t *)bu_calloc(bot->num_normals * 3, sizeof(fastf_t), "bot->normals");
    bot->face_normals = (int *)bu_calloc(bot->num_face_normals * 3, sizeof(int), "bot->face_normals");

    /* process each face */
    for (i=0; i<bot->num_faces; i++) {
	vect_t def_norm; /* default normal for this face */

	VMOVE(def_norm, normals[i]);

	/* process each vertex in his face */
	for (k=0; k<3; k++) {
	    vect_t ave_norm;

	    /* the actual vertex index */
	    vert_no = bot->faces[i*3+k];
	    VSETALL(ave_norm, 0.0);

	    /* find all the faces that use this vertex */
	    for (j=0; j<bot->num_faces*3; j++) {
		if (bot->faces[j] == vert_no) {
		    size_t the_face;

		    the_face = j / 3;

		    /* add all the normals that are within tolerance
		     * this also gets def_norm
		     */
		    if (VDOT(normals[the_face], def_norm) >= normal_dot_tol) {
			VADD2(ave_norm, ave_norm, normals[the_face]);
		    }
		}
	    }
	    VUNITIZE(ave_norm);
	    VMOVE(&bot->normals[(i*3+k)*3], ave_norm);
	    bot->face_normals[i*3+k] = i*3+k;
	}
    }

    bu_free(normals, "normals");
    normals = NULL;

    bot->bot_flags |= RT_BOT_HAS_SURFACE_NORMALS;
    bot->bot_flags |= RT_BOT_USE_NORMALS;

    return 0;
}


int
rt_bot_flip(struct rt_bot_internal *bot)
{
    size_t i;
    size_t tmp_index;

    RT_BOT_CK_MAGIC(bot);

    for (i = 0; i < bot->num_faces; ++i) {
	/* Swap any two vertex references. Here we're swapping 1 and 2. */
	tmp_index = bot->faces[i*3+1];
	bot->faces[i*3+1] = bot->faces[i*3+2];
	bot->faces[i*3+2] = tmp_index;
    }

    switch (bot->orientation) {
	case RT_BOT_CCW:
	    bot->orientation = RT_BOT_CW;
	    break;
	case RT_BOT_CW:
	    bot->orientation = RT_BOT_CCW;
	    break;
	case RT_BOT_UNORIENTED:
	default:
	    break;
    }

    return 0;
}


struct tri_edges {
    struct bu_list l;
    size_t edge_1[2];
    size_t edge_2[2];
    size_t edge_3[2];
    size_t tri;
};

struct tri_pts {
    struct bu_list l;
    int a;
    int b;
    int c;
    int tri;
};


void
rt_bot_sync_func(struct rt_bot_internal *bot,
		 struct tri_edges *tep,
		 struct tri_edges *headTep,
		 struct tri_edges *usedTep)
{
    struct tri_edges *neighbor_tep;

    for (BU_LIST_FOR(neighbor_tep, tri_edges, &headTep->l)) {
	if ((tep->edge_1[0] == neighbor_tep->edge_1[0] &&
	     tep->edge_1[1] == neighbor_tep->edge_1[1]) ||
	    (tep->edge_1[0] == neighbor_tep->edge_2[0] &&
	     tep->edge_1[1] == neighbor_tep->edge_2[1]) ||
	    (tep->edge_1[0] == neighbor_tep->edge_3[0] &&
	     tep->edge_1[1] == neighbor_tep->edge_3[1]) ||
	    (tep->edge_2[0] == neighbor_tep->edge_1[0] &&
	     tep->edge_2[1] == neighbor_tep->edge_1[1]) ||
	    (tep->edge_2[0] == neighbor_tep->edge_2[0] &&
	     tep->edge_2[1] == neighbor_tep->edge_2[1]) ||
	    (tep->edge_2[0] == neighbor_tep->edge_3[0] &&
	     tep->edge_2[1] == neighbor_tep->edge_3[1]) ||
	    (tep->edge_3[0] == neighbor_tep->edge_1[0] &&
	     tep->edge_3[1] == neighbor_tep->edge_1[1]) ||
	    (tep->edge_3[0] == neighbor_tep->edge_2[0] &&
	     tep->edge_3[1] == neighbor_tep->edge_2[1]) ||
	    (tep->edge_3[0] == neighbor_tep->edge_3[0] &&
	     tep->edge_3[1] == neighbor_tep->edge_3[1])) {
	    /* Found a shared edge of a neighboring triangle whose
	     * orientation needs to be reversed.
	     */
	    size_t tmp_index;

	    BU_LIST_DEQUEUE(&neighbor_tep->l);
	    BU_LIST_APPEND(&usedTep->l, &neighbor_tep->l);

	    /* Swap any two vertex references. Here we're swapping
	     * 1 and 2. */
	    tmp_index = bot->faces[neighbor_tep->tri*3+1];
	    bot->faces[neighbor_tep->tri*3+1] = bot->faces[neighbor_tep->tri*3+2];
	    bot->faces[neighbor_tep->tri*3+2] = tmp_index;

	    /* Also need to reverse the edges in neighbor_tep */
	    tmp_index = neighbor_tep->edge_1[0];
	    neighbor_tep->edge_1[0] = neighbor_tep->edge_1[1];
	    neighbor_tep->edge_1[1] = tmp_index;
	    tmp_index = neighbor_tep->edge_2[0];
	    neighbor_tep->edge_2[0] = neighbor_tep->edge_2[1];
	    neighbor_tep->edge_2[1] = tmp_index;
	    tmp_index = neighbor_tep->edge_3[0];
	    neighbor_tep->edge_3[0] = neighbor_tep->edge_3[1];
	    neighbor_tep->edge_3[1] = tmp_index;

	    rt_bot_sync_func(bot, neighbor_tep, headTep, usedTep);
	    neighbor_tep = headTep;
	} else if ((tep->edge_1[0] == neighbor_tep->edge_1[1] &&
		    tep->edge_1[1] == neighbor_tep->edge_1[0]) ||
		   (tep->edge_1[0] == neighbor_tep->edge_2[1] &&
		    tep->edge_1[1] == neighbor_tep->edge_2[0]) ||
		   (tep->edge_1[0] == neighbor_tep->edge_3[1] &&
		    tep->edge_1[1] == neighbor_tep->edge_3[0]) ||
		   (tep->edge_2[0] == neighbor_tep->edge_1[1] &&
		    tep->edge_2[1] == neighbor_tep->edge_1[0]) ||
		   (tep->edge_2[0] == neighbor_tep->edge_2[1] &&
		    tep->edge_2[1] == neighbor_tep->edge_2[0]) ||
		   (tep->edge_2[0] == neighbor_tep->edge_3[1] &&
		    tep->edge_2[1] == neighbor_tep->edge_3[0]) ||
		   (tep->edge_3[0] == neighbor_tep->edge_1[1] &&
		    tep->edge_3[1] == neighbor_tep->edge_1[0]) ||
		   (tep->edge_3[0] == neighbor_tep->edge_2[1] &&
		    tep->edge_3[1] == neighbor_tep->edge_2[0]) ||
		   (tep->edge_3[0] == neighbor_tep->edge_3[1] &&
		    tep->edge_3[1] == neighbor_tep->edge_3[0])) {
	    /* Found a shared edge of a neighboring triangle whose
	     * orientation is fine.
	     */

	    BU_LIST_DEQUEUE(&neighbor_tep->l);
	    BU_LIST_APPEND(&usedTep->l, &neighbor_tep->l);

	    rt_bot_sync_func(bot, neighbor_tep, headTep, usedTep);
	    neighbor_tep = headTep;
	}
    }
}


int
rt_bot_sync(struct rt_bot_internal *bot)
{
    size_t i;
    struct tri_edges headTep;
    struct tri_edges usedTep;
    struct tri_edges *tep;
    size_t pt_A, pt_B, pt_C;

    RT_BOT_CK_MAGIC(bot);

    /* Nothing to do */
    if (bot->num_faces < 2)
	return 0;

    BU_LIST_INIT(&headTep.l);
    BU_LIST_INIT(&usedTep.l);

    /* Initialize tep list */
    for (i = 0; i < bot->num_faces; ++i) {
	tep = (struct tri_edges *)bu_calloc(1, sizeof(struct tri_edges), "rt_bot_sync: tep");
	BU_LIST_APPEND(&headTep.l, &tep->l);

	pt_A = bot->faces[i*3+0];
	pt_B = bot->faces[i*3+1];
	pt_C = bot->faces[i*3+2];

	tep->tri = i;
	tep->edge_1[0] = pt_A;
	tep->edge_1[1] = pt_B;
	tep->edge_2[0] = pt_B;
	tep->edge_2[1] = pt_C;
	tep->edge_3[0] = pt_C;
	tep->edge_3[1] = pt_A;
    }

    while (BU_LIST_WHILE(tep, tri_edges, &headTep.l)) {
	BU_LIST_DEQUEUE(&tep->l);
	BU_LIST_APPEND(&usedTep.l, &tep->l);

	rt_bot_sync_func(bot, tep, &headTep, &usedTep);
    }

    while (BU_LIST_WHILE(tep, tri_edges, &usedTep.l)) {
	BU_LIST_DEQUEUE(&tep->l);
	bu_free(tep, "rt_bot_sync: tep");
	tep = NULL; /* sanity */
    }

    return 0;
}

void
rt_bot_split_func(struct tri_pts *tpp,
		  struct tri_pts *headTpp,
		  struct tri_pts *usedTpp)
{
    struct tri_pts *neighbor_tpp;

    for (BU_LIST_FOR(neighbor_tpp, tri_pts, &headTpp->l)) {
	if (tpp->a == neighbor_tpp->a ||
	    tpp->a == neighbor_tpp->b ||
	    tpp->a == neighbor_tpp->c ||
	    tpp->b == neighbor_tpp->a ||
	    tpp->b == neighbor_tpp->b ||
	    tpp->b == neighbor_tpp->c ||
	    tpp->c == neighbor_tpp->a ||
	    tpp->c == neighbor_tpp->b ||
	    tpp->c == neighbor_tpp->c) {
	    /* Found a shared pt of a neighboring triangle */

	    BU_LIST_DEQUEUE(&neighbor_tpp->l);
	    BU_LIST_APPEND(&usedTpp->l, &neighbor_tpp->l);

	    rt_bot_split_func(neighbor_tpp, headTpp, usedTpp);
	    neighbor_tpp = headTpp;
	}
    }
}

#define REMAP_BOT_VERTS(_oldbot,_newbot,_vmap,_vcount,_ovi,_i) { \
size_t vmi; \
\
for (vmi=0; vmi<_vcount; vmi++) { \
    if (_ovi == _vmap[vmi]) { \
	_newbot->faces[_i] = vmi; \
	break; \
    } \
} \
\
if (vmi == _vcount) { \
    _vmap[_vcount] = _ovi; \
    _newbot->faces[_i] = _vcount; \
    VMOVE(&_newbot->vertices[_vcount*3],&_oldbot->vertices[_ovi*3]); \
    ++_vcount; \
} \
}


struct rt_bot_internal *
rt_bot_create(struct rt_bot_internal *bot, struct tri_pts *newTpp)
{
    size_t i;
    struct tri_pts *tpp;
    struct rt_bot_internal *newbot;

    BU_GETSTRUCT(newbot, rt_bot_internal);

    newbot->num_faces = 0;
    for (BU_LIST_FOR(tpp, tri_pts, &newTpp->l)) {
	++newbot->num_faces;
    }

    newbot->magic = bot->magic;
    newbot->mode = bot->mode;
    newbot->orientation = bot->orientation;
    newbot->bot_flags = bot->bot_flags;

    {
	size_t vcount;
	int *vmap = (int *)bu_calloc(bot->num_vertices * 3, sizeof(int), "Bot vertices");

	newbot->vertices = (fastf_t *)bu_calloc(bot->num_vertices * 3, sizeof(fastf_t), "Bot vertices");
	newbot->faces = (int *)bu_calloc(newbot->num_faces * 3, sizeof(int), "Bot faces");
	if (bot->mode == RT_BOT_PLATE) {
	    newbot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t), "Bot thickness");
	    newbot->face_mode = bu_bitv_new(newbot->num_faces);
	}

	i = 0;
	vcount = 0;
	for (BU_LIST_FOR(tpp, tri_pts, &newTpp->l)) {

	    REMAP_BOT_VERTS(bot,newbot,vmap,vcount,tpp->a,i*3);
	    REMAP_BOT_VERTS(bot,newbot,vmap,vcount,tpp->b,i*3+1);
	    REMAP_BOT_VERTS(bot,newbot,vmap,vcount,tpp->c,i*3+2);

	    if (bot->mode == RT_BOT_PLATE) {
		newbot->thickness[i] = bot->thickness[tpp->tri];

		if (BU_BITTEST(bot->face_mode, tpp->tri))
		    BU_BITSET(newbot->face_mode, i);
		/* else already cleared via bu_bitv_new() */
	    }

	    ++i;
	}

	newbot->num_vertices = vcount;
	bu_free(vmap, "rt_bot_create: vmap");
    }

    return newbot;
}

struct rt_bot_list *
rt_bot_split(struct rt_bot_internal *bot)
{
    size_t i;
    size_t first;
    struct tri_pts headTp;
    struct tri_pts usedTp;
    struct tri_pts *tpp;
    struct rt_bot_list *headRblp = (struct rt_bot_list *)0;
    struct rt_bot_list *rblp;

    RT_BOT_CK_MAGIC(bot);

    BU_GETSTRUCT(headRblp, rt_bot_list);
    BU_LIST_INIT(&headRblp->l);

    /* Nothing to do */
    if (bot->num_faces < 2)
	return headRblp;

    BU_LIST_INIT(&headTp.l);
    BU_LIST_INIT(&usedTp.l);

    /* Initialize tpp list */
    for (i = 0; i < bot->num_faces; ++i) {
	tpp = (struct tri_pts *)bu_calloc(1, sizeof(struct tri_pts), "rt_bot_split: tpp");
	BU_LIST_APPEND(&headTp.l, &tpp->l);

	tpp->tri = i;
	tpp->a = bot->faces[i*3+0];
	tpp->b = bot->faces[i*3+1];
	tpp->c = bot->faces[i*3+2];
    }

    first = 1;
    while (BU_LIST_WHILE(tpp, tri_pts, &headTp.l)) {
	BU_LIST_DEQUEUE(&tpp->l);
	BU_LIST_APPEND(&usedTp.l, &tpp->l);

	rt_bot_split_func(tpp, &headTp, &usedTp);

	if (first) {
	    first = 0;

	    if (BU_LIST_NON_EMPTY(&headTp.l)) {
		/* Create a new bot */
		BU_GETSTRUCT(rblp, rt_bot_list);
		rblp->bot = rt_bot_create(bot, &usedTp);
		BU_LIST_APPEND(&headRblp->l, &rblp->l);
	    }
	} else {
	    /* Create a new bot */
	    BU_GETSTRUCT(rblp, rt_bot_list);
	    rblp->bot = rt_bot_create(bot, &usedTp);
	    BU_LIST_APPEND(&headRblp->l, &rblp->l);
	}

	while (BU_LIST_WHILE(tpp, tri_pts, &usedTp.l)) {
	    BU_LIST_DEQUEUE(&tpp->l);
	    bu_free(tpp, "rt_bot_split: tpp");
	    tpp = NULL; /* sanity */
	}
    }

    return headRblp;
}

void
rt_bot_list_free(struct rt_bot_list *headRblp, int fbflag)
{
    struct rt_bot_list *rblp;

    while (BU_LIST_WHILE(rblp, rt_bot_list, &headRblp->l)) {
	/* Remove from list and free */
	BU_LIST_DEQUEUE(&rblp->l);

	if (fbflag)
	    rt_bot_ifree2(rblp->bot);

	bu_free(rblp, "rt_bot_list_free: rblp");
    }

    bu_free(headRblp, "rt_bot_list_free: headRblp");
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
