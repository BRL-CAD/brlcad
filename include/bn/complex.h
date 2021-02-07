/*                        C O M P L E X . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup bn_complex
 * @brief
 * Complex numbers
 */
/** @{ */
/** @file complex.h */

#ifndef BN_COMPLEX_H
#define BN_COMPLEX_H

#include "common.h"
#include "bn/defines.h"

__BEGIN_DECLS

/** "complex number" data type */
typedef struct bn_complex {
    double re;		/**< @brief real part */
    double im;		/**< @brief imaginary part */
}  bn_complex_t;

/* functions that are efficiently done as macros: */

#define bn_cx_copy(ap, bp) {*(ap) = *(bp);}
#define bn_cx_neg(cp) { (cp)->re = -((cp)->re);(cp)->im = -((cp)->im);}
#define bn_cx_real(cp)		(cp)->re
#define bn_cx_imag(cp)		(cp)->im

#define bn_cx_add(ap, bp) { (ap)->re += (bp)->re; (ap)->im += (bp)->im;}
#define bn_cx_ampl(cp)		hypot((cp)->re, (cp)->im)
#define bn_cx_amplsq(cp)		((cp)->re * (cp)->re + (cp)->im * (cp)->im)
#define bn_cx_conj(cp) { (cp)->im = -(cp)->im; }
#define bn_cx_cons(cp, r, i) { (cp)->re = r; (cp)->im = i; }
#define bn_cx_phas(cp)		atan2((cp)->im, (cp)->re)
#define bn_cx_scal(cp, s) { (cp)->re *= (s); (cp)->im *= (s); }
#define bn_cx_sub(ap, bp) { (ap)->re -= (bp)->re; (ap)->im -= (bp)->im;}

#define bn_cx_mul(ap, bp)	 	\
    { register fastf_t a__re, b__re; \
	(ap)->re = ((a__re=(ap)->re)*(b__re=(bp)->re)) - (ap)->im*(bp)->im; \
	(ap)->im = a__re*(bp)->im + (ap)->im*b__re; }

/* Output variable "ap" is different from input variables "bp" or "cp" */
#define bn_cx_mul2(ap, bp, cp) { \
	(ap)->re = (cp)->re * (bp)->re - (cp)->im * (bp)->im; \
	(ap)->im = (cp)->re * (bp)->im + (cp)->im * (bp)->re; }

/**
 *@brief
 * Divide one complex by another
 *
 * bn_cx_div(&a, &b).  divides a by b.  Zero divisor fails.  a and b
 * may coincide.  Result stored in a.
 */
BN_EXPORT extern void bn_cx_div(bn_complex_t *ap,
				const bn_complex_t *bp);

/**
 *@brief
 * Compute square root of complex number
 *
 * bn_cx_sqrt(&out, &c) replaces out by sqrt(c)
 *
 * Note: This is a double-valued function; the result of bn_cx_sqrt()
 * always has nonnegative imaginary part.
 */
BN_EXPORT extern void bn_cx_sqrt(bn_complex_t *op,
				 const bn_complex_t *ip);

__END_DECLS

#endif  /* BN_COMPLEX_H */
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
