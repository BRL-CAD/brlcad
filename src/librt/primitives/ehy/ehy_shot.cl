#include "common.cl"

/* hit_surfno is set to one of these */
#define EHY_NORM_BODY	(1)		/* compute normal */
#define EHY_NORM_TOP	(2)		/* copy ehy_N */


struct ehy_shot_specific {
    double ehy_SoR[16];
    double ehy_V[3];
    double ehy_cprime;
};

__kernel void
ehy_shot(global struct hit *res, const double3 r_pt, const double3 r_dir,
global const struct ehy_shot_specific *ehy)
{
    const double16 SoR = vload16(0, &ehy->ehy_SoR[0]);
    const double3 V = vload3(0, &ehy->ehy_V[0]);
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

    // out, Mat, vect
    const double f = 1.0/SoR.sf;
    dp.x = dot(SoR.s012, r_dir) * f;
    dp.y = dot(SoR.s456, r_dir) * f;
    dp.z = dot(SoR.s89a, r_dir) * f;
    xlated = r_pt - V;
    pp.x = dot(SoR.s012, xlated) * f;
    pp.y = dot(SoR.s456, xlated) * f;
    pp.z = dot(SoR.s89a, xlated) * f;

    // Find roots of the equation, using formula for quadratic
    a = dp.z * dp.z - (2 * cp + 1) * (dp.x * dp.x + dp.y * dp.y);
    b = 2.0 * (dp.z * (pp.z + cp + 1) - (2 * cp + 1) * (dp.x * pp.x + dp.y * pp.y));
    c = pp.z * pp.z - (2 * cp + 1) * (pp.x * pp.x + pp.y * pp.y - 1.0) + 2 * (cp + 1) * pp.z;
    if (!NEAR_ZERO(a, RT_PCOEF_TOL)) {
        disc = b*b - 4 * a * c;
        if (disc > 0) {
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
	res[0].hit_surfno = INT_MAX;
        return; // MISS
    }

    if (hits[0].hit_dist < hits[1].hit_dist) {
	// entry is [0], exit is [1]
        res[0] = hits[0];
        res[1] = hits[1];
    } else {
	// entry is [1], exit is [0]
        res[0] = hits[1];
        res[1] = hits[0];
    }
    return; // HIT
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
