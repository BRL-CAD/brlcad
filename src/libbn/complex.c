/*                       C O M P L E X . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup complex */
/** @{ */
/** @file complex.c
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "bu.h"
#include "vmath.h"
#include "bn.h"


/* arbitrary numerical arguments, integer value: */
#define SIGN(x)	((long)(x) == 0L ? 0 : (long)(x) > 0L ? 1 : -1)
#define ABS(a)	((long)(a) >= 0L ? (a) : -(a))


/**
 * B N _ C X _ D I V
 *@brief
 * Divide one complex by another
 *
 * bn_cx_div(&a, &b).  divides a by b.  Zero divisor fails.  a and b
 * may coincide.  Result stored in a.
 */
void
bn_cx_div(register bn_complex_t *ap, register const bn_complex_t *bp)
{
    register fastf_t r, s;
    register fastf_t ap__re;

    /* Note: classical formula may cause unnecessary overflow */
    ap__re = ap->re;
    r = bp->re;
    s = bp->im;
    if (ABS(r) >= ABS(s)) {
	if (NEAR_ZERO(r, SQRT_SMALL_FASTF))
	    goto err;
	r = s / r;			/* <= 1 */
	s = 1.0 / (bp->re + r * s);
	ap->re = (ap->re + ap->im * r) * s;
	ap->im = (ap->im - ap__re * r) * s;
	return;
    }  else  {
	/* ABS(s) > ABS(r) */
	if (NEAR_ZERO(s, SQRT_SMALL_FASTF))
	    goto err;
	r = r / s;			/* < 1 */
	s = 1.0 / (s + r * bp->re);
	ap->re = (ap->re * r + ap->im) * s;
	ap->im = (ap->im * r - ap__re) * s;
	return;
    }
 err:
    bu_log("bn_cx_div: division by zero: %gR+%gI / %gR+%gI\n",
	   ap->re, ap->im, bp->re, bp->im);
    ap->re = ap->im = 1.0e20;		/* "INFINITY" */
}

/**
 * B N _ C X _ S Q R T
 *@brief
 * Compute square root of complex number
 *
 * bn_cx_sqrt(&out, &c)	replaces out by sqrt(c)
 *
 * Note: This is a double-valued function; the result of bn_cx_sqrt()
 * always has nonnegative imaginary part.
 */
void
bn_cx_sqrt(bn_complex_t *op, register const bn_complex_t *ip)
{
    register fastf_t ampl, temp;
    /* record signs of original real & imaginary parts */
    register int re_sign;
    register int im_sign;

    /* special cases are not necessary; they are here for speed */
    im_sign = SIGN(ip->im);
    re_sign = SIGN(ip->re);
    if (re_sign == 0) {
	if (im_sign == 0)
	    op->re = op->im = 0;
	else if (im_sign > 0)
	    op->re = op->im = sqrt(ip->im * 0.5);
	else			/* im_sign < 0 */
	    op->re = -(op->im = sqrt(ip->im * -0.5));
    } else if (im_sign == 0) {
	if (re_sign > 0) {
	    op->re = sqrt(ip->re);
	    op->im = 0.0;
	}  else  {
	    /* re_sign < 0 */
	    op->im = sqrt(-ip->re);
	    op->re = 0.0;
	}
    }  else  {
	/* no shortcuts */
	ampl = bn_cx_ampl(ip);
	if ((temp = (ampl - ip->re) * 0.5) < 0.0) {
	    /* This case happens rather often, when the hypot() in
	     * bn_cx_ampl() returns an ampl ever so slightly smaller
	     * than ip->re.  This can easily happen when ip->re ~=
	     * 10**20.  Just ignore the imaginary part.
	     */
	    op->im = 0;
	} else
	    op->im = sqrt(temp);

	if ((temp = (ampl + ip->re) * 0.5) < 0.0) {
	    op->re = 0.0;
	} else {
	    if (im_sign > 0)
		op->re = sqrt(temp);
	    else			/* im_sign < 0 */
		op->re = -sqrt(temp);
	}
    }
}

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
