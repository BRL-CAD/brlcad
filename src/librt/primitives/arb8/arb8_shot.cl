#include "common.cl"


struct arb_specific {
    double arb_peqns[4*6];
    int arb_nmfaces;
};

int arb_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct arb_specific *arb)
{
    const int nmfaces = arb->arb_nmfaces;

    int iplane, oplane;
    double in, out;	// ray in/out distances

    in = -INFINITY;
    out = INFINITY;
    iplane = oplane = -1;

    // consider each face
    for (int j = 0; j < nmfaces; j++) {
	const double4 peqn = vload4(j, arb->arb_peqns);
	double s;

	const double dxbdn = dot(peqn.xyz, r_pt) - peqn.w;
	const double dn = -dot(peqn.xyz, r_dir);

	if (dn < -SQRT_SMALL_FASTF) {
	    // exit point, when dir.N < 0.  out = min(out, s)
	    if (out > (s = dxbdn/dn)) {
		out = s;
		oplane = j;
	    }
	} else if (dn > SQRT_SMALL_FASTF) {
	    // entry point, when dir.N > 0.  in = max(in, s)
	    if (in < (s = dxbdn/dn)) {
		in = s;
		iplane = j;
	    }
	} else {
	    /* ray is parallel to plane when dir.N == 0.
	     * If it is outside the solid, stop now.
	     * Allow very small amount of slop, to catch
	     * rays that lie very nearly in the plane of a face.
	     */
	    if (dxbdn > SQRT_SMALL_FASTF) {
		return 0;	// MISS
	    }
	}
	if (in > out) {
	    return 0;	// MISS
	}
    }
    /* Validate */
    if (iplane == -1 || oplane == -1) {
	return 0;	// MISS
    }
    if (in >= out || out >= INFINITY) {
	return 0;	// MISS
    }

    struct hit hits[2];

    hits[0].hit_dist = in;
    hits[0].hit_surfno = iplane;
    hits[1].hit_dist = out;
    hits[1].hit_surfno = oplane;

    do_segp(res, idx, &hits[0], &hits[1]);
    return 2;		// HIT
}


void arb_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct arb_specific *arb)
{
    int h;

    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;
    h = hitp->hit_surfno;
    hitp->hit_normal = vload4(h, arb->arb_peqns).xyz;
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
