#include "common.cl"


/* hit_surfno is set to one of these */
#define EPA_NORM_BODY	(1)		/* compute normal */
#define EPA_NORM_TOP	(2)		/* copy epa_N */


struct epa_specific {
    double epa_V[3];		// vector to epa origin
    double epa_Hunit[3];	// unit H vector
    double epa_SoR[16];		// Scale(Rot(vect))
    double epa_invRoS[16];	// invRot(Scale(vect))
};

int epa_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct epa_specific *epa)
{
    double3 dp;			// D'
    double3 pp;			// P'
    double k1, k2;		// distance constants of solution
    double3 xlated;		// translated vector
    struct hit hits[2];		// 2 potential hit points
    struct hit *hitp;		// pointer to hit point

    dp = MAT4X3VEC(epa->epa_SoR, r_dir);
    xlated = r_pt - vload3(0, epa->epa_V);
    pp = MAT4X3VEC(epa->epa_SoR, xlated);

    // Find roots of the equation
    double a, b, c;		//coeffs of polynomial
    double disc;		// disc of radical

    hitp = &hits[0];

    a = dp.x * dp.x + dp.y * dp.y;
    b = 2 * (dp.x * pp.x + dp.y * pp.y) - dp.z;
    c = pp.x * pp.x + pp.y * pp.y - pp.z - 1.0;
    if (!NEAR_ZERO(a, RT_PCOEF_TOL)) {
	disc = b*b - 4 * a * c;
	if (disc > 0.0) {
	    disc = sqrt(disc);

	    k1 = (-b + disc) / (2.0 * a);
	    k2 = (-b - disc) / (2.0 * a);

	    /* k1 and k2 are potential solutions to intersection with
	     * side.  See if they fall in range.
	     */
	    hitp->hit_vpriv = pp + k1 * dp;		// hit'
	    if (hitp->hit_vpriv.z <= 0.0) {
		hitp->hit_dist = k1;
		hitp->hit_surfno = EPA_NORM_BODY;	// compute N
		hitp++;
	    }

	    hitp->hit_vpriv = pp + k2 * dp;		// hit'
	    if (hitp->hit_vpriv.z <= 0.0) {
		hitp->hit_dist = k2;
		hitp->hit_surfno = EPA_NORM_BODY;	// compute N
		hitp++;
	    }
	}
    } else if (!NEAR_ZERO(b, RT_PCOEF_TOL)) {
	k1 = -c/b;
	hitp->hit_vpriv = pp + k1 * dp;		// hit'
	if (hitp->hit_vpriv.z <= 0.0) {
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = EPA_NORM_BODY;	// compute N
	    hitp++;
	}
    }

    /*
     * Check for hitting the top plate.
     */
    /* check top plate */
    if (hitp == &hits[1]  &&  !ZERO(dp.z)) {
	// 1 hit so far, this is worthwhile
	k1 = -pp.z / dp.z;		// top plate
	hitp->hit_vpriv = pp + k1 * dp;		//hit'
	if (hitp->hit_vpriv.x * hitp->hit_vpriv.x +
	    hitp->hit_vpriv.y * hitp->hit_vpriv.y <= 1.0) {
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = EPA_NORM_TOP;	// -H
	    hitp++;
	}
    }

    if (hitp != &hits[2]) {
	return 0;	// MISS
    }

    if (hits[0].hit_dist < hits[1].hit_dist) {
	// entry is [0], exit is [1]
	do_segp(res, idx, &hits[0], &hits[1]);
    } else {
	/* entry is [1], exit is [0] */
	do_segp(res, idx, &hits[1], &hits[0]);
    }
    return 2;	// HIT
}


void epa_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct epa_specific *epa)
{
    double scale;
    double3 can_normal;	// normal to canonical epa

    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;
    switch (hitp->hit_surfno) {
	case EPA_NORM_BODY:
	    can_normal = (double3){
		hitp->hit_vpriv.x,
		hitp->hit_vpriv.y,
	        -0.5};
	    hitp->hit_normal = MAT4X3VEC(epa->epa_invRoS, can_normal);
	    scale = 1.0 / length(hitp->hit_normal);
	    hitp->hit_normal = hitp->hit_normal * scale;

	    // tuck away this scale for the curvature routine
	    //hitp->hit_vpriv.x = scale;
	    break;
	case EPA_NORM_TOP:
	    hitp->hit_normal = -vload3(0, epa->epa_Hunit);
	    break;
	default:
	    break;
    }
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
