/*                    T E S T _ C O M P L E X . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "common.h"

#include "test_util.h"


static int
complex_close(const bn_complex_t *c, double re, double im, double tol)
{
    return scalar_close(c->re, re, tol) && scalar_close(c->im, im, tol);
}


static int
test_complex_arithmetic(void)
{
    int failures = 0;
    const char *test = "complex_arithmetic";
    bn_complex_t a = {1.5, -2.0};
    bn_complex_t b = {-3.0, 0.5};
    bn_complex_t c = {0.0, 0.0};
    bn_complex_t d = {0.0, 0.0};

    c = a;
    bn_cx_add(&c, &b);
    if (!complex_close(&c, -1.5, -1.5, 0.0)) {
	report_failure(test, "bn_cx_add produced the wrong sum");
	failures++;
    }

    c = a;
    bn_cx_sub(&c, &b);
    if (!complex_close(&c, 4.5, -2.5, 0.0)) {
	report_failure(test, "bn_cx_sub produced the wrong difference");
	failures++;
    }

    c = a;
    bn_cx_mul(&c, &b);
    if (!complex_close(&c, -3.5, 6.75, 1.0e-12)) {
	report_failure(test, "bn_cx_mul produced the wrong product");
	failures++;
    }

    bn_cx_mul2(&d, &a, &b);
    if (!complex_close(&d, -3.5, 6.75, 1.0e-12)) {
	report_failure(test, "bn_cx_mul2 produced the wrong product");
	failures++;
    }

    c = a;
    bn_cx_conj(&c);
    if (!complex_close(&c, 1.5, 2.0, 0.0)) {
	report_failure(test, "bn_cx_conj produced the wrong conjugate");
	failures++;
    }

    c = a;
    bn_cx_neg(&c);
    if (!complex_close(&c, -1.5, 2.0, 0.0)) {
	report_failure(test, "bn_cx_neg produced the wrong negation");
	failures++;
    }

    bn_cx_cons(&c, 1.0, 2.0);
    bn_cx_scal(&c, 0.5);
    if (!complex_close(&c, 0.5, 1.0, 0.0)) {
	report_failure(test, "bn_cx_scal produced the wrong scaled value");
	failures++;
    }

    bn_cx_cons(&c, 3.0, 4.0);
    if (!scalar_close(bn_cx_ampl(&c), 5.0, 1.0e-12) ||
	!scalar_close(bn_cx_amplsq(&c), 25.0, 1.0e-12)) {
	report_failure(test, "bn_cx_ampl or bn_cx_amplsq produced the wrong magnitude");
	failures++;
    }

    bn_cx_cons(&c, 1.0, 1.0);
    if (!scalar_close(bn_cx_phas(&c), M_PI * 0.25, 1.0e-12)) {
	report_failure(test, "bn_cx_phas produced the wrong phase");
	failures++;
    }

    bn_cx_copy(&d, &c);
    if (!scalar_close(bn_cx_real(&d), 1.0, 0.0) ||
	!scalar_close(bn_cx_imag(&d), 1.0, 0.0)) {
	report_failure(test, "bn_cx_copy, bn_cx_real, or bn_cx_imag produced the wrong values");
	failures++;
    }

    return failures;
}


static int
test_complex_division(void)
{
    int failures = 0;
    const char *test = "complex_division";
    bn_complex_t numer = {6.3, 4.2};
    bn_complex_t denom = {9.8, 7.7};
    bn_complex_t quot = numer;
    bn_complex_t prod = {0.0, 0.0};
    bn_complex_t same = {2.5, -0.5};
    bn_complex_t zero = {0.0, 0.0};

    bn_cx_div(&quot, &denom);
    bn_cx_mul2(&prod, &quot, &denom);
    if (!complex_close(&prod, numer.re, numer.im, 1.0e-12)) {
	report_failure(test, "bn_cx_div failed to invert multiplication");
	failures++;
    }

    bn_cx_div(&same, &same);
    if (!complex_close(&same, 1.0, 0.0, 1.0e-12)) {
	report_failure(test, "bn_cx_div failed for in-place self-division");
	failures++;
    }

    numer.re = 1.0;
    numer.im = 1.0;
    bn_cx_div(&numer, &zero);
    if (!complex_close(&numer, 1.0e20, 1.0e20, 0.0)) {
	report_failure(test, "bn_cx_div did not report division by zero as documented");
	failures++;
    }

    return failures;
}


static int
test_complex_sqrt(void)
{
    int failures = 0;
    const char *test = "complex_sqrt";
    bn_complex_t in = {0.0, 0.0};
    bn_complex_t root = {0.0, 0.0};
    bn_complex_t square = {0.0, 0.0};

    bn_cx_cons(&in, 4.0, 0.0);
    bn_cx_sqrt(&root, &in);
    if (!complex_close(&root, 2.0, 0.0, 1.0e-12)) {
	report_failure(test, "bn_cx_sqrt failed for a positive real input");
	failures++;
    }

    bn_cx_cons(&in, -9.0, 0.0);
    bn_cx_sqrt(&root, &in);
    if (!complex_close(&root, 0.0, 3.0, 1.0e-12)) {
	report_failure(test, "bn_cx_sqrt failed for a negative real input");
	failures++;
    }

    bn_cx_cons(&in, 0.0, -4.0);
    bn_cx_sqrt(&root, &in);
    bn_cx_mul2(&square, &root, &root);
    if (root.im < 0.0 || !complex_close(&square, in.re, in.im, 1.0e-12)) {
	report_failure(test, "bn_cx_sqrt failed for a negative imaginary input");
	failures++;
    }

    bn_cx_cons(&in, 6.3, 4.2);
    bn_cx_sqrt(&root, &in);
    bn_cx_mul2(&square, &root, &root);
    if (!complex_close(&square, in.re, in.im, 1.0e-10)) {
	report_failure(test, "bn_cx_sqrt failed to square back to the input");
	failures++;
    }

    return failures;
}


static const struct bn_api_case complex_cases[] = {
    {"arithmetic", test_complex_arithmetic},
    {"division", test_complex_division},
    {"sqrt", test_complex_sqrt},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, complex_cases);
}
