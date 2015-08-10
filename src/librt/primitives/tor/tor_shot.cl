#include "common.cl"


struct tor_shot_specific {
    double tor_SoR[16];
    double tor_V[3];
    double tor_alpha, tor_r1;
};

int tor_shot(global struct hit *res, const double3 r_pt, const double3 r_dir, global const struct tor_shot_specific *tor)
{
    global const double *SoR = tor->tor_SoR;
    global const double *V = tor->tor_V;
    const double alpha = tor->tor_alpha;
    const double r1 = tor->tor_r1;

    double3 dprime;		// D'
    double3 pprime;		// P'
    double3 work;		// temporary vector
    double C[5];		// The final equation
    bn_complex_t val[4];	// The complex roots
    double k[4];		// The real roots
    int i;
    int j;
    double A[3];
    double Asqr[5];
    double X2_Y2[3];		// X**2 + Y**2
    double3 cor_pprime;		// new ray origin
    double cor_proj;

    /* Convert vector into the space of the unit torus */
    const double f = 1.0/SoR[15];
    dprime.x = dot(vload3(0, &SoR[0]), r_dir) * f;
    dprime.y = dot(vload3(0, &SoR[4]), r_dir) * f;
    dprime.z = dot(vload3(0, &SoR[8]), r_dir) * f;
    dprime = normalize(dprime);

    work = r_pt - vload3(0, V);
    pprime.x = dot(vload3(0, &SoR[0]), work) * f;
    pprime.y = dot(vload3(0, &SoR[4]), work) * f;
    pprime.z = dot(vload3(0, &SoR[8]), work) * f;

    /* normalize distance from torus.  substitute corrected pprime
     * which contains a translation along ray direction to closest
     * approach to vertex of torus.  Translating ray origin along
     * direction of ray to closest pt. to origin of solid's coordinate
     * system, new ray origin is 'cor_pprime'.
     */
    cor_proj = dot(pprime, dprime);
    cor_pprime = pprime - dprime * cor_proj;

    /* Given a line and a ratio, alpha, finds the equation of the unit
     * torus in terms of the variable 't'.
     *
     * The equation for the torus is:
     *
     * [ X**2 + Y**2 + Z**2 + (1 - alpha**2) ]**2 - 4*(X**2 + Y**2) = 0
     *
     * First, find X, Y, and Z in terms of 't' for this line, then
     * substitute them into the equation above.
     *
     * Wx = Dx*t + Px
     *
     * Wx**2 = Dx**2 * t**2  +  2 * Dx * Px  +  Px**2
     *		[0]                [1]           [2]    dgr=2
     */
    X2_Y2[0] = dot(dprime.xy, dprime.xy);
    X2_Y2[1] = 2.0 * dot(dprime.xy, cor_pprime.xy);
    X2_Y2[2] = dot(cor_pprime.xy, cor_pprime.xy);

    /* A = X2_Y2 + Z2 */
    A[0] = X2_Y2[0] + dprime.z * dprime.z;
    A[1] = X2_Y2[1] + 2.0 * dprime.z * cor_pprime.z;
    A[2] = X2_Y2[2] + cor_pprime.z * cor_pprime.z + 1.0 - alpha * alpha;

    /* Inline expansion of (void) bn_poly_mul(&Asqr, &A, &A) */
    /* Both polys have degree two */
    Asqr[0] = A[0] * A[0];
    Asqr[1] = A[0] * A[1] + A[1] * A[0];
    Asqr[2] = A[0] * A[2] + A[1] * A[1] + A[2] * A[0];
    Asqr[3] = A[1] * A[2] + A[2] * A[1];
    Asqr[4] = A[2] * A[2];

    /* Inline expansion of bn_poly_scale(&X2_Y2, 4.0) and
     * bn_poly_sub(&C, &Asqr, &X2_Y2).
     */
    C[0] = Asqr[0];
    C[1] = Asqr[1];
    C[2] = Asqr[2] - X2_Y2[0] * 4.0;
    C[3] = Asqr[3] - X2_Y2[1] * 4.0;
    C[4] = Asqr[4] - X2_Y2[2] * 4.0;

    /* It is known that the equation is 4th order.  Therefore, if the
     * root finder returns other than 4 roots, error.
     */
    if ((i = rt_poly_roots(C, 4, val)) != 4) {
	return 0;		// MISS
    }

    /* Only real roots indicate an intersection in real space.
     *
     * Look at each root returned; if the imaginary part is zero or
     * sufficiently close, then use the real part as one value of 't'
     * for the intersections
     */
    for (j=0, i=0; j < 4; j++) {
	if (NEAR_ZERO(val[j].im, RT_PCOEF_TOL))
	    k[i++] = val[j].re;
    }

    /* reverse above translation by adding distance to all 'k' values.
     */
    for (j = 0; j < i; ++j)
	k[j] -= cor_proj;

    /* Here, 'i' is number of points found */
    switch (i) {
	default:
	case 0:
	    return 0;		// No hit
	case 2:
	    {
		/* Sort most distant to least distant. */
		double u;
		if ((u=k[0]) < k[1]) {
		    /* bubble larger towards [0] */
		    k[0] = k[1];
		    k[1] = u;
		}
	    }
	    break;
	case 4:
	    {
		short n, lim;

		/* Inline rt_pt_sort().  Sorts k[] into descending order. */
		for (lim = i-1; lim > 0; lim--) {
		    for (n = 0; n < lim; n++) {
			double u;
			if ((u=k[n]) < k[n+1]) {
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
    if (i == 2) {
        res[0].hit_surfno = 0;
        res[0].hit_dist = k[1]*r1;
        res[1].hit_surfno = 0;
        res[1].hit_dist = k[0]*r1;
	/* Set aside vector for rt_tor_norm() later */
        res[0].hit_vpriv = pprime + k[1] * dprime;
        res[1].hit_vpriv = pprime + k[0] * dprime;
    	return 2;		// HIT
    } else {
	/* 4 points */
	/* k[3] is entry point, and k[2] is exit point */
        res[0].hit_surfno = 1;
        res[0].hit_dist = k[3]*r1;
        res[1].hit_surfno = 1;
        res[1].hit_dist = k[2]*r1;

        res[2].hit_surfno = 0;
        res[2].hit_dist = k[1]*r1;
        res[3].hit_surfno = 0;
        res[3].hit_dist = k[0]*r1;

	/* Set aside vector for rt_tor_norm() later */
        res[0].hit_vpriv = pprime + k[3] * dprime;
        res[1].hit_vpriv = pprime + k[2] * dprime;
        res[2].hit_vpriv = pprime + k[1] * dprime;
        res[3].hit_vpriv = pprime + k[0] * dprime;
    	return 4;		// HIT
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

