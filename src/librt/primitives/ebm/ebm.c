/*                           E B M . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2025 United States Government as represented by
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
/** @file primitives/ebm/ebm.c
 *
 * Intersect a ray with an Extruded Bitmap, where the bitmap is taken
 * from a bw(5) file.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"

#include "bu/parallel.h"
#include "vmath.h"
#include "rt/db4.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "../fixpt.h"
#include "../../librt_private.h"


struct rt_ebm_specific {
    struct rt_ebm_internal ebm_i;
    vect_t ebm_xnorm;	/* local +X norm in model coords */
    vect_t ebm_ynorm;
    vect_t ebm_znorm;
    vect_t ebm_cellsize;/* ideal coords: size of each cell */
    vect_t ebm_origin;	/* local coords of grid origin (0, 0, 0) for now */
    vect_t ebm_large;	/* local coords of XYZ max */
    mat_t ebm_mat;	/* model to ideal space */
};


#define RT_EBM_NULL ((struct rt_ebm_specific *)0)

#define RT_EBM_O(m) bu_offsetof(struct rt_ebm_internal, m)

const struct bu_structparse rt_ebm_parse[] = {
    {"%s",	RT_EBM_NAME_LEN, "file", bu_offsetof(struct rt_ebm_internal, name), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%s",	RT_EBM_NAME_LEN, "name", bu_offsetof(struct rt_ebm_internal, name), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%c",	1, "src",	RT_EBM_O(datasrc),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "w",		RT_EBM_O(xdim),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "n",		RT_EBM_O(ydim),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1, "d",		RT_EBM_O(tallness),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	16, "mat", bu_offsetof(struct rt_ebm_internal, mat), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0, 0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


struct ebm_hit_private {
    int x_cell;
    int y_cell;
};


extern int rt_ebm_dda(struct xray *rp, struct soltab *stp,
		      struct application *ap, struct seg *seghead);
extern int rt_seg_planeclip(struct seg *out_hd, struct seg *in_hd,
			    vect_t out_norm, fastf_t in, fastf_t out,
			    struct xray *rp, struct application *ap);

/*
 * Codes to represent surface normals.  In a bitmap, there are only 4
 * possible normals.  With this code, reverse the sign to reverse the
 * direction.  As always, the normal is expected to point outwards.
 */
#define NORM_ZPOS 3
#define NORM_YPOS 2
#define NORM_XPOS 1
#define NORM_XNEG (-1)
#define NORM_YNEG (-2)
#define NORM_ZNEG (-3)

/*
 * Regular bit addressing is used: (0..W-1, 0..N-1), but the bitmap is
 * stored with two cells of zeros all around, so permissible
 * subscripts run (-2..W+1, -2..N+1).  This eliminates special-case
 * code for the boundary conditions.
 */
#define BIT_XWIDEN 2
#define BIT_YWIDEN 2

unsigned char *
bit(struct rt_ebm_internal *eip, size_t x, size_t y)
{
    unsigned char *ret = NULL;
    switch (eip->datasrc) {
	case RT_EBM_SRC_FILE:
	    ret = &(((unsigned char *)(eip->mp->apbuf))[(y+BIT_YWIDEN)*(eip->xdim+BIT_XWIDEN*2)+x+BIT_XWIDEN]);
	    break;
	case RT_EBM_SRC_OBJ:
	    ret = &((eip->buf)[(y+BIT_YWIDEN)*(eip->xdim+BIT_XWIDEN*2)+x+BIT_XWIDEN]);
	    break;
	default:
	    bu_log("Unrecognized EBM data source (datasrc='%d')\n", eip->datasrc);
	    bu_bomb("Internal Error, exiting.\n");
    }
    return ret;
}


#ifdef USE_OPENCL
/* largest data members first */
struct clt_ebm_specific {
    cl_double ebm_xnorm[3];	/* local +X norm in model coords */
    cl_double ebm_ynorm[3];
    cl_double ebm_znorm[3];
    cl_double ebm_cellsize[3];	/* ideal coords: size of each cell */
    cl_double ebm_origin[3];	/* local coords of grid origin (0, 0, 0) for now */
    cl_double ebm_large[3];	/* local coords of XYZ max */
    cl_double ebm_mat[16];	/* model to ideal space */

    cl_double tallness;		/**< @brief Z dimension (mm) */
    cl_uint xdim;		/**< @brief X dimension (w cells) */
    cl_uint ydim;		/**< @brief Y dimension (n cells) */
    cl_uchar apbuf[];
};

/* Pad struct to multiple of 8 bytes (sizeof double). */
#define CL_PADDED_SIZE(bytes)	((bytes+4) & ~7)

size_t
clt_ebm_pack(struct bu_pool *pool, struct soltab *stp)
{
    struct rt_ebm_specific *ebm =
	(struct rt_ebm_specific *)stp->st_specific;
    struct clt_ebm_specific *args;

    struct rt_ebm_internal *eip = &ebm->ebm_i;
    unsigned int x, y, id;

    const size_t npixels =
	(eip->xdim+BIT_XWIDEN*2)*(eip->ydim+BIT_YWIDEN*2);
    const size_t nbytes = CL_PADDED_SIZE(npixels/8+1);
    size_t size = sizeof(*args);

    size += nbytes;
    args = (struct clt_ebm_specific*)bu_pool_alloc(pool, 1, size);

    VMOVE(args->ebm_xnorm, ebm->ebm_xnorm);
    VMOVE(args->ebm_ynorm, ebm->ebm_ynorm);
    VMOVE(args->ebm_znorm, ebm->ebm_znorm);
    VMOVE(args->ebm_cellsize, ebm->ebm_cellsize);
    VMOVE(args->ebm_origin, ebm->ebm_origin);
    VMOVE(args->ebm_large, ebm->ebm_large);
    MAT_COPY(args->ebm_mat, ebm->ebm_mat);

    args->tallness = eip->tallness;
    args->xdim = eip->xdim;
    args->ydim = eip->ydim;
    memset(args->apbuf, 0, nbytes);

    for (y = 0; y < eip->ydim; y++) {
	for (x = 0; x < eip->xdim; x++) {
	    if (*bit(eip, x, y) != 0) {
		id = (y+BIT_YWIDEN)*(eip->xdim + BIT_XWIDEN*2)+x+BIT_XWIDEN;
		args->apbuf[(id >> 3)] |= (1 << (id & 7));
	    }
	}
    }
    return size;
}

#endif /* USE_OPENCL */


/**
 * Computes centroid of an extruded bitmap
 */
void
rt_ebm_centroid(point_t *cent, const struct rt_db_internal *ip)
{
    struct rt_ebm_internal *eip;
    register unsigned int x, y;
    unsigned long int xsum, ysum, totalcells;
    fastf_t avgx, avgy, avgz;
    point_t bmcentroid;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_ebm_internal *)ip->idb_ptr;
    RT_EBM_CK_MAGIC(eip);

    bu_log("WARNING: EBM centroids have not been verified\n");

    xsum = 0;
    ysum = 0;
    totalcells = 0;
    for (y = 0; y < eip->ydim; y++) {
	for (x = 0; x < eip->xdim; x++) {
	    if (*bit(eip, x, y) == 1) {
		xsum += x;
		ysum += y;
		totalcells++;
	    }
	}
    }
    avgx = (fastf_t)xsum / totalcells + 0.5;
    avgy = (fastf_t)ysum / totalcells + 0.5;
    avgz = eip->tallness / 2;
    VSET(bmcentroid, avgx, avgy, avgz);
    MAT4X3VEC(*cent, eip->mat, bmcentroid);
}


/**
 * Take a segment chain, in sorted order (ascending hit_dist), and
 * clip to the range (in, out) along the normal "out_norm".  For the
 * particular ray "rp", find the parametric distances:
 *
 * kmin is the minimum permissible parameter, "in" units away
 * kmax is the maximum permissible parameter, "out" units away
 *
 * Returns -
 * 1 OK: trimmed segment chain, still in sorted order
 * 0 ERROR
 */
int
rt_seg_planeclip(struct seg *out_hd, struct seg *in_hd, vect_t out_norm, fastf_t in, fastf_t out, struct xray *rp, struct application *ap)
{
    fastf_t norm_dist_min, norm_dist_max;
    fastf_t slant_factor;
    fastf_t kmin, kmax;
    vect_t in_norm;
    register struct seg *curr;
    int out_norm_code;
    int count;

    norm_dist_min = in - VDOT(rp->r_pt, out_norm);
    slant_factor = VDOT(rp->r_dir, out_norm);	/* always abs < 1 */
    if (ZERO(slant_factor)) {
	if (norm_dist_min < 0.0) {
	    bu_log("rt_seg_planeclip ERROR -- ray parallel to baseplane, outside \n");
	    /* XXX Free segp chain */
	    return 0;
	}
	kmin = -INFINITY;
    } else
	kmin =  norm_dist_min / slant_factor;

    VREVERSE(in_norm, out_norm);
    norm_dist_max = out - VDOT(rp->r_pt, in_norm);
    slant_factor = VDOT(rp->r_dir, in_norm);	/* always abs < 1 */
    if (ZERO(slant_factor)) {
	if (norm_dist_max < 0.0) {
	    bu_log("rt_seg_planeclip ERROR -- ray parallel to baseplane, outside \n");
	    /* XXX Free segp chain */
	    return 0;
	}
	kmax = INFINITY;
    } else
	kmax =  norm_dist_max / slant_factor;

    if (kmin > kmax) {
	/* If r_dir[Z] < 0, will need to swap min & max */
	slant_factor = kmax;
	kmax = kmin;
	kmin = slant_factor;
	out_norm_code = NORM_ZPOS;
    } else {
	out_norm_code = NORM_ZNEG;
    }
    if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("kmin=%g, kmax=%g, out_norm_code=%d\n", kmin, kmax, out_norm_code);

    count = 0;
    while (BU_LIST_WHILE(curr, seg, &(in_hd->l))) {
	BU_LIST_DEQUEUE(&(curr->l));
	if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log(" rt_seg_planeclip seg(%g, %g)\n", curr->seg_in.hit_dist, curr->seg_out.hit_dist);
	if (curr->seg_out.hit_dist <= kmin) {
	    if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("seg_out %g <= kmin %g, freeing\n", curr->seg_out.hit_dist, kmin);
	    RT_FREE_SEG(curr, ap->a_resource);
	    continue;
	}
	if (curr->seg_in.hit_dist >= kmax) {
	    if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("seg_in  %g >= kmax %g, freeing\n", curr->seg_in.hit_dist, kmax);
	    RT_FREE_SEG(curr, ap->a_resource);
	    continue;
	}
	if (curr->seg_in.hit_dist <= kmin) {
	    if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("seg_in = kmin %g\n", kmin);
	    curr->seg_in.hit_dist = kmin;
	    curr->seg_in.hit_surfno = out_norm_code;
	}
	if (curr->seg_out.hit_dist >= kmax) {
	    if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("seg_out= kmax %g\n", kmax);
	    curr->seg_out.hit_dist = kmax;
	    curr->seg_out.hit_surfno = (-out_norm_code);
	}
	BU_LIST_INSERT(&(out_hd->l), &(curr->l));
	count += 2;
    }
    return count;
}


static int rt_ebm_normtab[3] = { NORM_XPOS, NORM_YPOS, NORM_ZPOS };


/**
 * Step through the 2-D array, in local coordinates ("ideal space").
 */
int
rt_ebm_dda(register struct xray *rp, struct soltab *stp, struct application *ap, struct seg *seghead)
{
    register struct rt_ebm_specific *ebmp =
	(struct rt_ebm_specific *)stp->st_specific;
    vect_t invdir;
    double t0;		/* in point of cell */
    double t1;		/* out point of cell */
    double tmax;	/* out point of entire grid */
    vect_t t;		/* next t value for XYZ cell plane intersect */
    vect_t delta;	/* spacing of XYZ cell planes along ray */
    size_t igrid[3];	/* Grid cell coordinates of cell (integerized) */
    vect_t P;		/* hit point */
    int inside;		/* inside/outside a solid flag */
    int in_index;
    int out_index;
    int j;

    /* Compute inverse of the direction cosines */
    VINVDIR(invdir, rp->r_dir);

    /* intersect ray with ideal grid rpp */
    VSETALL(P, 0);
    if (! rt_in_rpp(rp, invdir, P, ebmp->ebm_large))
	return 0;	/* MISS */

    VJOIN1(P, rp->r_pt, rp->r_min, rp->r_dir);
    /* P is hit point (on RPP?) */

    if (RT_G_DEBUG&RT_DEBUG_EBM)VPRINT("ebm_origin", ebmp->ebm_origin);
    if (RT_G_DEBUG&RT_DEBUG_EBM)VPRINT("r_pt", rp->r_pt);
    if (RT_G_DEBUG&RT_DEBUG_EBM)VPRINT("P", P);
    if (RT_G_DEBUG&RT_DEBUG_EBM)VPRINT("cellsize", ebmp->ebm_cellsize);
    tmax = rp->r_max;
    if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("[shoot: r_min=%g, r_max=%g]\n", rp->r_min, rp->r_max);

    /* find grid cell where ray first hits ideal space bounding RPP */
    igrid[X] = (P[X] - ebmp->ebm_origin[X]) / ebmp->ebm_cellsize[X];
    igrid[Y] = (P[Y] - ebmp->ebm_origin[Y]) / ebmp->ebm_cellsize[Y];
    if (igrid[X] >= ebmp->ebm_i.xdim) {
	igrid[X] = ebmp->ebm_i.xdim-1;
    }
    if (igrid[Y] >= ebmp->ebm_i.ydim) {
	igrid[Y] = ebmp->ebm_i.ydim-1;
    }
    if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("g[X] = %zu, g[Y] = %zu\n", igrid[X], igrid[Y]);

    if (ZERO(rp->r_dir[X]) && ZERO(rp->r_dir[Y])) {
	register struct seg *segp;

	/* Ray is traveling exactly along Z axis.  Just check the one
	 * cell hit.  Depend on higher level to clip ray to Z extent.
	 */
	if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("ray on local Z axis\n");
	if (*bit(&ebmp->ebm_i, igrid[X], igrid[Y]) == 0)
	    return 0;	/* MISS */
	RT_GET_SEG(segp, ap->a_resource);
	segp->seg_stp = stp;
	segp->seg_in.hit_dist = 0;
	segp->seg_out.hit_dist = INFINITY;

	segp->seg_in.hit_vpriv[X] =
	    (double) igrid[X] / ebmp->ebm_i.xdim;
	segp->seg_in.hit_vpriv[Y] =
	    (double) igrid[Y] / ebmp->ebm_i.ydim;

	segp->seg_out.hit_vpriv[X] =
	    (double) igrid[X] / ebmp->ebm_i.xdim;
	segp->seg_out.hit_vpriv[Y] =
	    (double) igrid[Y] / ebmp->ebm_i.ydim;

	if (rp->r_dir[Z] < 0) {
	    segp->seg_in.hit_surfno = NORM_ZPOS;
	    segp->seg_out.hit_surfno = NORM_ZNEG;
	} else {
	    segp->seg_in.hit_surfno = NORM_ZNEG;
	    segp->seg_out.hit_surfno = NORM_ZPOS;
	}
	BU_LIST_INSERT(&(seghead->l), &(segp->l));
	return 2;			/* HIT */
    }

    /* X setup */
    if (ZERO(rp->r_dir[X])) {
	t[X] = INFINITY;
	delta[X] = 0;
    } else {
	j = igrid[X];
	if (rp->r_dir[X] < 0) j++;
	t[X] = (ebmp->ebm_origin[X] + j*ebmp->ebm_cellsize[X] -
		rp->r_pt[X]) * invdir[X];
	delta[X] = ebmp->ebm_cellsize[X] * fabs(invdir[X]);
    }
    /* Y setup */
    if (ZERO(rp->r_dir[Y])) {
	t[Y] = INFINITY;
	delta[Y] = 0;
    } else {
	j = igrid[Y];
	if (rp->r_dir[Y] < 0) j++;
	t[Y] = (ebmp->ebm_origin[Y] + j*ebmp->ebm_cellsize[Y] -
		rp->r_pt[Y]) * invdir[Y];
	delta[Y] = ebmp->ebm_cellsize[Y] * fabs(invdir[Y]);
    }

    /* The delta[] elements *must* be positive, as t must increase */
    if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("t[X] = %g, delta[X] = %g\n", t[X], delta[X]);
    if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("t[Y] = %g, delta[Y] = %g\n", t[Y], delta[Y]);

    /* Find face of entry into first cell -- max initial t value */
    if (ZERO(t[X] - INFINITY)) {
	in_index = Y;
	t0 = t[Y];
    } else if (ZERO(t[Y] - INFINITY)) {
	in_index = X;
	t0 = t[X];
    } else if (t[X] >= t[Y]) {
	in_index = X;
	t0 = t[X];
    } else {
	in_index = Y;
	t0 = t[Y];
    }
    if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("Entry index is %s, t0=%g\n", in_index==X ? "X" : "Y", t0);

    /* Advance to next exits */
    t[X] += delta[X];
    t[Y] += delta[Y];

    /* Ensure that next exit is after first entrance */
    if (t[X] < t0) {
	bu_log("*** advancing t[X]\n");
	t[X] += delta[X];
    }
    if (t[Y] < t0) {
	bu_log("*** advancing t[Y]\n");
	t[Y] += delta[Y];
    }
    if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("Exit t[X]=%g, t[Y]=%g\n", t[X], t[Y]);

    inside = 0;

    while (t0 < tmax) {
	int val;
	struct seg *segp;

	/* find minimum exit t value */
	out_index = t[X] < t[Y] ? X : Y;

	t1 = t[out_index];

	/* Ray passes through cell igrid[XY] from t0 to t1 */
	val = *bit(&ebmp->ebm_i, igrid[X], igrid[Y]);
	if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("igrid [%zu %zu] from %g to %g, val=%d\n",
					igrid[X], igrid[Y],
					t0, t1, val);
	if (RT_G_DEBUG&RT_DEBUG_EBM)bu_log("Exit index is %s, t[X]=%g, t[Y]=%g\n",
					out_index==X ? "X" : "Y", t[X], t[Y]);


	if (t1 <= t0) bu_log("ERROR ebm t1=%g < t0=%g\n", t1, t0);
	if (!inside) {
	    if (val > 0) {
		/* Handle the transition from vacuum to solid */
		/* Start of segment (entering a full voxel) */
		inside = 1;

		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in.hit_dist = t0;

		segp->seg_in.hit_vpriv[X] =
		    (double) igrid[X] / ebmp->ebm_i.xdim;
		segp->seg_in.hit_vpriv[Y] =
		    (double) igrid[Y] / ebmp->ebm_i.ydim;

		/* Compute entry normal */
		if (rp->r_dir[in_index] < 0) {
		    /* Go left, entry norm goes right */
		    segp->seg_in.hit_surfno =
			rt_ebm_normtab[in_index];
		} else {
		    /* go right, entry norm goes left */
		    segp->seg_in.hit_surfno =
			(-rt_ebm_normtab[in_index]);
		}
		BU_LIST_INSERT(&(seghead->l), &(segp->l));

		if (RT_G_DEBUG&RT_DEBUG_EBM) bu_log("START t=%g, surfno=%d\n",
						 t0, segp->seg_in.hit_surfno);
	    } else {
		/* Do nothing, marching through void */
	    }
	} else {
	    register struct seg *tail;
	    if (val > 0) {
		/* Do nothing, marching through solid */
	    } else {
		/* End of segment (now in an empty voxel) */
		/* Handle transition from solid to vacuum */
		inside = 0;

		tail = BU_LIST_LAST(seg, &(seghead->l));
		tail->seg_out.hit_dist = t0;

		tail->seg_out.hit_vpriv[X] =
		    (double) igrid[X] / ebmp->ebm_i.xdim;
		tail->seg_out.hit_vpriv[Y] =
		    (double) igrid[Y] / ebmp->ebm_i.ydim;

		/* Compute exit normal */
		if (rp->r_dir[in_index] < 0) {
		    /* Go left, exit normal goes left */
		    tail->seg_out.hit_surfno =
			(-rt_ebm_normtab[in_index]);
		} else {
		    /* go right, exit norm goes right */
		    tail->seg_out.hit_surfno =
			rt_ebm_normtab[in_index];
		}
		if (RT_G_DEBUG&RT_DEBUG_EBM) bu_log("END t=%g, surfno=%d\n",
						 t0, tail->seg_out.hit_surfno);
	    }
	}

	/* Take next step */
	t0 = t1;
	in_index = out_index;
	t[out_index] += delta[out_index];
	if (rp->r_dir[out_index] > 0) {
	    igrid[out_index]++;
	} else {
	    igrid[out_index]--;
	}
    }

    if (inside) {
	register struct seg *tail;

	/* Close off the final segment */
	tail = BU_LIST_LAST(seg, &(seghead->l));
	tail->seg_out.hit_dist = tmax;
	VSETALL(tail->seg_out.hit_vpriv, 0.0);

	/* Compute exit normal.  Previous out_index is now in_index */
	if (rp->r_dir[in_index] < 0) {
	    /* Go left, exit normal goes left */
	    tail->seg_out.hit_surfno = (-rt_ebm_normtab[in_index]);
	} else {
	    /* go right, exit norm goes right */
	    tail->seg_out.hit_surfno = rt_ebm_normtab[in_index];
	}
	if (RT_G_DEBUG&RT_DEBUG_EBM) bu_log("closed END t=%g, surfno=%d\n",
					 tmax, tail->seg_out.hit_surfno);
    }

    if (BU_LIST_IS_EMPTY(&(seghead->l)))
	return 0;
    return 2;
}


/**
 * Retrieve data for EBM from external file.
 * Returns:
 * 0 Success
 * !0 Failure
 */
static int
get_file_data(struct rt_ebm_internal *eip, const struct db_i *dbip)
{
    struct bu_mapped_file *mp;
    int nbytes;

    /* get file */
    mp = bu_open_mapped_file_with_path(dbip->dbi_filepath, eip->name, "ebm");
    if (!mp) {
	bu_log("mapped file open failure: %s%c%s\n",
	       *dbip->dbi_filepath, BU_DIR_SEPARATOR, eip->name);
	return -1;
    }
    eip->mp = mp;
    if (mp->buflen < (size_t)(eip->xdim*eip->ydim)) {
	bu_log("EBM buffer wrong size: %lu s/b %u ",
	       (long unsigned int) mp->buflen, eip->xdim*eip->ydim);
	return -1;
    }

    nbytes = (eip->xdim+BIT_XWIDEN*2)*(eip->ydim+BIT_YWIDEN*2);

    /* If first use of this file, prepare in-memory buffer */
    if (!mp->apbuf) {
	size_t y;
	unsigned char* cp;

	/* Prevent a multi-processor race */
	bu_semaphore_acquire(RT_SEM_MODEL);
	if (mp->apbuf) {
	    /* someone else beat us, nothing more to do */
	    bu_semaphore_release(RT_SEM_MODEL);
	    return 0;
	}
	mp->apbuf = (uint8_t *)bu_calloc(1, nbytes, "rt_ebm_import4 bitmap");
	mp->apbuflen = nbytes;
	bu_semaphore_release(RT_SEM_MODEL);

	/* Because of in-memory padding, read each scanline separately */
	cp = (unsigned char*)mp->buf;
	for (y = 0; y < eip->ydim; y++) {
	    /* bit() addresses into mp->apbuf */
	    memcpy(bit(eip, 0, y), cp, eip->xdim);
	    cp += eip->xdim;
	}
    }
    return 0;
}


extern int rt_retrieve_binunif(struct rt_db_internal *intern, const struct db_i *dbip, const char *name);
extern int rt_binunif_describe(struct bu_vls  *str, const struct rt_db_internal *ip, int verbose, double mm2local);


/**
 * Retrieve data for EBM from a database object.
 * Returns:
 * 0 Success
 * !0 Failure
 */
static int
get_obj_data(struct rt_ebm_internal *eip, const struct db_i *dbip)
{
    struct rt_binunif_internal *bip;
    int ret;
    int nbytes;

    BU_ALLOC(eip->bip, struct rt_db_internal);

    ret = rt_retrieve_binunif(eip->bip, dbip, eip->name);
    if (ret)
	return -1;

    if (RT_G_DEBUG & RT_DEBUG_HF) {
	bu_log("db_internal magic: 0x%08x  major: %d  minor: %d\n",
		eip->bip->idb_magic,
		eip->bip->idb_major_type,
		eip->bip->idb_minor_type);
    }

    bip = (struct rt_binunif_internal *)eip->bip->idb_ptr;

    if (RT_G_DEBUG & RT_DEBUG_HF)
	bu_log("binunif magic: 0x%08x  type: %d count:%zu data[0]:%u\n",
		bip->magic, bip->type, bip->count, bip->u.uint8[0]);

    if (bip->type != DB5_MINORTYPE_BINU_8BITINT_U
	|| (size_t)bip->count != (size_t)(eip->xdim*eip->ydim))
    {
	size_t i = 0;
	size_t size;
	struct bu_vls binudesc = BU_VLS_INIT_ZERO;
	rt_binunif_describe(&binudesc, eip->bip, 0, dbip->dbi_base2local);

	/* skip the first title line */
	size = bu_vls_strlen(&binudesc);
	while (size > 0 && i < size && bu_vls_cstr(&binudesc)[0] != '\n') {
	    bu_vls_nibble(&binudesc, 1);
	}
	if (bu_vls_cstr(&binudesc)[0] == '\n')
	    bu_vls_nibble(&binudesc, 1);

	bu_log("ERROR: Binary object '%s' has invalid data (expected type %d, found %d).\n"
	       "       Expecting %zu 8-bit unsigned char (nuc) integer data values.\n"
	       "       Encountered %s\n",
	       eip->name,
	       DB5_MINORTYPE_BINU_8BITINT_U,
	       bip->type,
	       (size_t)(eip->xdim*eip->ydim),
	       bu_vls_cstr(&binudesc));
	return -2;
    }

    nbytes = (eip->xdim+BIT_XWIDEN*2)*(eip->ydim+BIT_YWIDEN*2);

    if (!eip->buf) {
	size_t y;
	unsigned char* cp;

	/* Prevent a multi-processor race */
	bu_semaphore_acquire(RT_SEM_MODEL);
	if (eip->buf) {
	    /* someone else beat us, nothing more to do */
	    bu_semaphore_release(RT_SEM_MODEL);
	    return 0;
	}
	eip->buf = (unsigned char *)bu_calloc(1, nbytes, "rt_ebm_import4 bitmap");
	bu_semaphore_release(RT_SEM_MODEL);

	/* Because of in-memory padding, read each scanline separately */
	cp = (unsigned char *)bip->u.uint8;
	for (y = 0; y < eip->ydim; y++) {
	    /* bit() addresses into buf */
	    memcpy(bit(eip, 0, y), cp, eip->xdim);
	    cp += eip->xdim;
	}
    }
    return 0;
}


/**
 * Apply the modelling transform and fetch actual data.
 *
 * Return:
 * 0 success
 * !0 failure
 */
static int
ebm_get_data(struct rt_ebm_internal *eip, const struct db_i *dbip)
{
    char *p;

    p = eip->name;

    switch (eip->datasrc) {
	case RT_EBM_SRC_FILE:
	    /* Retrieve the data from an external file */
	    if (RT_G_DEBUG & RT_DEBUG_HF)
		bu_log("getting data from file \"%s\"\n", p);

	    if (get_file_data(eip, dbip) != 0) {
		p = "file";
	    } else {
		return 0;
	    }
	    break;
	case RT_EBM_SRC_OBJ:
	    /* Retrieve the data from an internal db object */
	    if (RT_G_DEBUG & RT_DEBUG_HF)
		bu_log("getting data from object \"%s\"\n", p);

	    if (get_obj_data(eip, dbip) != 0) {
		p = "object";
	    } else {
		RT_CK_DB_INTERNAL(eip->bip);
		RT_CK_BINUNIF(eip->bip->idb_ptr);
		return 0;
	    }
	    break;
	default:
	    bu_log("%s:%d Odd ebm data src '%c' s/b '%c' or '%c'\n",
		    __FILE__, __LINE__, eip->datasrc,
		    RT_EBM_SRC_FILE, RT_EBM_SRC_OBJ);
	    return -1;
    }

    bu_log("Cannot retrieve EBM data from %s \"%s\"\n", p,
	   eip->name);

    eip->mp = (struct bu_mapped_file *)NULL;
    eip->buf = NULL;

    return 1;
}


/**
 * Read in the information from the string solid record.  Then, as a
 * service to the application, read in the bitmap and set up some of
 * the associated internal variables.
 */
int
rt_ebm_import4(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    union record *rp;
    register struct rt_ebm_internal *eip;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    int nbytes;
    mat_t tmat;
    struct bu_mapped_file *mp;

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    if (rp->u_id != DBID_STRSOL) {
	bu_log("rt_ebm_import4: defective strsol record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_EBM;
    ip->idb_meth = &OBJ[ID_EBM];
    BU_ALLOC(ip->idb_ptr, struct rt_ebm_internal);

    eip = (struct rt_ebm_internal *)ip->idb_ptr;
    eip->magic = RT_EBM_INTERNAL_MAGIC;
    MAT_IDN(eip->mat);

    /* default source is file for compatibility with old EBM */
    eip->datasrc = RT_EBM_SRC_FILE;

    bu_vls_strcpy(&str, rp->ss.ss_args);
    if (bu_struct_parse(&str, rt_ebm_parse, (char *)eip, NULL) < 0) {
	bu_vls_free(&str);
	bu_free((char *)eip, "rt_ebm_import4: eip");
	ip->idb_type = ID_NULL;
	ip->idb_ptr = (void *)NULL;
	return -2;
    }
    bu_vls_free(&str);

    /* Check for reasonable values */
    if (eip->name[0] == '\0' || eip->tallness <= 0.0 ||
	eip->xdim < 1 || eip->ydim < 1 || eip->mat[15] <= 0.0) {
	bu_struct_print("Unreasonable EBM parameters", rt_ebm_parse, (char *)eip);
	bu_free((char *)eip, "rt_ebm_import4: eip");
	ip->idb_type = ID_NULL;
	ip->idb_ptr = (void *)NULL;
	return -1;
    }

    /* Apply any modeling transforms to get final matrix */
    if (mat == NULL) mat = bn_mat_identity;
    bn_mat_mul(tmat, mat, eip->mat);
    MAT_COPY(eip->mat, tmat);

    /* Get bit map from .bw(5) file */
    mp = bu_open_mapped_file_with_path(dbip->dbi_filepath, eip->name, "ebm");
    if (!mp) {
	bu_log("rt_ebm_import4() unable to open '%s'\n", eip->name);
	bu_free((char *)eip, "rt_ebm_import4: eip");
    fail:
	ip->idb_type = ID_NULL;
	ip->idb_ptr = (void *)NULL;
	return -1;
    }
    eip->mp = mp;
    if (mp->buflen < (size_t)(eip->xdim*eip->ydim)) {
	bu_log("rt_ebm_import4() file '%s' is too short %zu < %u\n",
	       eip->name, mp->buflen, eip->xdim*eip->ydim);
	goto fail;
    }

    nbytes = (eip->xdim+BIT_XWIDEN*2)*(eip->ydim+BIT_YWIDEN*2);

    /* If first use of this file, prepare in-memory buffer */
    if (!mp->apbuf) {
	size_t y;
	unsigned char *cp;

	/* Prevent a multi-processor race */
	bu_semaphore_acquire(RT_SEM_MODEL);
	if (mp->apbuf) {
	    /* someone else beat us, nothing more to do */
	    bu_semaphore_release(RT_SEM_MODEL);
	    return 0;
	}
	mp->apbuf = (unsigned char *)bu_calloc(1, nbytes, "rt_ebm_import4 bitmap");
	mp->apbuflen = nbytes;

	bu_semaphore_release(RT_SEM_MODEL);

	/* Because of in-memory padding, read each scanline separately */
	cp = (unsigned char *)mp->buf;
	for (y = 0; y < eip->ydim; y++) {
	    /* bit() addresses into mp->apbuf */
	    memcpy(bit(eip, 0, y), cp, eip->xdim);
	    cp += eip->xdim;
	}
    }
    return 0;
}


/**
 * The name will be added by the caller.
 */
int
rt_ebm_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_ebm_internal *eip;
    struct rt_ebm_internal ebm;	/* scaled version */
    union record *rec;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_EBM) return -1;
    eip = (struct rt_ebm_internal *)ip->idb_ptr;
    RT_EBM_CK_MAGIC(eip);
    ebm = *eip;			/* struct copy */

    /* Apply scale factor */
    ebm.mat[15] /= local2mm;

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record)*DB_SS_NGRAN;
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "ebm external");
    rec = (union record *)ep->ext_buf;

    bu_vls_struct_print(&str, rt_ebm_parse, (char *)&ebm);

    rec->ss.ss_id = DBID_STRSOL;
    bu_strlcpy(rec->ss.ss_keyword, "ebm", sizeof(rec->ss.ss_keyword));
    bu_strlcpy(rec->ss.ss_args, bu_vls_addr(&str), DB_SS_LEN);
    bu_vls_free(&str);

    return 0;
}


int
rt_ebm_mat(struct rt_db_internal *rop, const mat_t mat, const struct rt_db_internal *ip)
{
    if (!rop || !ip || !mat)
	return BRLCAD_OK;

    struct rt_ebm_internal *tip = (struct rt_ebm_internal *)ip->idb_ptr;
    RT_EBM_CK_MAGIC(tip);
    struct rt_ebm_internal *top = (struct rt_ebm_internal *)rop->idb_ptr;
    RT_EBM_CK_MAGIC(top);

    /* Apply transform */
    mat_t tmp;
    bn_mat_mul(tmp, mat, tip->mat);
    MAT_COPY(top->mat, tmp);

    return BRLCAD_OK;
}

/**
 * Read in the information from the string solid record.  Then, as a
 * service to the application, read in the bitmap and set up some of
 * the associated internal variables.
 */
int
rt_ebm_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
    register struct rt_ebm_internal *eip;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    BU_CK_EXTERNAL(ep);
    RT_CK_DB_INTERNAL(ip);

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_EBM;
    ip->idb_meth = &OBJ[ID_EBM];
    BU_ALLOC(ip->idb_ptr, struct rt_ebm_internal);

    eip = (struct rt_ebm_internal *)ip->idb_ptr;
    eip->magic = RT_EBM_INTERNAL_MAGIC;
    MAT_IDN(eip->mat);

    /* default source is file for compatibility with old EBM */
    eip->datasrc = RT_EBM_SRC_FILE;

    bu_vls_strcpy(&str, (const char *)ep->ext_buf);
    if (bu_struct_parse(&str, rt_ebm_parse, (char *)eip, NULL) < 0) {
	bu_vls_free(&str);
	bu_free((char *)eip, "rt_ebm_import5: eip");
	ip->idb_type = ID_NULL;
	ip->idb_ptr = (void *)NULL;
	return -2;
    }
    bu_vls_free(&str);

    /* Check for reasonable values */
    if (eip->name[0] == '\0' || eip->tallness <= 0.0 ||
	eip->xdim < 1 || eip->ydim < 1 || eip->mat[15] <= 0.0 ||
	(eip->datasrc != RT_EBM_SRC_FILE && eip->datasrc != RT_EBM_SRC_OBJ)) {
	bu_struct_print("Unreasonable EBM parameters", rt_ebm_parse, (char *)eip);
	bu_free((char *)eip, "rt_ebm_import5: eip");
	ip->idb_type = ID_NULL;
	ip->idb_ptr = (void *)NULL;
	return -1;
    }

    /* Apply modelling transforms and fetch actual data */
    if (mat == NULL) mat = bn_mat_identity;
    rt_ebm_mat(ip, mat, ip);
    if (ebm_get_data(eip, dbip) != 0) {
	bu_log("rt_ebm_import5(%d) '%s' %s\n", __LINE__,
		eip->name, "unable to load bitmap data");
	bu_free((char *)eip, "rt_ebm_import5: eip");
	ip->idb_type = ID_NULL;
	ip->idb_ptr = (void *)NULL;
	return -1;
    }
    return 0;
}


/**
 * The name will be added by the caller.
 */
int
rt_ebm_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_ebm_internal *eip;
    struct rt_ebm_internal ebm;	/* scaled version */
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_EBM) return -1;
    eip = (struct rt_ebm_internal *)ip->idb_ptr;
    RT_EBM_CK_MAGIC(eip);
    ebm = *eip;			/* struct copy */

    /* Apply scale factor */
    ebm.mat[15] /= local2mm;

    BU_CK_EXTERNAL(ep);

    bu_vls_struct_print(&str, rt_ebm_parse, (char *)&ebm);

    ep->ext_nbytes = bu_vls_strlen(&str) + 1;
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "ebm external");

    bu_strlcpy((char *)ep->ext_buf, bu_vls_addr(&str), ep->ext_nbytes);
    bu_vls_free(&str);

    return 0;
}


/**
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_ebm_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
    register struct rt_ebm_internal *eip =
	(struct rt_ebm_internal *)ip->idb_ptr;
    int i;
    struct bu_vls substr = BU_VLS_INIT_ZERO;

    RT_EBM_CK_MAGIC(eip);

    bu_vls_strcat(str, "extruded bitmap (EBM)\n\t");

    if (!verbose)
	return 0;

    bu_vls_printf(&substr, "  file=\"%s\" w=%u n=%u depth=%g\n   mat=",
		  eip->name, eip->xdim, eip->ydim, INTCLAMP(eip->tallness*mm2local));
    bu_vls_vlscat(str, &substr);
    for (i = 0; i < 15; i++) {
	bu_vls_trunc(&substr, 0);
	bu_vls_printf(&substr, "%g, ", INTCLAMP(eip->mat[i]));
	bu_vls_vlscat(str, &substr);
    }
    bu_vls_trunc(&substr, 0);
    bu_vls_printf(&substr, "%g\n", INTCLAMP(eip->mat[15]));
    bu_vls_vlscat(str, &substr);

    bu_vls_free(&substr);

    return 0;
}


/**
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_ebm_ifree(struct rt_db_internal *ip)
{
    register struct rt_ebm_internal *eip;

    RT_CK_DB_INTERNAL(ip);

    eip = (struct rt_ebm_internal *)ip->idb_ptr;
    RT_EBM_CK_MAGIC(eip);

    bu_close_mapped_file(eip->mp);

    eip->magic = 0;			/* sanity */
    eip->mp = (struct bu_mapped_file *)0;
    bu_free((char *)eip, "ebm ifree");
    ip->idb_ptr = ((void *)0);	/* sanity */
    ip->idb_type = ID_NULL;		/* sanity */
}


/**
 * Calculate bounding RPP for ebm
 */
int
rt_ebm_bbox(struct rt_db_internal *ip, point_t *min, point_t *max, const struct bn_tol *UNUSED(tol))
{
    register struct rt_ebm_internal *eip;
    vect_t v1, localspace;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_ebm_internal *)ip->idb_ptr;
    RT_EBM_CK_MAGIC(eip);

    /* Find bounding RPP of rotated local RPP */
    VSETALL(v1, 0);
    VSET(localspace, eip->xdim, eip->ydim, eip->tallness);
    bg_rotate_bbox((*min), (*max), eip->mat, v1, localspace);
    return 0;
}


/**
 * Returns -
 * 0 OK
 * !0 Failure
 *
 * Implicit return -
 * A struct rt_ebm_specific is created, and its address is stored
 * in stp->st_specific for use by rt_ebm_shot().
 */
int
rt_ebm_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_ebm_internal *eip;
    register struct rt_ebm_specific *ebmp;
    vect_t norm;
    vect_t radvec;
    vect_t diam;

    if (rtip) RT_CK_RTI(rtip);

    eip = (struct rt_ebm_internal *)ip->idb_ptr;
    RT_EBM_CK_MAGIC(eip);

    BU_GET(ebmp, struct rt_ebm_specific);
    ebmp->ebm_i = *eip;		/* struct copy */

    /* "steal" the bitmap storage */
    eip->mp = (struct bu_mapped_file *)0;	/* "steal" the mapped file */

    /* build Xform matrix from model(world) to ideal(local) space */
    bn_mat_inv(ebmp->ebm_mat, eip->mat);

    /* Pre-compute the necessary normals.  Rotate only. */
    VSET(norm, 1, 0, 0);
    MAT3X3VEC(ebmp->ebm_xnorm, eip->mat, norm);
    VSET(norm, 0, 1, 0);
    MAT3X3VEC(ebmp->ebm_ynorm, eip->mat, norm);
    VSET(norm, 0, 0, 1);
    MAT3X3VEC(ebmp->ebm_znorm, eip->mat, norm);

    stp->st_specific = (void *)ebmp;

    /* Find bounding RPP of rotated local RPP */
    rt_ebm_bbox(ip, &(stp->st_min), &(stp->st_max), &rtip->rti_tol);
    VSET(ebmp->ebm_large, ebmp->ebm_i.xdim, ebmp->ebm_i.ydim, ebmp->ebm_i.tallness);

    /* for now, EBM origin in ideal coordinates is at origin */
    VSETALL(ebmp->ebm_origin, 0);
    VADD2(ebmp->ebm_large, ebmp->ebm_large, ebmp->ebm_origin);

    /* for now, EBM cell size in ideal coordinates is one unit/cell */
    VSETALL(ebmp->ebm_cellsize, 1);

    VSUB2(diam, stp->st_max, stp->st_min);
    VADD2SCALE(stp->st_center, stp->st_min, stp->st_max, 0.5);
    VSCALE(radvec, diam, 0.5);
    stp->st_aradius = stp->st_bradius = MAGNITUDE(radvec);

    return 0;		/* OK */
}


void
rt_ebm_print(register const struct soltab *stp)
{
    register const struct rt_ebm_specific *ebmp =
	(struct rt_ebm_specific *)stp->st_specific;

    bu_log("ebm name = %s\n", ebmp->ebm_i.name);
    bu_log("dimensions = (%u, %u, %g)\n",
	   ebmp->ebm_i.xdim, ebmp->ebm_i.ydim,
	   ebmp->ebm_i.tallness);
    VPRINT("model cellsize", ebmp->ebm_cellsize);
    VPRINT("model grid origin", ebmp->ebm_origin);
}


/**
 * Intersect a ray with an extruded bitmap.  If intersection occurs, a
 * pointer to a sorted linked list of "struct seg"s will be returned.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_ebm_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    register struct rt_ebm_specific *ebmp =
	(struct rt_ebm_specific *)stp->st_specific;
    vect_t norm;
    struct xray ideal_ray;
    struct seg myhead;
    int i;

    BU_LIST_INIT(&(myhead.l));

    /* Transform actual ray into ideal space at origin in X-Y plane */
    MAT4X3PNT(ideal_ray.r_pt, ebmp->ebm_mat, rp->r_pt);
    MAT4X3VEC(ideal_ray.r_dir, ebmp->ebm_mat, rp->r_dir);

    if (rt_ebm_dda(&ideal_ray, stp, ap, &myhead) <= 0)
	return 0;

    VSET(norm, 0, 0, -1);		/* letters grow in +z, which is "inside" the halfspace */
    i = rt_seg_planeclip(seghead, &myhead, norm, 0.0, ebmp->ebm_i.tallness,
			 &ideal_ray, ap);
    return i;
}


/**
 * Given one ray distance, return the normal and entry/exit point.
 * This is mostly a matter of translating the stored code into the
 * proper normal.
 */
void
rt_ebm_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
    register struct rt_ebm_specific *ebmp =
	(struct rt_ebm_specific *)stp->st_specific;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);

    switch (hitp->hit_surfno) {
	case NORM_XPOS:
	    VMOVE(hitp->hit_normal, ebmp->ebm_xnorm);
	    break;
	case NORM_XNEG:
	    VREVERSE(hitp->hit_normal, ebmp->ebm_xnorm);
	    break;

	case NORM_YPOS:
	    VMOVE(hitp->hit_normal, ebmp->ebm_ynorm);
	    break;
	case NORM_YNEG:
	    VREVERSE(hitp->hit_normal, ebmp->ebm_ynorm);
	    break;

	case NORM_ZPOS:
	    VMOVE(hitp->hit_normal, ebmp->ebm_znorm);
	    break;
	case NORM_ZNEG:
	    VREVERSE(hitp->hit_normal, ebmp->ebm_znorm);
	    break;

	default:
	    bu_log("ebm_norm(%s): surfno=%d bad\n",
		   stp->st_name, hitp->hit_surfno);
	    VSETALL(hitp->hit_normal, 0);
	    break;
    }
}


/**
 * Everything has sharp edges.  This makes things easy.
 */
void
rt_ebm_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
    struct rt_ebm_specific *ebmp = (struct rt_ebm_specific *)stp->st_specific;
    if (!ebmp) return;

    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
    cvp->crv_c1 = cvp->crv_c2 = 0;
}


/**
 * Map the hit point in 2-D into the range 0..1 untransformed X
 * becomes U, and Y becomes V.
 */
void
rt_ebm_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
    if (ap) RT_CK_APPLICATION(ap);
    if (stp) RT_CK_SOLTAB(stp);

    uvp->uv_u = hitp->hit_vpriv[X];
    uvp->uv_v = hitp->hit_vpriv[Y];

    /* XXX should compute this based upon footprint of ray in ebm space */
    uvp->uv_du = 0.0;
    uvp->uv_dv = 0.0;
}


void
rt_ebm_free(struct soltab *stp)
{
    register struct rt_ebm_specific *ebmp =
	(struct rt_ebm_specific *)stp->st_specific;

    bu_close_mapped_file(ebmp->ebm_i.mp);

    BU_PUT(ebmp, struct rt_ebm_specific);
}


/* either x1==x2, or y1==y2 */
void
rt_ebm_plate(int x_1, int y_1, int x_2, int y_2, double t, register fastf_t *mat, struct bu_list *vlfree, register struct bu_list *vhead)
{
    point_t s, p;
    point_t srot, prot;

    BU_CK_LIST_HEAD(vhead);
    VSET(s, x_1, y_1, 0.0);
    MAT4X3PNT(srot, mat, s);
    BV_ADD_VLIST(vlfree, vhead, srot, BV_VLIST_LINE_MOVE);

    VSET(p, x_1, y_1, t);
    MAT4X3PNT(prot, mat, p);
    BV_ADD_VLIST(vlfree, vhead, prot, BV_VLIST_LINE_DRAW);

    VSET(p, x_2, y_2, t);
    MAT4X3PNT(prot, mat, p);
    BV_ADD_VLIST(vlfree, vhead, prot, BV_VLIST_LINE_DRAW);

    p[Z] = 0;
    MAT4X3PNT(prot, mat, p);
    BV_ADD_VLIST(vlfree, vhead, prot, BV_VLIST_LINE_DRAW);

    BV_ADD_VLIST(vlfree, vhead, srot, BV_VLIST_LINE_DRAW);
}


int
rt_ebm_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *UNUSED(tol), const struct bview *UNUSED(info))
{
    register struct rt_ebm_internal *eip;
    size_t x, y;
    register int following;
    register int base;

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    struct bu_list *vlfree = &rt_vlfree;
    eip = (struct rt_ebm_internal *)ip->idb_ptr;
    RT_EBM_CK_MAGIC(eip);

    /* Find vertical lines */
    base = 0;	/* lint */
    for (x = 0; x <= eip->xdim; x++) {
	following = 0;
	for (y = 0; y <= eip->ydim; y++) {
	    if (following) {
		if ((*bit(eip, x-1, y) == 0) != (*bit(eip, x, y) == 0))
		    continue;
		rt_ebm_plate(x, base, x, y, eip->tallness,
			     eip->mat, vlfree, vhead);
		following = 0;
	    } else {
		if ((*bit(eip, x-1, y) == 0) == (*bit(eip, x, y) == 0))
		    continue;
		following = 1;
		base = y;
	    }
	}
    }

    /* Find horizontal lines */
    for (y = 0; y <= eip->ydim; y++) {
	following = 0;
	for (x = 0; x <= eip->xdim; x++) {
	    if (following) {
		if ((*bit(eip, x, y-1) == 0) != (*bit(eip, x, y) == 0))
		    continue;
		rt_ebm_plate(base, y, x, y, eip->tallness,
			     eip->mat, vlfree, vhead);
		following = 0;
	    } else {
		if ((*bit(eip, x, y-1) == 0) == (*bit(eip, x, y) == 0))
		    continue;
		following = 1;
		base = x;
	    }
	}
    }
    return 0;
}


struct ebm_edge
{
    struct bu_list l;
    int x1, y1;
    int x2, y2;
    int left;	/* 1=>material to left, 0=>material to right */
    struct vertex *v;	/* vertex at x1, y1 */
};


/* either x1==x2, or y1==y2 */
static void
rt_ebm_edge(int x_1, int y_1, int x_2, int y_2, int left, struct ebm_edge *edges)
{
    struct ebm_edge *new_edge;

    BU_ALLOC(new_edge, struct ebm_edge);

    /* make all edges go from lower values to larger */
    if (y_1 < y_2 || x_1 < x_2) {
	new_edge->x1 = x_1;
	new_edge->y1 = y_1;
	new_edge->x2 = x_2;
	new_edge->y2 = y_2;
	new_edge->left = left;
    } else {
	new_edge->x1 = x_2;
	new_edge->y1 = y_2;
	new_edge->x2 = x_1;
	new_edge->y2 = y_1;
	new_edge->left = (!left);
    }
    new_edge->v = (struct vertex *)NULL;
    BU_LIST_APPEND(&edges->l, &new_edge->l);
}


static int
rt_ebm_sort_edges(struct ebm_edge *edges)
{
    struct ebm_edge loops;
    int vertical;
    int done;
    int from_x, from_y, to_x, to_y;
    int start_x, start_y;
    int max_loop_length = 0;
    int loop_length;

    /* create another list to hold the edges as they are sorted */
    BU_LIST_INIT(&loops.l);

    while (BU_LIST_NON_EMPTY(&edges->l)) {
	struct ebm_edge *start, *next;

	/* look for a vertical edge starting in lower left (smallest x and y) */
	start = (struct ebm_edge *)NULL;
	next = BU_LIST_FIRST(ebm_edge, &edges->l);
	while (BU_LIST_NOT_HEAD(&next->l, &edges->l)) {
	    if (next->x1 != next->x2) {
		next = BU_LIST_PNEXT(ebm_edge, &next->l);
		continue;	/* not a vertical edge */
	    }

	    if (!start)
		start = next;
	    else if (next->x1 < start->x1 || next->y1 < start->y1)
		start = next;

	    next = BU_LIST_PNEXT(ebm_edge, &next->l);
	}

	if (!start)
	    bu_bomb("rt_ebm_tess: rt_ebm_sort_edges: no vertical edges left!\n");

	/* put starting edge on the loop list */
	BU_LIST_DEQUEUE(&start->l);
	BU_LIST_INSERT(&loops.l, &start->l);

	next = (struct ebm_edge *)NULL;
	vertical = 0; 	/* look for horizontal edge */
	done = 0;
	to_x = start->x2;
	to_y = start->y2;
	from_x = start->x1;
	from_y = start->y1;
	start_x = from_x;
	start_y = from_y;
	loop_length = 1;
	while (!done) {
	    struct ebm_edge *e, *e_poss[3];
	    int poss;

	    /* now find an edge that starts where this one stops (at to_x, to_y) */
	    poss = 0;
	    for (BU_LIST_FOR(e, ebm_edge, &edges->l)) {
		if ((vertical && e->y1 == e->y2) ||
		    (!vertical && e->x1 == e->x2))
		    continue;

		if ((e->x1 == to_x && e->y1 == to_y) ||
		    (e->x2 == to_x && e->y2 == to_y))
		    e_poss[poss++] = e;
		if (poss > 2)
		    bu_bomb("rt_ebm_tess: rt_ebm_sort_edges: too many edges at one point\n");
	    }

	    if (poss == 0)
		bu_bomb("rt_ebm_tess: rt_ebm_sort_edges: no edge to continue loop\n");
	    if (poss == 1) {
		next = e_poss[0];
	    } else {
		/* must choose between two possibilities */
		if (vertical) {
		    if (to_x < from_x) {
			if (e_poss[0]->y1 > to_y || e_poss[0]->y2 > to_y)
			    next = e_poss[0];
			else
			    next = e_poss[1];
		    } else {
			if (e_poss[0]->y1 < to_y || e_poss[0]->y2 < to_y)
			    next = e_poss[0];
			else
			    next = e_poss[1];
		    }
		} else {
		    if (to_y < from_y) {
			if (e_poss[0]->x1 < to_x || e_poss[0]->x2 < to_x)
			    next = e_poss[0];
			else
			    next = e_poss[1];
		    } else {
			if (e_poss[0]->x1 > to_x || e_poss[0]->x2 > to_x)
			    next = e_poss[0];
			else
			    next = e_poss[1];
		    }
		}
	    }

	    if (next->x2 == to_x && next->y2 == to_y) {
		/* reverse direction of edge */
		next->x2 = next->x1;
		next->y2 = next->y1;
		next->x1 = to_x;
		next->y1 = to_y;
		next->left = (!next->left);
	    }
	    to_x = next->x2;
	    to_y = next->y2;
	    from_x = next->x1;
	    from_y = next->y1;
	    loop_length++;

	    BU_LIST_DEQUEUE(&next->l);
	    BU_LIST_INSERT(&loops.l, &next->l);

	    if (to_x == start_x && to_y == start_y) {
		if (loop_length > max_loop_length)
		    max_loop_length = loop_length;
		done = 1;	/* complete loop */
	    }

	    if (vertical)
		vertical = 0;
	    else
		vertical = 1;
	}
    }

    /* move sorted list back to "edges" */
    BU_LIST_INSERT_LIST(&edges->l, &loops.l);

    return max_loop_length;
}


int
rt_ebm_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *UNUSED(ttol), const struct bn_tol *tol)
{
    struct rt_ebm_internal *eip;
    struct shell *s;
    struct faceuse *fu=(struct faceuse*)NULL;
    struct vertex ***vertp;	/* dynam array of ptrs to pointers */
    struct vertex **loop_verts;
    struct ebm_edge edges;		/* list of edges */
    struct ebm_edge *e, *start_loop;
    size_t i;
    size_t start, x, y, left;
    size_t max_loop_length;
    size_t loop_length;
    vect_t height, h;
    struct bu_list *vlfree = &rt_vlfree;

    BN_CK_TOL(tol);
    NMG_CK_MODEL(m);

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_ebm_internal *)ip->idb_ptr;
    RT_EBM_CK_MAGIC(eip);

    BU_LIST_INIT(&edges.l);

    x = 0;
    while (x <= eip->xdim) {
	y = 0;
	while (y <= eip->ydim) {
	    if ((*bit(eip, x-1, y) == 0) != (*bit(eip, x, y) == 0)) {
		/* a vertical edge starts here */
		start = y;
		left = (*bit(eip, x, y) != 0);

		/* find other end */
		while ((*bit(eip, x-1, y) == 0) != (*bit(eip, x, y) == 0) &&
		       (*bit(eip, x, y) != 0) == left)
		    y++;
		rt_ebm_edge(x, start, x, y, left, &edges);
	    } else {
		y++;
	    }
	}
	x++;
    }

    y = 0;
    while (y <= eip->ydim) {
	x = 0;
	while (x <= eip->xdim) {
	    if ((*bit(eip, x, y-1) ==0 ) != (*bit(eip, x, y) == 0)) {
		/* a horizontal edge starts here */
		start = x;
		left = (*bit(eip, x, y-1) != 0);

		/* find other end */
		while ((*bit(eip, x, y-1) == 0) != (*bit(eip, x, y) == 0) &&
		       (*bit(eip, x, y-1) != 0) == left)
		    x++;
		rt_ebm_edge(start, y, x, y, left, &edges);
	    } else
		x++;
	}
	y++;
    }

    /* Sort edges into loops */
    max_loop_length = rt_ebm_sort_edges(&edges);

    /* Make NMG structures */

    /* make region, shell, vertex */
    *r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);


    vertp = (struct vertex ***)bu_calloc(max_loop_length, sizeof(struct vertex **) ,
					 "rt_ebm_tess: vertp");
    loop_verts = (struct vertex **)bu_calloc(max_loop_length, sizeof(struct vertex *),
					     "rt_ebm_tess: loop_verts");

    e = BU_LIST_FIRST(ebm_edge, &edges.l);
    while (BU_LIST_NOT_HEAD(&e->l, &edges.l)) {
	start_loop = e;
	loop_length = 0;
	vertp[loop_length++] = &start_loop->v;

	e = BU_LIST_PNEXT(ebm_edge, &start_loop->l);
	while (BU_LIST_NOT_HEAD(&e->l, &edges.l)) {
	    vertp[loop_length++] = &e->v;
	    if (e->x2 == start_loop->x1 && e->y2 == start_loop->y1) {
		struct faceuse *fu1;
		struct ebm_edge *e1;
		point_t pt_ebm, pt_model;
		int done = 0;

		if (e->left) {
		    /* make a face */
		    fu1 = nmg_cmface(s, vertp, loop_length);
		    NMG_CK_FACEUSE(fu1);

		    /* assign geometry to the vertices used in this face */
		    e1 = start_loop;
		    while (!done) {
			if (e1->v) {
			    VSET(pt_ebm, e1->x1, e1->y1, 0.0);
			    MAT4X3PNT(pt_model, eip->mat, pt_ebm);
			    nmg_vertex_gv(e1->v, pt_model);
			}
			if (e1 == e)
			    done = 1;
			else
			    e1 = BU_LIST_PNEXT(ebm_edge, &e1->l);
		    }

		    /* assign face geometry */
		    if (nmg_fu_planeeqn(fu1, tol))
			goto fail;

		    if (!fu)
			fu = fu1;
		} else {
		    /* make a hole */
		    for (i = 0; i < loop_length; i++) {
			if (*vertp[loop_length-i-1])
			    loop_verts[i] = (*vertp[loop_length-i-1]);
			else
			    loop_verts[i] = (struct vertex *)NULL;
		    }

		    (void) nmg_add_loop_to_face(s, fu, loop_verts ,
						loop_length, OT_OPPOSITE);

		    /* Assign geometry to new vertices */
		    done = 0;
		    e1 = start_loop;
		    i = loop_length - 1;
		    while (!done) {
			if (!loop_verts[i]->vg_p) {
			    VSET(pt_ebm, e1->x1, e1->y1, 0.0);
			    MAT4X3PNT(pt_model, eip->mat, pt_ebm);
			    nmg_vertex_gv(loop_verts[i], pt_model);
			    e1->v = loop_verts[i];
			}
			if (e1 == e) {
			    done = 1;
			} else {
			    e1 = BU_LIST_PNEXT(ebm_edge, &e1->l);
			    i--;
			}
		    }
		}
		break;
	    }
	    e = BU_LIST_PNEXT(ebm_edge, &e->l);
	}
	e = BU_LIST_PNEXT(ebm_edge, &e->l);
    }

    /* all faces should merge into one */
    nmg_shell_coplanar_face_merge(s, tol, 1, vlfree);

    fu = BU_LIST_FIRST(faceuse, &s->fu_hd);
    NMG_CK_FACEUSE(fu);

    VSET(h, 0.0, 0.0, eip->tallness);
    MAT4X3VEC(height, eip->mat, h);

    nmg_extrude_face(fu, height, vlfree, tol);

    nmg_region_a(*r, tol);

    (void)nmg_mark_edges_real(&s->l.magic, vlfree);

    bu_free((char *)vertp, "rt_ebm_tess: vertp");
    bu_free((char *)loop_verts, "rt_ebm_tess: loop_verts");
    while (BU_LIST_NON_EMPTY(&edges.l)) {
	e = BU_LIST_FIRST(ebm_edge, &edges.l);
	BU_LIST_DEQUEUE(&e->l);
	bu_free((char *)e, "rt_ebm_tess: e");
    }
    return 0;

fail:
    bu_free((char *)vertp, "rt_ebm_tess: vertp");
    bu_free((char *)loop_verts, "rt_ebm_tess: loop_verts");
    while (BU_LIST_NON_EMPTY(&edges.l)) {
	e = BU_LIST_FIRST(ebm_edge, &edges.l);
	BU_LIST_DEQUEUE(&e->l);
	bu_free((char *)e, "rt_ebm_tess: e");
    }

    return -1;
}


/**
 * Routine to format the parameters of an EBM for "db get"
 *
 * Legal requested parameters are:
 * "F" or "file" - bitmap file to extrude
 * "W" - number of cells in X direction
 * "N" - number of cells in Y direction
 * "H" - height of each cell (mm)
 * "M" - matrix to transform EBM solid into model coordinates
 *
 * no parameters requested returns all
 */
int
rt_ebm_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    register struct rt_ebm_internal *ebm=(struct rt_ebm_internal *)intern->idb_ptr;
    int i;

    RT_EBM_CK_MAGIC(ebm);

    if (attr == (char *)NULL) {
	bu_vls_strcpy(logstr, "ebm");
	bu_vls_printf(logstr, " F %s W %u N %u H %.25g",
		      ebm->name, ebm->xdim, ebm->ydim, ebm->tallness);
	bu_vls_printf(logstr, " M {");
	for (i = 0; i < 16; i++)
	    bu_vls_printf(logstr, " %.25g", ebm->mat[i]);
	bu_vls_printf(logstr, " }");
    } else if (BU_STR_EQUAL(attr, "F") || BU_STR_EQUAL(attr, "file")) {
	bu_vls_printf(logstr, "%s", ebm->name);
    } else if (BU_STR_EQUAL(attr, "W")) {
	bu_vls_printf(logstr, "%u", ebm->xdim);
    } else if (BU_STR_EQUAL(attr, "N")) {
	bu_vls_printf(logstr, "%u", ebm->ydim);
    } else if (BU_STR_EQUAL(attr, "H")) {
	bu_vls_printf(logstr, "%.25g", ebm->tallness);
    } else if (BU_STR_EQUAL(attr, "M")) {
	for (i = 0; i < 16; i++)
	    bu_vls_printf(logstr, "%.25g ", ebm->mat[i]);
    } else {
	bu_vls_printf(logstr, "ERROR: Unknown attribute, choices are F, W, N, or H\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/**
 * Routine to adjust the parameters of an EBM
 *
 * Legal parameters are:
 * "F" or "file" - bitmap file to extrude
 * "W" - number of cells in X direction
 * "N" - number of cells in Y direction
 * "H" - height of each cell (mm)
 * "M" - matrix to transform EBM solid into model coordinates
 */
int
rt_ebm_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    struct rt_ebm_internal *ebm;

    RT_CK_DB_INTERNAL(intern);

    ebm = (struct rt_ebm_internal *)intern->idb_ptr;
    RT_EBM_CK_MAGIC(ebm);

    while (argc >= 2) {
	if (BU_STR_EQUAL(argv[0], "F") || BU_STR_EQUAL(argv[0], "file")) {
	    if (strlen(argv[1]) >= RT_EBM_NAME_LEN) {
		bu_vls_printf(logstr, "ERROR: File name too long");
		return BRLCAD_ERROR;
	    }
	    bu_strlcpy(ebm->name, argv[1], RT_EBM_NAME_LEN);
	    /* ebm->datasrc = 'f'; */
	} else if (BU_STR_EQUAL(argv[0], "O") || BU_STR_EQUAL(argv[0], "object")) {
	    if (strlen(argv[1]) >= RT_EBM_NAME_LEN) {
		bu_vls_printf(logstr, "ERROR: Object name too long");
		return BRLCAD_ERROR;
	    }
	    bu_strlcpy(ebm->name, argv[1], RT_EBM_NAME_LEN);
	    /* ebm->datasrc = 'o'; */
	} else if (BU_STR_EQUAL(argv[0], "W")) {
	    ebm->xdim = atoi(argv[1]);
	} else if (BU_STR_EQUAL(argv[0], "N")) {
	    ebm->ydim = atoi(argv[1]);
	} else if (BU_STR_EQUAL(argv[0], "H")) {
	    ebm->tallness = atof(argv[1]);
	} else if (BU_STR_EQUAL(argv[0], "M")) {
	    int len = 16;
	    fastf_t array[16];
	    fastf_t *ar_ptr;
	    ar_ptr = array;
	    if (_rt_tcl_list_to_fastf_array(argv[1], &ar_ptr, &len) != len) {
		bu_vls_printf(logstr, "ERROR: incorrect number of coefficients for matrix\n");
		return BRLCAD_ERROR;
	    }
	    MAT_COPY(ebm->mat, array);
	} else {
	    bu_vls_printf(logstr, "ERROR: illegal argument, choices are F, W, N, or H\n");
	    return BRLCAD_ERROR;
	}
	argc -= 2;
	argv += 2;
    }
    return BRLCAD_OK;
}


int
rt_ebm_form(struct bu_vls *logstr, const struct rt_functab *ftp)
{
    RT_CK_FUNCTAB(ftp);

    bu_vls_printf(logstr, "F %%s W %%d N %%d H %%f M { %%f %%f %%f %%f %%f %%f %%f %%f %%f %%f %%f %%f %%f %%f %%f %%f }");

    return BRLCAD_OK;

}


/**
 * Routine to make a new EBM solid. The only purpose of this routine
 * is to initialize the matrix and height to legal values.
 */
void
rt_ebm_make(const struct rt_functab *ftp, struct rt_db_internal *intern)
{
    struct rt_ebm_internal *ebm;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_EBM;
    BU_ASSERT(&OBJ[intern->idb_type] == ftp);

    intern->idb_meth = ftp;
    BU_ALLOC(ebm, struct rt_ebm_internal);

    intern->idb_ptr = (void *)ebm;
    ebm->magic = RT_EBM_INTERNAL_MAGIC;
    MAT_IDN(ebm->mat);
    ebm->datasrc = RT_EBM_SRC_FILE;

    ebm->tallness = 1.0;
}


int
rt_ebm_params(struct pc_pc_set *ps, const struct rt_db_internal *ip)
{
    if (!ps) return 0;
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


/*
 * Computes the surface area of the ebm.
 *
 * The vertices are numbered from left to right, front to back.
 * This method calculates the position of all the 8 vertices of each cell and
 * then calculates the area of the top and bottom faces, then any other
 * necessary faces.
 */
void
rt_ebm_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    struct rt_ebm_internal *eip;
    unsigned int x, y;
    point_t x0, x1, x2, x3, x4, x5, x6, x7;
    point_t _x0, _x1, _x2, _x3, _x4, _x5, _x6, _x7;
    vect_t d3_0, d2_1, d7_4, d6_5;
    vect_t d6_0, d4_2, d4_1, d5_0, d5_3, d7_1, d7_2, d6_3, _cross;
    fastf_t _x, _y, det, _area = 0.0;

    if (area == NULL || ip == NULL) {
	return;
    }
    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_ebm_internal *)ip->idb_ptr;
    RT_EBM_CK_MAGIC(eip);

    det = fabs(bn_mat_determinant(eip->mat));
    if (EQUAL(det, 0.0)) {
	*area = -1.0;
	return;
    }

    for (y = 0; y < eip->ydim; y++) {
	for (x = 0; x < eip->xdim; x++) {
	    if (*bit(eip, x, y) != 0) {
		_x = (fastf_t)x;
		_y = (fastf_t)y;
		VSET(_x0, _x - 0.5, _y - 0.5, 0.0);
		VSET(_x1, _x + 0.5, _y - 0.5, 0.0);
		VSET(_x2, _x - 0.5, _y + 0.5, 0.0);
		VSET(_x3, _x + 0.5, _y + 0.5, 0.0);
		VSET(_x4, _x - 0.5, _y - 0.5, eip->tallness);
		VSET(_x5, _x + 0.5, _y - 0.5, eip->tallness);
		VSET(_x6, _x - 0.5, _y + 0.5, eip->tallness);
		VSET(_x7, _x + 0.5, _y + 0.5, eip->tallness);
		MAT4X3PNT(x0, eip->mat, _x0);
		MAT4X3PNT(x1, eip->mat, _x1);
		MAT4X3PNT(x2, eip->mat, _x2);
		MAT4X3PNT(x3, eip->mat, _x3);
		MAT4X3PNT(x4, eip->mat, _x4);
		MAT4X3PNT(x5, eip->mat, _x5);
		MAT4X3PNT(x6, eip->mat, _x6);
		MAT4X3PNT(x7, eip->mat, _x7);

		VSUB2(d3_0, x3, x0);
		VSUB2(d2_1, x2, x1);
		VSUB2(d7_4, x7, x4);
		VSUB2(d6_5, x6, x5);

		VCROSS(_cross, d3_0, d2_1);
		_area += 0.5 * MAGNITUDE(_cross);
		VCROSS(_cross, d7_4, d6_5);
		_area += 0.5 * MAGNITUDE(_cross);

		if (*bit(eip, x + 1, y) == 0) {
		    VSUB2(d5_3, x5, x3);
		    VSUB2(d7_1, x7, x1);
		    VCROSS(_cross, d5_3, d7_1);
		    _area += 0.5 * MAGNITUDE(_cross);
		}
		if (*bit(eip, x - 1, y) == 0) {
		    VSUB2(d6_0, x6, x0);
		    VSUB2(d4_2, x4, x2);
		    VCROSS(_cross, d6_0, d4_2);
		    _area += 0.5 * MAGNITUDE(_cross);
		}
		if (*bit(eip, x, y + 1) == 0) {
		    VSUB2(d7_2, x7, x2);
		    VSUB2(d6_3, x6, x3);
		    VCROSS(_cross, d7_2, d6_3);
		    _area += 0.5 * MAGNITUDE(_cross);
		}
		if (*bit(eip, x, y - 1) == 0) {
		    VSUB2(d4_1, x4, x1);
		    VSUB2(d5_0, x5, x0);
		    VCROSS(_cross, d4_1, d5_0);
		    _area += 0.5 * MAGNITUDE(_cross);
		}
	    }
	}
    }
    *area = _area;
}


const char *
rt_ebm_keypoint(point_t *pt, const char *keystr, const mat_t mat, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol))
{
    if (!pt || !ip)
	return NULL;

    point_t mpt = VINIT_ZERO;
    struct rt_ebm_internal *ebm = (struct rt_ebm_internal *)ip->idb_ptr;
    RT_EBM_CK_MAGIC(ebm);

    static const char *default_keystr = "V";
    const char *k = (keystr) ? keystr : default_keystr;

    if (!BU_STR_EQUAL(k, default_keystr))
	return NULL;

    point_t pnt = VINIT_ZERO;
    MAT4X3PNT(mpt, ebm->mat, pnt);
    MAT4X3PNT(*pt, mat, mpt);

    return k;
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
