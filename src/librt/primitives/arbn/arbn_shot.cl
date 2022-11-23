#include "common.cl"

struct arbn_specific {
    int neqn;
    int padding;

    double eqn[1];
};


/**
 * Intersect a ray with an ARBN.
 * Find the largest "in" distance and the smallest "out" distance.
 * Cyrus & Beck algorithm for convex polyhedra.
 *
 * Returns -
 *  0 MISS
 * >0 HIT
 */
int arbn_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct arbn_specific *arb)
{
    int i;
    int iplane, oplane;
    double in, out;	// ray in/out distances

    in = -INFINITY;
    out = INFINITY;
    iplane = oplane = -1;

    for (i = arb->neqn-1; i >= 0; --i) {
	const double4 peqn = vload4(i, arb->eqn);
	double slant_factor;	/* Direction dot Normal */
	double norm_dist = dot(peqn.xyz, r_pt) - peqn.w;
	double s;

	if ((slant_factor = -dot(peqn.xyz, r_dir)) < -1.0e-10) {
	    /* exit point, when dir.N < 0.  out = min(out, s) */
	    if (out > (s = norm_dist/slant_factor)) {
		out = s;
		oplane = i;
	    }
	} else if (slant_factor > 1.0e-10) {
	    /* entry point, when dir.N > 0.  in = max(in, s) */
	    if (in < (s = norm_dist/slant_factor)) {
		in = s;
		iplane = i;
	    }
	} else {
	    /* ray is parallel to plane when dir.N == 0.
	    * If it is outside the solid, stop now
	    * Allow very small amount of slop, to catch
	    * rays that lie very nearly in the plane of a face.
	    */
	    if (norm_dist > SQRT_SMALL_FASTF)
		return 0;	/* MISS */
	}
	if (in > out)
	    return 0;	/* MISS */
    }

    /* Validate */
    if (iplane == -1 || oplane == -1) {
	return 0;	/* MISS */
    }
    if (in >= out || out >= INFINITY)
	return 0;	/* MISS */

    struct hit hits[2];

    hits[0].hit_dist = in;
    hits[0].hit_surfno = iplane;
    hits[1].hit_dist = out;
    hits[1].hit_surfno = oplane;

    do_segp(res, idx, &hits[0], &hits[1]);
    return 2;		/* HIT */
}


void arbn_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct arbn_specific *arb)
{
    int h;

    hitp->hit_point = r_pt + r_dir*hitp->hit_dist;
    h = hitp->hit_surfno;

    if (h > arb->neqn) {
	hitp->hit_normal = (double3)(0.0);
	return;
    }
    hitp->hit_normal = vload4(h, arb->eqn).xyz;
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
