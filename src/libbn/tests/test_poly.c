/*                        T E S T _ P O L Y . C
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


static bn_poly_t
poly_from_coeffs(int degree, const double *coeffs)
{
    bn_poly_t p = BN_POLY_INIT_ZERO;
    int i;

    p.dgr = degree;
    for (i = 0; i <= degree; i++) {
	p.cf[i] = coeffs[i];
    }

    return p;
}


static bn_complex_t
cx_mul(const bn_complex_t *a, const bn_complex_t *b)
{
    bn_complex_t out;

    out.re = a->re * b->re - a->im * b->im;
    out.im = a->re * b->im + a->im * b->re;

    return out;
}


static bn_complex_t
cx_add_real(const bn_complex_t *a, double b)
{
    bn_complex_t out = *a;

    out.re += b;
    return out;
}


static bn_complex_t
poly_eval_complex(const bn_poly_t *poly, const bn_complex_t *z)
{
    int i;
    bn_complex_t acc;

    acc.re = poly->cf[0];
    acc.im = 0.0;

    for (i = 1; i <= poly->dgr; i++) {
	acc = cx_mul(&acc, z);
	acc = cx_add_real(&acc, poly->cf[i]);
    }

    return acc;
}


static int
root_is_valid(const bn_poly_t *poly, const bn_complex_t *root, double tol)
{
    bn_complex_t value = poly_eval_complex(poly, root);
    return hypot(value.re, value.im) <= tol;
}


static int
has_real_root(const bn_complex_t *roots, int n, double expected, double tol)
{
    int i;

    for (i = 0; i < n; i++) {
	if (fabs(roots[i].im) <= tol && fabs(roots[i].re - expected) <= tol) {
	    return 1;
	}
    }

    return 0;
}


static int
test_poly_ops(void)
{
    int failures = 0;
    const char *test = "poly_ops";
    const double p1c[] = {1.0, -1.0};
    const double p2c[] = {1.0, -2.0};
    const double cubicc[] = {1.0, -6.0, 11.0, -6.0};
    bn_poly_t p1 = poly_from_coeffs(1, p1c);
    bn_poly_t p2 = poly_from_coeffs(1, p2c);
    bn_poly_t cubic = poly_from_coeffs(3, cubicc);
    bn_poly_t product = BN_POLY_INIT_ZERO;
    bn_poly_t sum = BN_POLY_INIT_ZERO;
    bn_poly_t diff = BN_POLY_INIT_ZERO;
    bn_poly_t quotient = BN_POLY_INIT_ZERO;
    bn_poly_t remainder = BN_POLY_INIT_ZERO;

    bn_poly_mul(&product, &p1, &p2);
    if (product.dgr != 2 ||
	!scalar_close(product.cf[0], 1.0, 0.0) ||
	!scalar_close(product.cf[1], -3.0, 0.0) ||
	!scalar_close(product.cf[2], 2.0, 0.0)) {
	report_failure(test, "bn_poly_mul produced the wrong quadratic product");
	failures++;
    }

    bn_poly_add(&sum, &p1, &p2);
    if (sum.dgr != 1 ||
	!scalar_close(sum.cf[0], 2.0, 0.0) ||
	!scalar_close(sum.cf[1], -3.0, 0.0)) {
	report_failure(test, "bn_poly_add produced the wrong linear sum");
	failures++;
    }

    bn_poly_sub(&diff, &p2, &p1);
    if (diff.dgr != 1 ||
	!scalar_close(diff.cf[0], 0.0, 0.0) ||
	!scalar_close(diff.cf[1], -1.0, 0.0)) {
	report_failure(test, "bn_poly_sub produced the wrong linear difference");
	failures++;
    }

    bn_poly_scale(&product, 0.5);
    if (!scalar_close(product.cf[0], 0.5, 0.0) ||
	!scalar_close(product.cf[1], -1.5, 0.0) ||
	!scalar_close(product.cf[2], 1.0, 0.0)) {
	report_failure(test, "bn_poly_scale produced the wrong scaled coefficients");
	failures++;
    }

    bn_poly_synthetic_division(&quotient, &remainder, &cubic, &p1);
    if (quotient.dgr != 2 ||
	!scalar_close(quotient.cf[0], 1.0, 1.0e-12) ||
	!scalar_close(quotient.cf[1], -5.0, 1.0e-12) ||
	!scalar_close(quotient.cf[2], 6.0, 1.0e-12) ||
	!scalar_close(remainder.cf[0], 0.0, 1.0e-12)) {
	report_failure(test, "bn_poly_synthetic_division produced the wrong quotient or remainder");
	failures++;
    }

    return failures;
}


static int
test_poly_roots(void)
{
    int failures = 0;
    const char *test = "poly_roots";
    const double linearc[] = {0.0, 1.0, -3.0};
    const double constc[] = {0.0, 0.0, 1.0};
    const double quadc[] = {1.0, -5.0, 6.0};
    const double quadic[] = {1.0, 0.0, 1.0};
    const double cubicc[] = {1.0, -6.0, 11.0, -6.0};
    const double cubicmixc[] = {1.0, 0.0, 0.0, 1.0};
    const double quarticc[] = {1.0, 0.0, -10.0, 0.0, 9.0};
    bn_poly_t linear = poly_from_coeffs(2, linearc);
    bn_poly_t constant = poly_from_coeffs(2, constc);
    bn_poly_t quad = poly_from_coeffs(2, quadc);
    bn_poly_t quadic = poly_from_coeffs(2, quadic);
    bn_poly_t cubic = poly_from_coeffs(3, cubicc);
    bn_poly_t cubicmix = poly_from_coeffs(3, cubicmixc);
    bn_poly_t quartic = poly_from_coeffs(4, quarticc);
    bn_complex_t roots2[4];
    bn_complex_t roots3[4];
    bn_complex_t roots4[4];
    int i;

    if (bn_poly_quadratic_roots(roots2, &constant) != 0) {
	report_failure(test, "bn_poly_quadratic_roots should reject a constant polynomial");
	failures++;
    }

    if (!bn_poly_quadratic_roots(roots2, &linear) ||
	!scalar_close(roots2[0].re, 3.0, 1.0e-12) ||
	!scalar_close(roots2[1].re, 3.0, 1.0e-12) ||
	!scalar_close(roots2[0].im, 0.0, 0.0) ||
	!scalar_close(roots2[1].im, 0.0, 0.0)) {
	report_failure(test, "bn_poly_quadratic_roots failed for a degenerate linear polynomial");
	failures++;
    }

    if (!bn_poly_quadratic_roots(roots2, &quad)) {
	report_failure(test, "bn_poly_quadratic_roots failed for a real quadratic");
	failures++;
    } else {
	for (i = 0; i < 2; i++) {
	    if (!root_is_valid(&quad, &roots2[i], 1.0e-10)) {
		report_failure(test, "quadratic root %d did not satisfy the polynomial", i);
		failures++;
	    }
	}
	if (!has_real_root(roots2, 2, 2.0, 1.0e-10) ||
	    !has_real_root(roots2, 2, 3.0, 1.0e-10)) {
	    report_failure(test, "quadratic roots did not contain the expected real solutions");
	    failures++;
	}
    }

    if (!bn_poly_quadratic_roots(roots2, &quadic)) {
	report_failure(test, "bn_poly_quadratic_roots failed for a complex quadratic");
	failures++;
    } else {
	for (i = 0; i < 2; i++) {
	    if (!root_is_valid(&quadic, &roots2[i], 1.0e-10)) {
		report_failure(test, "complex quadratic root %d did not satisfy the polynomial", i);
		failures++;
	    }
	}
    }

    if (!bn_poly_cubic_roots(roots3, &cubic)) {
	report_failure(test, "bn_poly_cubic_roots failed for a real cubic");
	failures++;
    } else {
	for (i = 0; i < 3; i++) {
	    if (!root_is_valid(&cubic, &roots3[i], 1.0e-8)) {
		report_failure(test, "cubic root %d did not satisfy the polynomial", i);
		failures++;
	    }
	}
	if (!has_real_root(roots3, 3, 1.0, 1.0e-8) ||
	    !has_real_root(roots3, 3, 2.0, 1.0e-8) ||
	    !has_real_root(roots3, 3, 3.0, 1.0e-8)) {
	    report_failure(test, "cubic roots did not contain the expected real solutions");
	    failures++;
	}
    }

    if (!bn_poly_cubic_roots(roots3, &cubicmix)) {
	report_failure(test, "bn_poly_cubic_roots failed for a mixed cubic");
	failures++;
    } else {
	for (i = 0; i < 3; i++) {
	    if (!root_is_valid(&cubicmix, &roots3[i], 1.0e-8)) {
		report_failure(test, "mixed cubic root %d did not satisfy the polynomial", i);
		failures++;
	    }
	}
	if (!has_real_root(roots3, 3, -1.0, 1.0e-8)) {
	    report_failure(test, "mixed cubic roots did not contain the expected real solution");
	    failures++;
	}
    }

    if (!bn_poly_quartic_roots(roots4, &quartic)) {
	report_failure(test, "bn_poly_quartic_roots failed for a real quartic");
	failures++;
    } else {
	for (i = 0; i < 4; i++) {
	    if (!root_is_valid(&quartic, &roots4[i], 1.0e-6)) {
		report_failure(test, "quartic root %d did not satisfy the polynomial", i);
		failures++;
	    }
	}
	if (!has_real_root(roots4, 4, -3.0, 1.0e-6) ||
	    !has_real_root(roots4, 4, -1.0, 1.0e-6) ||
	    !has_real_root(roots4, 4, 1.0, 1.0e-6) ||
	    !has_real_root(roots4, 4, 3.0, 1.0e-6)) {
	    report_failure(test, "quartic roots did not contain the expected real solutions");
	    failures++;
	}
    }

    return failures;
}


static const struct bn_api_case poly_cases[] = {
    {"ops", test_poly_ops},
    {"roots", test_poly_roots},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, poly_cases);
}
