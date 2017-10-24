#include "common.cl"


struct ebm_specific {
    double ebm_xnorm[3];    /* local +X norm in model coords */
    double ebm_ynorm[3];
    double ebm_znorm[3];
    double ebm_cellsize[3]; /* ideal coords: size of each cell */
    double ebm_origin[3];   /* local coords of grid origin (0, 0, 0) for now */
    double ebm_large[3];    /* local coords of XYZ max */
    double ebm_mat[16];	    /* model to ideal space */

    double tallness;	    /**< @brief Z dimension (mm) */
    uint xdim;		    /**< @brief X dimension (w cells) */
    uint ydim;		    /**< @brief Y dimension (n cells) */
    uchar apbuf[16];
};


#define MAX_HITS 32

/*
 * Regular bit addressing is used: (0..W-1, 0..N-1), but the bitmap is
 * stored with two cells of zeros all around, so permissible
 * subscripts run (-2..W+1, -2..N+1).  This eliminates special-case
 * code for the boundary conditions.
 */
#define BIT_XWIDEN 2
#define BIT_YWIDEN 2
#define BIT(_eip, _x, _y) \
    (((_eip)->apbuf))[((_y)+BIT_YWIDEN)*((_eip)->xdim+BIT_XWIDEN*2)+(_x)+BIT_XWIDEN]

/*
 * Codes to represent surface normals.  In a bitmap, there are only 4
 * possible normals.  With this code, reverse the sign to reverse the
 * direction.  As always, the normal is expected to point outwards.
 */
#define NORM_ZPOS (3)
#define NORM_YPOS (2)
#define NORM_XPOS (1)
#define NORM_XNEG (-1)
#define NORM_YNEG (-2)
#define NORM_ZNEG (-3)

#define X 0
#define Y 1
#define Z 2

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
static int
ebm_planeclip(RESULT_TYPE *res, struct hit *hits, int nhits,
	      const double3 r_pt, const double3 r_dir,
	      const uint idx, double in, double out)
{
    double3 out_norm = (double3){0.0, 0.0, -1.0};
    double norm_dist_min, norm_dist_max;
    double slant_factor;
    double kmin, kmax;
    double3 in_norm;
    short out_norm_code;
    short i;
    int npts;

    norm_dist_min = in - dot(r_pt, out_norm);
    slant_factor = dot(r_dir, out_norm);    /* always abs < 1 */

    if (ZERO(slant_factor)) {
	if (norm_dist_min < 0.0) {
	    return 0;
	}
	kmin = -INFINITY;
    } else
	kmin = norm_dist_min / slant_factor;

    in_norm = -out_norm;
    norm_dist_max = out - dot(r_pt, in_norm);
    slant_factor = dot(r_dir, in_norm);	    /* always abs < 1 */
    if (ZERO(slant_factor)) {
	if (norm_dist_max < 0.0) {
	    return 0;
	}
	kmax = INFINITY;
    } else
	kmax =  norm_dist_max / slant_factor;

    if (kmin > kmax) {
	/* if r_dir.z < 0, will need to swap min & max */
	slant_factor = kmax;
	kmax = kmin;
	kmin = slant_factor;
	out_norm_code = NORM_ZPOS;
    } else {
	out_norm_code = NORM_ZNEG;
    }

    npts = 0;
    for (i=0; i<nhits; i+=2) {
	if (hits[i+1].hit_dist <= kmin)
	    continue;
	if (hits[i+0].hit_dist >= kmax)
	    continue;
	if (hits[i+0].hit_dist <= kmin) {
	    hits[i+0].hit_dist = kmin;
	    hits[i+0].hit_surfno =  out_norm_code;
	}
	if (hits[i+1].hit_dist >= kmax) {
	    hits[i+1].hit_dist = kmax;
	    hits[i+1].hit_surfno = -out_norm_code;
	}
	do_segp(res, idx, &hits[i+0], &hits[i+1]);
	npts += 2;
    }
    return npts;
}

/**
 * Step through the 2-D array, in local coordinates ("ideal space").
 */
static int
ebm_dda(struct hit *hits, const double3 r_pt, const double3 dir,
	global const struct ebm_specific *ebm)
{
    double r_dir[4];
    double3 invdir;
    double t0;		/* in point of cell */
    double t1;		/* out point of cell */
    double tmax;	/* out point of entire grid */
    double t[2];	/* next t value for XY cell plane intersect */
    double delta[2];	/* spacing of XY cell planes along ray */
    uint igrid[2];	/* grid cell coordinates of cell (integerized) */
    double3 P;		/* hit point */
    int i;
    uchar in_index;
    uchar out_index;
    uchar val;
    uchar inside;	/* inside/outside a solid flag */
    const char rt_ebm_normtab[3] = { NORM_XPOS, NORM_YPOS, NORM_ZPOS };
    uchar pad[1];

    vstore3(dir, 0, r_dir);

    /* compute the inverse of the direction cosines */
    if (!ZERO(r_dir[X])) {
	invdir.x = 1.0/r_dir[X];
    } else {
	invdir.x = INFINITY;
	r_dir[X] = 0.0;
    }
    if (!ZERO(r_dir[Y])) {
	invdir.y = 1.0/r_dir[Y];
    } else {
	invdir.y = INFINITY;
	r_dir[Y] = 0.0;
    }
    if (!ZERO(r_dir[Z])) {
	invdir.z = 1.0/r_dir[Z];
    } else {
	invdir.z = INFINITY;
	r_dir[Z] = 0.0;
    }

    /* intersect ray with ideal grid rpp */
    if (!rt_in_rpp2(r_pt, invdir, ebm->ebm_origin, ebm->ebm_large, &t0, &tmax))
	return 0;   /* MISS */

    P = r_pt + vload3(0, r_dir) * t0;
    /* P is hit point (on RPP?) */

    /* find grid cell where ray first hits ideal space bounding RPP */
    igrid[X] = (P.x - ebm->ebm_origin[X]) / ebm->ebm_cellsize[X];
    igrid[Y] = (P.y - ebm->ebm_origin[Y]) / ebm->ebm_cellsize[Y];
    if (igrid[X] >= ebm->xdim) {
	igrid[X] = ebm->xdim-1;
    }
    if (igrid[Y] >= ebm->ydim) {
	igrid[Y] = ebm->ydim-1;
    }

    if (ZERO(r_dir[X]) && ZERO(r_dir[Y])) {
	/* ray is traveling exactly along Z axis.  just check the one
	 * cell hit.  depend on higher level to clip ray to Z extent.
	 */
	if (BIT(ebm, igrid[X], igrid[Y]) == 0) {
	    return 0;	/* MISS */
	} else {
	    hits[0].hit_dist = 0;
	    hits[1].hit_dist = INFINITY;

	    hits[0].hit_vpriv.x = (double)igrid[X] / ebm->xdim;
	    hits[0].hit_vpriv.y = (double)igrid[Y] / ebm->ydim;

	    hits[1].hit_vpriv.x = (double)igrid[X] / ebm->xdim;
	    hits[1].hit_vpriv.y = (double)igrid[Y] / ebm->ydim;

	    if (r_dir[Z] < 0) {
		hits[0].hit_surfno = NORM_ZPOS;
		hits[1].hit_surfno = NORM_ZNEG;
	    } else {
		hits[0].hit_surfno = NORM_ZNEG;
		hits[1].hit_surfno = NORM_ZPOS;
	    }
	    return 2;	/* HIT */
	}
    }

    /* X setup */
    if (ZERO(r_dir[X])) {
	t[X] = INFINITY;
	delta[X] = 0.0;
    } else {
	uint j = igrid[X];
	if (r_dir[X] < 0) j++;
	t[X] = (ebm->ebm_origin[X] + j*ebm->ebm_cellsize[X] - r_pt.x) * invdir.x;
	delta[X] = ebm->ebm_cellsize[X] * fabs(invdir.x);
    }
    /* Y setup */
    if (ZERO(r_dir[Y])) {
	t[Y] = INFINITY;
	delta[Y] = 0.0;
    } else {
	uint j = igrid[Y];
	if (r_dir[Y] < 0) j++;
	t[Y] = (ebm->ebm_origin[Y] + j*ebm->ebm_cellsize[Y] - r_pt.y) * invdir.y;
	delta[Y] = ebm->ebm_cellsize[Y] * fabs(invdir.y);
    }

    /* the delta[] elements *must* be positive, as t must increase */
    /* find face of entry into first cell -- max initial t value */
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

    /* advance to next exits */
    t[X] += delta[X];
    t[Y] += delta[Y];

    /* ensure that next exit is after first entrance */
    if (t[X] < t0) {
	t[X] += delta[X];
    }
    if (t[Y] < t0) {
	t[Y] += delta[Y];
    }

    i = 0;
    inside = false;
    while (t0 < tmax) {
	/* find minimum exit t value */
	out_index = t[X] < t[Y] ? X : Y;

	t1 = t[out_index];

	/* ray passes through cell igrid[XY] from t0 to t1 */
	val = BIT(ebm, igrid[X], igrid[Y]);

	if (inside) {
	    if (val > 0) {
		/* do nothing, marching through solid */
	    } else {
		/* end of segment (now in an empty voxel) */
		/* handle transition from solid to vacuum */
		inside = false;

		hits[i].hit_dist = t0;
		hits[i].hit_vpriv.x = (double)igrid[X] / ebm->xdim;
		hits[i].hit_vpriv.y = (double)igrid[Y] / ebm->ydim;

		/* compute exit normal */
		if (r_dir[in_index] < 0) {
		    /* go left, exit normal goes left */
		    hits[i].hit_surfno = -rt_ebm_normtab[in_index];
		} else {
		    /* go right, exit norm goes right */
		    hits[i].hit_surfno =  rt_ebm_normtab[in_index];
		}
		i++;
		if (i >= MAX_HITS) break;
	    }
	} else {
	    if (val > 0) {
		/* handle the transition from vacuum to solid */
		/* start of segment (entering a full voxel) */
		inside = true;

		hits[i].hit_dist = t0;
		hits[i].hit_vpriv.x = (double)igrid[X] / ebm->xdim;
		hits[i].hit_vpriv.y = (double)igrid[Y] / ebm->ydim;

		/* compute entry normal */
		if (r_dir[in_index] < 0) {
		    /* go left, entry norm goes right */
		    hits[i].hit_surfno =  rt_ebm_normtab[in_index];
		} else {
		    /* go right, entry norm goes left */
		    hits[i].hit_surfno = -rt_ebm_normtab[in_index];
		}
		i++;
		if (i >= MAX_HITS) break;
	    } else {
		/* do nothing, marching through void */
	    }
	}

	/* take next step */
	t0 = t1;
	in_index = out_index;
	t[out_index] += delta[out_index];
	if (r_dir[out_index] > 0) {
	    igrid[out_index]++;
	} else {
	    igrid[out_index]--;
	}
    }

    if (inside) {
	/* close off the final segment */
	hits[i].hit_dist = tmax;
	hits[i].hit_vpriv = (double3)(0.0);

	/* compute exit normal.  previous out_index is now in_index */
	if (r_dir[in_index] < 0) {
	    /* go left, exit normal goes left */
	    hits[i].hit_surfno = -rt_ebm_normtab[in_index];
	} else {
	    /* go right, exit norm goes right */
	    hits[i].hit_surfno =  rt_ebm_normtab[in_index];
	}
	i++;
    }
    return i;
}

int ebm_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct ebm_specific *ebm)
{
    struct hit hits[MAX_HITS];
    double3 ir_pt, ir_dir;
    int npts;

    /* transform actual ray into ideal space at origin in X-Y plane */
    ir_pt = MAT4X3PNT(vload16(0, ebm->ebm_mat), r_pt);
    ir_dir = MAT4X3VEC(ebm->ebm_mat, r_dir);

    if ((npts = ebm_dda(hits, ir_pt, ir_dir, ebm)) == 0) {
	return 0;
    } else {
	/* letters grow in +z, which is "inside" the halfspace */
	return ebm_planeclip(res, hits, npts, ir_pt, ir_dir, idx, 0.0, ebm->tallness);
    }
}


void ebm_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct ebm_specific *ebm)
{
    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;

    switch (hitp->hit_surfno) {
	case NORM_XPOS: hitp->hit_normal =  vload3(0, ebm->ebm_xnorm); break;
	case NORM_XNEG: hitp->hit_normal = -vload3(0, ebm->ebm_xnorm); break;
	case NORM_YPOS: hitp->hit_normal =  vload3(0, ebm->ebm_ynorm); break;
	case NORM_YNEG: hitp->hit_normal = -vload3(0, ebm->ebm_ynorm); break;
	case NORM_ZPOS: hitp->hit_normal =  vload3(0, ebm->ebm_znorm); break;
	case NORM_ZNEG: hitp->hit_normal = -vload3(0, ebm->ebm_znorm); break;
	default: hitp->hit_normal = (double3)(0.0); break;
    }
}

