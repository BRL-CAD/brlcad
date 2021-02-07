/*                        P O L Y . H
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

/** @addtogroup bn_poly
 *
 *  @brief Library for dealing with polynomials.
 */
/** @{ */
/** @file poly.h */

#ifndef BN_POLY_H
#define BN_POLY_H

#include "common.h"

#include "vmath.h"

#include "bu/magic.h"
#include "bn/defines.h"
#include "bn/complex.h"

__BEGIN_DECLS

/* This could be larger, or even dynamic... */
#define BN_MAX_POLY_DEGREE 6	/**< Maximum Poly Order */

/**
 * Polynomial data type
 * bn_poly->cf[n] corresponds to X^n
 */
typedef struct bn_poly {
    uint32_t magic;
    size_t dgr;
    fastf_t cf[BN_MAX_POLY_DEGREE+1];
}  bn_poly_t;

#define BN_CK_POLY(_p) BU_CKMAG(_p, BN_POLY_MAGIC, "struct bn_poly")
#define BN_POLY_NULL   ((struct bn_poly *)NULL)
#define BN_POLY_INIT_ZERO { BN_POLY_MAGIC, 0, {0.0} }


/**
 * bn_poly_mul
 *
 * @brief
 * multiply two polynomials
 */
BN_EXPORT extern struct bn_poly *bn_poly_mul(struct bn_poly *product,
					     const struct bn_poly *m1,
					     const struct bn_poly *m2);

/**
 * bn_poly_scale
 *
 * @brief
 * scale a polynomial
 */
BN_EXPORT extern struct bn_poly *bn_poly_scale(struct bn_poly *eqn,
					       double factor);

/**
 * bn_poly_add
 * @brief
 * add two polynomials
 */
BN_EXPORT extern struct bn_poly *bn_poly_add(struct bn_poly *sum,
					     const struct bn_poly *poly1,
					     const struct bn_poly *poly2);

/**
 * bn_poly_sub
 * @brief
 * subtract two polynomials
 */
BN_EXPORT extern struct bn_poly *bn_poly_sub(struct bn_poly *diff,
					     const struct bn_poly *poly1,
					     const struct bn_poly *poly2);

/**
 * @brief
 * Divides any polynomial into any other polynomial using synthetic
 * division.  Both polynomials must have real coefficients.
 */
BN_EXPORT extern void bn_poly_synthetic_division(struct bn_poly *quo,
						 struct bn_poly *rem,
						 const struct bn_poly *dvdend,
						 const struct bn_poly *dvsor);

/**
 *@brief
 * Uses the quadratic formula to find the roots (in `complex' form) of
 * any quadratic equation with real coefficients.
 *
 *	@return 1 for success
 *	@return 0 for fail.
 */
BN_EXPORT extern int bn_poly_quadratic_roots(struct bn_complex roots[],
					     const struct bn_poly *quadrat);

/**
 *@brief
 * Uses the cubic formula to find the roots (in `complex' form)
 * of any cubic equation with real coefficients.
 *
 * to solve a polynomial of the form:
 *
 * X**3 + c1*X**2 + c2*X + c3 = 0,
 *
 * first reduce it to the form:
 *
 * Y**3 + a*Y + b = 0,
 *
 * where
 * Y = X + c1/3,
 * and
 * a = c2 - c1**2/3,
 * b = (2*c1**3 - 9*c1*c2 + 27*c3)/27.
 *
 * Then we define the value delta,   D = b**2/4 + a**3/27.
 *
 * If D > 0, there will be one real root and two conjugate
 * complex roots.
 * If D = 0, there will be three real roots at least two of
 * which are equal.
 * If D < 0, there will be three unequal real roots.
 *
 * Returns 1 for success, 0 for fail.
 */
BN_EXPORT extern int bn_poly_cubic_roots(struct bn_complex roots[],
					 const struct bn_poly *eqn);

/**
 *@brief
 * Uses the quartic formula to find the roots (in `complex' form)
 * of any quartic equation with real coefficients.
 *
 *	@return 1 for success
 *	@return 0 for fail.
 */
BN_EXPORT extern int bn_poly_quartic_roots(struct bn_complex roots[],
					   const struct bn_poly *eqn);

BN_EXPORT extern int bn_poly_findroot(bn_poly_t *eqn,
				      bn_complex_t *nxZ,
				      const char *str);

BN_EXPORT extern void bn_poly_eval_w_2derivatives(bn_complex_t *cZ,
						  bn_poly_t *eqn,
						  bn_complex_t *b,
						  bn_complex_t *c,
						  bn_complex_t *d);

BN_EXPORT extern int bn_poly_checkroots(bn_poly_t *eqn,
					bn_complex_t *roots,
					int nroots);

BN_EXPORT extern void bn_poly_deflate(bn_poly_t *oldP,
				      bn_complex_t *root);

BN_EXPORT extern int bn_poly_roots(bn_poly_t *eqn,
				   bn_complex_t roots[],
				   const char *name);

/**
 * Print out the polynomial.
 */
BN_EXPORT extern void bn_pr_poly(const char *title,
				 const struct bn_poly *eqn);

/**
 * Print out the roots of a given polynomial (complex numbers)
 */
BN_EXPORT extern void bn_pr_roots(const char *title,
				  const struct bn_complex roots[],
				  int n);

__END_DECLS

#endif  /* BN_POLY_H */
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
