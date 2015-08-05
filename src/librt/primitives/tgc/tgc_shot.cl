#ifdef cl_khr_fp64
    #pragma OPENCL EXTENSION cl_khr_fp64 : enable
#elif defined(cl_amd_fp64)
    #pragma OPENCL EXTENSION cl_amd_fp64 : enable
#else
    #error "Double precision floating point not supported by OpenCL implementation."
#endif

#include "solver.cl"

/* hit_surfno is set to one of these */
#define TGC_NORM_BODY	(1)	/* compute normal */
#define TGC_NORM_TOP	(2)	/* copy tgc_N */
#define TGC_NORM_BOT	(3)	/* copy reverse tgc_N */

__constant double rti_tol_dist = 0.0005;

__kernel void
tgc_shot(global __write_only double4 *dist, global __write_only int4 *surfno,
global __write_only double8 *inv, global __write_only double8 *outv,
const double3 r_pt, const double3 r_dir,
const double3 V, const double sH, const double A, const double B,
const double CdAm1, const double DdBm1, const double AAdCC, const double BBdDD,
const double3 N, const double16 ScShR, const char AD_CB)
{
    double3 pprime;
    double3 dprime;
    double3 work;
    double k[6];
    int hit_type[6];
    double t, b, zval, dir;
    double t_scale;
    double alf1, alf2;
    int npts;
    int intersect;
    double3 cor_pprime;		/* corrected P prime */
    double cor_proj = 0.0;	/* corrected projected dist */
    int i;
    double C0;
    double Xsqr[3], Ysqr[3];
    double R[2], Rsqr[3];

    /* find rotated point and direction */
    const double f = 1.0/ScShR.sf;

#define ALPHA(x, y, c, d)	((x)*(x)*(c) + (y)*(y)*(d))

    dprime.x = dot(ScShR.s012, r_dir) * f;
    dprime.y = dot(ScShR.s456, r_dir) * f;
    dprime.z = dot(ScShR.s89a, r_dir) * f;

    /* A vector of unit length in model space (r_dir) changes length
     * in the special unit-tgc space.  This scale factor will restore
     * proper length after hit points are found.
     */
    t_scale = length(dprime);
    if (ZERO(t_scale)) {
	*surfno = (int4){INT_MAX, INT_MAX, INT_MAX, INT_MAX};
	return;
    }
    t_scale = 1.0/t_scale;
    dprime *= t_scale;

    if (NEAR_ZERO(dprime.z, RT_PCOEF_TOL)) {
	dprime.z = 0.0;	/* prevent rootfinder heartburn */
    }

    work = r_pt - V;
    pprime.x = dot(ScShR.s012, work) * f;
    pprime.y = dot(ScShR.s456, work) * f;
    pprime.z = dot(ScShR.s89a, work) * f;

    /* Translating ray origin along direction of ray to closest pt. to
     * origin of solids coordinate system, new ray origin is
     * 'cor_pprime'.
     */
    cor_proj = -dot(pprime, dprime);
    cor_pprime = pprime + cor_proj * dprime;

    /* The TGC is defined in "unit" space, so the parametric distance
     * from one side of the TGC to the other is on the order of 2.
     * Therefore, any vector/point coordinates that are very small
     * here may be considered to be zero, since double precision only
     * has 18 digits of significance.  If these tiny values were left
     * in, then as they get squared (below) they will cause
     * difficulties.
     */
    /* Direction cosines */
    dprime = select(dprime, 0, NEAR_ZERO(dprime, 1e-10));
    /* Position in -1..+1 coordinates */
    cor_pprime = select(cor_pprime, 0, NEAR_ZERO(cor_pprime, 1e-20));

    /* Given a line and the parameters for a standard cone, finds the
     * roots of the equation for that cone and line.  Returns the
     * number of real roots found.
     *
     * Given a line and the cone parameters, finds the equation of the
     * cone in terms of the variable 't'.
     *
     * The equation for the cone is:
     *
     * X**2 * Q**2 + Y**2 * R**2 - R**2 * Q**2 = 0
     *
     * where R = a + ((c - a)/|H'|)*Z
     * Q = b + ((d - b)/|H'|)*Z
     *
     * First, find X, Y, and Z in terms of 't' for this line, then
     * substitute them into the equation above.
     *
     * Express each variable (X, Y, and Z) as a linear equation in
     * 'k', e.g., (dprime.x * k) + cor_pprime.x, and substitute into
     * the cone equation.
     */
    Xsqr[0] = dprime.x * dprime.x;
    Xsqr[1] = 2.0 * dprime.x * cor_pprime.x;
    Xsqr[2] = cor_pprime.x * cor_pprime.x;

    Ysqr[0] = dprime.y * dprime.y;
    Ysqr[1] = 2.0 * dprime.y * cor_pprime.y;
    Ysqr[2] = cor_pprime.y * cor_pprime.y;

    R[0] = dprime.z * CdAm1;
    /* A vector is unitized (tgc_A == 1.0) */
    R[1] = (cor_pprime.z * CdAm1) + 1.0;

    /* (void) rt_poly_mul(&Rsqr, &R, &R); */
    Rsqr[0] = R[0] * R[0];
    Rsqr[1] = R[0] * R[1] * 2;
    Rsqr[2] = R[1] * R[1];

    /* If the eccentricities of the two ellipses are the same, then
     * the cone equation reduces to a much simpler quadratic form.
     * Otherwise it is a (gah!) quartic equation.
     *
     * this can only be done when C0 is not too small! (JRA)
     */
    C0 = Xsqr[0] + Ysqr[0] - Rsqr[0];
    if (AD_CB && !NEAR_ZERO(C0, 1.0e-10)) {
	double C[3];	/* final equation */
	double roots;

	/*
	 * (void) bn_poly_add(&sum, &Xsqr, &Ysqr);
	 * (void) bn_poly_sub(&C, &sum, &Rsqr);
	 */
	C[0] = C0;
	C[1] = Xsqr[1] + Ysqr[1] - Rsqr[1];
	C[2] = Xsqr[2] + Ysqr[2] - Rsqr[2];

	/* Find the real roots the easy way.  C.dgr==2 */
	if ((roots = C[1]*C[1] - 4 * C[0] * C[2]) < 0) {
	    npts = 0;	/* no real roots */
	} else {
	    double f;
	    roots = sqrt(roots);
	    k[0] = (roots - C[1]) * (f = 0.5 / C[0]);
	    hit_type[0] = TGC_NORM_BODY;
	    k[1] = (roots + C[1]) * -f;
	    hit_type[1] = TGC_NORM_BODY;
	    npts = 2;
	}
    } else {
	double C[5];		/* final equation */
	double Q[2], Qsqr[3];
	bn_complex_t val[4];	/* roots of final equation */
	int l;
	int nroots;

	Q[0] = dprime.z * DdBm1;
	/* B vector is unitized (tgc_B == 1.0) */
	Q[1] = (cor_pprime.z * DdBm1) + 1.0;

	/* (void) bn_poly_mul(&Qsqr, &Q, &Q); */
	Qsqr[0] = Q[0] * Q[0];
	Qsqr[1] = Q[0] * Q[1] * 2;
	Qsqr[2] = Q[1] * Q[1];

	/*
	 * (void) bn_poly_mul(&T1, &Qsqr, &Xsqr);
	 * (void) bn_poly_mul(&T2 &Rsqr, &Ysqr);
	 * (void) bn_poly_mul(&T1, &Rsqr, &Qsqr);
	 * (void) bn_poly_add(&sum, &T1, &T2);
	 * (void) bn_poly_sub(&C, &sum, &T3);
	 */
	C[0] = Qsqr[0] * Xsqr[0] +
	    Rsqr[0] * Ysqr[0] -
	    (Rsqr[0] * Qsqr[0]);
	C[1] = Qsqr[0] * Xsqr[1] + Qsqr[1] * Xsqr[0] +
	    Rsqr[0] * Ysqr[1] + Rsqr[1] * Ysqr[0] -
	    (Rsqr[0] * Qsqr[1] + Rsqr[1] * Qsqr[0]);
	C[2] = Qsqr[0] * Xsqr[2] + Qsqr[1] * Xsqr[1] +
	    Qsqr[2] * Xsqr[0] +
	    Rsqr[0] * Ysqr[2] + Rsqr[1] * Ysqr[1] +
	    Rsqr[2] * Ysqr[0] -
	    (Rsqr[0] * Qsqr[2] + Rsqr[1] * Qsqr[1] +
	     Rsqr[2] * Qsqr[0]);
	C[3] = Qsqr[1] * Xsqr[2] + Qsqr[2] * Xsqr[1] +
	    Rsqr[1] * Ysqr[2] + Rsqr[2] * Ysqr[1] -
	    (Rsqr[1] * Qsqr[2] + Rsqr[2] * Qsqr[1]);
	C[4] = Qsqr[2] * Xsqr[2] +
	    Rsqr[2] * Ysqr[2] -
	    (Rsqr[2] * Qsqr[2]);

	/* The equation is 4th order, so we expect 0 to 4 roots */
	nroots = rt_poly_roots(C, 4, val);

	/* bn_pr_roots("roots", val, nroots); */

	/* Only real roots indicate an intersection in real space.
	 *
	 * Look at each root returned; if the imaginary part is zero
	 * or sufficiently close, then use the real part as one value
	 * of 't' for the intersections
	 */
	for (l=0, npts=0; l < nroots; l++) {
	    if (NEAR_ZERO(val[l].im, 1e-2)) {
		hit_type[npts] = TGC_NORM_BODY;
		k[npts++] = val[l].re;
	    }
	}
	/* bu_log("npts rooted is %d; ", npts); */

	/* Here, 'npts' is number of points being returned */
	if (npts != 0 && npts != 2 && npts != 4 && npts > 0) {
	    /* LOG */
	} else if (nroots < 0) {
	    /* LOG */
	}
    }

    /*
     * Reverse above translation by adding distance to all 'k' values.
     */
    for (i = 0; i < npts; ++i) {
	k[i] += cor_proj;
    }

    /* bu_log("npts before elimination is %d; ", npts); */
    /*
     * Eliminate hits beyond the end planes
     */
    i = 0;
    while (i < npts) {
	zval = k[i]*dprime.z + pprime.z;
	/* Height vector is unitized (tgc_sH == 1.0) */
	if (zval >= 1.0 || zval <= 0.0) {
	    int j;
	    /* drop this hit */
	    npts--;
	    for (j=i; j<npts; j++) {
		hit_type[j] = hit_type[j+1];
		k[j] = k[j+1];
	    }
	} else {
	    i++;
	}
    }

    /*
     * Consider intersections with the end ellipses
     */
    /* bu_log("npts before base is %d; ", npts); */
    dir = dot(N, r_dir);
    if (!ZERO(dprime.z) && !NEAR_ZERO(dir, RT_DOT_TOL)) {
	b = (-pprime.z)/dprime.z;
	/* Height vector is unitized (tgc_sH == 1.0) */
	t = (1.0 - pprime.z)/dprime.z;

	/* the top end */
	work = pprime + b * dprime;
	/* A and B vectors are unitized (tgc_A == _B == 1.0) */
	/* alf1 = ALPHA(work.x, work.y, 1.0, 1.0) */
	alf1 = work.x*work.x + work.y*work.y;

	/* the bottom end */
	work = pprime + t * dprime;
	/* Must scale C and D vectors */
	alf2 = ALPHA(work.x, work.y, AAdCC, BBdDD);

	/*
	   bu_log("alf1 is %f, alf2 is %f\n", alf1, alf2);
	   bu_log("work[x]=%f, work[y]=%f, aadcc=%f, bbddd=%f\n", work.x, work.y, tgc_AAdCC, tgc_BBdDD);
	 */
	if (alf1 <= 1.0) {
	    hit_type[npts] = TGC_NORM_BOT;
	    k[npts++] = b;
	}
	if (alf2 <= 1.0) {
	    hit_type[npts] = TGC_NORM_TOP;
	    k[npts++] = t;
	}
    }

    /* bu_log("npts FINAL is %d\n", npts); */

    /* Sort Most distant to least distant: rt_pt_sort(k, npts) */
    {
	double u;
	short lim, n;
	int type;

	for (lim = npts-1; lim > 0; lim--) {
	    for (n = 0; n < lim; n++) {
		if ((u=k[n]) < k[n+1]) {
		    /* bubble larger towards [0] */
		    type = hit_type[n];
		    hit_type[n] = hit_type[n+1];
		    hit_type[n+1] = type;
		    k[n] = k[n+1];
		    k[n+1] = u;
		}
	    }
	}
    }
    /* Now, k[0] > k[npts-1] */

    if (npts%2) {
	/* odd number of hits!!!
	 * perhaps we got two hits on an edge
	 * check for duplicate hit distances
	 */

	for (i=npts-1; i>0; i--) {
	    double diff;

	    diff = k[i-1] - k[i];	/* non-negative due to sorting */
	    if (diff < 0.0/*rti_tol_dist*/) {
		/* remove this duplicate hit */
		int j;

		npts--;
		for (j=i; j<npts; j++) {
		    hit_type[j] = hit_type[j+1];
		    k[j] = k[j+1];
		}

		/* now have even number of hits */
		break;
	    }
	}
    }

    if (npts != 0 && npts != 2 && npts != 4) {
	*surfno = (int4){INT_MAX, INT_MAX, INT_MAX, INT_MAX};
	return;			/* No hit */
    }

    double3 in[2];
    double3 out[2];

    intersect = 0;
    for (i=npts-1; i>0; i -= 2) {
	if (hit_type[i] == TGC_NORM_BODY){
	    in[intersect] = pprime + k[i] * dprime;
	} else {
	    if (dir > 0.0) {
		hit_type[i] = TGC_NORM_BOT;
	    } else {
		hit_type[i] = TGC_NORM_TOP;
	    }
	}
	if (hit_type[i-1] == TGC_NORM_BODY){
	    out[intersect] = pprime + k[i-1] * dprime;
	} else {
	    if (dir > 0.0) {
		hit_type[i-1] = TGC_NORM_TOP;
	    } else {
		hit_type[i-1] = TGC_NORM_BOT;
	    }
	}
	intersect++;
    }

    switch (intersect) {
    case 0:
	*surfno = (int4){INT_MAX, INT_MAX, INT_MAX, INT_MAX};
	return;			/* No hit */
    case 1:
	*surfno = (int4){hit_type[1], hit_type[0], INT_MAX, INT_MAX};
	*dist = (double4){k[1] * t_scale, k[0] * t_scale, 0.0, 0.0};

	*inv = (double8){in[0].x, in[0].y, in[0].z, 0.0, 0.0, 0.0, 0.0, 0.0};
	*outv = (double8){out[0].x, out[0].y, out[0].z, 0.0, 0.0, 0.0, 0.0, 0.0};
    	return;			// HIT
    case 2:
	*surfno = (int4){hit_type[3], hit_type[2], hit_type[1], hit_type[0]};
	*dist = (double4){k[3] * t_scale, k[2] * t_scale, k[1] * t_scale, k[0] * t_scale};

	*inv = (double8){in[0].x, in[0].y, in[0].z, in[1].x, in[1].y, in[1].z, 0.0, 0.0};
	*outv = (double8){out[0].x, out[0].y, out[0].z, out[1].x, out[1].y, out[1].z, 0.0, 0.0};
    	return;			// HIT
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

