#include "common.cl"


struct hrt_specific {
    double hrt_V[3]; /* Vector to center of heart */
    double hrt_SoR[16]; /* Scale(Rot(vect)) */
};

int hrt_shot(RESULT_TYPE *res, const double3 r_pt, const double3 r_dir, const uint idx, global const struct hrt_specific *hrt)
{
    double3 dprime;		/* D' : The new shot direction */
    double3 pprime;		/* P' : The new shot point */
    double3 trans;		/* Translated shot vector */
    double3 norm_pprime;	/* P' with normalized distance from heart */
    double Xsqr[3], Ysqr[3], Zsqr[3];	/* X^2, 9/4*Y^2, Z^2 - 1 */
    double Acube[7], S[7];	/* The sextic equation (of power 6) and A^3 */
    double Zcube[4], A[3];	/* Z^3 and  X^2 + 9/4 * Y^2 + Z^2 - 1 */
    double X2_Y2[3], Z3_X2_Y2[6];	/* X^2 + 9/80*Y^2 and Z^3*(X^2 + 9/80*Y^2) */
    bn_complex_t val[6];	/* The complex roots */
    double k[6];		/* The real roots */
    int npts;
    int i;
    int j;

    /* Translate the ray point */
    trans = r_pt - vload3(0, hrt->hrt_V);

    /* Scale and Rotate point to get P' */
    pprime = MAT4X3VEC(hrt->hrt_SoR, trans);
    /* Translate ray direction vector */
    dprime = MAT4X3VEC(hrt->hrt_SoR, r_dir);
    dprime = normalize(dprime);

    /* Normalize distance from the heart. Substitutes a corrected ray
     * point, which contains a translation along the ray direction to
     * the closest approach to vertex of the heart.Translating the ray
     * along the direction of the ray to the closest point near the
     * heart's center vertex. Thus, the New ray origin is normalized.
     */
    norm_pprime = dprime * dot(pprime, dprime);
    norm_pprime = pprime - norm_pprime;

    /**
     * Generate the sextic equation S(t) = 0 to be passed through the root finder.
     */
    /* X**2 */
    Xsqr[0] = dprime.x * dprime.x;
    Xsqr[1] = 2 * dprime.x * pprime.x;
    Xsqr[2] = pprime.x * pprime.x;

    /* 9/4 * Y**2*/
    Ysqr[0] = 9/4 * dprime.y * dprime.y;
    Ysqr[1] = 9/2 * dprime.y * pprime.y;
    Ysqr[2] = 9/4 * (pprime.y * pprime.y);

    /* Z**2 - 1 */
    Zsqr[0] = dprime.z * dprime.z;
    Zsqr[1] = 2 * dprime.z * pprime.z;
    Zsqr[2] = pprime.z * pprime.z - 1.0 ;

    /* A = X^2 + 9/4 * Y^2 + Z^2 - 1 */
    A[0] = Xsqr[0] + Ysqr[0] + Zsqr[0];
    A[1] = Xsqr[1] + Ysqr[1] + Zsqr[1];
    A[2] = Xsqr[2] + Ysqr[2] + Zsqr[2];

    /* Z**3 */
    Zcube[0] = dprime.z * Zsqr[0];
    Zcube[1] = 1.5 * dprime.z * Zsqr[1];
    Zcube[2] = 1.5 * pprime.z * Zsqr[1];
    Zcube[3] = pprime.z * ( Zsqr[2] + 1.0 );

    Acube[0] = A[0] * A[0] * A[0];
    Acube[1] = 3.0 * A[0] * A[0] * A[1];
    Acube[2] = 3.0 * (A[0] * A[0] * A[2] + A[0] * A[1] * A[1]);
    Acube[3] = 6.0 * A[0] * A[1] * A[2] + A[1] * A[1] * A[1];
    Acube[4] = 3.0 * (A[0] * A[2] * A[2] + A[1] * A[1] * A[2]);
    Acube[5] = 3.0 * A[1] * A[2] * A[2];
    Acube[6] = A[2] * A[2] * A[2];

    /* X**2 + 9/80 Y**2 */
    X2_Y2[0] = Xsqr[0] + Ysqr[0] / 20;
    X2_Y2[1] = Xsqr[1] + Ysqr[1] / 20;
    X2_Y2[2] = Xsqr[2] + Ysqr[2] / 20;

    /* Z**3 * (X**2 + 9/80 * Y**2) */
    Z3_X2_Y2[0] = Zcube[0] * X2_Y2[0];
    Z3_X2_Y2[1] = X2_Y2[0] * Zcube[1];
    Z3_X2_Y2[2] = X2_Y2[0] * Zcube[2] + X2_Y2[1] * Zcube[0] + X2_Y2[1] * Zcube[1] + X2_Y2[2] * Zcube[0];
    Z3_X2_Y2[3] = X2_Y2[0] * Zcube[3] + X2_Y2[1] * Zcube[2] + X2_Y2[2] * Zcube[1];
    Z3_X2_Y2[4] = X2_Y2[1] * Zcube[3] + X2_Y2[2] * Zcube[2];
    Z3_X2_Y2[5] = X2_Y2[2] * Zcube[3];

    S[0] = Acube[0];
    S[1] = Acube[1] - Z3_X2_Y2[0];
    S[2] = Acube[2] - Z3_X2_Y2[1];
    S[3] = Acube[3] - Z3_X2_Y2[2];
    S[4] = Acube[4] - Z3_X2_Y2[3];
    S[5] = Acube[5] - Z3_X2_Y2[4];
    S[6] = Acube[6] - Z3_X2_Y2[5];

    /* It is known that the equation is sextic (of order 6). Therefore, if the
     * root finder returns other than 6 roots, return an error.
     */
    if ((i = rt_poly_roots(S, 6, val)) != 6) {
        return 0;               /* MISS */
    }

    /* Only real roots indicate an intersection in real space.
     *
     * Look at each root returned; if the imaginary part is zero or
     * sufficiently close, then use the real part as one value of 't'
     * for the intersections
     */
    for (j=0, npts=0; j < i; j++) {
        if (NEAR_ZERO(val[j].im, RT_PCOEF_TOL))
            k[npts++] = val[j].re;
    }
    /* Here, 'npts' is number of points found */
    if (npts != 2 && npts != 4 && npts != 6) {
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
    /* k[5] is entry point, and k[4] is exit point */
    for (i=npts-1; i>0; i -= 2) {
	struct hit hits[2];

	hits[0].hit_dist = k[i];
	hits[1].hit_dist = k[i-1];
	hits[0].hit_surfno = hits[1].hit_surfno = 0;
	/* Set aside vector for rt_hrt_norm() later */
	hits[0].hit_vpriv = pprime + k[i] * dprime;
	hits[1].hit_vpriv = pprime + k[i-1] * dprime;

	do_segp(res, idx, &hits[0], &hits[1]);
    }
    return npts;			/* HIT */
}


void hrt_norm(struct hit *hitp, const double3 r_pt, const double3 r_dir, global const struct hrt_specific *hrt)
{
    double w, fx, fy, fz;
    double3 work;

    hitp->hit_point = r_pt + r_dir * hitp->hit_dist;
    w = hitp->hit_vpriv.x * hitp->hit_vpriv.x
        + 9.0/4.0 * hitp->hit_vpriv.y * hitp->hit_vpriv.y
        + hitp->hit_vpriv.z * hitp->hit_vpriv.z - 1.0;
    fx = (w * w - 1/3 * hitp->hit_vpriv.z * hitp->hit_vpriv.z * hitp->hit_vpriv.z) * hitp->hit_vpriv.x;
    fy = hitp->hit_vpriv.y * (12/27 * w * w - 80/3 * hitp->hit_vpriv.z * hitp->hit_vpriv.z * hitp->hit_vpriv.z);
    fz = (w * w - 0.5 * hitp->hit_vpriv.z * (hitp->hit_vpriv.x * hitp->hit_vpriv.x + 9/80 * hitp->hit_vpriv.y * hitp->hit_vpriv.y)) * hitp->hit_vpriv.z;
    work = (double3){fx, fy, fz};
    hitp->hit_normal = work;
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
