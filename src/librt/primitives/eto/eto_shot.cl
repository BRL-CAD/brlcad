#include "common.cl"

struct eto_specific {
    double eto_V[3];		/* Vector to center of eto */
    double eto_r;		/* radius of revolution */
    double eto_rc;		/* semi-major axis of ellipse */
    double eto_rd;		/* semi-minor axis of ellipse */
    double eto_R[16];		/* Rot(vect) */
    double eto_invR[16];	/* invRot(vect') */
    double eu, ev, fu, fv;
};

int eto_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct eto_specific *eto)
{
    double3 dprime;		/* D' */
    double3 pprime;		/* P' */
    double3 work;		/* temporary vector */
    double C[5];
    bn_complex_t val[4];	/* The complex roots */
    double k[4];		/* The real roots */
    int i;
    int j;
    double3 cor_pprime;		/* new ray origin */
    double cor_proj;
    double A1, A2, A3, A4, A5, A6, A7, A8, B1, B2, B3, C1, C2, C3, D1, term;

    /* Convert vector into the space of the unit eto */
    dprime = MAT4X3VEC(eto->eto_R, r_dir);
    dprime = normalize(dprime);

    work = r_pt - vload3(0, eto->eto_V);
    pprime = MAT4X3VEC(eto->eto_R, work);

    /* normalize distance from eto.  substitute corrected pprime which
     * contains a translation along ray direction to closest approach
     * to vertex of eto.  Translating ray origin along direction of
     * ray to closest pt. to origin of solid's coordinate system, new
     * ray origin is 'cor_pprime'.
     */
    cor_proj = dot(pprime, dprime);
    cor_pprime = dprime * cor_proj;
    cor_pprime = pprime - cor_pprime;

    /*
     * NOTE: The following code is based on code in eto.f by ERIM for
     * GIFT.
     *
     * Given a line, finds the equation of the eto in terms of the
     * variable 't'.
     *
     * The equation for the eto is:
     *
     _______                           ________
     / 2    2              2           / 2    2               2
     [Eu(+- \/ x  + y   - R) + Ev z]  + [Fu(+-\/ x  + y   - R) + Fv z ]
     --------------------------------    -------------------------------  = 1
     2                                      2
     Rc                                     Rd
     *
     *                  ^   ^
     *       where Ev = C . N
     *
     *                  ^   ^
     *             Eu = C . A
     *
     *                  ^   ^
     *             Fv = C . A
     *
     *                  ^   ^
     *             Fu =-C . N.
     *
     * First, find X, Y, and Z in terms of 't' for this line, then
     * substitute them into the equation above.
     *
     * Wx = Dx*t + Px, etc.
     *
     * Regrouping coefficients and constants, the equation can then be
     * rewritten as:
     *
     * [A1*sqrt(C1 + C2*t + C3*t^2) + A2 + A3*t]^2 +
     * [B1*sqrt(C1 + C2*t + C3*t^2) + B2 + B3*t]^2 - D1 = 0
     *
     * where, (variables defined in code below)
     */
    A1 = eto->eto_rd * eto->eu;
    B1 = eto->eto_rc * eto->fu;
    C1 = cor_pprime.x * cor_pprime.x + cor_pprime.y * cor_pprime.y;
    C2 = 2 * (dprime.x * cor_pprime.x + dprime.y * cor_pprime.y);
    C3 = dprime.x * dprime.x + dprime.y * dprime.y;
    A2 = -eto->eto_rd * eto->eto_r * eto->eu + eto->eto_rd * eto->ev * cor_pprime.z;
    B2 = -eto->eto_rc * eto->eto_r * eto->fu + eto->eto_rc * eto->fv * cor_pprime.z;
    A3 = eto->eto_rd * eto->ev * dprime.z;
    B3 = eto->eto_rc * eto->fv * dprime.z;
    D1 = eto->eto_rc * eto->eto_rc * eto->eto_rd * eto->eto_rd;

    /*
     * Squaring the two terms above and again regrouping coefficients
     * the equation now becomes:
     *
     * A6*t^2 + A5*t + A4 = -(A8*t + A7)*sqrt(C1 + C2*t + C3*t^2)
     *
     * where, (variables defined in code)
     */
    A4 = A1*A1*C1 + B1*B1*C1 + A2*A2 + B2*B2 - D1;
    A5 = A1*A1*C2 + B1*B1*C2 + 2*A2*A3 + 2*B2*B3;
    A6 = A1*A1*C3 + B1*B1*C3 + A3*A3 + B3*B3;
    A7 = 2*(A1*A2 + B1*B2);
    A8 = 2*(A1*A3 + B1*B3);
    term = A6*A6 - A8*A8*C3;

    /*
     * Squaring both sides and subtracting RHS from LHS yields:
     */
    C[4] = (A4*A4 - A7*A7*C1);						/* t^0 */
    C[3] = (2*A4*A5 - A7*A7*C2 - 2*A7*A8*C1);				/* t^1 */
    C[2] = (2*A4*A6 + A5*A5 - A7*A7*C3 - 2*A7*A8*C2 - A8*A8*C1);	/* t^2 */
    C[1] = (2*A5*A6 - 2*A7*A8*C3 - A8*A8*C2);				/* t^3 */
    C[0] = term;							/* t^4 */
    /* NOTE: End of ERIM based code */

    /* It is known that the equation is 4th order.  Therefore, if the
     * root finder returns other than 4 roots, error.
     */
    if ((i = rt_poly_roots(C, 4, val)) != 4) {
	/* LOG */
	return 0;		/* MISS */
    }

    /* Only real roots indicate an intersection in real space.
     *
     * Look at each root returned; if the imaginary part is zero or
     * sufficiently close, then use the real part as one value of 't'
     * for the intersections
     */
    for (j = 0, i = 0; j < 4; j++) {
	if (NEAR_ZERO(val[j].im, 0.0001))
	    k[i++] = val[j].re;
    }

    /* reverse above translation by adding distance to all 'k' values. */
    for (j = 0; j < i; ++j)
	k[j] -= cor_proj;

    /* Here, 'i' is number of points found */
    switch (i) {
	case 0:
	    return 0;		/* No hit */

	default:
	    /* LOG */
	    return 0;		/* No hit */

	case 2: {
	    /* Sort most distant to least distant. */
	    double u;
	    if ((u = k[0]) < k[1]) {
		/* bubble larger towards [0] */
		k[0] = k[1];
		k[1] = u;
	    }
	}
	    break;

	case 4: {
	    short n;
	    short lim;

	    /* Inline rt_pt_sort().  Sorts k[] into descending order. */
	    for (lim = i-1; lim > 0; lim--) {
		for (n = 0; n < lim; n++) {
		    double u;
		    if ((u = k[n]) < k[n+1]) {
			/* bubble larger towards [0] */
			k[n] = k[n+1];
			k[n+1] = u;
		    }
		}
	    }
	}
	    break;
    }

    /* Now, t[0] > t[npts-1] */
    /* k[1] is entry point, and k[0] is farthest exit point */
    struct hit hits[2];
    hits[0].hit_dist = k[1];
    hits[1].hit_dist = k[0];
    hits[0].hit_surfno = 0;
    hits[1].hit_surfno = 0;
    /* Set aside vector for rt_eto_norm() later */
    hits[0].hit_vpriv = pprime + k[1] * dprime;
    hits[1].hit_vpriv = pprime + k[0] * dprime;
    do_segp(res, idx, &hits[0], &hits[1]);

    if (i == 2)
	return 2;			/* HIT */

    /* 4 points */
    /* k[3] is entry point, and k[2] is exit point */
    hits[0].hit_dist = k[3];
    hits[1].hit_dist = k[2];
    hits[0].hit_surfno = 0;
    hits[1].hit_surfno = 0;
    hits[0].hit_vpriv = pprime + k[3] * dprime;
    hits[1].hit_vpriv = pprime + k[2] * dprime;
    do_segp(res, idx, &hits[0], &hits[1]);
    return 4;			/* HIT */
}

void eto_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct eto_specific *eto)
{
    double sqrt_x2y2, efact, ffact;
    double3 normp;

    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;

    sqrt_x2y2 = sqrt(hitp->hit_vpriv.x * hitp->hit_vpriv.x
		     + hitp->hit_vpriv.y * hitp->hit_vpriv.y);

    efact = 2 * eto->eto_rd * eto->eto_rd * (eto->eu *
					     (sqrt_x2y2 - eto->eto_r) + eto->ev * hitp->hit_vpriv.z);

    ffact = 2 * eto->eto_rc * eto->eto_rc * (eto->fu *
					     (sqrt_x2y2 - eto->eto_r) + eto->fv * hitp->hit_vpriv.z);

    normp.x = (efact * eto->eu + ffact * eto->fu) / sqrt_x2y2;

    normp.y = hitp->hit_vpriv.y * normp.x;
    normp.x = hitp->hit_vpriv.x * normp.x;
    normp.z = efact * eto->ev + ffact * eto->fv;

    normp = normalize(normp);
    hitp->hit_normal = MAT3X3VEC(eto->eto_invR, normp);
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
