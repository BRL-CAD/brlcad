#include "common.cl"


/* hit_surfno is set to one of these */
#define EHY_NORM_BODY	(1)		/* compute normal */
#define EHY_NORM_TOP	(2)		/* copy ehy_N */


struct ehy_specific {
    double ehy_V[3];		/* vector to ehy origin */
    double ehy_Hunit[3];	/* unit H vector */
    double ehy_SoR[16];		/* Scale(Rot(vect)) */
    double ehy_invRoS[16];	/* invRot(Scale(vect)) */
    double ehy_cprime;		/* c / |H| */
};

int ehy_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct ehy_specific *ehy)
{
    const double cp = ehy->ehy_cprime;

    double3 dp;		// D'
    double3 pp;		// P'
    double k1, k2;	// distance constants of solution
    double3 xlated;	// translated vector
    struct hit hits[3];	// 2 potential hit points
    struct hit *hitp;	// pointer to hit point

    // for finding roots
    double a, b, c;	// coeffs of polynomial
    double disc;	// discriminant

    hitp = &hits[0];

    dp = MAT4X3VEC(ehy->ehy_SoR, r_dir);
    xlated = r_pt - vload3(0, ehy->ehy_V);
    pp = MAT4X3VEC(ehy->ehy_SoR, xlated);

    // Find roots of the equation, using formula for quadratic
    a = dp.z * dp.z - (2 * cp + 1) * (dp.x * dp.x + dp.y * dp.y);
    b = 2.0 * (dp.z * (pp.z + cp + 1) - (2 * cp + 1) * (dp.x * pp.x + dp.y * pp.y));
    c = pp.z * pp.z - (2 * cp + 1) * (pp.x * pp.x + pp.y * pp.y - 1.0) + 2 * (cp + 1) * pp.z;
    if (!NEAR_ZERO(a, RT_PCOEF_TOL)) {
        disc = b*b - 4 * a * c;
        if (disc > 0.0) {
            disc = sqrt(disc);

            k1 = (-b + disc) / (2.0 * a);
            k2 = (-b - disc) / (2.0 * a);

            /*
             * k1 and k2 are potential solutions to intersection with
             * side.  See if they fall in range.
             */
             hitp->hit_vpriv = pp + k1 * dp;		// hit'
             if (hitp->hit_vpriv.z >= -1.0
		 && hitp->hit_vpriv.z <= 0.0) {
		hitp->hit_dist = k1;
		hitp->hit_surfno = EHY_NORM_BODY;	// compute N
                hitp++;
	     }

	     hitp->hit_vpriv = pp + k2 * dp;		// hit'
	     if (hitp->hit_vpriv.z >= -1.0
		 && hitp->hit_vpriv.z <= 0.0) {
		hitp->hit_dist = k2;
		hitp->hit_surfno = EHY_NORM_BODY;	// compute N
                hitp++;
	     }
        }
    } else if (!NEAR_ZERO(b, RT_PCOEF_TOL)) {
        k1 = -c/b;
	hitp->hit_vpriv = pp + k1 * dp;			// hit'
	if (hitp->hit_vpriv.z >= -1.0
	    && hitp->hit_vpriv.z <= 0.0) {   // hit'
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = EHY_NORM_BODY;		// compute N
	    hitp++;
        }
    }

    /*
     * Check for hitting the top plate.
     */
    /* check top plate */
    if (hitp == &hits[1] && !ZERO(dp.z)) {
        // 1 hit so far, this is worthwhile
        k1 = -pp.z/dp.z;    // top plate

	hitp->hit_vpriv = pp + k1 * dp;   /* hit' */
        if (hitp->hit_vpriv.x * hitp->hit_vpriv.x +
	    hitp->hit_vpriv.y * hitp->hit_vpriv.y <= 1.0) {
	    hitp->hit_dist = k1;
	    hitp->hit_surfno = EHY_NORM_TOP;   // -H
	    hitp++;
        }
    }

    if (hitp != &hits[2]) {
        return 0; // MISS
    }

    if (hits[0].hit_dist < hits[1].hit_dist) {
	// entry is [0], exit is [1]
	do_segp(res, idx, &hits[0], &hits[1]);
    } else {
	// entry is [1], exit is [0]
	do_segp(res, idx, &hits[1], &hits[0]);
    }
    return 2; // HIT
}


void ehy_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct ehy_specific *ehy)
{
    double3 can_normal;	/* normal to canonical ehy */
    double cp, scale;

    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;
    switch (hitp->hit_surfno) {
	case EHY_NORM_BODY:
	    cp = ehy->ehy_cprime;
	    can_normal = (double3){
		 hitp->hit_vpriv.x * (2 * cp + 1),
		 hitp->hit_vpriv.y * (2 * cp + 1),
		 -(hitp->hit_vpriv.z + cp + 1)};
	    hitp->hit_normal = MAT4X3VEC(ehy->ehy_invRoS, can_normal);
	    scale = 1.0 / length(hitp->hit_normal);
	    hitp->hit_normal = hitp->hit_normal * scale;

	    /* tuck away this scale for the curvature routine */
	    //hitp->hit_vpriv.x = scale;
	    break;
	case EHY_NORM_TOP:
	    hitp->hit_normal = -vload3(0, ehy->ehy_Hunit);
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
