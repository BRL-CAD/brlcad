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
#include <stdio.h>
#include <string.h>

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
    SCAN_FLOAT, SCAN_DOUBLE, SCAN_LDOUBLE
};

static void
test_sscanf(int type, const char *src, const char *fmt) {
    int ret, bu_ret;
    void *val, *bu_val;

    ret = bu_ret = 0;
    val = bu_val = NULL;

    printf("\"%s\", \"%s\"\n", src, fmt);

    /* call sscanf and bu_sscanf with appropriately cast pointers */
#define SSCANF_TYPE(type) \
    val = bu_malloc(sizeof(type), "test_sscanf val"); \
    bu_val = bu_malloc(sizeof(type), "test_sscanf bu_val"); \
    ret = sscanf(src, fmt, (type*)val); \
    bu_ret = bu_sscanf(src, fmt, (type*)bu_val);

    switch (type) {
    case SCAN_INT:
	SSCANF_TYPE(int);
	break;
    case SCAN_UINT:
	SSCANF_TYPE(unsigned);
	break;
    case SCAN_SHORT:
	SSCANF_TYPE(short);
	break;
    case SCAN_USHORT:
	SSCANF_TYPE(unsigned short);
	break;
    case SCAN_SHORTSHORT:
	SSCANF_TYPE(char);
	break;
    case SCAN_USHORTSHORT:
	SSCANF_TYPE(unsigned char);
	break;
    case SCAN_LONG:
	SSCANF_TYPE(long);
	break;
    case SCAN_ULONG:
	SSCANF_TYPE(unsigned long);
	break;
    case SCAN_POINTER:
	ret = sscanf(src, fmt, &val);
	bu_ret = bu_sscanf(src, fmt, &bu_val);
	break;
    case SCAN_FLOAT:
	SSCANF_TYPE(float);
	break;
    case SCAN_DOUBLE:
	SSCANF_TYPE(double);
	break;
    case SCAN_LDOUBLE:
	SSCANF_TYPE(long double);
	break;
    default:
	bu_exit(1, "Error: test_sscanf was given an unrecognized pointer type.\n");
    }

    if (ret != 1) {
	printf("\tWarning: no assignment done.\n");
    }

    /* return values equal? */
    if (bu_ret != ret) {
	printf("\t[FAIL] sscanf returned %d but bu_sscanf returned %d.\n",
		ret, bu_ret);
	return;
    }


#define CHECK_INT(type, conv_spec) \
    if (*(type*)val != *(type*)bu_val) { \
	printf("\t[FAIL] conversion value mismatch.\n" \
		"\t(sscanf) %" # conv_spec " != %" # conv_spec " (bu_sscanf).\n", \
		*(type*)val, *(type*)bu_val); \
    }

#define CHECK_FLOAT(type, conv_spec) \
    if (!NEAR_EQUAL(*(type*)val, *(type*)bu_val, FLOAT_TOL)) { \
	printf("\t[FAIL] conversion value mismatch.\n" \
		"\t(sscanf) %" # conv_spec " != %" # conv_spec " (bu_sscanf).\n", \
		*(type*)val, *(type*)bu_val); \
    }

    /* conversion values equal? */
    if (val != NULL && bu_val != NULL) {

	switch (type) {
	case SCAN_INT:
	    CHECK_INT(int, d);
	    break;
	case SCAN_UINT:
	    CHECK_INT(unsigned, u);
	    break;
	case SCAN_SHORT:
	    CHECK_INT(short, hd);
	    break;
	case SCAN_USHORT:
	    CHECK_INT(unsigned short, hu);
	    break;
	case SCAN_SHORTSHORT:
	    CHECK_INT(char, hhd);
	    break;
	case SCAN_USHORTSHORT:
	    CHECK_INT(unsigned char, hhu);
	    break;
	case SCAN_LONG:
	    CHECK_INT(long, ld);
	    break;
	case SCAN_ULONG:
	    CHECK_INT(unsigned long, lu);
	    break;
	case SCAN_POINTER:
	    if (val != bu_val) {
		printf("\t[FAIL] conversion value mismatch.\n"
			"\t(sscanf) %p != %p (bu_sscanf).\n",
			val, bu_val);
	    }
	    val = bu_val = NULL;
	    break;
	case SCAN_FLOAT:
	    CHECK_FLOAT(float, e);
	    break;
	case SCAN_DOUBLE:
	    CHECK_FLOAT(double, le);
	    break;
	case SCAN_LDOUBLE:
	    CHECK_FLOAT(long double, Le);
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
doNumericTests()
{
#define TEST_SIGNED_CONSTANT(str, fmt) \
    test_sscanf(SCAN_SHORTSHORT, str, "%hh" bu_cpp_str(fmt)); \
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

#define TEST_UNSIGNED_CONSTANTS(small, med,large, fmt) \
    test_sscanf(SCAN_USHORTSHORT, bu_cpp_str(small), "%hh" bu_cpp_str(fmt)); \
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

    TEST_FLOAT_VARIANT(f);
    TEST_FLOAT_VARIANT(e);
    TEST_FLOAT_VARIANT(E);
    TEST_FLOAT_VARIANT(g);
}

/* string test routine */
static void
test_sscanf_s(const char *src, const char *fmt)
{
    int ret, bu_ret;
    char dest[STR_SIZE], bu_dest[STR_SIZE];

    ret = bu_ret = 0;

    /* ensure NULL termination even for c and [...] */
    memset(dest, '\0', STR_SIZE);
    memset(bu_dest, '\0', STR_SIZE);

    printf("\"%s\", \"%s\"\n", src, fmt);

    ret = sscanf(src, fmt, dest);
    bu_ret = bu_sscanf(src, fmt, bu_dest);

    if (ret != 1) {
	printf("\tWarning: no assignments done.\n");
    }

    /* return values equal? */
    if (bu_ret != ret) {
	printf("\t[FAIL] sscanf returned %d but bu_sscanf returned %d.\n",
		ret, bu_ret);
	return;
    }

    if (!BU_STR_EQUAL(dest, bu_dest)) {
	printf("\t[FAIL] conversion value mismatch.\n"
		"\t(sscanf) %s != %s (bu_sscanf).\n",
		dest, bu_dest);
    }
}

static void
doStringTests()
{
    int bu_ret;
    char buf[STR_SIZE];
    char *cp;

#define TEST_STR_SPACE " \t\naBc\n"
#define TEST_STR_NOSPACE "aBc"

#define TEST_TERMINATION(src, fmt) \
    puts("\"" src "\", \"" fmt "\""); \
    /* init so that 'X' appears after the last char written by bu_sscanf */ \
    memset(buf, 'X', STR_SIZE); \
    bu_ret = bu_sscanf(src, fmt, buf); \
    if (bu_ret != 1) { \
	printf("\t[FAIL] bu_sscanf returned %d. Expected 1.\n", bu_ret); \
    } \
    cp = strchr(buf, 'X');

    /* %s should append '\0' */
    TEST_TERMINATION(TEST_STR_SPACE, "%s");
    if (cp != NULL) {
	/* cp != NULL implies strchr found 'X' before finding '\0' */
	printf("\t[FAIL] bu_sscanf didn't null-terminate %%s conversion.\n");
    }

    /* %[...] should append '\0' */
    TEST_TERMINATION(TEST_STR_SPACE, "%[^z]");
    if (cp != NULL) {
	/* cp != NULL implies strchr found 'X' before finding '\0' */
	printf("\t[FAIL] bu_sscanf null-terminated %%[...] conversion.\n");
    }

    /* %c should NOT append '\0' */
    TEST_TERMINATION(TEST_STR_SPACE, "%" bu_cpp_xstr(STR_SIZE) "c");
    if (cp == NULL) {
	/* cp == NULL implies strchr found '\0' before finding 'X' */
	printf("\t[FAIL] bu_sscanf null-terminated %%c conversion.\n");
    }

    /* For %s sscanf should not include leading whitespace in the string, so
     * the following should all be quivalent.
     */
    test_sscanf_s(TEST_STR_NOSPACE, "%s");
    test_sscanf_s(TEST_STR_NOSPACE, " %s");
    test_sscanf_s(TEST_STR_SPACE, "%s");
    test_sscanf_s(TEST_STR_SPACE, " %s");

    /* For %c, leading whitespace should be included unless the conversion
     * specifier is preceeded by whitespace.
     */
    test_sscanf_s(TEST_STR_SPACE, "%c");  /* should assign ' ' */
    test_sscanf_s(TEST_STR_SPACE, " %c"); /* should assign 'a' */

    /* For %[...], should act like %s, but should assign any/only characters
     * in the class.
     */
    test_sscanf_s(TEST_STR_SPACE, "%[^A-Z]");  /* should assign " \t\na" */
    test_sscanf_s(TEST_STR_SPACE, " %[^A-Z]"); /* should assign "a"      */
    test_sscanf_s(TEST_STR_SPACE, " %[a-z]");  /* should assign "a"      */

    /* should be able to include literal ] and - characters */
    test_sscanf_s("[-[- ", "%[[-]");
    test_sscanf_s(" zZ [- ", "%[^[-]");
    test_sscanf_s(" zZ -[ ", "%[^[-]");
}

static void
doPointerTests()
{
    test_sscanf(SCAN_POINTER, "0", "%p");
    test_sscanf(SCAN_POINTER, bu_cpp_xstr(UNSIGNED_L_HEX), "%p");
}

static void
doNonConversionTests()
{
    int ret, bu_ret, count, bu_count;

#define TEST_SSCANF_NOCONV(src, fmt) \
    puts("\"" src "\", \"" fmt "\""); \
    count = bu_count = 0; \
    ret = sscanf(src, fmt, &count); \
    bu_ret = bu_sscanf(src, fmt, &bu_count); \
    if (ret != 0) { \
	bu_exit(1, "Error: sscanf returned %d. Expected 0.\n", ret); \
    } \
    if (bu_ret != ret) { \
	printf("\t[FAIL] sscanf returned %d but bu_sscanf returned %d.\n", \
		ret, bu_ret); \
    } \
    if (bu_count != count) { \
	printf("\t[FAIL] sscanf consumed %d chars, " \
		"but bu_sscanf consumed %d.\n", count, bu_count); \
    }

    /* %n - don't convert/assign, but do store consumed char count */
    TEST_SSCANF_NOCONV(". \tg\t\tn i    RTSA si sihT", ". g n i RTSA%n");
    TEST_SSCANF_NOCONV(" foo", "foo%n");

    /* %% - don't convert/assign, but do scan literal % */
    TEST_SSCANF_NOCONV("%%n    %", "%%%%n %%%n");

    /* suppressed assignments */
    TEST_SSCANF_NOCONV(bu_cpp_xstr(SIGNED_DEC), "%*d%n");
    TEST_SSCANF_NOCONV(bu_cpp_xstr(UNSIGNED_DEC), "%*u%n");
    TEST_SSCANF_NOCONV(bu_cpp_xstr(LARGE_FLT), "%*f%n");
    TEST_SSCANF_NOCONV("42 42  4.2e1", "%*d %*u %*f%n");
}

int
main(int argc, char *argv[])
{
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

    doNumericTests();
    doStringTests();
    doPointerTests();
    doNonConversionTests();

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
