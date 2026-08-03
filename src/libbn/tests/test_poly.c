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
has_complex_root(const bn_complex_t *roots, int n, double expected_re, double expected_im, double tol)
{
    int i;

    for (i = 0; i < n; i++) {
	if (fabs(roots[i].re - expected_re) <= tol &&
	    fabs(roots[i].im - expected_im) <= tol) {
	    return 1;
	}
    }

    return 0;
}


static int
poly_close(const bn_poly_t *poly, int degree, const double *coeffs, double tol)
{
    int i;

    if ((int)poly->dgr != degree) {
	return 0;
    }

    for (i = 0; i <= degree; i++) {
	if (!scalar_close(poly->cf[i], coeffs[i], tol)) {
	    return 0;
	}
    }

    return 1;
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
test_poly_legacy(void)
{
    int failures = 0;
    const char *test = "poly_legacy";
    const double zero2c[] = {0.0, 0.0, 0.0};
    const double zero4c[] = {0.0, 0.0, 0.0, 0.0, 0.0};
    const double neg2c[] = {-4853.0, -324.0, -275.0};
    const double pos2c[] = {61685316.0, 33552288.0, 27339096.0};
    const double mulnegc[] = {-4.0, -3.0, -2.0};
    const double mulposc[] = {7854.0, 2136.0, 1450.0};
    const double add_neg_expected[] = {-9706.0, -648.0, -550.0};
    const double add_pos_expected[] = {123370632.0, 67104576.0, 54678192.0};
    const double sub_pos_expected[] = {61690169.0, 33552612.0, 27339371.0};
    const double scale_neg_expected[] = {-8000.0, -6000.0, -4000.0};
    const double scale_pos_expected[] = {-3141600.0, -854400.0, -580000.0};
    const double mul_neg_expected[] = {16.0, 24.0, 25.0, 12.0, 4.0};
    const double mul_pos_expected[] = {61685316.0, 33552288.0, 27339096.0, 6194400.0, 2102500.0};
    const double divisorc[] = {-4.0, -3.0, -2.0, -38.0};
    const double dividendc[] = {5478.0, 5485.0, 458.0, 258564.0, 54785.0};
    const double quo_expected[] = {-1369.5, -344.125};
    const double rem_expected[] = {-3313.375, 205834.75, 41708.25};
    bn_poly_t zero2 = poly_from_coeffs(2, zero2c);
    bn_poly_t neg2 = poly_from_coeffs(2, neg2c);
    bn_poly_t pos2 = poly_from_coeffs(2, pos2c);
    bn_poly_t mulneg = poly_from_coeffs(2, mulnegc);
    bn_poly_t mulpos = poly_from_coeffs(2, mulposc);
    bn_poly_t divisor = poly_from_coeffs(3, divisorc);
    bn_poly_t dividend = poly_from_coeffs(4, dividendc);
    bn_poly_t out = BN_POLY_INIT_ZERO;
    bn_poly_t quotient = BN_POLY_INIT_ZERO;
    bn_poly_t remainder = BN_POLY_INIT_ZERO;

    bn_poly_add(&out, &zero2, &zero2);
    if (!poly_close(&out, 2, zero2c, 0.0)) {
	report_failure(test, "legacy zero polynomial add case failed");
	failures++;
    }

    bn_poly_add(&out, &neg2, &neg2);
    if (!poly_close(&out, 2, add_neg_expected, 0.0)) {
	report_failure(test, "legacy negative polynomial add case failed");
	failures++;
    }

    bn_poly_add(&out, &pos2, &pos2);
    if (!poly_close(&out, 2, add_pos_expected, 0.0)) {
	report_failure(test, "legacy positive polynomial add case failed");
	failures++;
    }

    bn_poly_sub(&out, &zero2, &zero2);
    if (!poly_close(&out, 2, zero2c, 0.0)) {
	report_failure(test, "legacy zero polynomial subtract case failed");
	failures++;
    }

    bn_poly_sub(&out, &neg2, &zero2);
    if (!poly_close(&out, 2, neg2c, 0.0)) {
	report_failure(test, "legacy negative polynomial subtract case failed");
	failures++;
    }

    bn_poly_sub(&out, &pos2, &neg2);
    if (!poly_close(&out, 2, sub_pos_expected, 0.0)) {
	report_failure(test, "legacy positive polynomial subtract case failed");
	failures++;
    }

    out = zero2;
    bn_poly_scale(&out, 0.0);
    if (!poly_close(&out, 2, zero2c, 0.0)) {
	report_failure(test, "legacy zero polynomial scale case failed");
	failures++;
    }

    out = neg2;
    bn_poly_scale(&out, 2000.0);
    if (!poly_close(&out, 2, scale_neg_expected, 0.0)) {
	report_failure(test, "legacy negative polynomial scale case failed");
	failures++;
    }

    out = mulpos;
    bn_poly_scale(&out, -400.0);
    if (!poly_close(&out, 2, scale_pos_expected, 0.0)) {
	report_failure(test, "legacy positive polynomial scale case failed");
	failures++;
    }

    bn_poly_mul(&out, &zero2, &zero2);
    if (!poly_close(&out, 4, zero4c, 0.0)) {
	report_failure(test, "legacy zero polynomial multiply case failed");
	failures++;
    }

    bn_poly_mul(&out, &mulneg, &mulneg);
    if (!poly_close(&out, 4, mul_neg_expected, 0.0)) {
	report_failure(test, "legacy negative polynomial multiply case failed");
	failures++;
    }

    bn_poly_mul(&out, &mulpos, &mulpos);
    if (!poly_close(&out, 4, mul_pos_expected, 0.0)) {
	report_failure(test, "legacy positive polynomial multiply case failed");
	failures++;
    }

    bn_poly_synthetic_division(&quotient, &remainder, &dividend, &divisor);
    if (!poly_close(&quotient, 1, quo_expected, 1.0e-12) ||
	!poly_close(&remainder, 2, rem_expected, 1.0e-12)) {
	report_failure(test, "legacy synthetic division case failed");
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
    const double cubicnegc[] = {-4.0, -3.0, -2.0, -25.0};
    const double cubicposc[] = {5478.0, 5485.0, 458.0, 786.0};
    const double quarticc[] = {1.0, 0.0, -10.0, 0.0, 9.0};
    const double quarticnegc[] = {-4.0, -3.0, -2.0, -25.0, -38.0};
    const double quarticposc[] = {5478.0, 5485.0, 458.0, 258564.0, 54785.0};
    static const bn_complex_t cubic_neg_expected[] = {
	{-0.49359, 0.0},
	{0.20679876865588492, 0.5304573452575734},
	{0.20679876865588492, -0.5304573452575734}
    };
    static const bn_complex_t cubic_pos_expected[] = {
	{-0.9509931181746001, 0.0},
	{0.18414795857839417, 2.700871695081346},
	{0.18414795857839417, -2.700871695081346}
    };
    static const bn_complex_t quartic_neg_expected[] = {
	{0.2613656082942032, 0.4284631324677022},
	{0.2613656082942032, -0.4284631324677022},
	{-0.5903129767152558, 0.263475942656035},
	{-0.5903129767152558, -0.263475942656035}
    };
    static const bn_complex_t quartic_pos_expected[] = {
	{0.12889648467110737, 0.25711127015404556},
	{0.12889648467110737, -0.25711127015404556},
	{-0.25602234520349354, 0.0},
	{-4.721383656903164, 0.0}
    };
    bn_poly_t linear = poly_from_coeffs(2, linearc);
    bn_poly_t constant = poly_from_coeffs(2, constc);
    bn_poly_t quad = poly_from_coeffs(2, quadc);
    bn_poly_t quadic = poly_from_coeffs(2, quadic);
    bn_poly_t cubic = poly_from_coeffs(3, cubicc);
    bn_poly_t cubicmix = poly_from_coeffs(3, cubicmixc);
    bn_poly_t cubicneg = poly_from_coeffs(3, cubicnegc);
    bn_poly_t cubicpos = poly_from_coeffs(3, cubicposc);
    bn_poly_t quartic = poly_from_coeffs(4, quarticc);
    bn_poly_t quarticneg = poly_from_coeffs(4, quarticnegc);
    bn_poly_t quarticpos = poly_from_coeffs(4, quarticposc);
    bn_complex_t roots2[4];
    bn_complex_t roots3[4];
    bn_complex_t roots4[4];
    const double legacy_tol = 1.0e-5;
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

    if (!bn_poly_cubic_roots(roots3, &cubicneg)) {
	report_failure(test, "bn_poly_cubic_roots failed for the legacy negative cubic");
	failures++;
    } else {
	for (i = 0; i < 3; i++) {
	    if (!root_is_valid(&cubicneg, &roots3[i], 1.0e-7)) {
		report_failure(test, "legacy negative cubic root %d did not satisfy the polynomial", i);
		failures++;
	    }
	}
	if (!has_complex_root(roots3, 3, cubic_neg_expected[0].re, cubic_neg_expected[0].im, legacy_tol) ||
	    !has_complex_root(roots3, 3, cubic_neg_expected[1].re, cubic_neg_expected[1].im, legacy_tol) ||
	    !has_complex_root(roots3, 3, cubic_neg_expected[2].re, cubic_neg_expected[2].im, legacy_tol)) {
	    report_failure(test, "legacy negative cubic roots did not match the GNU Octave reference set");
	    failures++;
	}
    }

    if (!bn_poly_cubic_roots(roots3, &cubicpos)) {
	report_failure(test, "bn_poly_cubic_roots failed for the legacy positive cubic");
	failures++;
    } else {
	for (i = 0; i < 3; i++) {
	    if (!root_is_valid(&cubicpos, &roots3[i], 1.0e-6)) {
		report_failure(test, "legacy positive cubic root %d did not satisfy the polynomial", i);
		failures++;
	    }
	}
	if (!has_complex_root(roots3, 3, cubic_pos_expected[0].re, cubic_pos_expected[0].im, legacy_tol) ||
	    !has_complex_root(roots3, 3, cubic_pos_expected[1].re, cubic_pos_expected[1].im, legacy_tol) ||
	    !has_complex_root(roots3, 3, cubic_pos_expected[2].re, cubic_pos_expected[2].im, legacy_tol)) {
	    report_failure(test, "legacy positive cubic roots did not match the GNU Octave reference set");
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

    if (!bn_poly_quartic_roots(roots4, &quarticneg)) {
	report_failure(test, "bn_poly_quartic_roots failed for the legacy negative quartic");
	failures++;
    } else {
	for (i = 0; i < 4; i++) {
	    if (!root_is_valid(&quarticneg, &roots4[i], 1.0e-6)) {
		report_failure(test, "legacy negative quartic root %d did not satisfy the polynomial", i);
		failures++;
	    }
	}
	if (!has_complex_root(roots4, 4, quartic_neg_expected[0].re, quartic_neg_expected[0].im, legacy_tol) ||
	    !has_complex_root(roots4, 4, quartic_neg_expected[1].re, quartic_neg_expected[1].im, legacy_tol) ||
	    !has_complex_root(roots4, 4, quartic_neg_expected[2].re, quartic_neg_expected[2].im, legacy_tol) ||
	    !has_complex_root(roots4, 4, quartic_neg_expected[3].re, quartic_neg_expected[3].im, legacy_tol)) {
	    report_failure(test, "legacy negative quartic roots did not match the GNU Octave reference set");
	    failures++;
	}
    }

    if (!bn_poly_quartic_roots(roots4, &quarticpos)) {
	report_failure(test, "bn_poly_quartic_roots failed for the legacy positive quartic");
	failures++;
    } else {
	for (i = 0; i < 4; i++) {
	    if (!root_is_valid(&quarticpos, &roots4[i], 1.0e-6)) {
		report_failure(test, "legacy positive quartic root %d did not satisfy the polynomial", i);
		failures++;
	    }
	}
	if (!has_complex_root(roots4, 4, quartic_pos_expected[0].re, quartic_pos_expected[0].im, legacy_tol) ||
	    !has_complex_root(roots4, 4, quartic_pos_expected[1].re, quartic_pos_expected[1].im, legacy_tol) ||
	    !has_complex_root(roots4, 4, quartic_pos_expected[2].re, quartic_pos_expected[2].im, legacy_tol) ||
	    !has_complex_root(roots4, 4, quartic_pos_expected[3].re, quartic_pos_expected[3].im, legacy_tol)) {
	    report_failure(test, "legacy positive quartic roots did not match the GNU Octave reference set");
	    failures++;
	}
    }

    return failures;
}


static const struct bn_api_case poly_cases[] = {
    {"ops", test_poly_ops},
    {"legacy", test_poly_legacy},
    {"roots", test_poly_roots},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, poly_cases);
}
