#include "common.cl"


struct tor_specific {
    double tor_alpha;       /* 0 < (R2/R1) <= 1 */
    double tor_r1;          /* for inverse scaling of k values. */
    double tor_V[3];        /* Vector to center of torus */
    double tor_SoR[16];     /* Scale(Rot(vect)) */
    double tor_invR[16];    /* invRot(vect') */
};

int tor_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct tor_specific *tor)
{
    double3 dprime;		// D'
    double3 pprime;		// P'
    double3 work;		// temporary vector
    double C[5];		// The final equation
    bn_complex_t val[4];	// The complex roots
    int l;
    int nroots;
    double k[4];		// The real roots
    int i;
    int j;
    double A[3];
    double Asqr[5];
    double X2_Y2[3];		// X**2 + Y**2
    int npts;
    double3 cor_pprime;		// new ray origin
    double cor_proj;

    dprime = MAT4X3VEC(tor->tor_SoR, r_dir);
    dprime = normalize(dprime);

    work = r_pt - vload3(0, tor->tor_V);
    pprime = MAT4X3VEC(tor->tor_SoR, work);

    /* normalize distance from torus.  substitute corrected pprime
     * which contains a translation along ray direction to closest
     * approach to vertex of torus.  Translating ray origin along
     * direction of ray to closest pt. to origin of solids coordinate
     * system, new ray origin is 'cor_pprime'.
     */
    cor_proj = -dot(pprime, dprime);
    cor_pprime = pprime + cor_proj * dprime;

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
    A[2] = X2_Y2[2] + cor_pprime.z * cor_pprime.z
        + 1.0 - tor->tor_alpha * tor->tor_alpha;

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
    nroots = rt_poly_roots(C, 4, val);

    /* Only real roots indicate an intersection in real space.
     *
     * Look at each root returned; if the imaginary part is zero
     * or sufficiently close, then use the real part as one value
     * of 't' for the intersections
     */
    for (l=0, npts=0; l < nroots; l++) {
	if (NEAR_ZERO(val[l].im, RT_PCOEF_TOL)) {
	    k[npts++] = val[l].re;
	}
    }

    /*
     * Reverse above translation by adding distance to all 'k' values.
     */
    for (i = 0; i < npts; ++i) {
	k[i] += cor_proj;
    }

    /* Here, 'npts' is number of points found */
    if (npts != 0 && npts != 2 && npts != 4) {
	return 0;		/* No hit */
    }

    /* Sort Most distant to least distant: rt_pt_sort(k, npts) */
    {
	double u;
	short n, lim;

	/* Inline rt_pt_sort().  Sorts k[] into descending order. */
	for (lim = npts-1; lim > 0; lim--) {
	    for (n = 0; n < lim; n++) {
		if ((u=k[n]) < k[n+1]) {
		    /* bubble larger towards [0] */
		    k[n] = k[n+1];
		    k[n+1] = u;
		}
	    }
	}
    }

    /* Now, t[0] > t[npts-1] */
    /* k[1] is entry point, and k[0] is farthest exit point */
    /* k[3] is entry point, and k[2] is exit point */
    for (i=npts-1; i>0; i -= 2) {
        struct hit hits[2];

	hits[0].hit_dist = k[i] * tor->tor_r1;
	hits[0].hit_surfno = i/2;
	/* Set aside vector for rt_tor_norm() later */
	hits[0].hit_vpriv = pprime + k[i] * dprime;

	hits[1].hit_dist = k[i-1] * tor->tor_r1;
	hits[1].hit_surfno = i/2;
	/* Set aside vector for rt_tor_norm() later */
	hits[1].hit_vpriv = pprime + k[i-1] * dprime;

	do_segp(res, idx, &hits[0], &hits[1]);
    }
    return npts;
}


void tor_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct tor_specific *tor)
{
    double w;
    double3 work;

    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;
    w = dot(hitp->hit_vpriv, hitp->hit_vpriv) +
	1.0 - tor->tor_alpha*tor->tor_alpha;
    work = (double3){
	 (w - 2.0) * hitp->hit_vpriv.x,
	 (w - 2.0) * hitp->hit_vpriv.y,
	 w * hitp->hit_vpriv.z};
    work = normalize(work);

    hitp->hit_normal = MAT3X3VEC(tor->tor_invR, work);
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
