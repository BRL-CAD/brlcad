/*                         R O O T S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2010 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file roots.c
 *
 * Find the roots of a polynomial
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"


static const bn_poly_t bn_Zero_poly = { BN_POLY_MAGIC, 0, {0.0} };


/**
 * R T _ P O L Y _ E V A L _ W _ 2 D E R I V A T I V E S
 *
 * Reverses the order of coefficients to conform to new convention
 * Subsequently calls bn_poly_eval_w_2derivatives located in libbn/poly.c
 * which in effect performs the following.
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
void
rt_poly_eval_w_2derivatives(register bn_complex_t *cZ, register bn_poly_t *eqn, register bn_complex_t *b, register bn_complex_t *c, register bn_complex_t *d)
/* input */
/* input */
/* outputs */
{
    register int n;
    bn_poly_t equation;
    equation.dgr = eqn->dgr;
    for (n=0; n<=eqn->dgr; n++) {
	equation.cf[n] = eqn->cf[eqn->dgr - n];
    }
    bn_poly_eval_w_2derivatives(cZ, &equation, b, c, d);
}


/**
 * R T _ P O L Y _ F I N D R O O T
 *
 * Calls bn_poly_findroot located in libbn/poly.c
 * which in effect performs the following.
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
int
rt_poly_findroot(register bn_poly_t *eqn, /* polynomial */
		 register bn_complex_t *nxZ, /* initial guess for root */
		 const char *str)
{
    register int n;
    n = bn_poly_findroot(eqn, nxZ, str);
    return n;
}


/**
 * R T _ P O L Y _ C H E C K R O O T S
 *
 * Reverses the order of coefficients to conform to new convention
 * Calls bn_poly_ceckroots located in libbn/poly.c
 * which in effect performs the following.
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
int
rt_poly_checkroots(register bn_poly_t *eqn, bn_complex_t *roots, register int nroots)
{
    int n;
    bn_poly_t equation;
    equation.dgr = eqn->dgr;
    for (n=0; n<=eqn->dgr/2; n++) {
	equation.cf[n] = eqn->cf[eqn->dgr - n];
    }
    n = bn_poly_checkroots(&equation, roots, nroots);
    return n;
}


/**
 * R T _ P O L Y _ D E F L A T E
 *
 * Calls bn_poly_deflate located in libbn/poly.c
 * Deflates a polynomial by a given root.
 */
void
rt_poly_deflate(register bn_poly_t *oldP, register bn_complex_t *root)
{
    bn_poly_deflate(oldP, root);
}


/**
 * R T _ P O L Y _ R O O T S
 *
 * Reverses the order of coefficients to conform to new convention
 * Calls bn_poly_roots located in libbn/poly.c
 * NOTE : This routine is written for polynomials with real
 * coefficients ONLY.  To use with complex coefficients, the Complex
 * Math library should be used throughout.  Some changes in the
 * algorithm will also be required.
 */
int
rt_poly_roots(register bn_poly_t *eqn,	/* equation to be solved */
	      register bn_complex_t roots[], /* space to put roots found */
	      const char *name) /* name of the primitive being checked */
{
    register int n;		/* number of roots found */
    bn_poly_t equation;
    equation.dgr = eqn->dgr;
    for (n=0; n<=eqn->dgr; n++) {
	equation.cf[n] = eqn->cf[eqn->dgr - n];
    }
    n = bn_poly_roots(&equation, roots, name);
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
