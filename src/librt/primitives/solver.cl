#include "common.cl"


#define bn_cx_add(ap, bp)	{ (ap)->re += (bp)->re; (ap)->im += (bp)->im;}
#define bn_cx_ampl(cp)		hypot((cp)->re, (cp)->im)
#define bn_cx_amplsq(cp)	((cp)->re * (cp)->re + (cp)->im * (cp)->im)
#define bn_cx_conj(cp)		{ (cp)->im = -(cp)->im; }
#define bn_cx_cons(cp, r, i)	{ (cp)->re = r; (cp)->im = i; }
#define bn_cx_scal(cp, s)	{ (cp)->re *= (s); (cp)->im *= (s); }
#define bn_cx_sub(ap, bp)	{ (ap)->re -= (bp)->re; (ap)->im -= (bp)->im;}

#define bn_cx_mul(ap, bp) 	\
    { double a__re, b__re; \
	(ap)->re = ((a__re=(ap)->re)*(b__re=(bp)->re)) - (ap)->im*(bp)->im; \
	(ap)->im = a__re*(bp)->im + (ap)->im*b__re; }

/* Output variable "ap" is different from input variables "bp" or "cp" */
#define bn_cx_mul2(ap, bp, cp) { \
    (ap)->re = (cp)->re * (bp)->re - (cp)->im * (bp)->im; \
    (ap)->im = (cp)->re * (bp)->im + (cp)->im * (bp)->re; }


static void
bn_cx_div(bn_complex_t *ap, const bn_complex_t *bp)
{
    double r, s;
    double ap__re;

    /* Note: classical formula may cause unnecessary overflow */
    ap__re = ap->re;
    r = bp->re;
    s = bp->im;
    if (fabs(r) >= fabs(s)) {
	if (ZERO(r)) {
	    ap->re = ap->im = 1.0e20;		/* "INFINITY" */
	    return;
	} else {
	    r = s / r;			/* <= 1 */
	    s = 1.0 / (bp->re + r * s);
	    ap->re = (ap->re + ap->im * r) * s;
	    ap->im = (ap->im - ap__re * r) * s;
	    return;
	}
    } else {
	if (ZERO(s)) {
	    ap->re = ap->im = 1.0e20;		/* "INFINITY" */
	    return;
	} else {
	    r = r / s;			/* < 1 */
	    s = 1.0 / (s + r * bp->re);
	    ap->re = (ap->re * r + ap->im) * s;
	    ap->im = (ap->im * r - ap__re) * s;
	    return;
	}
    }
}

static void
bn_cx_sqrt(bn_complex_t *op, const bn_complex_t *ip)
{
    const double re = ip->re;
    const double im = ip->im;

    /* special cases are not necessary; they are here for speed */
    if (ZERO(re)) {
	if (ZERO(im)) {
	    op->re = op->im = 0.0;
	} else {
	    op->re = copysign(op->im = sqrt(fabs(im) * 0.5), im);
	}
    } else if (ZERO(im)) {
	if (re > 0.0) {
	    op->re = sqrt(re);
	    op->im = 0.0;
	} else {
	    /* ip->re < 0.0 */
	    op->im = sqrt(-re);
	    op->re = 0.0;
	}
    } else {
	double ampl, temp;

	/* no shortcuts */
	ampl = bn_cx_ampl(ip);
	if ((temp = (ampl - re) * 0.5) < 0.0) {
	    /* This case happens rather often, when the hypot() in
	     * bn_cx_ampl() returns an ampl ever so slightly smaller
	     * than ip->re.  This can easily happen when ip->re ~=
	     * 10**20.  Just ignore the imaginary part.
	     */
	    op->im = 0.0;
	} else {
	    op->im = sqrt(temp);
	}

	if ((temp = (ampl + re) * 0.5) < 0.0) {
	    op->re = 0.0;
	} else {
	    op->re = copysign(sqrt(temp), im);
	}
    }
}


static inline int
bn_poly_quadratic_roots(bn_complex_t *roots, const double *eqn)
{
    double disc, denom;

    if (!NEAR_ZERO(eqn[0], RT_PCOEF_TOL)) {
	disc = eqn[1]*eqn[1] - 4.0 * eqn[0]*eqn[2];
	denom = 0.5 / eqn[0];

	if (disc > 0.0) {
	    double q, r1, r2;

	    disc = sqrt(disc);

	    q = -0.5 * (eqn[1] + copysign(disc, eqn[1]));
	    r1 = q / eqn[0];
	    r2 = eqn[2] / q;

	    roots[0].re = fmin(r1, r2);
	    roots[1].re = fmax(r1, r2);
	    roots[1].im = roots[0].im = 0.0;
	} else if (ZERO(disc)) {
	    roots[1].re = roots[0].re = -eqn[1] * denom;
	    roots[1].im = roots[0].im = 0.0;
	} else {
	    roots[1].re = roots[0].re = -eqn[1] * denom;
	    roots[1].im = -(roots[0].im = sqrt(-disc) * denom);
	}
	return 2;	/* OK */
    } else if (!NEAR_ZERO(eqn[1], RT_PCOEF_TOL)) {
	roots[0].re = -eqn[2]/eqn[1];
	roots[0].im = 0.0;
	return 1;	/* OK */
    } else {
	/* No solution.  Now what? */
	/* bu_log("bn_poly_quadratic_roots(): ERROR, no solution\n"); */
	return 0;
    }
}

static int
bn_poly_cubic_roots(bn_complex_t *roots, const double *eqn)
{
    const double THIRD = 1.0 / 3.0;
    const double TWENTYSEVENTH = 1.0 / 27.0;

    double a, b, c1, c1_3rd, delta;
    int i;

    c1 = eqn[1];
    if (fabs(c1) > SQRT_MAX_FASTF) return 0;	/* FAIL */

    c1_3rd = c1 * THIRD;
    a = eqn[2] - c1*c1_3rd;
    if (fabs(a) > SQRT_MAX_FASTF) return 0;	/* FAIL */
    b = (2.0*c1*c1*c1 - 9.0*c1*eqn[2] + 27.0*eqn[3])*TWENTYSEVENTH;
    if (fabs(b) > SQRT_MAX_FASTF) return 0;	/* FAIL */

    if ((delta = a*a) > SQRT_MAX_FASTF) return 0;	/* FAIL */
    delta = b*b*0.25 + delta*a*TWENTYSEVENTH;

    if (delta > 0.0) {
	double r_delta, A, B;

	r_delta = sqrt(delta);
	A = B = -0.5 * b;
	A += r_delta;
	B -= r_delta;

	A = cbrt(A);
	B = cbrt(B);

	roots[2].re = roots[1].re = -0.5 * (roots[0].re = A + B);

	roots[0].im = 0.0;
	roots[2].im = -(roots[1].im = (A - B)*M_SQRT3*0.5);
    } else if (ZERO(delta)) {
	double b_2;
	b_2 = -0.5 * b;

	roots[0].re = 2.0* cbrt(b_2);
	roots[2].re = roots[1].re = -0.5 * roots[0].re;
	roots[2].im = roots[1].im = roots[0].im = 0.0;
    } else {
	double phi, fact;
	double cs_phi, sn_phi_s3;

	if (a >= 0.0) {
	    fact = 0.0;
	    cs_phi = 1.0;		/* cos(phi); */
	    sn_phi_s3 = 0.0;	/* sin(phi) * M_SQRT3; */
	} else {
	    double f;
	    a *= -THIRD;
	    fact = sqrt(a);
	    if ((f = b * (-0.5) / (a*fact)) >= 1.0) {
		cs_phi = 1.0;		/* cos(phi); */
		sn_phi_s3 = 0.0;	/* sin(phi) * M_SQRT3; */
	    }  else if (f <= -1.0) {
		phi = M_PI_3;
		sn_phi_s3 = sincos(phi, &cs_phi) * M_SQRT3;
	    } else {
		phi = acos(f) * THIRD;
		sn_phi_s3 = sincos(phi, &cs_phi) * M_SQRT3;
	    }
	}

	roots[0].re = 2.0*fact*cs_phi;
	roots[1].re = fact*(sn_phi_s3 - cs_phi);
	roots[2].re = fact*(-sn_phi_s3 - cs_phi);
	roots[2].im = roots[1].im = roots[0].im = 0.0;
    }
    for (i=0; i < 3; ++i)
	roots[i].re -= c1_3rd;

    return 3;		/* OK */
}

static int
bn_poly_quartic_roots(bn_complex_t *roots, const double *eqn)
{
    double cube[4], quad1[3], quad2[3];
    bn_complex_t u[3];
    double U, p, q, q1, q2;
    int nroots;

    /* something considerably larger than squared floating point fuss */
    const double small = 1.0e-8;

#define Max3(a, b, c) (fmax(fmax(a, b), c))

    cube[0] = 1.0;
    cube[1] = -eqn[2];
    cube[2] = eqn[3]*eqn[1] - 4*eqn[4];
    cube[3] = -eqn[3]*eqn[3] - eqn[4]*eqn[1]*eqn[1] + 4*eqn[4]*eqn[2];

    if (!bn_poly_cubic_roots(u, cube)) {
	return 0;		/* FAIL */
    }
    if (!ZERO(u[1].im)) {
	U = u[0].re;
    } else {
	U = Max3(u[0].re, u[1].re, u[2].re);
    }

    p = eqn[1]*eqn[1]*0.25 + U - eqn[2];
    U *= 0.5;
    q = U*U - eqn[4];
    if (p < 0) {
	if (p < -small) {
	    return 0;	/* FAIL */
	}
	p = 0;
    } else {
	p = sqrt(p);
    }
    if (q < 0) {
	if (q < -small) {
	    return 0;	/* FAIL */
	}
	q = 0;
    } else {
	q = sqrt(q);
    }

    quad1[0] = quad2[0] = 1.0;
    quad1[1] = eqn[1]*0.5;
    quad2[1] = quad1[1] + p;
    quad1[1] -= p;

    q1 = U - q;
    q2 = U + q;

    p = quad1[1]*q2 + quad2[1]*q1 - eqn[3];
    if (NEAR_ZERO(p, small)) {
	quad1[2] = q1;
	quad2[2] = q2;
    } else {
	q = quad1[1]*q1 + quad2[1]*q2 - eqn[3];
	if (NEAR_ZERO(q, small)) {
	    quad1[2] = q2;
	    quad2[2] = q1;
	} else {
	    return 0;	/* FAIL */
	}
    }

    nroots = 0;
    nroots += bn_poly_quadratic_roots(&roots[nroots], quad1);
    nroots += bn_poly_quadratic_roots(&roots[nroots], quad2);
    return nroots;	/* SUCCESS */
}

/**
 * Evaluates p(Z) for any Z (real or complex).  In this case, test all
 * "nroots" entries of roots[] to ensure that they are roots (zeros)
 * of this polynomial.
 *
 * Returns -
 * 0 all roots are true zeros
 * 1 at least one "root[]" entry is not a true zero
 *
 * Given an equation of the form
 *
 * p(Z) = a0*Z^n + a1*Z^(n-1) +... an != 0,
 *
 * the function value can be computed using the formula
 *
 * p(Z) = bn,	where
 *
 * b0 = a0,	bi = b(i-1)*Z + ai,	i = 1, 2, ...n
 */
static int
rt_poly_checkroots(double *eqn, uint dgr, bn_complex_t *roots, int nroots)
{
    double er, ei;		/* "epoly" */
    double zr, zi;		/* Z value to evaluate at */
    size_t n;
    int m;

    for (m=0; m < nroots; ++m) {
	/* Select value of Z to evaluate at */
	zr = roots[m].re;
	zi = roots[m].im;

	/* Initialize */
	er = eqn[0];
	/* ei = 0.0; */

	/* n=1 step.  Broken out because ei = 0.0 */
	ei = er*zi;		/* must come before er= */
	er = er*zr + eqn[1];

	/* Remaining steps */
	for (n=2; n <= dgr; ++n) {
	    double tr, ti;	/* temps */
	    tr = er*zr - ei*zi + eqn[n];
	    ti = er*zi + ei*zr;
	    er = tr;
	    ei = ti;
	}
	if (fabs(er) > 1.0e-5 || fabs(ei) > 1.0e-5)
	    return 1;	/* FAIL */
    }
    /* Evaluating polynomial for all Z values gives zero */
    return 0;			/* OK */
}

/**
 * Evaluates p(Z), p'(Z), and p''(Z) for any Z (real or complex).
 * Given an equation of the form:
 *
 * p(Z) = a0*Z^n + a1*Z^(n-1) +... an != 0,
 *
 * the function value and derivatives needed for the iterations can be
 * computed by synthetic division using the formulas
 *
 * p(Z) = bn,    p'(Z) = c(n-1),    p''(Z) = d(n-2),
 *
 * where
 *
 * b0 = a0,	bi = b(i-1)*Z + ai,	i = 1, 2, ...n
 * c0 = b0,	ci = c(i-1)*Z + bi,	i = 1, 2, ...n-1
 * d0 = c0,	di = d(i-1)*Z + ci,	i = 1, 2, ...n-2
 */
static void
rt_poly_eval_w_2derivatives(bn_complex_t *cZ, double *eqn, uint dgr, bn_complex_t *b, bn_complex_t *c, bn_complex_t *d)
/* input */
/* input */
/* outputs */
{
    size_t n;
    int m;

    bn_cx_cons(b, eqn[0], 0.0);
    *c = *b;
    *d = *c;

    for (n=1; (m = dgr - n) >= 0; ++n) {
	bn_cx_mul(b, cZ);
	b->re += eqn[n];
	if (m > 0) {
	    bn_cx_mul(c, cZ);
	    bn_cx_add(c, b);
	}
	if (m > 1) {
	    bn_cx_mul(d, cZ);
	    bn_cx_add(d, c);
	}
    }
}

/**
 * Calculates one root of a polynomial (p(Z)) using Laguerre's
 * method.  This is an iterative technique which has very good global
 * convergence properties.  The formulas for this method are
 *
 *				n * p(Z)
 *	newZ  =  Z - -----------------------  ,
 *			p'(Z) +- sqrt(H(Z))
 *
 * where
 *	H(Z) = (n-1) [ (n-1)(p'(Z))^2 - n p(Z)p''(Z) ],
 *
 * where n is the degree of the polynomial.  The sign in the
 * denominator is chosen so that |newZ - Z| is as small as possible.
 */
static int
rt_poly_findroot(double *eqn, /* polynomial */
		 uint dgr,
		 bn_complex_t *nxZ) /* initial guess for root */
{
    bn_complex_t p0, p1, p2;	/* evaluated polynomial+derivatives */
    bn_complex_t p1_H;		/* p1 - H, temporary */
    bn_complex_t cZ, cH;	/* 'Z' and H(Z) in comment */
    bn_complex_t T;		/* temporary for making H */
    double diff=0.0;		/* test values for convergence */
    double b=0.0;		/* floating temps */
    int n;
    int i;

    p0 = p1 = p2 = (bn_complex_t){{0.0, 0.0}};

    for (i=0; i < 100; i++) {
	cZ = *nxZ;
	rt_poly_eval_w_2derivatives(&cZ, eqn, dgr, &p0, &p1, &p2);

	/* Compute H for Laguerre's method. */
	n = dgr-1;
	bn_cx_mul2(&cH, &p1, &p1);
	bn_cx_scal(&cH, (double)(n*n));
	bn_cx_mul2(&T, &p2, &p0);
	bn_cx_scal(&T, (double)(dgr*n));
	bn_cx_sub(&cH, &T);

	/* Calculate the next iteration for Laguerre's method.  Test
	 * to see whether addition or subtraction gives the larger
	 * denominator for the next 'Z', and use the appropriate value
	 * in the formula.
	 */
	bn_cx_sqrt(&cH, &cH);
	p1_H = p1;
	bn_cx_sub(&p1_H, &cH);
	bn_cx_add(&p1, &cH);		/* p1 <== p1+H */
	bn_cx_scal(&p0, (double)(dgr));
	if (bn_cx_amplsq(&p1_H) > bn_cx_amplsq(&p1)) {
	    bn_cx_div(&p0, &p1_H);
	    bn_cx_sub(nxZ, &p0);
	} else {
	    bn_cx_div(&p0, &p1);
	    bn_cx_sub(nxZ, &p0);
	}

	/* Use proportional convergence test to allow very small roots
	 * and avoid wasting time on large roots.  The original
	 * version used bn_cx_ampl(), which requires a square root.
	 * Using bn_cx_amplsq() saves lots of cycles, but changes loop
	 * termination conditions somewhat.
	 *
	 * diff is |p0|**2.  nxZ = Z - p0.
	 */
	b = bn_cx_amplsq(nxZ);
	diff = bn_cx_amplsq(&p0);

	if (b < diff)
	    continue;

	if (ZERO(diff))
	    return i; /* OK -- can't do better */

	/* FIXME: figure out why SMALL_FASTF is too sensitive, why
	 * anything smaller than 1.0e-5 is too sensitive and causes
	 * eto off-by-many differences.
	 */
	if (diff > (b - diff) * 1.0e-5)
	    continue;

	return i;			/* OK */
    }

    /* If the thing hasn't converged yet, it probably won't. */
    return -1;		/* ERROR */
}

static void
bn_poly_synthetic_division(double *quo, uint *quo_dgr, double *rem, uint *rem_dgr, const double *dvdend, uint *dvdend_dgr, const double *dvsor, uint *dvsor_dgr)
{
    size_t divisor;
    size_t n;

    *quo = *dvdend;
    *quo_dgr = *dvdend_dgr;
    *rem = 0.0;
    *rem_dgr = 0;

    if (*dvsor_dgr > *dvdend_dgr) {
	*quo_dgr = -1;
    } else {
	*quo_dgr = *dvdend_dgr - *dvsor_dgr;
    }
    if ((*rem_dgr = *dvsor_dgr - 1) > *dvdend_dgr)
	*rem_dgr = *dvdend_dgr;

    for (n=0; n <= *quo_dgr; ++n) {
	quo[n] /= dvsor[0];
	for (divisor=1; divisor <= *dvsor_dgr; ++divisor) {
	    quo[n+divisor] -= quo[n] * dvsor[divisor];
	}
    }
    for (n=1; n<=(*rem_dgr+1); ++n) {
	rem[n-1] = quo[*quo_dgr+n];
	quo[*quo_dgr+n] = 0;
    }
}

/**
 * Deflates a polynomial by a given root.
 */
static void
rt_poly_deflate(double *oldP, uint *oldP_dgr, bn_complex_t *root)
{
    double divisor[3] = {0.0, 0.0, 0.0};
    uint divisor_dgr = 0;
    double rem[7] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    uint rem_dgr = 0;

    /* Make a polynomial out of the given root:  Linear for a real
     * root, Quadratic for a complex root (since they come in con-
     * jugate pairs).
     */
    if (ZERO(root->im)) {
	/* root is real */
	divisor_dgr = 1;
	divisor[0] = 1;
	divisor[1] = - root->re;
    } else {
	/* root is complex */
	divisor_dgr = 2;
	divisor[0] = 1;
	divisor[1] = -2 * root->re;
	divisor[2] = bn_cx_amplsq(root);
    }

    /* Use synthetic division to find the quotient (new polynomial)
     * and the remainder (should be zero if the root was really a
     * root).
     */
    bn_poly_synthetic_division(oldP, oldP_dgr, rem, &rem_dgr, oldP, oldP_dgr, divisor, &divisor_dgr);
}

static inline double*
bn_poly_scale(double *eqn, uint dgr, double factor)
{
    for (uint cnt=0; cnt <= dgr; ++cnt) {
	eqn[cnt] *= factor;
    }
    return eqn;
}

int
rt_poly_roots(double *eqn,	/* equation to be solved */
uint dgr,
bn_complex_t *roots)		/* space to put roots found */
{
    size_t n;		/* number of roots found */
    double factor;	/* scaling factor for copy */

    /* Remove leading coefficients which are too close to zero,
     * to prevent the polynomial factoring from blowing up, below.
     */
    while (ZERO(eqn[0])) {
	for (n=0; n <= dgr; n++) {
	    eqn[n] = eqn[n+1];
	}
	if (--dgr <= 0)
	    return 0;
    }

    /* Factor the polynomial so the first coefficient is one
     * for ease of handling.
     */
    factor = 1.0 / eqn[0];
    (void)bn_poly_scale(eqn, dgr, factor);
    n = 0;		/* Number of roots found */

    /* A trailing coefficient of zero indicates that zero
     * is a root of the equation.
     */
    while (ZERO(eqn[dgr])) {
	roots[n].re = roots[n].im = 0.0;
	--dgr;
	++n;
    }

    while (dgr > 2) {
	if (dgr == 4) {
	    if (bn_poly_quartic_roots(&roots[n], eqn)) {
		if (rt_poly_checkroots(eqn, dgr, &roots[n], 4) == 0) {
		    return n+4;
		}
	    }
	} else if (dgr == 3) {
	    if (bn_poly_cubic_roots(&roots[n], eqn)) {
		if (rt_poly_checkroots(eqn, dgr, &roots[n], 3) == 0) {
		    return n+3;
		}
	    }
	}

	/*
	 * Set initial guess for root to almost zero.
	 * This method requires a small nudge off the real axis.
	 */
	bn_cx_cons(&roots[n], 0.0, SQRT_SMALL_FASTF);			// <-- SMALL
	if ((rt_poly_findroot(eqn, dgr, &roots[n])) < 0)
	    return n;	/* return those we found, anyways */

	if (fabs(roots[n].im) > 1.0e-5* fabs(roots[n].re)) {
	    /* If root is complex, its complex conjugate is
	     * also a root since complex roots come in con-
	     * jugate pairs when all coefficients are real.
	     */
	    ++n;
	    roots[n] = roots[n-1];
	    bn_cx_conj(&roots[n]);
	} else {
	    /* Change 'practically real' to real */
	    roots[n].im = 0.0;
	}

	rt_poly_deflate(eqn, &dgr, &roots[n]);
	++n;
    }

    /* For polynomials of lower degree, iterative techniques
     * are an inefficient way to find the roots.
     */
    if (dgr == 1) {
	roots[n].re = -(eqn[1]);
	roots[n].im = 0.0;
	++n;
    } else if (dgr == 2) {
	bn_poly_quadratic_roots(&roots[n], eqn);
	n += 2;
    }
    return n;
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

