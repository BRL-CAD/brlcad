/*                   T E S T _ S S C A N F . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file test_sscanf.c
 *
 * Tests for bu_sscanf().
 *
 * bu_sscanf() is a self-contained reimplementation of sscanf() (see
 * src/libbu/sscanf.c), not a thin wrapper around the platform's C
 * library.  Its job is to provide identical, well-defined behavior on
 * every platform regardless of how the host sscanf() happens to behave.
 *
 * These tests therefore assert bu_sscanf()'s own contract directly:
 * each case feeds a fixed input and format to bu_sscanf() and checks the
 * return value (number of fields assigned, 0 for a matching failure, or
 * EOF for an input failure) and the converted value against constants
 * that are correct on all platforms.  There is deliberately no
 * comparison against the host sscanf() and no platform-specific
 * conditioning -- bu_sscanf() is expected to give the same answer
 * everywhere, and that is precisely what is being verified.
 *
 * Failures are reported and counted rather than aborting on the first
 * one, so a single run shows every problem.
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "bu.h"
#include "vmath.h"


/* Floating point comparisons are exact enough for these literals, but
 * use a small tolerance to stay robust against representation noise.
 */
#define FLOAT_TOL 0.0005


static int tests_run = 0;
static int tests_failed = 0;


/* Failure reporters.  Each prints the offending case and bumps the
 * failure count; the caller continues to the next test.
 */
static void
fail_return(const char *src, const char *fmt, int expected, int actual)
{
    bu_log("[FAIL] bu_sscanf(\"%s\", \"%s\"): returned %d, expected %d\n",
	   src, fmt, actual, expected);
    ++tests_failed;
}

static void
fail_signed(const char *src, const char *fmt, long long expected, long long actual)
{
    bu_log("[FAIL] bu_sscanf(\"%s\", \"%s\"): value %lld, expected %lld\n",
	   src, fmt, actual, expected);
    ++tests_failed;
}

static void
fail_unsigned(const char *src, const char *fmt, unsigned long long expected, unsigned long long actual)
{
    bu_log("[FAIL] bu_sscanf(\"%s\", \"%s\"): value %llu, expected %llu\n",
	   src, fmt, actual, expected);
    ++tests_failed;
}

static void
fail_float(const char *src, const char *fmt, double expected, double actual)
{
    bu_log("[FAIL] bu_sscanf(\"%s\", \"%s\"): value %g, expected %g\n",
	   src, fmt, actual, expected);
    ++tests_failed;
}

static void
fail_string(const char *src, const char *fmt, const char *expected, const char *actual)
{
    bu_log("[FAIL] bu_sscanf(\"%s\", \"%s\"): value \"%s\", expected \"%s\"\n",
	   src, fmt, actual, expected);
    ++tests_failed;
}


/* Typed integer checkers.  A correctly-typed destination is required so
 * that bu_sscanf()'s width-modifier handling (hh/h/l/ll/z/t) writes the
 * right number of bytes; passing the wrong type would corrupt the stack.
 * The destination is pre-seeded with a sentinel so an unexpected write
 * on a non-assigning case is caught.
 */
#define DEFINE_SIGNED_CHECK(name, TYPE) \
static void \
name(const char *src, const char *fmt, int expect_ret, long long expect_val) \
{ \
    TYPE got = (TYPE)-99; \
    int ret; \
    ++tests_run; \
    ret = bu_sscanf(src, fmt, &got); \
    if (ret != expect_ret) { fail_return(src, fmt, expect_ret, ret); return; } \
    if (expect_ret >= 1 && (long long)got != expect_val) \
	fail_signed(src, fmt, expect_val, (long long)got); \
}

#define DEFINE_UNSIGNED_CHECK(name, TYPE) \
static void \
name(const char *src, const char *fmt, int expect_ret, unsigned long long expect_val) \
{ \
    TYPE got = (TYPE)0; \
    int ret; \
    ++tests_run; \
    ret = bu_sscanf(src, fmt, &got); \
    if (ret != expect_ret) { fail_return(src, fmt, expect_ret, ret); return; } \
    if (expect_ret >= 1 && (unsigned long long)got != expect_val) \
	fail_unsigned(src, fmt, expect_val, (unsigned long long)got); \
}

#define DEFINE_FLOAT_CHECK(name, TYPE) \
static void \
name(const char *src, const char *fmt, int expect_ret, double expect_val) \
{ \
    TYPE got = (TYPE)0; \
    int ret; \
    ++tests_run; \
    ret = bu_sscanf(src, fmt, &got); \
    if (ret != expect_ret) { fail_return(src, fmt, expect_ret, ret); return; } \
    if (expect_ret >= 1 && !NEAR_EQUAL((double)got, expect_val, FLOAT_TOL)) \
	fail_float(src, fmt, expect_val, (double)got); \
}

DEFINE_SIGNED_CHECK(check_schar, signed char)
DEFINE_UNSIGNED_CHECK(check_uchar, unsigned char)
DEFINE_SIGNED_CHECK(check_short, short)
DEFINE_UNSIGNED_CHECK(check_ushort, unsigned short)
DEFINE_SIGNED_CHECK(check_int, int)
DEFINE_UNSIGNED_CHECK(check_uint, unsigned int)
DEFINE_SIGNED_CHECK(check_long, long)
DEFINE_UNSIGNED_CHECK(check_ulong, unsigned long)
DEFINE_SIGNED_CHECK(check_llong, long long)
DEFINE_UNSIGNED_CHECK(check_ullong, unsigned long long)
DEFINE_UNSIGNED_CHECK(check_size, size_t)
DEFINE_SIGNED_CHECK(check_ptrdiff, ptrdiff_t)

DEFINE_FLOAT_CHECK(check_float, float)
DEFINE_FLOAT_CHECK(check_double, double)
DEFINE_FLOAT_CHECK(check_ldouble, long double)


/* Scan into a fixed char buffer (%s, %c, %[...]).  The buffer is zeroed
 * so that comparisons work even for %c/%[...], which do not append a
 * terminating NUL.
 */
static void
check_str(const char *src, const char *fmt, int expect_ret, const char *expect_val)
{
    char got[256];
    int ret;
    ++tests_run;
    memset(got, 0, sizeof(got));
    ret = bu_sscanf(src, fmt, got);
    if (ret != expect_ret) { fail_return(src, fmt, expect_ret, ret); return; }
    if (expect_ret >= 1 && !BU_STR_EQUAL(got, expect_val))
	fail_string(src, fmt, expect_val, got);
}

/* Scan into a bu_vls (the %V / %#V extension). */
static void
check_vls(const char *src, const char *fmt, int expect_ret, const char *expect_val)
{
    struct bu_vls got = BU_VLS_INIT_ZERO;
    int ret;
    ++tests_run;
    ret = bu_sscanf(src, fmt, &got);
    if (ret != expect_ret) {
	fail_return(src, fmt, expect_ret, ret);
	bu_vls_free(&got);
	return;
    }
    if (expect_ret >= 1 && !BU_STR_EQUAL(bu_vls_cstr(&got), expect_val))
	fail_string(src, fmt, expect_val, bu_vls_cstr(&got));
    bu_vls_free(&got);
}

/* Check the return value only, for cases that assign nothing (pure
 * literal/whitespace matching or all-suppressed conversions).
 */
static void
check_ret(const char *src, const char *fmt, int expect_ret)
{
    int ret;
    ++tests_run;

    int (*f)(const char*, const char*, ...) = bu_sscanf; // bypass compiler warning about fmt being non-literal
    ret = f(src, fmt);
    if (ret != expect_ret)
	fail_return(src, fmt, expect_ret, ret);
}

/* Two-integer checker, for partial-assignment return semantics. */
static void
check_int2(const char *src, const char *fmt, int expect_ret, int expect_a, int expect_b)
{
    int a = -99, b = -99, ret;
    ++tests_run;
    ret = bu_sscanf(src, fmt, &a, &b);
    if (ret != expect_ret) { fail_return(src, fmt, expect_ret, ret); return; }
    if (expect_ret >= 1 && a != expect_a) fail_signed(src, fmt, expect_a, a);
    if (expect_ret >= 2 && b != expect_b) fail_signed(src, fmt, expect_b, b);
}

/* %n reports characters consumed so far; it is not counted as an
 * assignment, so the surrounding conversions determine the return value.
 */
static void
check_n(const char *src, const char *fmt, int expect_ret, int expect_n)
{
    int n = -1, ret;
    ++tests_run;
    ret = bu_sscanf(src, fmt, &n);
    if (ret != expect_ret) { fail_return(src, fmt, expect_ret, ret); return; }
    if (n != expect_n) fail_signed(src, fmt, expect_n, n);
}

/* %p should round-trip a pointer printed by the C library. */
static void
check_ptr(void *p)
{
    char buf[64];
    void *got = NULL;
    int ret;
    ++tests_run;
    snprintf(buf, sizeof(buf), "%p", p);
    ret = bu_sscanf(buf, "%p", &got);
    if (ret != 1) { fail_return(buf, "%p", 1, ret); return; }
    if (got != p) {
	bu_log("[FAIL] bu_sscanf(\"%s\", \"%%p\"): value %p, expected %p\n", buf, got, p);
	++tests_failed;
    }
}


static void
test_integers(void)
{
    /* every length modifier, signed and unsigned, at its type limit */
    check_schar("127", "%hhd", 1, 127);
    check_schar("-128", "%hhd", 1, -128);
    check_uchar("255", "%hhu", 1, 255);
    check_short("32767", "%hd", 1, 32767);
    check_short("-32768", "%hd", 1, -32768);
    check_ushort("65535", "%hu", 1, 65535);
    check_int("2147483647", "%d", 1, 2147483647);
    check_int("-2147483648", "%d", 1, -2147483647 - 1);
    check_uint("4294967295", "%u", 1, 4294967295ULL);
    check_long("2147483647", "%ld", 1, 2147483647);
    check_ulong("4294967295", "%lu", 1, 4294967295ULL);
    check_llong("9223372036854775807", "%lld", 1, 9223372036854775807LL);
    check_ullong("18446744073709551615", "%llu", 1, 18446744073709551615ULL);
    check_size("123456", "%zu", 1, 123456);
    check_ptrdiff("-123456", "%td", 1, -123456);

    /* signs and explicit plus */
    check_int("+42", "%d", 1, 42);
    check_int("-42", "%d", 1, -42);

    /* %i base detection: 0x -> hex, leading 0 -> octal, else decimal */
    check_int("42", "%i", 1, 42);
    check_int("0x1F", "%i", 1, 31);
    check_int("0755", "%i", 1, 493);

    /* explicit bases */
    check_uint("0755", "%o", 1, 493);
    check_uint("deadBEEF", "%x", 1, 0xdeadbeefULL);
    check_uint("0x1f", "%x", 1, 31);
}

static void
test_floats(void)
{
    check_float("3.14159", "%f", 1, 3.14159);
    check_float(".5", "%f", 1, 0.5);
    check_float("-2.5", "%f", 1, -2.5);
    check_double("3.14159", "%lf", 1, 3.14159);
    check_double("1.5e3", "%lf", 1, 1500.0);
    check_double("-2.5E-2", "%lf", 1, -0.025);
    check_double("6.022e23", "%lg", 1, 6.022e23);
    check_ldouble("2.718281828", "%Lf", 1, 2.718281828);
}

static void
test_strings(void)
{
    /* bu_sscanf intentionally requires %s and %[...] to carry a maximum
     * field width so a conversion can never overrun its buffer; the
     * widths below are part of the contract being exercised.
     */

    /* %s: skips leading whitespace, stops at whitespace */
    check_str("hello", "%255s", 1, "hello");
    check_str("  hello", "%255s", 1, "hello");
    check_str("hello world", "%255s", 1, "hello");
    check_str("hello", "%3s", 1, "hel");

    /* %c: no whitespace skipping, no NUL appended (width-bounded already) */
    check_str("abc", "%c", 1, "a");
    check_str("abc", "%3c", 1, "abc");

    /* %[...]: scanset, negated scanset, width, no whitespace skipping */
    check_str("abc123", "%255[a-c]", 1, "abc");
    check_str("abc123", "%255[^0-9]", 1, "abc");
    check_str("aaabbb", "%255[a]", 1, "aaa");
    check_str("aaaa", "%2[a]", 1, "aa");
}

static void
test_vls(void)
{
    /* %V grabs the entire remaining input (no whitespace stop) */
    check_vls("hello", "%V", 1, "hello");
    check_vls("hello world", "%V", 1, "hello world");
    check_vls("hello", "%3V", 1, "hel");

    /* %#V skips leading whitespace and stops at whitespace, like %s */
    check_vls("  hello world", "%#V", 1, "hello");
}

static void
test_misc_conversions(void)
{
    /* %p round-trips */
    check_ptr(NULL);
    check_ptr((void *)&tests_run);

    /* %n records characters consumed, does not count as an assignment */
    check_n("12345", "%*d%n", 0, 5);
    check_n("abcdef", "%*3s%n", 0, 3);

    /* assignment suppression: suppressed fields are consumed but not counted */
    check_int("42 99", "%*d %d", 1, 99);
    check_ret("42 99", "%*d %*d", 0);

    /* literal and whitespace matching */
    check_ret("abc", "abc", 0);
    check_int("val=42", "val=%d", 1, 42);
    check_int("  42", "%d", 1, 42);

    /* %% matches a literal percent */
    check_ret("100%", "100%%", 0);
    check_int("50% off", "%d%% off", 1, 50);
}

static void
test_return_semantics(void)
{
    /* full success */
    check_int2("12 34", "%d %d", 2, 12, 34);

    /* matching failure at the first directive -> 0 */
    check_int("abc", "%d", 0, 0);
    check_int("xx42", "%d", 0, 0);

    /* input failure (empty input) before any conversion -> EOF */
    check_int("", "%d", EOF, 0);

    /* partial assignment: first succeeds, second fails to match -> 1 */
    check_int2("12 abc", "%d %d", 1, 12, 0);

    /* input exhausted after the first assignment completes -> count so
     * far (1), not EOF, because input was already consumed
     */
    check_int2("12", "%d %d", 1, 12, 0);

    /* input exhausted during the final conversion, but earlier
     * (suppressed) conversions already consumed input -> matching
     * failure (0), not EOF.  The host sscanf() disagrees across
     * platforms here; bu_sscanf() is defined to return 0 everywhere.
     */
    check_int("12 34", "%*d %*d %d", 0, 0);
}


int
main(int argc, char *argv[])
{
    if (argc > 1)
	printf("Warning: %s takes no arguments.\n", argv[0]);

    bu_setprogname(argv[0]);

    printf("bu_sscanf: running tests\n");

    test_integers();
    test_floats();
    test_strings();
    test_vls();
    test_misc_conversions();
    test_return_semantics();

    printf("bu_sscanf: %d tests run, %d failed\n", tests_run, tests_failed);

    return (tests_failed == 0) ? 0 : 1;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
