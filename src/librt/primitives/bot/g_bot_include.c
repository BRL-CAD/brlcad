/*                   G _ B O T _ I N C L U D E . C
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
/** @file primitives/bot/g_bot_include.c
 *
 * This file contains all the routines for "g_bot.c" that contain
 * references to "tri_specific" structs. This file is included in
 * "g_bot.c" twice. Each time the macro TRI_TYPE is defined to reflect
 * the desired version of the "tri_specific" structure:
 *
 * TRI_TYPE == float  -> use the "tri_float_specific" struct
 * TRI_TYPE == double -> use the original "tri_specific" struct
 *
 */


/**
 * R T _ B O T F A C E
 *
 * This function is called with pointers to 3 points, and is used to
 * prepare BOT faces.  ap, bp, cp point to vect_t points.
 *
 * Returns 0 if the 3 points didn't form a plane (eg, colinear, etc).
 * Returns # pts (3) if a valid plane resulted.
 */
int
XGLUE(rt_botface_w_normals_, TRI_TYPE)(struct soltab *stp,
				       struct bot_specific *bot,
				       fastf_t *ap,
				       fastf_t *bp,
				       fastf_t *cp,
				       fastf_t *vertex_normals, /* array of nine values (three unit normals vectors) */
				       size_t face_no,
				       const struct bn_tol *tol)
{
    register XGLUE(tri_specific_, TRI_TYPE) *trip;
    vect_t work;
    fastf_t m1, m2, m3, m4;
    size_t i;

    BU_GETTYPE(trip, XGLUE(tri_specific_, TRI_TYPE));
    VMOVE(trip->tri_A, ap);
    VSUB2(trip->tri_BA, bp, ap);
    VSUB2(trip->tri_CA, cp, ap);
    VCROSS(trip->tri_wn, trip->tri_BA, trip->tri_CA);
    trip->tri_surfno = face_no;

    /* Check to see if this plane is a line or pnt */
    m1 = MAGSQ(trip->tri_BA);
    m2 = MAGSQ(trip->tri_CA);
    VSUB2(work, bp, cp);
    m3 = MAGSQ(work);
    m4 = MAGSQ(trip->tri_wn);
    if (m1 < tol->dist_sq
	|| m2 < tol->dist_sq
	|| m3 < tol->dist_sq
	|| m4 < tol->dist_sq)
    {
	bu_free((char *)trip, "getstruct tri_specific");

	if (RT_G_DEBUG & DEBUG_SHOOT) {
	    bu_log("%s: degenerate facet #%zu\n",
		   stp->st_name, face_no);
	    bu_log("\t(%g %g %g) (%g %g %g) (%g %g %g)\n",
		   V3ARGS(ap), V3ARGS(bp), V3ARGS(cp));
	}
	return 0;			/* BAD */
    }

    if ((bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) && (bot->bot_flags & RT_BOT_USE_NORMALS) && vertex_normals) {
	trip->tri_normals = (NORM_TYPE *)bu_malloc(9 * sizeof(NORM_TYPE), "trip->tri_normals");
	for (i=0; i<3; i++) {
	    size_t j;

	    for (j=0; j<3; j++) {
		trip->tri_normals[i*3+j] = vertex_normals[i*3+j] * NORMAL_SCALE;
	    }
	}
    } else {
	trip->tri_normals = (NORM_TYPE *)NULL;
    }

    /* wn is a normal of not necessarily unit length.
     * N is an outward pointing unit normal.
     * We depend on the points being given in CCW order here.
     */
    VMOVE(trip->tri_N, trip->tri_wn);
    VUNITIZE(trip->tri_N);
    if (bot->bot_orientation == RT_BOT_CW)
	VREVERSE(trip->tri_N, trip->tri_N);

    /* Add this face onto the linked list for this solid */
    trip->tri_forw = (XGLUE(tri_specific_, TRI_TYPE) *)bot->bot_facelist;
    bot->bot_facelist = (genptr_t)trip;
    return 3;				/* OK */
}


/*
 * Do the prep to support pieces for a BOT/ARS
 */
void
XGLUE(rt_bot_prep_pieces_, TRI_TYPE)(struct bot_specific *bot,
				     struct soltab *stp,
				     size_t ntri,
				     const struct bn_tol *tol)
{
    struct bound_rpp *minmax = (struct bound_rpp *)NULL;
    XGLUE(tri_specific_, TRI_TYPE) **fap;
    register XGLUE(tri_specific_, TRI_TYPE) *trip;
    point_t b, c;
    point_t d, e, f;
    vect_t offset;
    fastf_t los;
    size_t surfno;
    long num_rpps;
    size_t tri_per_piece, tpp_m1;

    tri_per_piece = bot->bot_tri_per_piece = rt_bot_tri_per_piece;

    num_rpps = ntri / tri_per_piece;
    if (ntri % tri_per_piece) num_rpps++;

    stp->st_npieces = num_rpps;

    fap = (XGLUE(tri_specific_, TRI_TYPE) **)
	bu_malloc(sizeof(XGLUE(tri_specific_, TRI_TYPE) *) * ntri,
		  "bot_facearray");
    bot->bot_facearray = (genptr_t *)fap;

    stp->st_piece_rpps = (struct bound_rpp *)
	bu_malloc(sizeof(struct bound_rpp) * num_rpps,
		  "st_piece_rpps");


    tpp_m1 = tri_per_piece - 1;
    trip = bot->bot_facelist;
    minmax = &stp->st_piece_rpps[num_rpps-1];
    minmax->min[X] = minmax->max[X] = trip->tri_A[X];
    minmax->min[Y] = minmax->max[Y] = trip->tri_A[Y];
    minmax->min[Z] = minmax->max[Z] = trip->tri_A[Z];
    for (surfno=ntri-1; trip; trip = trip->tri_forw, surfno--) {

	if ((surfno % tri_per_piece) == tpp_m1) {
	    /* top most surfno in a piece group */
	    /* first surf for this piece */
	    minmax = &stp->st_piece_rpps[surfno / tri_per_piece];

	    minmax->min[X] = minmax->max[X] = trip->tri_A[X];
	    minmax->min[Y] = minmax->max[Y] = trip->tri_A[Y];
	    minmax->min[Z] = minmax->max[Z] = trip->tri_A[Z];
	} else {
	    VMINMAX(minmax->min, minmax->max, trip->tri_A);
	}

	fap[surfno] = trip;

	if (bot->bot_mode == RT_BOT_PLATE || bot->bot_mode == RT_BOT_PLATE_NOCOS) {
	    if (BU_BITTEST(bot->bot_facemode, surfno)) {
		/* Append full thickness on both sides */
		los = bot->bot_thickness[surfno];
	    } else {
		/* Center thickness.  Append 1/2 thickness on both sides */
		los = bot->bot_thickness[surfno] * 0.51;
	    }
	} else {
	    /* Prevent the RPP from being 0 thickness */
	    los = tol->dist;	/* typ 0.0005mm */
	    if (los < SMALL_FASTF)
		los = SMALL_FASTF;
	}

	VADD2(b, trip->tri_BA, trip->tri_A);
	VADD2(c, trip->tri_CA, trip->tri_A);
	VMINMAX(minmax->min, minmax->max, b);
	VMINMAX(minmax->min, minmax->max, c);

	/* Offset face in +los */
	VSCALE(offset, trip->tri_N, los);
	VADD2(d, trip->tri_A, offset);
	VADD2(e, b, offset);
	VADD2(f, c, offset);
	VMINMAX(minmax->min, minmax->max, d);
	VMINMAX(minmax->min, minmax->max, e);
	VMINMAX(minmax->min, minmax->max, f);

	/* Offset face in -los */
	VSCALE(offset, trip->tri_N, -los);
	VADD2(d, trip->tri_A, offset);
	VADD2(e, b, offset);
	VADD2(f, c, offset);
	VMINMAX(minmax->min, minmax->max, d);
	VMINMAX(minmax->min, minmax->max, e);
	VMINMAX(minmax->min, minmax->max, f);

	VMINMAX(stp->st_min, stp->st_max, minmax->min);
	VMINMAX(stp->st_min, stp->st_max, minmax->max);

    }

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
XGLUE(rt_bot_prep_, TRI_TYPE)(struct soltab *stp, struct rt_bot_internal *bot_ip, struct rt_i *rtip)
{
    register struct bot_specific *bot;
    const struct bn_tol *tol = &rtip->rti_tol;
    size_t tri_index, i;
    fastf_t dx, dy, dz;
    fastf_t f;
    size_t ntri = 0;
    fastf_t los = tol->dist;

    if (los < SMALL_FASTF)
	los = SMALL_FASTF;

    RT_BOT_CK_MAGIC(bot_ip);

    BU_GET(bot, struct bot_specific);
    stp->st_specific = (genptr_t)bot;
    bot->bot_mode = bot_ip->mode;
    bot->bot_orientation = bot_ip->orientation;
    bot->bot_flags = bot_ip->bot_flags;
    if (bot_ip->thickness) {
	bot->bot_thickness = (fastf_t *)bu_calloc(bot_ip->num_faces, sizeof(fastf_t), "bot_thickness");
	for (tri_index=0; tri_index < bot_ip->num_faces; tri_index++)
	    bot->bot_thickness[tri_index] = bot_ip->thickness[tri_index];
    }
    if (bot_ip->face_mode)
	bot->bot_facemode = bu_bitv_dup(bot_ip->face_mode);
    bot->bot_facelist = (XGLUE(tri_specific_, TRI_TYPE) *)NULL;

    for (tri_index=0; tri_index < bot_ip->num_faces; tri_index++) {
	point_t p1, p2, p3;
	long default_normal=-1;
	size_t pt1, pt2, pt3;

	pt1 = bot_ip->faces[tri_index*3];
	pt2 = bot_ip->faces[tri_index*3 + 1];
	pt3 = bot_ip->faces[tri_index*3 + 2];
	if (pt1 >= bot_ip->num_vertices
	    || pt2 >= bot_ip->num_vertices
	    || pt3 >= bot_ip->num_vertices)
	{
	    bu_log("face number %zu of bot(%s) references a non-existent vertex\n",
		   tri_index, stp->st_name);
	    return -1;
	}

	VMOVE(p1, &bot_ip->vertices[pt1*3]);
	VMOVE(p2, &bot_ip->vertices[pt2*3]);
	VMOVE(p3, &bot_ip->vertices[pt3*3]);
	
	if (rt_bot_minpieces <= 0 || bot_ip->num_faces <= rt_bot_minpieces) { 	 
	    VMINMAX(stp->st_min, stp->st_max, p1); 	 
	    VMINMAX(stp->st_min, stp->st_max, p2); 	 
	    VMINMAX(stp->st_min, stp->st_max, p3); 	 
	}

	if ((bot_ip->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) && (bot_ip->bot_flags & RT_BOT_USE_NORMALS)
	    && (bot_ip->num_normals > 0) && (bot_ip->num_face_normals > tri_index)) {
	    for (i=0; i<3; i++) {
		size_t idx;

		idx = bot_ip->face_normals[tri_index*3 + i];
		if (idx < bot_ip->num_normals) {
		    default_normal = idx;
		}
	    }
	    if (default_normal < 0) {
		if (rt_botface(stp, bot, p1, p2, p3, tri_index, tol) > 0)
		    ntri++;
	    } else {
		fastf_t normals[9];

		for (i=0; i<3; i++) {
		    size_t idx;

		    idx = bot_ip->face_normals[tri_index*3 + i];
		    if (idx > bot_ip->num_normals) {
			VMOVE(&normals[i*3], &bot_ip->normals[default_normal*3]);
		    } else {
			VMOVE(&normals[i*3], &bot_ip->normals[idx*3]);
		    }
		}
		if (rt_botface_w_normals(stp, bot, p1, p2, p3, normals, tri_index, tol) > 0)
		    ntri++;
	    }
	} else {
	    if (rt_botface(stp, bot, p1, p2, p3, tri_index, tol) > 0)
		ntri++;
	}
    }

    if (bot->bot_facelist == (XGLUE(tri_specific_, TRI_TYPE) *)0) {
	bu_log("bot(%s):  no faces\n", stp->st_name);
	return -1;             /* BAD */
    }

    bot->bot_ntri = ntri;

    if (rt_bot_minpieces > 0 && bot_ip->num_faces > rt_bot_minpieces) {
	rt_bot_prep_pieces(bot, stp, ntri, tol);
    }

    /* zero thickness will get missed by the raytracer */
    if (NEAR_EQUAL(stp->st_min[X], stp->st_max[X], los)) {
	stp->st_min[X] -= los;
	stp->st_max[X] += los;
    }
    if (NEAR_EQUAL(stp->st_min[Y], stp->st_max[Y], los)) {
	stp->st_min[Y] -= los;
	stp->st_max[Y] += los;
    }
    if (NEAR_EQUAL(stp->st_min[Z], stp->st_max[Z], los)) {
	stp->st_min[Z] -= los;
	stp->st_max[Z] += los;
    }

    VADD2SCALE(stp->st_center, stp->st_max, stp->st_min, 0.5);

    dx = (stp->st_max[X] - stp->st_min[X])/2;
    f = dx;
    dy = (stp->st_max[Y] - stp->st_min[Y])/2;
    if (dy > f) f = dy;
    dz = (stp->st_max[Z] - stp->st_min[Z])/2;
    if (dz > f) f = dz;
    stp->st_aradius = f;
    stp->st_bradius = sqrt(dx*dx + dy*dy + dz*dz);

    /*
     * Support for solid 'pieces'
     *
     * Each piece can represent a number of triangles.  This is
     * encoded in bot->bot_tri_per_piece.
     *
     * These array allocations can't be made until the number of
     * triangles are known.
     *
     * If the number of triangles is too small, don't bother making
     * pieces, the overhead isn't worth it.
     *
     * To disable BoT pieces, on the RT command line specify:
     * -c "set rt_bot_minpieces=0"
     */

    return 0;
}


static int
XGLUE(rt_bot_plate_segs_, TRI_TYPE)(struct hit *hits,
				    size_t nhits,
				    struct soltab *stp,
				    struct xray *rp,
				    struct application *ap,
				    struct seg *seghead,
				    struct bot_specific *bot)
{
    register struct seg *segp;
    register size_t i;
    register fastf_t los;
    size_t surfno;
    static const int IN = 0;
    static const int OUT = 1;

    if (rp) RT_CK_RAY(rp);

    for (i=0; i < nhits; i++) {
	XGLUE(tri_specific_, TRI_TYPE) *trip=(XGLUE(tri_specific_, TRI_TYPE) *)hits[i].hit_private;

	surfno = hits[i].hit_surfno;

	if (bot->bot_mode == RT_BOT_PLATE_NOCOS)
	    los = bot->bot_thickness[surfno];
	else {
	    los = bot->bot_thickness[surfno] / hits[i].hit_vpriv[X];
	    if (los < 0.0)
		los = -los;
	}

	if (BU_BITTEST(bot->bot_facemode, hits[i].hit_surfno)) {

	    /* append thickness to hit point */
	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;

	    /* set in hit */
	    segp->seg_in = hits[i];
	    BOT_UNORIENTED_NORM(ap, &segp->seg_in, IN);

	    /* set out hit */
	    segp->seg_out.hit_surfno = surfno;
	    segp->seg_out.hit_dist = segp->seg_in.hit_dist + los;
	    VMOVE(segp->seg_out.hit_vpriv, hits[i].hit_vpriv);
	    BOT_UNORIENTED_NORM(ap, &segp->seg_out, OUT);
	    segp->seg_out.hit_private = segp->seg_in.hit_private;
	    segp->seg_out.hit_rayp = &ap->a_ray;

	    BU_LIST_INSERT(&(seghead->l), &(segp->l));
	} else {
	    /* center thickness about hit point */
	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;

	    /* set in hit */
	    segp->seg_in.hit_surfno = surfno;
	    VMOVE(segp->seg_in.hit_vpriv, hits[i].hit_vpriv);
	    BOT_UNORIENTED_NORM(ap, &segp->seg_in, IN);
	    segp->seg_in.hit_private = hits[i].hit_private;
	    segp->seg_in.hit_dist = hits[i].hit_dist - (los*0.5);
	    segp->seg_in.hit_rayp = &ap->a_ray;

	    /* set out hit */
	    segp->seg_out.hit_surfno = surfno;
	    segp->seg_out.hit_dist = segp->seg_in.hit_dist + los;
	    VMOVE(segp->seg_out.hit_vpriv, hits[i].hit_vpriv);
	    BOT_UNORIENTED_NORM(ap, &segp->seg_out, OUT);
	    segp->seg_out.hit_private = hits[i].hit_private;
	    segp->seg_out.hit_rayp = &ap->a_ray;

	    BU_LIST_INSERT(&(seghead->l), &(segp->l));
	}
    }
    /* Every hit turns into two, and makes a seg.  No leftovers */
    return nhits*2;
}


static int
XGLUE(rt_bot_unoriented_segs_, TRI_TYPE)(struct hit *hits,
					 size_t nhits,
					 struct soltab *stp,
					 struct xray *rp,
					 struct application *ap,
					 struct seg *seghead)
{
    register struct seg *segp;
    register size_t i, j;

    /*
     * RT_BOT_SOLID, RT_BOT_UNORIENTED.
     */
    fastf_t rm_dist=0.0;
    int removed=0;
    static const int IN = 0;
    static const int OUT = 1;

    if (nhits == 1) {
	XGLUE(tri_specific_, TRI_TYPE) *trip=(XGLUE(tri_specific_, TRI_TYPE) *)hits[0].hit_private;

	/* make a zero length partition */
	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;

	/* set in hit */
	segp->seg_in = hits[0];
	BOT_UNORIENTED_NORM(ap, &segp->seg_in, IN);

	/* set out hit */
	segp->seg_out = hits[0];
	BOT_UNORIENTED_NORM(ap, &segp->seg_out, OUT);

	BU_LIST_INSERT(&(seghead->l), &(segp->l));
	return 1;
    }

    /* Remove duplicate hits */
    for (i=0; i<nhits-1; i++) {
	fastf_t dist;

	dist = hits[i].hit_dist - hits[i+1].hit_dist;
	if (NEAR_ZERO(dist, ap->a_rt_i->rti_tol.dist)) {
	    removed++;
	    rm_dist = hits[i+1].hit_dist;
	    for (j=i; j<nhits-1; j++)
		hits[j] = hits[j+1];
	    nhits--;
	    i--;
	}
    }


    if (nhits == 1)
	return 0;

    if (nhits&1 && removed) {
	/* If we have an odd number of hits and have removed a
	 * duplicate, then it was likely on an edge, so remove the one
	 * we left.
	 */
	for (i=0; i<nhits; i++) {
	    if (ZERO(hits[i].hit_dist - rm_dist)) {
		for (j=i; j<nhits-1; j++)
		    hits[j] = hits[j+1];
		nhits--;
		i--;
		break;
	    }
	}
    }

    for (i=0; i<(nhits&~1); i += 2) {
	XGLUE(tri_specific_, TRI_TYPE) *trip;

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;

	/* set in hit */
	segp->seg_in = hits[i];
	trip = (XGLUE(tri_specific_, TRI_TYPE) *)hits[i].hit_private;
	BOT_UNORIENTED_NORM(ap, &segp->seg_in, IN);

	/* set out hit */
	segp->seg_out = hits[i+1];
	trip = (XGLUE(tri_specific_, TRI_TYPE) *)hits[i+1].hit_private;
	BOT_UNORIENTED_NORM(ap, &segp->seg_out, OUT);

	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }
    if (nhits&1) {
	if (RT_G_DEBUG & DEBUG_SHOOT) {
	    bu_log("rt_bot_unoriented_segs(%s): WARNING: odd number of hits (%zu), last hit ignored\n",
		   stp->st_name, nhits);
	    bu_log("\tray = -p %g %g %g -d %g %g %g\n",
		   V3ARGS(rp->r_pt), V3ARGS(rp->r_dir));
	}
	nhits--;
    }
    return nhits;
}


/**
 * R T _ B O T _ M A K E S E G S
 *
 * Given an array of hits, make segments out of them.  Exactly how
 * this is to be done depends on the mode of the BoT.
 */
HIDDEN int
XGLUE(rt_bot_makesegs_, TRI_TYPE)(struct hit *hits, size_t nhits, struct soltab *stp,
				  struct xray *rp, struct application *ap,
				  struct seg *seghead, struct rt_piecestate *psp)
{
    struct bot_specific *bot = (struct bot_specific *)stp->st_specific;
    register struct seg *segp;
    register ssize_t i;
    static const int IN = 0;
    static const int OUT = 1;
    /* TODO: review the use of a signed tmp var. Var i was changed to be signed in
     * r44239 as a bug in another project was segfaulting. */
    ssize_t snhits = (ssize_t)nhits;

    RT_CK_SOLTAB(stp);

    if (bot->bot_mode == RT_BOT_SURFACE) {
	for (i=0; i<snhits; i++) {
	    XGLUE(tri_specific_, TRI_TYPE) *trip=(XGLUE(tri_specific_, TRI_TYPE) *)hits[i].hit_private;

	    RT_GET_SEG(segp, ap->a_resource);
	    segp->seg_stp = stp;

	    /* set in hit */
	    segp->seg_in = hits[i];
	    BOT_UNORIENTED_NORM(ap, &segp->seg_in, IN);

	    /* set out hit */
	    segp->seg_out = hits[i];
	    BOT_UNORIENTED_NORM(ap, &segp->seg_out, OUT);
	    BU_LIST_INSERT(&(seghead->l), &(segp->l));
	}
	/* Every hit turns into two, and makes a seg.  No leftovers */
	return snhits*2;
    }

    BU_ASSERT(bot->bot_mode == RT_BOT_SOLID);

    if (bot->bot_orientation == RT_BOT_UNORIENTED) {
	return rt_bot_unoriented_segs(hits, snhits, stp, rp, ap, seghead, bot);
    }

    /*
     * RT_BOT_SOLID, RT_BOT_ORIENTED.
     *
     * From this point on, process very similar to a polysolid
     */

    /* Remove duplicate hits */
    {
	register ssize_t j, k, l;

	for (i=0; i<snhits-1; i++) {
	    fastf_t dist;
	    fastf_t dn;

	    dn = hits[i].hit_vpriv[X];

	    k = i + 1;
	    dist = hits[i].hit_dist - hits[k].hit_dist;

	    /* count number of hits at this distance */
	    while (NEAR_ZERO(dist, ap->a_rt_i->rti_tol.dist)) {
		k++;
		if (k > snhits - 1)
		    break;
		dist = hits[i].hit_dist - hits[k].hit_dist;
	    }

	    if ((k - i) == 2 && dn * hits[i+1].hit_vpriv[X] > 0) {
		/* a pair of hits at the same distance and both are exits or entrances,
		 * likely an edge hit, remove one */
		for (j=i; j<snhits-1; j++)
		    hits[j] = hits[j+1];
		if (psp) {
		    psp->htab.end--;
		}
		snhits--;
		i--;
		continue;
	    } else if ((k - i) > 2) {
		ssize_t keep1=-1, keep2=-1;
		ssize_t enters=0, exits=0;
		int reorder=0;
		int reorder_failed=0;

		/* more than two hits at the same distance, likely a vertex hit
		 * try to keep just two, one entrance and one exit.
		 * unless they are all entrances or all exits, then just keep one */

		/* first check if we need to do anything */
		for (j=0; j<k; j++) {
		    if (hits[j].hit_vpriv[X] > 0)
			exits++;
		    else
			enters++;
		}

		if (k%2) {
		    if (exits == (enters - 1)) {
			reorder = 1;
		    }
		} else {
		    if (exits == enters) {
			reorder = 1;
		    }
		}

		if (reorder) {
		    struct hit tmp_hit;
		    int changed=0;

		    for (j=i; j<k; j++) {

			if (j%2) {
			    if (hits[j].hit_vpriv[X] > 0) {
				continue;
			    }
			    /* should be an exit here */
			    l = j+1;
			    while (l < k) {
				if (hits[l].hit_vpriv[X] > 0) {
				    /* swap with this exit */
				    tmp_hit = hits[j];
				    hits[j] = hits[l];
				    hits[l] = tmp_hit;
				    changed = 1;
				    break;
				}
				l++;
			    }
			    if (hits[j].hit_vpriv[X] < 0) {
				reorder_failed = 1;
				break;
			    }
			} else {
			    if (hits[j].hit_vpriv[X] < 0) {
				continue;
			    }
			    /* should be an entrance here */
			    l = j+1;
			    while (l < k) {
				if (hits[l].hit_vpriv[X] < 0) {
				    /* swap with this entrance */
				    tmp_hit = hits[j];
				    hits[j] = hits[l];
				    hits[l] = tmp_hit;
				    changed = 1;
				    break;
				}
				l++;
			    }
			    if (hits[j].hit_vpriv[X] > 0) {
				reorder_failed = 1;
				break;
			    }
			}
		    }
		    if (changed) {
			/* if we have re-ordered these hits, make sure they are really
			 * at the same distance.
			 */
			for (j=i+1; j<k; j++) {
			    hits[j].hit_dist = hits[i].hit_dist;
			}
		    }
		}
		if (!reorder || reorder_failed) {

		    exits = 0;
		    enters = 0;
		    if (i == 0) {
			dn = 1.0;
		    } else {
			dn = hits[i-1].hit_vpriv[X];
		    }
		    for (j=i; j<k; j++) {
			if (hits[j].hit_vpriv[X] > 0)
			    exits++;
			else
			    enters++;
			if (dn * hits[j].hit_vpriv[X] < 0) {
			    if (keep1 == -1) {
				keep1 = j;
				dn = hits[j].hit_vpriv[X];
			    } else if (keep2 == -1) {
				keep2 = j;
				dn = hits[j].hit_vpriv[X];
				break;
			    }
			}
		    }

		    if (keep2 == -1) {
			/* did not find two keepers, perhaps they were
			 * all entrances or all exits.
			 */
			if (exits == k - i || enters == k - i) {
			    /* eliminate all but one entrance or exit */
			    for (j=k-1; j>i; j--) {
				/* delete this hit */
				for (l=j; l<snhits-1; l++)
				    hits[l] = hits[l+1];
				if (psp) {
				    psp->htab.end--;
				}
				snhits--;
			    }
			    i--;
			}
		    } else {
			/* found an entrance and an exit to keep */
			for (j=k-1; j>=i; j--) {
			    if (j != keep1 && j != keep2) {
				/* delete this hit */
				for (l=j; l<snhits-1; l++)
				    hits[l] = hits[l+1];
				if (psp) {
				    psp->htab.end--;
				}
				snhits--;
			    }
			}
			i--;
		    }
		}
	    }
	}

	/*
	 * Handle cases where there are multiple adjacent entrances or
	 * exits along the shotline.  Using the FILO approach where we
	 * keep the "First In" from multiple entrances and the "Last
	 * Out" for multiple exits.
	 *
	 * Many of these cases were being generated when the shot ray
	 * grazed a surface.  Grazing shots should USUALLY be treated
	 * as non-hits (and is the case for other primitives).  With
	 * BoTs, however, these can cause multiple entrances and exits
	 * when adjacent surfaces are hit.
	 *
	 * Example #1: CROSS-SECTION OF CONVEX SOLID
	 *
	 *                      --------------
	 *                      |            |
	 *                      |entrance    |exit
	 *   ray-->  ------------            ------------
	 *           |entrance                           |exit
	 *           |                                   |
	 *
	 * For this grazing hit, LOS(X) was being shown as:
	 *                      --------------
	 *                      |            |
	 *                      |entrance    |exit
	 *   ray-->  XXXXXXXXXXXX            XXXXXXXXXXXXX
	 *           |entrance                           |exit
	 *           |                                   |
	 *
	 * Using a FILO approach, now LOS(X) shows as:
	 *                      --------------
	 *                      |            |
	 *                      |entrance    |exit
	 *   ray-->  XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
	 *           |entrance                           |exit
	 *           |                                   |
	 *
	 */
	for (i=0; i<snhits-1; i++) {
	    if (hits[i].hit_vpriv[X] < 0.0) { /* entering */
		k = i + 1;
		while ((k < snhits) && (hits[k].hit_vpriv[X] < 0.0)) {
		    for (j=i; j<snhits-1; j++)
			hits[j] = hits[j+1];
		    snhits--;
		}
	    } else if (hits[i].hit_vpriv[X] > 0.0) { /* exiting */
		k = i + 1;
		while ((k < snhits) && (hits[k].hit_vpriv[X] > 0.0)) {
		    for (j=i+1; j<snhits-1; j++)
			hits[j] = hits[j+1];
		    snhits--;
		}
	    }
	}

	/*
	 * Note that we could try to use a LIFO approach so that we
	 * more consistently count all grazing hits as a miss, but
	 * there's an increased chance of slipping through a crack
	 * with BoTs without a change to check mesh neighbors:
	 *
	 * Using a LIFO approach, the above LOS(X) would have been:
	 *
	 *                      --------------
	 *                      |            |
	 *                      |entrance    |exit
	 *   ray-->  -----------XXXXXXXXXXXXXX------------
	 *           |entrance                           |exit
	 *           |                                   |
	 *
	 * Example #2: CROSS-SECTION OF CONCAVE SOLID
	 *
	 *   ray-->  ------------            ------------
	 *           |entrance  |exit        |entrance  |exit
	 *           |          |            |          |
	 *                      --------------
	 *
	 * Using LIFO, we would report a miss for the concave case,
	 * but with FILO this will return two hit segments.
	 *
	 *   ray-->  XXXXXXXXXXXX            XXXXXXXXXXXX
	 *           |entrance  |exit        |entrance  |exit
	 *           |          |            |          |
	 *                      --------------
	 */
    }

    /* if first hit is an exit, it is likely due to the "piece" for
     * the corresponding entrance not being processed (this is OK, but
     * we need to eliminate the stray exit hit)
     */
    while (snhits > 0 && hits[0].hit_vpriv[X] > 0.0) {
	ssize_t j;

	for (j=1; j<snhits; j++) {
	    hits[j-1] = hits[j];
	}
	snhits--;
    }

    /* similar for trailing entrance hits */
    while (snhits > 0 && hits[snhits-1].hit_vpriv[X] < 0.0) {
	snhits--;
    }

    if ((snhits&1)) {
	/*
	 * If this condition exists, it is almost certainly due to the
	 * dn==0 check above.  Thus, we will make the last surface
	 * rather thin.  This at least makes the presence of this
	 * solid known.  There may be something better we can do.
	 */

	if (snhits > 2) {
	    fastf_t dot1, dot2;
	    ssize_t j;

	    /* likely an extra hit, look for consecutive entrances or
	     * exits.
	     */

	    dot2 = 1.0;
	    i = 0;
	    while (i<snhits) {
		dot1 = dot2;
		dot2 = hits[i].hit_vpriv[X];
		if (dot1 > 0.0 && dot2 > 0.0) {
		    /* two consectutive exits, manufacture an entrance
		     * at same distance as second exit.
		     */
		    /* XXX This consumes an extra hit structure in the array */
		    if (psp) {
			/* using pieces */
			(void)rt_htbl_get(&psp->htab);	/* make sure space exists in the hit array */
			hits = psp->htab.hits;
		    } else if (snhits + 1 >= MAXHITS) {
			/* not using pieces */
			bu_log("rt_bot_makesegs: too many hits on %s\n", stp->st_dp->d_namep);
			i++;
			continue;
		    }
		    for (j=snhits; j>i; j--)
			hits[j] = hits[j-1];	/* struct copy */

		    hits[i].hit_vpriv[X] = -hits[i].hit_vpriv[X];
		    dot2 = hits[i].hit_vpriv[X];
		    snhits++;
		    bu_log("\t\tadding fictitious entry at %f (%s)\n", hits[i].hit_dist, stp->st_name);
		    bu_log("\t\t\tray = (%g %g %g) -> (%g %g %g)\n", V3ARGS(ap->a_ray.r_pt), V3ARGS(ap->a_ray.r_dir));
		} else if (dot1 < 0.0 && dot2 < 0.0) {
		    /* two consectutive entrances, manufacture an exit
		     * between them.
		     */
		    /* XXX This consumes an extra hit structure in the
		     * array.
		     */

		    if (psp) {
			/* using pieces */
			(void)rt_htbl_get(&psp->htab);	/* make sure space exists in the hit array */
			hits = psp->htab.hits;
		    } else if (snhits + 1 >= MAXHITS) {
			/* not using pieces */
			bu_log("rt_bot_makesegs: too many hits on %s\n", stp->st_dp->d_namep);
			i++;
			continue;
		    }
		    for (j=snhits; j>i; j--)
			hits[j] = hits[j-1];	/* struct copy */

		    hits[i] = hits[i-1];	/* struct copy */
		    hits[i].hit_vpriv[X] = -hits[i].hit_vpriv[X];
		    dot2 = hits[i].hit_vpriv[X];
		    snhits++;
		    bu_log("\t\tadding fictitious exit at %f (%s)\n", hits[i].hit_dist, stp->st_name);
		    bu_log("\t\t\tray = (%g %g %g) -> (%g %g %g)\n", V3ARGS(ap->a_ray.r_pt), V3ARGS(ap->a_ray.r_dir));
		}
		i++;
	    }
	}
    }

    if ((snhits&1)) {
	/* XXX This consumes an extra hit structure in the array */
	if (psp) {
	    (void)rt_htbl_get(&psp->htab);	/* make sure space exists in the hit array */
	    hits = psp->htab.hits;
	}
	if (!psp && (snhits + 1 >= MAXHITS)) {
	    bu_log("rt_bot_makesegs: too many hits on %s\n", stp->st_dp->d_namep);
	    snhits--;
	} else {
	    hits[snhits] = hits[snhits-1];	/* struct copy */
	    hits[snhits].hit_vpriv[X] = -hits[snhits].hit_vpriv[X];
	    snhits++;
	}
    }

    /* snhits is even, build segments */
    for (i=0; i < snhits; i += 2) {
	XGLUE(tri_specific_, TRI_TYPE) *trip;

	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in = hits[i];	/* struct copy */
	trip = (XGLUE(tri_specific_, TRI_TYPE) *)hits[i].hit_private;
	BOT_UNORIENTED_NORM(ap, &segp->seg_in, IN);
	segp->seg_out = hits[i+1];	/* struct copy */
	trip = (XGLUE(tri_specific_, TRI_TYPE) *)hits[i+1].hit_private;
	BOT_UNORIENTED_NORM(ap, &segp->seg_out, OUT);
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }

    return snhits;			/* HIT */
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
XGLUE(rt_bot_shot_, TRI_TYPE)(struct soltab *stp, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct bot_specific *bot = (struct bot_specific *)stp->st_specific;
    register XGLUE(tri_specific_, TRI_TYPE) *trip = bot->bot_facelist;
    struct hit hits[MAXHITS];
    register struct hit *hp;
    size_t nhits;
    fastf_t toldist, dn_plus_tol;

    nhits = 0;
    hp = &hits[0];
    if (bot->bot_orientation != RT_BOT_UNORIENTED && bot->bot_mode == RT_BOT_SOLID) {
	toldist = stp->st_aradius / 10.0e+6;
    } else {
	toldist = 0.0;
    }

    /* consider each face */
    for (; trip; trip = trip->tri_forw) {
	fastf_t dn;		/* Direction dot Normal */
	fastf_t abs_dn;
	fastf_t k;
	fastf_t alpha, beta;
	vect_t wxb;		/* vertex - ray_start */
	vect_t xp;		/* wxb cross ray_dir */

	/*
	 * Ray Direction dot N.  (N is outward-pointing normal) wn
	 * points inwards, and is not unit length.
	 */
	dn = VDOT(trip->tri_wn, rp->r_dir);

	/*
	 * If ray lies directly along the face, (ie, dot product is
	 * zero), drop this face.
	 */
	abs_dn = dn >= 0.0 ? dn : (-dn);
	if (abs_dn < BOT_MIN_DN) {
	    continue;
	}
	VSUB2(wxb, trip->tri_A, rp->r_pt);
	VCROSS(xp, wxb, rp->r_dir);

	dn_plus_tol = toldist + abs_dn;

	/* Check for exceeding along the one side */
	alpha = VDOT(trip->tri_CA, xp);
	if (dn < 0.0) alpha = -alpha;
	if (alpha < -toldist || alpha > dn_plus_tol) {
	    continue;
	}

	/* Check for exceeding along the other side */
	beta = VDOT(trip->tri_BA, xp);
	if (dn > 0.0) beta = -beta;
	if (beta < -toldist || beta > dn_plus_tol) {
	    continue;
	}
	if (alpha+beta > dn_plus_tol) {
	    continue;
	}
	k = VDOT(wxb, trip->tri_wn) / dn;
	/* HIT is within planar face */
	hp->hit_magic = RT_HIT_MAGIC;
	hp->hit_dist = k;
	hp->hit_private = (genptr_t)trip;
	hp->hit_vpriv[X] = VDOT(trip->tri_N, rp->r_dir);
	hp->hit_vpriv[Y] = alpha / abs_dn;
	hp->hit_vpriv[Z] = beta / abs_dn;
	hp->hit_surfno = trip->tri_surfno;
	hp->hit_rayp = &ap->a_ray;
	if (++nhits >= MAXHITS) {
	    bu_log("rt_bot_shot(%s): too many hits (%zu)\n", stp->st_name, nhits);
	    break;
	}
	hp++;
    }
    if (nhits == 0)
	return 0;		/* MISS */

    /* Sort hits, Near to Far */
    rt_hitsort(hits, nhits);

    /* build segments */
    return rt_bot_makesegs(hits, nhits, stp, rp, ap, seghead, NULL);
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
XGLUE(rt_bot_piece_shot_, TRI_TYPE)(struct rt_piecestate *psp, struct rt_piecelist *plp,
				    double dist_corr, struct xray *rp, struct application *ap, struct seg *seghead)
{
    struct resource *resp;
    long *sol_piece_subscr_p;
    struct soltab *stp;
    long piecenum;
    register struct hit *hp;
    struct bot_specific *bot;
    const int debug_shoot = RT_G_DEBUG & DEBUG_SHOOT;
    int starting_hits;
    fastf_t toldist, dn_plus_tol;
    size_t trinum;

    RT_CK_PIECELIST(plp);
    stp = plp->stp;

    RT_CK_APPLICATION(ap);
    resp = ap->a_resource;
    RT_CK_RESOURCE(resp);

    RT_CK_SOLTAB(stp);
    bot = (struct bot_specific *)stp->st_specific;

    RT_CK_PIECESTATE(psp);
    starting_hits = psp->htab.end;

    if (bot->bot_orientation != RT_BOT_UNORIENTED &&
	bot->bot_mode == RT_BOT_SOLID) {

	toldist = psp->stp->st_aradius / 10.0e+6;
    } else {
	toldist = 0.0;
    }

    if (debug_shoot) {
	bu_log("In rt_bot_piece_shot(), looking at %zu pieces\n", plp->npieces);
    }
    sol_piece_subscr_p = &(plp->pieces[plp->npieces-1]);
    for (; sol_piece_subscr_p >= plp->pieces; sol_piece_subscr_p--) {
	fastf_t dn;		/* Direction dot Normal */
	fastf_t abs_dn;
	fastf_t k;
	fastf_t alpha, beta;
	vect_t wxb;		/* vertex - ray_start */
	vect_t xp;		/* wxb cross ray_dir */
	size_t face_array_index;
	size_t tris_in_piece;

	piecenum = *sol_piece_subscr_p;

	if (BU_BITTEST(psp->shot, piecenum)) {
	    if (debug_shoot)
		bu_log("%s piece %d already shot\n",
		       stp->st_name, piecenum);

	    resp->re_piece_ndup++;
	    continue;	/* this piece already shot */
	}

	/* Shoot a ray */
	BU_BITSET(psp->shot, piecenum);
	if (debug_shoot)
	    bu_log("%s piece %d ...\n", stp->st_name, piecenum);

	/* Now intersect with each piece, which means intesecting with
	 * each triangle that makes up the piece.
	 */
	face_array_index = piecenum*bot->bot_tri_per_piece;
	tris_in_piece = bot->bot_ntri - face_array_index;
	if (tris_in_piece > bot->bot_tri_per_piece) {
	    tris_in_piece = bot->bot_tri_per_piece;
	}
	for (trinum=0; trinum<tris_in_piece; trinum++) {
	    register XGLUE(tri_specific_, TRI_TYPE) *trip = bot->bot_facearray[face_array_index+trinum];
	    fastf_t dN, abs_dN;
	    /*
	     * Ray Direction dot N.  (N is outward-pointing normal) wn
	     * points inwards, and is not unit length.  Therefore, wn
	     * is not a good choice for this test
	     */
	    dn = VDOT(trip->tri_wn, rp->r_dir);
	    dN = VDOT(trip->tri_N, rp->r_dir);

	    /*
	     * If ray lies directly along the face, (ie, dot product
	     * is zero), drop this face.
	     */
	    abs_dN = dN >= 0.0 ? dN : (-dN);
	    abs_dn = dn >= 0.0 ? dn : (-dn);
	    if (abs_dN < BOT_MIN_DN) {
		continue;
	    }
	    VSUB2(wxb, trip->tri_A, rp->r_pt);
	    VCROSS(xp, wxb, rp->r_dir);

	    dn_plus_tol = toldist + abs_dn;

	    /* Check for exceeding along the one side */
	    alpha = VDOT(trip->tri_CA, xp);
	    if (dn < 0.0) alpha = -alpha;
	    if (alpha < -toldist || alpha > dn_plus_tol) {
		continue;
	    }

	    /* Check for exceeding along the other side */
	    beta = VDOT(trip->tri_BA, xp);
	    if (dn > 0.0) beta = -beta;
	    if (beta < -toldist || beta > dn_plus_tol) {
		continue;
	    }
	    if (alpha+beta > dn_plus_tol) {
		continue;
	    }
	    k = VDOT(wxb, trip->tri_wn) / dn;

	    /* HIT is within planar face */
	    hp = rt_htbl_get(&psp->htab);
	    hp->hit_magic = RT_HIT_MAGIC;
	    hp->hit_dist = k + dist_corr;
	    hp->hit_private = (genptr_t)trip;
	    hp->hit_vpriv[X] = VDOT(trip->tri_N, rp->r_dir);
	    hp->hit_vpriv[Y] = alpha / abs_dn;
	    hp->hit_vpriv[Z] = beta / abs_dn;
	    hp->hit_surfno = trip->tri_surfno;
	    hp->hit_rayp = &ap->a_ray;
	    if (debug_shoot)
		bu_log("%s piece %d surfno %d ... HIT %g\n",
		       stp->st_name, piecenum, trip->tri_surfno, hp->hit_dist);
	} /* for (trinum...) */
    } /* for (;sol_piece_subscr_p...) */

    if (psp->htab.end > 0 &&
	(bot->bot_mode == RT_BOT_PLATE ||
	 bot->bot_mode == RT_BOT_PLATE_NOCOS)) {
	/*
	 * Each of these hits is really two, resulting in an instant
	 * seg.  Saving an odd number of these will confuse a_onehit
	 * processing.
	 */
	rt_hitsort(psp->htab.hits, psp->htab.end);
	return rt_bot_makesegs(psp->htab.hits, psp->htab.end,
			       stp, rp, ap, seghead, psp);
    }
    return psp->htab.end - starting_hits;
}


/**
 * R T _ B O T _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
XGLUE(rt_bot_norm_, TRI_TYPE)(struct bot_specific *bot, struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    vect_t old_norm;
    XGLUE(tri_specific_, TRI_TYPE) *trip=(XGLUE(tri_specific_, TRI_TYPE) *)hitp->hit_private;

    if (stp) RT_CK_SOLTAB(stp);

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    VMOVE(old_norm, hitp->hit_normal);

    if ((bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) && (bot->bot_flags & RT_BOT_USE_NORMALS) && trip->tri_normals) {
	fastf_t old_ray_dot_norm, new_ray_dot_norm;
	fastf_t u, v, w; /*barycentric coords of hit point */
	size_t i;

	old_ray_dot_norm = VDOT(hitp->hit_normal, rp->r_dir);

	v = hitp->hit_vpriv[Y];
	if (v < 0.0) v = 0.0;
	if (v > 1.0) v = 1.0;

	w = hitp->hit_vpriv[Z];
	if (w < 0.0) w = 0.0;
	if (w > 1.0) w =  1.0;

	u = 1.0 - v - w;
	if (u < 0.0) u = 0.0;
	VSETALL(hitp->hit_normal, 0.0);

	for (i=X ; i<=Z; i++) {
	    hitp->hit_normal[i] = u*trip->tri_normals[i]*ONE_OVER_SCALE + v*trip->tri_normals[i+3]*ONE_OVER_SCALE + w*trip->tri_normals[i+6]*ONE_OVER_SCALE;
	}
	VUNITIZE(hitp->hit_normal);

	if (bot->bot_mode == RT_BOT_PLATE || bot->bot_mode == RT_BOT_PLATE_NOCOS) {
	    if (VDOT(old_norm, hitp->hit_normal) < 0.0) {
		VREVERSE(hitp->hit_normal, hitp->hit_normal);
	    }
	}

	new_ray_dot_norm = VDOT(hitp->hit_normal, rp->r_dir);

	if ((old_ray_dot_norm < 0.0 && new_ray_dot_norm > 0.0) ||
	    (old_ray_dot_norm > 0.0 && new_ray_dot_norm < 0.0)) {
	    /* surface normal interpolation has produced an
	     * incompatible normal direction clamp the normal to 90
	     * degrees to the ray direction
	     */

	    vect_t tmp;

	    VCROSS(tmp, rp->r_dir, hitp->hit_normal);
	    VCROSS(hitp->hit_normal, tmp, rp->r_dir);
	}

	VUNITIZE(hitp->hit_normal);
    }
}


/**
 * R T _ B O T _ F R E E
 */
void
XGLUE(rt_bot_free_, TRI_TYPE)(struct bot_specific *bot)
{
    register XGLUE(tri_specific_, TRI_TYPE) *tri, *ptr;

    if (bot->bot_facearray) {
	bu_free((char *)bot->bot_facearray, "bot_facearray");
	bot->bot_facearray = NULL;
    }

    if (bot->bot_thickness) {
	bu_free((char *)bot->bot_thickness, "bot_thickness");
	bot->bot_thickness = NULL;
    }
    if (bot->bot_facemode) {
	bu_free((char *)bot->bot_facemode, "bot_facemode");
	bot->bot_facemode = NULL;
    }
    ptr = bot->bot_facelist;
    while (ptr) {
	tri = ptr->tri_forw;
	if (ptr) {
	    if (ptr->tri_normals) {
		bu_free((char *)ptr->tri_normals, "bot tri_specific normals");
	    }
	    bu_free((char *)ptr, "bot tri_specific");
	}
	ptr = tri;
    }
    bot->bot_facelist = NULL;
    bu_free((char *)bot, "bot_specific");
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
