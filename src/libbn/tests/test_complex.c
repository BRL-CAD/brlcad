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
    static const struct {
	bn_complex_t a;
	bn_complex_t b;
	bn_complex_t expected;
    } add_cases[] = {
	{{5.2, 3.8}, {0.0, 0.0}, {5.2, 3.8}},
	{{0.0, 0.0}, {5.2, 3.8}, {5.2, 3.8}},
	{{1.4, 2.9}, {-1.4, -2.9}, {0.0, 0.0}},
	{{7.4, -2.3}, {8.9, 6.4}, {16.3, 4.1}},
	{{8.9, 6.4}, {7.4, -2.3}, {16.3, 4.1}}
    };
    static const struct {
	bn_complex_t a;
	bn_complex_t b;
	bn_complex_t expected;
    } sub_cases[] = {
	{{5.2, 3.8}, {0.0, 0.0}, {5.2, 3.8}},
	{{0.0, 0.0}, {5.2, 3.8}, {-5.2, -3.8}},
	{{1.4, 2.9}, {1.4, 2.9}, {0.0, 0.0}},
	{{7.4, -2.3}, {8.9, 6.4}, {-1.5, -8.7}},
	{{8.9, 6.4}, {7.4, -2.3}, {1.5, 8.7}}
    };
    static const struct {
	bn_complex_t a;
	bn_complex_t b;
	bn_complex_t expected;
    } mul_cases[] = {
	{{5.2, 3.8}, {0.0, 0.0}, {0.0, 0.0}},
	{{0.0, 0.0}, {5.2, 3.8}, {0.0, 0.0}},
	{{7.4, 2.3}, {0.123231, -0.0383014}, {1.0, 0.0}},
	{{0.123231, -0.0383014}, {7.4, 2.3}, {1.0, 0.0}}
    };
    static const struct {
	bn_complex_t in;
	bn_complex_t expected;
    } neg_cases[] = {
	{{0.0, 0.0}, {0.0, 0.0}},
	{{2.0, 0.0}, {-2.0, 0.0}},
	{{0.0, 2.0}, {0.0, -2.0}},
	{{6.3, 4.2}, {-6.3, -4.2}}
    };
    static const struct {
	bn_complex_t in;
	bn_complex_t expected;
    } conj_cases[] = {
	{{0.0, 0.0}, {0.0, 0.0}},
	{{2.0, 0.0}, {2.0, 0.0}},
	{{0.0, 2.0}, {0.0, -2.0}},
	{{6.3, 4.2}, {6.3, -4.2}}
    };
    bn_complex_t a = {1.5, -2.0};
    bn_complex_t b = {-3.0, 0.5};
    bn_complex_t c = {0.0, 0.0};
    bn_complex_t d = {0.0, 0.0};
    int i;

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

    for (i = 0; i < (int)(sizeof(add_cases) / sizeof(add_cases[0])); i++) {
	c = add_cases[i].a;
	bn_cx_add(&c, &add_cases[i].b);
	if (!complex_close(&c, add_cases[i].expected.re, add_cases[i].expected.im, 1.0e-12)) {
	    report_failure(test, "legacy add case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(sub_cases) / sizeof(sub_cases[0])); i++) {
	c = sub_cases[i].a;
	bn_cx_sub(&c, &sub_cases[i].b);
	if (!complex_close(&c, sub_cases[i].expected.re, sub_cases[i].expected.im, 1.0e-12)) {
	    report_failure(test, "legacy subtract case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(mul_cases) / sizeof(mul_cases[0])); i++) {
	c = mul_cases[i].a;
	bn_cx_mul(&c, &mul_cases[i].b);
	bn_cx_mul2(&d, &mul_cases[i].a, &mul_cases[i].b);
	if (!complex_close(&c, mul_cases[i].expected.re, mul_cases[i].expected.im, 1.0e-6) ||
	    !complex_close(&d, mul_cases[i].expected.re, mul_cases[i].expected.im, 1.0e-6)) {
	    report_failure(test, "legacy multiply case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(neg_cases) / sizeof(neg_cases[0])); i++) {
	c = neg_cases[i].in;
	bn_cx_neg(&c);
	if (!complex_close(&c, neg_cases[i].expected.re, neg_cases[i].expected.im, 1.0e-12)) {
	    report_failure(test, "legacy negate case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(conj_cases) / sizeof(conj_cases[0])); i++) {
	c = conj_cases[i].in;
	bn_cx_conj(&c);
	if (!complex_close(&c, conj_cases[i].expected.re, conj_cases[i].expected.im, 1.0e-12)) {
	    report_failure(test, "legacy conjugate case %d failed", i);
	    failures++;
	}
	if (!scalar_close(bn_cx_real(&conj_cases[i].in), conj_cases[i].in.re, 0.0) ||
	    !scalar_close(bn_cx_imag(&conj_cases[i].in), conj_cases[i].in.im, 0.0)) {
	    report_failure(test, "legacy parts case %d failed", i);
	    failures++;
	}
    }

    return failures;
}


static int
test_complex_division(void)
{
    int failures = 0;
    const char *test = "complex_division";
    static const struct {
	bn_complex_t a;
	bn_complex_t b;
	bn_complex_t expected;
    } div_cases[] = {
	{{1.0, 3.0}, {1.0, 3.0}, {1.0, 0.0}},
	{{3.0, 1.0}, {3.0, 1.0}, {1.0, 0.0}},
	{{6.3, 4.2}, {9.8, 7.7}, {0.605678, -0.0473186}},
	{{0.0, 0.0}, {1.0, 1.0}, {0.0, 0.0}},
	{{1.0, 1.0}, {0.0, 0.0}, {1.0e20, 1.0e20}}
    };
    bn_complex_t numer = {6.3, 4.2};
    bn_complex_t denom = {9.8, 7.7};
    bn_complex_t quot = numer;
    bn_complex_t prod = {0.0, 0.0};
    bn_complex_t same = {2.5, -0.5};
    bn_complex_t zero = {0.0, 0.0};
    int i;

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

    for (i = 0; i < (int)(sizeof(div_cases) / sizeof(div_cases[0])); i++) {
	quot = div_cases[i].a;
	bn_cx_div(&quot, &div_cases[i].b);
	if (!complex_close(&quot, div_cases[i].expected.re, div_cases[i].expected.im, 1.0e-6)) {
	    report_failure(test, "legacy divide case %d failed", i);
	    failures++;
	}
    }

    return failures;
}


static int
test_complex_sqrt(void)
{
    int failures = 0;
    const char *test = "complex_sqrt";
    static const struct {
	bn_complex_t in;
	bn_complex_t expected;
	double tol;
    } sqrt_cases[] = {
	{{0.0, 2.0}, {1.0, 1.0}, 1.0e-6},
	{{2.0, 0.0}, {1.414214, 0.0}, 1.0e-6},
	{{0.0, 0.0}, {0.0, 0.0}, 0.0},
	{{6.3, 4.2}, {2.63360, 0.797389}, 1.0e-5},
	{{9.8, 7.7}, {3.33640, 1.15394}, 1.0e-5}
    };
    bn_complex_t in = {0.0, 0.0};
    bn_complex_t root = {0.0, 0.0};
    bn_complex_t square = {0.0, 0.0};
    int i;

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

    for (i = 0; i < (int)(sizeof(sqrt_cases) / sizeof(sqrt_cases[0])); i++) {
	bn_cx_sqrt(&root, &sqrt_cases[i].in);
	if (!complex_close(&root, sqrt_cases[i].expected.re, sqrt_cases[i].expected.im, sqrt_cases[i].tol)) {
	    report_failure(test, "legacy sqrt case %d failed", i);
	    failures++;
	}
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
