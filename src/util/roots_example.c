/*                    R O O T S _ E X A M P L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file roots_example.c
 *
 *  Simple example of how to use libbn for solving the roots to an
 *  arbitrary polynomail.
 *
 *  Author -
 *	Christopher Sean Morrison
 *
 *  cc -I/usr/brlcad/include/brlcad -L/usr/brlcad/lib -o roots_example roots_example.c -lrt -lbn -lbu
 */
#include "common.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"

int
main(int argc, char *argv[])
{
    bn_poly_t equation; /* holds our polynomial equation */
    bn_complex_t roots[4]; /* stash up to four roots */
    int num_roots;

    /*********************************************
     * Linear polynomial (1st degree equation):
     *  A*X + B = 0 
     * [0]   [1]     <=coefficients
     */
    equation.dgr = 1;
    equation.cf[0] = 1;  /* A */
    equation.cf[1] = -2;  /* B */

    /* print the equation */
    bu_log("\n*** LINEAR ***\n");
    bn_pr_poly("Solving for Linear", &equation);

    /* solve for the roots */
    num_roots = rt_poly_roots(&equation, roots, "My Linear Polynomial");
    if (num_roots == 0) {
	bu_log("No roots found!\n");
	return 0;
    } else if (num_roots < 0) {
	bu_log("The root solver failed to converge on a solution\n");
	return 1;
    }

    /*  A*X + B = 0 
     *  1*X + -2 = 0
     *    X -  2 = 0
     *    X      = 2
     */ 
    /* print the roots */
    bu_log("The root should be 2\n");
    bn_pr_roots("My Linear Polynomial", roots, num_roots);



    /*********************************************
     * Quadratic polynomial (2nd degree equation):
     *  A*X^2 + B*X + C = 0 
     * [0]     [1]   [2]     <=coefficients
     */
    equation.dgr = 2;
    equation.cf[0] = 1;  /* A */
    equation.cf[1] = 0;  /* B */
    equation.cf[2] = -4; /* C */

    /* print the equation */
    bu_log("\n*** QUADRATIC ***\n");
    bn_pr_poly("Solving for Quadratic", &equation);

    /* solve for the roots */
    num_roots = rt_poly_roots(&equation, roots, "My Quadratic Polynomial");
    if (num_roots == 0) {
	bu_log("No roots found!\n");
	return 0;
    } else if (num_roots < 0) {
	bu_log("The root solver failed to converge on a solution\n");
	return 1;
    }

    /*  A*X^2 + B*X +  C = 0 
     *  1*X^2 + 0*X + -4 = 0
     *    X^2       -  4 = 0
     * (X - 2) * (X + 2) = 0
     *  X - 2 = 0, X + 2 = 0
     *  X = 2, X = -2
     */ 
    /* print the roots */
    bu_log("The roots should be 2 and -2\n");
    bn_pr_roots("My Quadratic Polynomial", roots, num_roots);



    /*****************************************
     * Cubic polynomial (3rd degree equation):
     *  A*X^3 + B*X^2 + C*X + D = 0
     * [0]     [1]     [2]   [3]     <=coefficients
     */
    equation.dgr = 3;
    equation.cf[0] = 45;
    equation.cf[1] = 24;
    equation.cf[2] = -7;
    equation.cf[3] = -2;

    /* print the equation */
    bu_log("\n*** CUBIC ***\n");
    bn_pr_poly("Solving for Cubic", &equation);

    /* solve for the roots */
    num_roots = rt_poly_roots(&equation, roots, "My Cubic Polynomial");
    if (num_roots == 0) {
	bu_log("No roots found!\n");
	return 0;
    } else if (num_roots < 0) {
	bu_log("The root solver failed to converge on a solution\n");
	return 1;
    }

    /* print the roots */
    bu_log("The roots should be 1/3, -1/5, and -2/3\n");
    bn_pr_roots("My Cubic Polynomial", roots, num_roots);
 
    
    /*******************************************
     * Quartic polynomial (4th degree equation):
     *  A*X^4 + B*X^3 + C*X^2 + D*X + E = 0
     * [0]     [1]     [2]     [3]   [4]     <=coefficients
     */
    equation.dgr = 4;
    equation.cf[0] = 2;
    equation.cf[1] = 4;
    equation.cf[2] = -26;
    equation.cf[3] = -28;
    equation.cf[4] = 48;

    /* print the equation */
    bu_log("\n*** QUARTIC ***\n");
    bn_pr_poly("Solving for Quartic", &equation);

    /* solve for the roots */
    num_roots = rt_poly_roots(&equation, roots, "My Quartic Polynomial");
    if (num_roots == 0) {
	bu_log("No roots found!\n");
	return 0;
    } else if (num_roots < 0) {
	bu_log("The root solver failed to converge on a solution\n");
	return 1;
    }

    /* print the roots */
    bu_log("The roots should be 3, 1, -2, -4\n");
    bn_pr_roots("My Quartic Polynomial", roots, num_roots);
 
    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
