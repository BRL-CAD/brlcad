/*                   T E S T _ S S C A N F . C
 * BRL-CAD
 *
 * Copyright (c) 2012 United States Government as represented by
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
/** @file test_sscanf.c
 *
 * Test bu_sscanf routine.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <float.h>

#include "bu.h"
#include "vmath.h"

#define STR_SIZE 128
#define FLOAT_TOL .0005

/* scanf specification summarized from Linux man page:
 *
 * If processing of a directive fails, no further input is read, and
 * scanf returns.
 *
 * Returns number of input items successfully matched and assigned, or
 * EOF if a read error occurs (errno and stream error indicator are set),
 * EOI is encountered before the first successful conversion, or a
 * matching failure occurs.
 *
 * Can match 0 or more whitespace chars, ordinary character, or a
 * conversion specification:
 *
 * %[*][a][max_field_width][type_modifier]<conversion_specifier>
 *
 * '*' suppresses assignment. No pointer is needed, input is discarded,
 *     and conversion isn't added to the successful conversion count.
 *
 * 'a' in GNU, this represents an extension that automatically allocates
 *     a string large enough to hold a converted string and assigns the
 *     address to the supplied char*. In C99, a is synonymous with e/E/f/g.
 *
 * max_field_width - scan stops stops reading for a conversion at the
 *     first non-matching character or when max_field_width is reached
 *     (not including discarded leading whitespace or the '\0' appended
 *     to string conversions).
 *
 * modifiers:
 *  h - expect (signed|unsigned) short int (not int) pointer
 * hh - expect (signed|unsigned) char (not int) pointer
 *  j - expect (signed|unsigned) intmax_t or uintmax_t (not int) pointer
 *  l - expect (signed|unsigned) long int (not int) pointer, or double (not
 *      float), or wide char (not char) pointer
 *q/L - expect long double (not double) pointer
 *  t - expect ptrdiff_t (not int) pointer
 *  z - expect size_t (not int) pointer
 *
 * Note: the "long long" type extension is specified with 'q' for 4.4BSD,
 *       and 'll' or 'L' for GNU.
 *
 * conversion specifiers:
 *  % - NOT A CONVERSION. Literal %.
 *  d - signed/unsigned decimal integer
 *  i - Signed/unsigned base 8/10/16 integer. Base 16 chose if 0x or 0X is
 *      read first, base 8 if 0 is read first, and base 10 otherwise. Only
 *      characters valid for the base are read.
 *  o - unsigned octal
 *  u - unsigned decimal
 *x/X - unsigned hexadecimal
 *a/e/E/f/g - signed/unsigned float (a is C99)
 *  s - Match a sequence of non-white-space characters. Pointer must be
 *      large enough for all characters plus the '\0' scanf appends.
 *  c - Match a sequence (1 by default) of characters. Pointer must be large
 *      enough for all characters. No '\0' is appended. Leading white space
 *      IS NOT discarded.
 *[...] - Like c, but only accept the specified character class. Leading white
 *        space IS NOT discarded. To include ']' in the set, it must appear
 *        first after '[' or '^'. To include '-' in the set, it must appear
 *        last before ']'.
 *  p - pointer (printf format)
 *  n - NOT A CONVERSION. Stores number of characters read so far in the int
 *      pointer. PROBABLY shouldn't affect returned conversion count.
 */

/* Macro for creating type-specific test routines.
 *
 * conv_spec
 *     lf, d, c, ... creates test_sscanf_lf, test_sscanf_d, ...
 * val_type
 *     double, int, char, ...
 * val_test
 *     equality test for two val_type vals named a and b, like (a == b)
 */
#ifdef HAVE_VSSCANF
#define TEST_SSCANF(conv_spec, val_type, val_test) \
/* returns true if a and b are equal */ \
static int \
equal_##conv_spec(val_type a, val_type b) { \
    return val_test; \
} \
/* Runs vsscanf and bu_vsscanf, compares their return values and,
 * if they match, compares their conversion values. On any mismatch,
 * prints log and returns.
 */ \
static void \
test_sscanf_##conv_spec(const char *src, const char *fmt, ...) \
{ \
    va_list ap; \
    int i, ret, bu_ret; \
    val_type *values = NULL; \
    val_type *bu_values = NULL; \
    /* run system routine */ \
    va_start(ap, fmt); \
    ret = vsscanf(src, fmt, ap); \
    va_end(ap); \
    if (ret > 0) { \
	/* Create value arrays. Return tells us how many values we converted, \
	 * which will be less than or (hopefully) equal to the number of \
	 * pointers in the va_list. \
	 */ \
	values = (val_type*)bu_malloc(ret * sizeof(val_type), "test_sscanf: value array"); \
	bu_values = (val_type*)bu_malloc(ret * sizeof(val_type), "test_sscanf: bu_value array"); \
	/* copy vsscanf pointer values to array */ \
	va_start(ap, fmt); \
	for (i = 0; i < ret; ++i) { \
	    values[i] = *va_arg(ap, val_type*); \
	} \
	va_end(ap); \
    } else { \
	printf("Warning: no assignments done for %s.\n", fmt); \
    } \
    /* run bu routine */ \
    va_start(ap, fmt); \
    bu_ret = bu_vsscanf(src, fmt, ap); \
    va_end(ap); \
    /* return values equal? */ \
    if (bu_ret != ret) { \
	printf("[FAIL] sscanf returned %d but bu_sscanf returned %d\n", \
		ret, bu_ret); \
	return; \
    } \
    /* conversion values equal? */ \
    if (values != NULL && bu_values != NULL) { \
	/* copy bu_vsscanf pointer values to array */ \
	va_start(ap, fmt); \
	for (i = 0; i < bu_ret; ++i) { \
	    bu_values[i] = *va_arg(ap, val_type*); \
	} \
	va_end(ap); \
	/* compare vsscanf and bu_vsscanf values */ \
	for (i = 0; i < ret; ++i) { \
	    if (!equal_##conv_spec(values[i], bu_values[i])) { \
		printf("[FAIL] conversion value mismatch. " \
			"(sscanf) %" #conv_spec "!= %" #conv_spec " (bu_sscanf)\n", \
			values[i], bu_values[i]); \
		break; \
	    } \
	} \
	bu_free(values, "test_sscanf: value array\n"); \
	values = NULL; \
	bu_free(bu_values, "test_sscanf: bu_value array\n"); \
	bu_values = NULL; \
    } \
}
#else
#define TEST_SSCANF(conv_spec, val_type, val_test) /* nop */
#endif

/* string test routine */
#ifdef HAVE_VSSCANF
static void
test_sscanf_s(const char *src, const char *fmt, ...)
{
    va_list ap;
    int i, ret, bu_ret;
    char **values = NULL;
    char **bu_values = NULL;

    /* run system routine */
    va_start(ap, fmt);
    ret = vsscanf(src, fmt, ap);
    va_end(ap);

    if (ret > 0) {
	/* Create string arrays. Copy vsscanf pointer values to array. */
	values = (char**)bu_malloc(ret * sizeof(char*), "test_sscanf: value array");
	bu_values = (char**)bu_malloc(ret * sizeof(char*), "test_sscanf: value array");

	va_start(ap, fmt);
	for (i = 0; i < ret; ++i) {
	    values[i] = (char*)bu_malloc(STR_SIZE * sizeof(char),
		    "test_sscanf: value array");

	    bu_values[i] = (char*)bu_malloc(STR_SIZE * sizeof(char),
		    "test_sscanf: bu_value array");

	    strcpy(values[i], va_arg(ap, char*));
	}
	va_end(ap);
    } else {
	printf("Warning: no assignments done for %s.\n", fmt);
    }
    /* run bu routine */
    va_start(ap, fmt);
    bu_ret = bu_vsscanf(src, fmt, ap);
    va_end(ap);

    /* return values equal? */
    if (bu_ret != ret) {
	printf("[FAIL] sscanf returned %d but bu_sscanf returned %d\n",
		ret, bu_ret);
	printf("\tSource was \"%s\"\n", src);
	printf("\tFormat was \"%s\"\n", fmt);
	return;
    }

    /* conversion values equal? */
    if (values != NULL && bu_values != NULL) {

	/* copy bu_vsscanf pointer values to array */
	va_start(ap, fmt);
	for (i = 0; i < bu_ret; ++i) {
	    strcpy(bu_values[i], va_arg(ap, char*));
	}
	va_end(ap);

	/* compare vsscanf and bu_vsscanf values */
	for (i = 0; i < ret; ++i) {
	    if (!BU_STR_EQUAL(values[i], bu_values[i])) {
		printf("[FAIL] conversion value mismatch. "
			"(sscanf) %s != %s (bu_sscanf)\n",
			values[i], bu_values[i]);
		printf("\tSource was \"%s\"\n", src);
		printf("\tFormat was \"%s\"\n", fmt);
		break;
	    }
	}
	bu_free_array(ret, values, "test_sscanf: values\n");
	bu_free_array(ret, bu_values, "test_sscanf: bu_values\n");
	bu_free(values, "test_sscanf: value array\n");
	bu_free(bu_values, "test_sscanf: bu_value array\n");
	values = bu_values = NULL;
    }
}
#else
#define test_sscanf_s sscanf
#endif

/* integer test routines */
TEST_SSCANF(d, int, (a == b))
TEST_SSCANF(hd, short int, (a == b))
TEST_SSCANF(ld, long int, (a == b))
TEST_SSCANF(u, unsigned int, (a == b))
TEST_SSCANF(hu, unsigned short int, (a == b))
TEST_SSCANF(lu, unsigned long int, (a == b))
TEST_SSCANF(c, char, (a == b))

/* floating-point test routines */
TEST_SSCANF(f, float, (NEAR_EQUAL(a, b, FLOAT_TOL)))
TEST_SSCANF(lf, double, (NEAR_EQUAL(a, b, FLOAT_TOL)))
TEST_SSCANF(Lf, long double, (NEAR_EQUAL(a, b, FLOAT_TOL)))

int
main(int argc, char *argv[])
{
    int d_vals[3];
    short hd_vals[3];
    long ld_vals[3];
    unsigned u_vals[2];
    unsigned short hu_vals[2];
    unsigned long lu_vals[2];
    float f_vals[6];
    double lf_vals[6];
    long double Lf_vals[6];
    char c_vals[3];
    char s_vals[3][STR_SIZE];

#ifndef HAVE_VSSCANF
    printf("%s requires a system vsscanf for proper execution.\n", argv[0]);
    return 0;
#endif

    if (argc > 1) {
	printf("Warning: %s takes no arguments.\n", argv[0]);
    }

    printf("bu_sscanf: running tests\n");

/* Possible test cases:
 * 1) Simple conversions, signed and unsigned:
 *    %,d,i,o,u,x,X,a,e,E,f,g,s,c,[...],p,n
 * 2) Modified conversions:
 *    (h|hh|j|l|t|z)[diouxXn], (l|L)[aeEfg]
 * 3) Width specifiers:
 *    <width>[diouxXaeEfgsc[...]p]
 * 4) Non-conversions:
 *    *,%
 * 5) Whitespace skip; arbitrary combinations of whitespace characters
 *    used to separate strings and conversion specifiers. Preservation
 *    of leading space for %c and %[...] and discarding for all others.
 *
 */

    /* signed integer tests */
    test_sscanf_d("0 +" bu_cpp_xstr(INT_MAX) " -" bu_cpp_xstr(INT_MIN),
	    "%d %d %d", &d_vals[0], &d_vals[1], &d_vals[2]);
    test_sscanf_hd("0 +" bu_cpp_xstr(SHRT_MAX) " -" bu_cpp_xstr(SHRT_MIN),
	    "%hd %hd %hd", &hd_vals[0], &hd_vals[1], &hd_vals[2]);
    test_sscanf_ld("0 +" bu_cpp_xstr(LONG_MAX) " -" bu_cpp_xstr(LONG_MIN),
	    "%ld %ld %ld", &ld_vals[0], &ld_vals[1], &ld_vals[2]);

    /* unsigned integer tests */
    test_sscanf_u("0 " bu_cpp_xstr(UINT_MAX), "%u %u", &u_vals[0], &u_vals[1]);
    test_sscanf_hu("0 " bu_cpp_xstr(USHRT_MAX), "%hu %hu", &hu_vals[0], &hu_vals[1]);
    test_sscanf_lu("0 " bu_cpp_xstr(ULONG_MAX), "%lu %lu", &lu_vals[0], &lu_vals[1]);

    test_sscanf_lu("0 " bu_cpp_xstr(ULONG_MAX), "%lu %lu", &lu_vals[0], &lu_vals[1]);

    /* float tests */
    test_sscanf_f("0.0 .0123 .4567 8.91011 " bu_cpp_xstr(FLT_MIN) " "
	    bu_cpp_xstr(FLT_MAX), "%f %f %f %f %f %f", &f_vals[0], &f_vals[1],
	    &f_vals[2], &f_vals[3], &f_vals[4], &f_vals[5]);

    test_sscanf_lf("0.0 .0123 .4567 8.91011 " bu_cpp_xstr(DBL_MIN) " "
	    bu_cpp_xstr(DBL_MAX), "%lf %lf %lf %lf %lf %lf", &lf_vals[0],
	    &lf_vals[1], &lf_vals[2], &lf_vals[3], &lf_vals[4], &lf_vals[5]);

    test_sscanf_Lf("0.0 .0123 .4567 8.91011 " bu_cpp_xstr(LDBL_MIN) " "
	    bu_cpp_xstr(LDBL_MAX), "%Lf %Lf %Lf %Lf %Lf %Lf", &Lf_vals[0],
	    &Lf_vals[1], &Lf_vals[2], &Lf_vals[3], &Lf_vals[4], &Lf_vals[5]);

    /* string tests */
    test_sscanf_s(" aBc  DeF   gHi\t", "%s %s %s",
	    s_vals[0], s_vals[1], s_vals[2]);

    test_sscanf_c("a b c", "%c %c %c",
	    &c_vals[0], &c_vals[1], &c_vals[2]);


    printf("bu_sscanf: testing complete\n");
    return 0;
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
