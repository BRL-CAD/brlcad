/*                   T E S T _ S S C A N F . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"

#define TS_FLOAT_TOL .0005

#define TS_STR_SIZE 128
#define TS_STR_WIDTH "127"

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
 *  l - expect (signed|unsigned) long int (not int) pointer, or double (not
 *      float), or wide char (not char) pointer
 *  L - expect long double (not double) pointer
 *
 * C99 modifiers:
 *  j - expect (signed|unsigned) intmax_t or uintmax_t (not int) pointer
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

/* Using these constants to identify pointer type instead of adding all the
 * logic needed to parse the format string.
 */
enum {
    SCAN_SHORTSHORT, SCAN_USHORTSHORT,
    SCAN_SHORT, SCAN_USHORT,
    SCAN_INT, SCAN_UINT,
    SCAN_LONG, SCAN_ULONG,
    SCAN_POINTER,
    SCAN_FLOAT, SCAN_DOUBLE, SCAN_LDOUBLE,
    SCAN_SIZE,
    SCAN_PTRDIFF
};

static void
print_src_and_fmt(const char *src, const char *fmt)
{
    printf("\"%s\", \"%s\"\n", src, fmt);
}

/* This test is intended to catch errors in the tests themselves. If we make a
 * typo in a test format string causing fewer assignments than expected, then
 * we could end up silently comparing error behavior rather than the behavior
 * we actually want to test.
 */
static void
checkReturnVal(const char *funcStr, int actual, int expected)
{
    if (actual != expected) {
	bu_exit(1, "\tError: %s returned %d%s. Expected %d%s.\n", funcStr,
		actual, (actual == EOF) ? " (EOF)" : "",
		expected, (expected == EOF) ? " (EOF)" : "");
    }
}

/* Exit if returns from sscanf and bu_sscanf are not equal. */
static void
checkReturnsEqual(int ret, int bu_ret)
{
    if (bu_ret != ret) {
	bu_exit(1, "\t[FAIL] sscanf returned %d but bu_sscanf returned %d.\n",
		ret, bu_ret);
    }
}

#define CHECK_INT_VALS_EQUAL(int_type, pfmt, valp, bu_valp) \
{ \
    int_type _val = *(int_type*)(valp); \
    int_type _bu_val = *(int_type*)(bu_valp); \
    if (_val != _bu_val) { \
	bu_exit(1, "\t[FAIL] conversion value mismatch.\n" \
		"\t(sscanf) %" bu_cpp_str(pfmt) " != " \
		"%" bu_cpp_str(pfmt) " (bu_sscanf).\n", \
		_val, _bu_val); \
    } \
}

#define CHECK_FLOAT_VALS_EQUAL(float_type, pfmt, valp, bu_valp) \
{ \
    float_type _val = *(float_type*)(valp); \
    float_type _bu_val = *(float_type*)(bu_valp); \
    if (!NEAR_EQUAL(_val, _bu_val, TS_FLOAT_TOL)) { \
	bu_exit(1, "\t[FAIL] conversion value mismatch.\n" \
		"\t(sscanf) %" bu_cpp_str(pfmt) " != " \
		"%" bu_cpp_str(pfmt) " (bu_sscanf).\n", \
		_val, _bu_val); \
    } \
}

static void
test_sscanf(int type, const char *src, const char *fmt) {
    int ret, bu_ret;
    void *val, *bu_val;

    ret = bu_ret = 0;
    val = bu_val = NULL;

    print_src_and_fmt(src, fmt);

    /* call sscanf and bu_sscanf with appropriately cast pointers */
#define SSCANF_TYPE(type) \
    BU_ALLOC(val, type); \
    BU_ALLOC(bu_val, type); \
\
    ret = sscanf(src, fmt, (type*)val); \
    bu_ret = bu_sscanf(src, fmt, (type*)bu_val); \
\
    checkReturnVal("sscanf", ret, 1); \
    checkReturnsEqual(bu_ret, ret);

    switch (type) {
    case SCAN_INT:
	SSCANF_TYPE(int);
	CHECK_INT_VALS_EQUAL(int, d, val, bu_val);
	break;
    case SCAN_UINT:
	SSCANF_TYPE(unsigned);
	CHECK_INT_VALS_EQUAL(unsigned, u, val, bu_val);
	break;
    case SCAN_SHORT:
	SSCANF_TYPE(short);
	CHECK_INT_VALS_EQUAL(short, hd, val, bu_val);
	break;
    case SCAN_USHORT:
	SSCANF_TYPE(unsigned short);
	CHECK_INT_VALS_EQUAL(unsigned short, hu, val, bu_val);
	break;
    case SCAN_SHORTSHORT:
	SSCANF_TYPE(char);
	CHECK_INT_VALS_EQUAL(char, hhd, val, bu_val);
	break;
    case SCAN_USHORTSHORT:
	SSCANF_TYPE(unsigned char);
	CHECK_INT_VALS_EQUAL(unsigned char, hhu, val, bu_val);
	break;
    case SCAN_LONG:
	SSCANF_TYPE(long);
	CHECK_INT_VALS_EQUAL(long, ld, val, bu_val);
	break;
    case SCAN_ULONG:
	SSCANF_TYPE(unsigned long);
	CHECK_INT_VALS_EQUAL(unsigned long, lu, val, bu_val);
	break;
    case SCAN_SIZE:
	SSCANF_TYPE(size_t);
	CHECK_INT_VALS_EQUAL(size_t, z, val, bu_val);
	break;
    case SCAN_PTRDIFF:
	SSCANF_TYPE(ptrdiff_t);
	CHECK_INT_VALS_EQUAL(ptrdiff_t, t, val, bu_val);
	break;
    case SCAN_POINTER:
	ret = sscanf(src, fmt, &val);
	bu_ret = bu_sscanf(src, fmt, &bu_val);

	checkReturnVal("sscanf", ret, 1);
	checkReturnsEqual(bu_ret, ret);

	if (val != bu_val) {
	    bu_exit(1, "\t[FAIL] conversion value mismatch.\n"
		    "\t(sscanf) %p != %p (bu_sscanf).\n",
		    val, bu_val);
	}
	val = bu_val = NULL;
	break;
    case SCAN_FLOAT:
	SSCANF_TYPE(float);
	CHECK_FLOAT_VALS_EQUAL(float, e, val, bu_val);
	break;
    case SCAN_DOUBLE:
	SSCANF_TYPE(double);
	CHECK_FLOAT_VALS_EQUAL(double, le, val, bu_val);
	break;
    case SCAN_LDOUBLE:
	SSCANF_TYPE(long double);
	CHECK_FLOAT_VALS_EQUAL(long double, Le, val, bu_val);
	break;
    default:
	bu_exit(1, "Error: test_sscanf was given an unrecognized pointer type.\n");
    }
    if (val != NULL) {
	bu_free(val, "test_sscanf val");
	val = NULL;
    }
    if (bu_val != NULL) {
	bu_free(bu_val, "test_sscanf bu_val");
	bu_val = NULL;
    }
} /* test_sscanf */

/* Here we define printable constants that should be safe on any platform.
 *
 * Note that we don't use the macros in limits.h or float.h, because they
 * aren't necessarily printable, as in:
 *     #define INT_MIN (-INT_MAX - 1)
 */
#define   SIGNED_HH_DEC 127
#define UNSIGNED_HH_DEC 255
#define   SIGNED_HH_OCT 0177
#define UNSIGNED_HH_OCT 0377
#define   SIGNED_HH_HEX 0x7F
#define UNSIGNED_HH_HEX 0xFF

#define   SIGNED_DEC 32767
#define UNSIGNED_DEC 65535
#define   SIGNED_OCT 0077777
#define UNSIGNED_OCT 0177777
#define   SIGNED_HEX 0x7FFF
#define UNSIGNED_HEX 0xFFFF

#define   SIGNED_L_DEC 2147483647
#define UNSIGNED_L_DEC 4294967295
#define   SIGNED_L_OCT 17777777777
#define UNSIGNED_L_OCT 37777777777
#define   SIGNED_L_HEX 0x7FFFFFFF
#define UNSIGNED_L_HEX 0xFFFFFFFF

#define SMALL_FLT 1.0e-37
#define LARGE_FLT 1.0e+37

static void
doNumericTests(void)
{
#define TEST_SIGNED_CONSTANT(str, fmt) \
    test_sscanf(SCAN_SHORTSHORT, str, "%hh" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_SIZE, str, "%z" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_SHORT, str, "%h" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_INT, str, "%" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_LONG, str, "%l" bu_cpp_str(fmt));

    TEST_SIGNED_CONSTANT("0", d);
    TEST_SIGNED_CONSTANT("0", i);
    TEST_SIGNED_CONSTANT("000", i);
    TEST_SIGNED_CONSTANT("0x0", i);
    TEST_SIGNED_CONSTANT("0X0", i);

#define TEST_UNSIGNED_CONSTANT(str, fmt) \
    test_sscanf(SCAN_USHORTSHORT, str, "%hh" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_SIZE, str, "%z" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_USHORT, str, "%h" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_UINT, str, "%" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_ULONG, str, "%l" bu_cpp_str(fmt));

    TEST_UNSIGNED_CONSTANT("0", u);
    TEST_UNSIGNED_CONSTANT("0", o);
    TEST_UNSIGNED_CONSTANT("000", o);
    TEST_UNSIGNED_CONSTANT("0x0", x);
    TEST_UNSIGNED_CONSTANT("0x0", X);
    TEST_UNSIGNED_CONSTANT("0X0", x);
    TEST_UNSIGNED_CONSTANT("0X0", X);

#define TEST_SIGNED_CONSTANTS(small, med, large, fmt) \
    test_sscanf(SCAN_SHORTSHORT, "+" bu_cpp_str(small), "%hh" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_SHORTSHORT, bu_cpp_str(small), "%hh" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_SHORTSHORT, "-" bu_cpp_str(small), "%hh" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_SIZE, bu_cpp_str(small), "%z" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_SHORT, "+" bu_cpp_str(med), "%h" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_SHORT, bu_cpp_str(med), "%h" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_SHORT, "-" bu_cpp_str(med), "%h" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_INT, "+" bu_cpp_str(med), "%" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_INT, bu_cpp_str(med), "%" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_INT, "-" bu_cpp_str(med), "%" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_LONG, "+" bu_cpp_str(large), "%l" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_LONG, bu_cpp_str(large), "%l" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_LONG, "-" bu_cpp_str(large), "%l" bu_cpp_str(fmt));

    TEST_SIGNED_CONSTANTS(SIGNED_HH_DEC, SIGNED_DEC, SIGNED_L_DEC, d);
    TEST_SIGNED_CONSTANTS(SIGNED_HH_DEC, SIGNED_DEC, SIGNED_L_DEC, i);
    TEST_SIGNED_CONSTANTS(SIGNED_HH_OCT, SIGNED_OCT, SIGNED_L_OCT, i);
    TEST_SIGNED_CONSTANTS(SIGNED_HH_HEX, SIGNED_HEX, SIGNED_L_HEX, i);

#define TEST_UNSIGNED_CONSTANTS(small, med, large, fmt) \
    test_sscanf(SCAN_USHORTSHORT, bu_cpp_str(small), "%hh" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_SIZE, bu_cpp_str(small), "%z" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_USHORT, bu_cpp_str(med), "%h" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_UINT, bu_cpp_str(med), "%" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_ULONG, bu_cpp_str(large), "%l" bu_cpp_str(fmt));

    TEST_UNSIGNED_CONSTANTS(UNSIGNED_HH_DEC, UNSIGNED_DEC, UNSIGNED_L_DEC, u);
    TEST_UNSIGNED_CONSTANTS(UNSIGNED_HH_OCT, UNSIGNED_OCT, UNSIGNED_L_OCT, o);
    TEST_UNSIGNED_CONSTANTS(UNSIGNED_HH_HEX, UNSIGNED_HEX, UNSIGNED_L_HEX, x);
    TEST_UNSIGNED_CONSTANTS(UNSIGNED_HH_HEX, UNSIGNED_HEX, UNSIGNED_L_HEX, X);

#define TEST_FLOAT_CONSTANT(c, fmt) \
    test_sscanf(SCAN_FLOAT, c, "%" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_DOUBLE, c, "%l" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_LDOUBLE, c, "%L" bu_cpp_str(fmt));

#define TEST_FLOAT_VARIANT(fmt) \
    TEST_FLOAT_CONSTANT("0.0", fmt); \
    TEST_FLOAT_CONSTANT("0.0e0", fmt); \
    TEST_FLOAT_CONSTANT("0.0e1", fmt); \
    test_sscanf(SCAN_FLOAT, bu_cpp_xstr(SMALL_FLT), "%" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_FLOAT, bu_cpp_xstr(LARGE_FLT), "%" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_DOUBLE, bu_cpp_xstr(SMALL_FLT), "%l" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_DOUBLE, bu_cpp_xstr(LARGE_FLT), "%l" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_LDOUBLE, bu_cpp_xstr(SMALL_FLT), "%L" bu_cpp_str(fmt)); \
    test_sscanf(SCAN_LDOUBLE, bu_cpp_xstr(LARGE_FLT), "%L" bu_cpp_str(fmt));

    TEST_FLOAT_VARIANT(a);
    TEST_FLOAT_VARIANT(e);
    TEST_FLOAT_VARIANT(f);
    TEST_FLOAT_VARIANT(g);

    TEST_FLOAT_VARIANT(A);
    TEST_FLOAT_VARIANT(E);
    TEST_FLOAT_VARIANT(F);
    TEST_FLOAT_VARIANT(G);
}

/* string test routine */
static void
test_sscanf_s(const char *src, const char *fmt)
{
    int ret, bu_ret;
    char dest[TS_STR_SIZE], bu_dest[TS_STR_SIZE];

    ret = bu_ret = 0;

    /* ensure NULL termination even for c and [...] */
    memset(dest, '\0', TS_STR_SIZE);
    memset(bu_dest, '\0', TS_STR_SIZE);

    print_src_and_fmt(src, fmt);

    ret = sscanf(src, fmt, dest);
    bu_ret = bu_sscanf(src, fmt, bu_dest);

    checkReturnVal("sscanf", ret, 1);
    checkReturnsEqual(bu_ret, ret);

    if (!BU_STR_EQUAL(dest, bu_dest)) {
	bu_exit(1, "\t[FAIL] conversion value mismatch.\n"
		"\t(sscanf) %s != %s (bu_sscanf).\n",
		dest, bu_dest);
    }
}

static void
doStringTests(void)
{
    int bu_ret;
    char buf[TS_STR_SIZE];
    char *cp;

#define TEST_STR_SPACE " \t\naBc\n"
#define TEST_STR_NOSPACE "aBc"

#define TEST_TERMINATION(src, fmt) \
    print_src_and_fmt(src, fmt); \
    /* init so that 'X' appears after the last char written by bu_sscanf */ \
    memset(buf, 'X', TS_STR_SIZE); \
    bu_ret = bu_sscanf(src, fmt, buf); \
    checkReturnVal("bu_sscanf", bu_ret, 1); \
    cp = strchr(buf, 'X');

    /* %s should append '\0' */
    TEST_TERMINATION(TEST_STR_SPACE, "%" TS_STR_WIDTH "s");
    if (cp != NULL) {
	/* cp != NULL implies strchr found 'X' before finding '\0' */
	bu_exit(1, "\t[FAIL] bu_sscanf didn't null-terminate %%s conversion.\n");
    }

    /* %[...] should append '\0' */
    TEST_TERMINATION(TEST_STR_SPACE, "%" TS_STR_WIDTH "[^z]");
    if (cp != NULL) {
	/* cp != NULL implies strchr found 'X' before finding '\0' */
	bu_exit(1, "\t[FAIL] bu_sscanf null-terminated %%[...] conversion.\n");
    }

    /* %c should NOT append '\0' */
    TEST_TERMINATION(TEST_STR_SPACE, "%" TS_STR_WIDTH "c");
    if (cp == NULL) {
	/* cp == NULL implies strchr found '\0' before finding 'X' */
	bu_exit(1, "\t[FAIL] bu_sscanf null-terminated %%c conversion.\n");
    }

    /* For %s sscanf should not include leading whitespace in the string, so
     * the following should all be equivalent.
     */
    test_sscanf_s(TEST_STR_NOSPACE, "%" TS_STR_WIDTH "s");
    test_sscanf_s(TEST_STR_NOSPACE, " %" TS_STR_WIDTH "s");
    test_sscanf_s(TEST_STR_SPACE, "%" TS_STR_WIDTH "s");
    test_sscanf_s(TEST_STR_SPACE, " %" TS_STR_WIDTH "s");

    /* For %c, leading whitespace should be included unless the conversion
     * specifier is preceded by whitespace.
     */
    test_sscanf_s(TEST_STR_SPACE, "%c");  /* should assign ' ' */
    test_sscanf_s(TEST_STR_SPACE, " %c"); /* should assign 'a' */

    /* For %[...], should act like %s, but should assign any/only characters
     * in the class.
     */
    test_sscanf_s(TEST_STR_SPACE, "%" TS_STR_WIDTH "[^A-Z]");  /* should assign " \t\na" */
    test_sscanf_s(TEST_STR_SPACE, " %" TS_STR_WIDTH "[^A-Z]"); /* should assign "a"      */
    test_sscanf_s(TEST_STR_SPACE, " %" TS_STR_WIDTH "[a-z]");  /* should assign "a"      */

    /* should be able to include literal ] and - characters */
    test_sscanf_s("[-[- ", "%" TS_STR_WIDTH "[[-]");
    test_sscanf_s(" zZ [- ", "%" TS_STR_WIDTH "[^[-]");
    test_sscanf_s(" zZ -[ ", "%" TS_STR_WIDTH "[^[-]");
}

static void
doPointerTests(void)
{
    test_sscanf(SCAN_POINTER, "0", "%p");
    test_sscanf(SCAN_POINTER, bu_cpp_xstr(UNSIGNED_L_HEX), "%p");

    test_sscanf(SCAN_PTRDIFF, "0", "%ti");
    test_sscanf(SCAN_PTRDIFF, "0", "%td");
    test_sscanf(SCAN_PTRDIFF, "0", "%tu");
    test_sscanf(SCAN_PTRDIFF, bu_cpp_xstr(SIGNED_L_DEC), "%ti");
    test_sscanf(SCAN_PTRDIFF, bu_cpp_xstr(SIGNED_L_DEC), "%td");
    test_sscanf(SCAN_PTRDIFF, bu_cpp_xstr(UNSIGNED_L_DEC), "%tu");

    test_sscanf(SCAN_PTRDIFF, "000", "%ti");
    test_sscanf(SCAN_PTRDIFF, "000", "%to");
    test_sscanf(SCAN_PTRDIFF, bu_cpp_xstr(SIGNED_L_OCT), "%ti");
    test_sscanf(SCAN_PTRDIFF, bu_cpp_xstr(UNSIGNED_L_OCT), "%to");

    test_sscanf(SCAN_PTRDIFF, "0x0", "%ti");
    test_sscanf(SCAN_PTRDIFF, "0x0", "%tx");
    test_sscanf(SCAN_PTRDIFF, bu_cpp_xstr(SIGNED_L_HEX), "%ti");
    test_sscanf(SCAN_PTRDIFF, bu_cpp_xstr(UNSIGNED_L_HEX), "%tx");
}

static void
test_sscanf_noconv(const char *src, const char *fmt)
{
    int count, bu_count, ret, bu_ret;

    count = bu_count = 0;

    print_src_and_fmt(src, fmt);

    ret = sscanf(src, fmt, &count);
    bu_ret = bu_sscanf(src, fmt, &bu_count);

    checkReturnVal("sscanf", ret, 0);
    checkReturnsEqual(bu_ret, ret);

    if (bu_count != count) {
	bu_exit(1, "\t[FAIL] sscanf consumed %d chars, "
		"but bu_sscanf consumed %d.\n", count, bu_count);
    }
}

static void
doNonConversionTests(void)
{
    /* %n - don't convert/assign, but do store consumed char count */
    test_sscanf_noconv(". \tg\t\tn i    RTSA si sihT", ". g n i RTSA%n");
    test_sscanf_noconv(" foo", "foo%n");

    /* %% - don't convert/assign, but do scan literal % */
    test_sscanf_noconv("%%n    %", "%%%%n %%%n");

    /* suppressed assignments */
    test_sscanf_noconv(bu_cpp_xstr(SIGNED_DEC), "%*d%n");
    test_sscanf_noconv(bu_cpp_xstr(UNSIGNED_DEC), "%*u%n");
    test_sscanf_noconv(bu_cpp_xstr(LARGE_FLT), "%*f%n");
    test_sscanf_noconv("42 42  4.2e1", "%*d %*u %*f%n");
}

#define TS_NUM_ASSIGNMENTS 3

static void
test_string_width(const char *src, const char *fmt)
{
    int i, j, ret, bu_ret;
    char str_vals[TS_NUM_ASSIGNMENTS][TS_STR_SIZE];
    char bu_str_vals[TS_NUM_ASSIGNMENTS][TS_STR_SIZE];

    print_src_and_fmt(src, fmt);

    /* init so that 'X' appears after the last char written by bu_sscanf */
    for (i = 0; i < TS_NUM_ASSIGNMENTS; ++i) {
	memset(str_vals[i], 'X', TS_STR_SIZE);
	memset(bu_str_vals[i], 'X', TS_STR_SIZE);
    }

    ret = sscanf(src, fmt, str_vals[0], str_vals[1], str_vals[2]);
    bu_ret = bu_sscanf(src, fmt, bu_str_vals[0], bu_str_vals[1],
	    bu_str_vals[2]);
    checkReturnVal("sscanf", ret, 3);
    checkReturnsEqual(bu_ret, ret);

    /* each str should be exactly equivalent to each bu_str */
    for (i = 0; i < TS_NUM_ASSIGNMENTS; ++i) {
	for (j = 0; j < TS_STR_SIZE - 1; ++j) {
	    if (str_vals[i][j] != bu_str_vals[i][j]) {

		/* terminate for printing */
		str_vals[i][j + 1] = '\0';
		bu_str_vals[i][j + 1] = '\0';

		bu_exit(1, "\t[FAIL] conversion value mismatch.\n"
			"\t(sscanf) %s != %s (bu_sscanf)\n",
			str_vals[i], bu_str_vals[i]);
	    }
	}
    }
}

static void
doWidthTests(void)
{
    int i;
    int ret, bu_ret;
    void *val, *bu_val, *vals, *bu_vals;

#define SCAN_3_VALS(type, src, fmt) \
    print_src_and_fmt(src, fmt); \
    vals = bu_malloc(sizeof(type) * TS_NUM_ASSIGNMENTS, "test_sscanf vals"); \
    bu_vals = bu_malloc(sizeof(type) * TS_NUM_ASSIGNMENTS, "test_sscanf bu_vals"); \
    ret = sscanf(src, fmt, &((type*)vals)[0], &((type*)vals)[1], &((type*)vals)[2]); \
    bu_ret = bu_sscanf(src, fmt, &((type*)bu_vals)[0], &((type*)bu_vals)[1], &((type*)bu_vals)[2]); \
    checkReturnVal("sscanf", ret, 3); \
    checkReturnsEqual(bu_ret, ret);

#define TEST_INT_WIDTH(type, pfmt, src, fmt) \
    SCAN_3_VALS(type, src, fmt); \
    for (i = 0; i < TS_NUM_ASSIGNMENTS; ++i) { \
	val = (void*)&((type*)vals)[i]; \
	bu_val = (void*)&((type*)bu_vals)[i]; \
	CHECK_INT_VALS_EQUAL(type, pfmt, val, bu_val); \
    } \
    bu_free(vals, "test_sscanf vals"); \
    bu_free(bu_vals, "test_sscanf bu_vals");

#define TEST_FLOAT_WIDTH(type, pfmt, src, fmt) \
    SCAN_3_VALS(type, src, fmt); \
    for (i = 0; i < TS_NUM_ASSIGNMENTS; ++i) { \
	val = (void*)&((type*)vals)[i]; \
	bu_val = (void*)&((type*)bu_vals)[i]; \
	CHECK_FLOAT_VALS_EQUAL(type, pfmt, val, bu_val); \
    } \
    bu_free(vals, "test_sscanf vals"); \
    bu_free(bu_vals, "test_sscanf bu_vals");

    /* Stop at non-matching even if width not met. */
    TEST_INT_WIDTH(int, d, "12 34 5a6", "%5d %5d %5d");
    TEST_INT_WIDTH(int, i, "12 0042 0x3z8", "%5i %5i %5i");
    TEST_INT_WIDTH(unsigned, x, "0xC 0x22 0x38", "%5x %5x %5x");
    TEST_FLOAT_WIDTH(float, f, ".0012 .34 56.0a0", "%10f %10f %10f");
    TEST_FLOAT_WIDTH(double, f, ".0012 .34 56.0a0", "%10lf %10lf %10lf");
    test_string_width("aa AA aa", "%5s %5s %5s");
    test_string_width("1234512345123451", "%5c %5c %5c");
    test_string_width("aaAA  zzzzzz", "%5[a]%5[A] %5[z]");

    /* Stop at width even if there are more matching chars.
     * Do not include discarded whitespace in count.
     */
    TEST_INT_WIDTH(int, d, "  123\t456", " %1d%2d %3d");
    TEST_INT_WIDTH(int, i, "  10\t0x38", " %1i%1i %4i");
    TEST_INT_WIDTH(unsigned, x, "  0xC00X22\t0x38", " %4x%3x %3x");
    TEST_FLOAT_WIDTH(float, f, "  .0012\t.3456", " %3f%2f %4f");
    TEST_FLOAT_WIDTH(double, f, "  .0012\t.3456", " %3lf%2lf %4lf");
    test_string_width("  abc  ABCDE", " %2s%1s  %4s");
    test_string_width("abc  ABCD", "%2c%2c  %4c");
    test_string_width("aaAA   1%1%1%", "%2[aA]%3[A ] %5[1%]");
}

static void
doErrorTests(void)
{
    int i, ret, bu_ret;

#define FMT_ASSIGN3(fmt) \
    "%" bu_cpp_str(fmt) " %" bu_cpp_str(fmt) " %" bu_cpp_str(fmt)

#define FMT_READ2_ASSIGN1(fmt) \
    "%*" bu_cpp_str(fmt) " %*" bu_cpp_str(fmt) " %" bu_cpp_str(fmt)

    /* Attempt to assign 3 values from src.
     * If src begins with an invalid input value, should return 0 to indicate
     * a matching failure.
     * If src is empty, should return EOF to indicate input failure.
     */
#define TEST_FAILURE_1(type, type_fmt, type_init, src, expected_err) \
{ \
    type vals[3] = { type_init, type_init, type_init }; \
    print_src_and_fmt(src, FMT_ASSIGN3(type_fmt)); \
    ret = sscanf(src, FMT_ASSIGN3(type_fmt), &vals[0], &vals[1], &vals[2]); \
    bu_ret = sscanf(src, FMT_ASSIGN3(type_fmt), &vals[0], &vals[1], &vals[2]); \
    checkReturnVal("sscanf", ret, expected_err); \
    checkReturnsEqual(ret, bu_ret); \
    for (i = 0; i < 3; ++i) { \
	if (vals[i] != type_init) { \
	    bu_exit(1, "\t[FAIL] No assignment expected, but vals[%d] " \
		    "changed from %" bu_cpp_str(type_fmt) " to %" bu_cpp_str(type_fmt) ".\n", \
		    i, type_init, vals[i]); \
	} \
    } \
}

    /* Attempt to read 2 values and assign 1 value from src.
     * If src includes 2 valid and 1 invalid input value, should return 0 to
     * indicate matching failure.
     * If src includes 2 valid values and terminates, should return EOF to
     * indicate input failure.
     */
#define TEST_FAILURE_2(type, type_fmt, type_init, src, expected_err) \
{ \
    type val = type_init; \
    print_src_and_fmt(src, FMT_READ2_ASSIGN1(type_fmt)); \
    ret = sscanf(src, FMT_READ2_ASSIGN1(type_fmt), &val); \
    bu_ret = sscanf(src, FMT_READ2_ASSIGN1(type_fmt), &val); \
    checkReturnVal("sscanf", ret, expected_err); \
    checkReturnsEqual(ret, bu_ret); \
    if (val != type_init) { \
	bu_exit(1, "\t[FAIL] No assignment expected, but val " \
		"changed from %" bu_cpp_str(type_fmt) " to %" bu_cpp_str(type_fmt) ".\n", \
		type_init, val); \
    } \
}

#define EXPECT_MATCH_FAILURE 0
#define EXPECT_INPUT_FAILURE EOF

    TEST_FAILURE_1(int, d, 0, "xx 34 56", EXPECT_MATCH_FAILURE);
    TEST_FAILURE_2(int, d, 0, "12 34 xx", EXPECT_MATCH_FAILURE);
    TEST_FAILURE_2(int, d, 0, "12 34", EXPECT_INPUT_FAILURE);
    TEST_FAILURE_1(int, d, 0, "", EXPECT_INPUT_FAILURE);

    TEST_FAILURE_1(char, 1[123], 'a', "x 2 3", EXPECT_MATCH_FAILURE);
    TEST_FAILURE_2(char, 1[123], 'a', "1 2 x", EXPECT_MATCH_FAILURE);
    TEST_FAILURE_2(char, 1[123], 'a', "1 2", EXPECT_INPUT_FAILURE);
    TEST_FAILURE_1(char, 1[123], 'a', "", EXPECT_INPUT_FAILURE);
}

static void
test_vls(const char *src, const char *fmt, const char *expectedStr)
{
    int bu_ret;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    print_src_and_fmt(src, fmt);

    bu_ret = bu_sscanf(src, fmt, &vls);
    checkReturnVal("bu_sscanf", bu_ret, 1);

    if (!BU_STR_EQUAL(bu_vls_addr(&vls), expectedStr)) {
	bu_vls_free(&vls);
	bu_exit(1, "\t[FAIL] \"%s\" was assigned to vls instead of \"%s\".\n",
		bu_vls_addr(&vls), expectedStr);
    }
    bu_vls_free(&vls);
}

static void
doVlsTests(void)
{
    int bu_ret;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    /* %V */
    test_vls("de mus noc", "%V", "de mus noc");
    test_vls(" de mus noc", "%6V", " de mu");
    test_vls(" de mus noc", " %7V", "de mus ");
    test_vls("de mus noc", "%11V", "de mus noc");

    print_src_and_fmt("de mus noc", "%*11V");
    bu_ret = bu_sscanf("de mus noc", "%*11V", &vls);
    checkReturnVal("bu_sscanf", bu_ret, 0);

    /* %#V */
    test_vls(" \tabc ABC", "%#V", "abc");
    test_vls(" \tabc ABC", "%#4V", "abc");
    test_vls(" \tabc", "%#4Vs", "abc");

    print_src_and_fmt(" \tabc ABC", "%#*V");
    bu_vls_trunc(&vls, 0);
    bu_ret = bu_sscanf(" \tabc ABC", "%#*V", &vls);
    checkReturnVal("bu_sscanf", bu_ret, 0);

    bu_vls_free(&vls);
}

int
main(int argc, char *argv[])
{
    if (argc > 1) {
	printf("Warning: %s takes no arguments.\n", argv[0]);
    }

    printf("bu_sscanf: running tests\n");

    doNumericTests();
    doStringTests();
    doPointerTests();
    doNonConversionTests();
    doWidthTests();
    doErrorTests();
    doVlsTests();

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
