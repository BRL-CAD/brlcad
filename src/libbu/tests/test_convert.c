/*                  T E S T _ C O N V E R T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#include "common.h"

#include <stdint.h>

#include "test_api.h"


static int
test_cookie_signedness(void)
{
    const struct {
	const char *format;
	int expected_signed;
    } cases[] = {
	{"c", 1},
	{"s", 1},
	{"nl", 1},
	{"n16", 1},
	{"n32", 1},
	{"n64", 1},
	{"uc", 0},
	{"nus", 0},
	{"nul", 0},
	{"nss", 1},
	{"nsl", 1}
    };
    int errors = 0;
    size_t i;

    for (i = 0; i < ARRAY_LEN(cases); i++) {
	int cookie = bu_cv_cookie(cases[i].format);
	int is_signed = !!(cookie & CV_SIGNED_MASK);

	TEST_API_CHECK(cookie != 0,
	    "bu_cv_cookie rejected valid format \"%s\"", cases[i].format);
	TEST_API_CHECK(is_signed == cases[i].expected_signed,
	    "bu_cv_cookie(\"%s\") returned %s, expected %s",
	    cases[i].format, is_signed ? "signed" : "unsigned",
	    cases[i].expected_signed ? "signed" : "unsigned");
    }

    return errors ? BRLCAD_ERROR : BRLCAD_OK;
}


static int
test_network_signed_conversion(void)
{
    const unsigned char short_bytes[] = {
	0, 0x7f, 0xff, 0x80, 0x00, 0xff, 0xff, 0x00, 0x01
    };
    const unsigned char long_bytes[] = {
	0, 0x7f, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01
    };
    const short expected_shorts[] = {32767, -32768, -1, 1};
    const long expected_longs[] = {2147483647L, -2147483647L - 1L, -1L, 1L};
    short shorts[4] = {0};
    long longs[4] = {0};
    int errors = 0;
    size_t i;

    TEST_API_CHECK(bu_cv_ntohss(shorts, sizeof(shorts),
	(void *)(short_bytes + 1), 4) == 4,
	"bu_cv_ntohss did not convert all inputs");
    TEST_API_CHECK(bu_cv_ntohsl(longs, sizeof(longs),
	(void *)(long_bytes + 1), 4) == 4,
	"bu_cv_ntohsl did not convert all inputs");

    for (i = 0; i < 4; i++) {
	TEST_API_CHECK(shorts[i] == expected_shorts[i],
	    "bu_cv_ntohss[%zu] returned %d, expected %d", i,
	    (int)shorts[i], (int)expected_shorts[i]);
	TEST_API_CHECK(longs[i] == expected_longs[i],
	    "bu_cv_ntohsl[%zu] returned %ld, expected %ld", i,
	    longs[i], expected_longs[i]);
    }

    return errors ? BRLCAD_ERROR : BRLCAD_OK;
}


static int
test_64_bit_conversion(void)
{
    const unsigned char network_signed[] = {
	0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe
    };
    const unsigned char network_unsigned[] = {
	0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };
    const unsigned char signed_16[] = {0xff, 0xfe};
    const unsigned char signed_64[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe
    };
    unsigned char output_16[sizeof(signed_16)] = {0};
    unsigned char output_64[sizeof(signed_64)] = {0};
    const unsigned long expected_unsigned = (unsigned long)UINT64_MAX;
    unsigned long unsigned_value = 0;
    long signed_value = 0;
    int errors = 0;

    TEST_API_CHECK(bu_cv_w_cookie(&signed_value, bu_cv_cookie("hsl"),
	sizeof(signed_value), (void *)(network_signed + 1),
	bu_cv_cookie("nsl"), 1) == 1,
	"bu_cv_w_cookie did not convert signed network 64 to host long");
    TEST_API_CHECK(signed_value == -2,
	"signed network 64 conversion returned %ld, expected -2", signed_value);

    TEST_API_CHECK(bu_cv_w_cookie(&unsigned_value, bu_cv_cookie("hul"),
	sizeof(unsigned_value), (void *)(network_unsigned + 1),
	bu_cv_cookie("nul"), 1) == 1,
	"bu_cv_w_cookie did not convert unsigned network 64 to host long");
    TEST_API_CHECK(unsigned_value == expected_unsigned,
	"unsigned network 64 conversion returned %lu, expected %lu",
	unsigned_value, expected_unsigned);

    signed_value = -2;
    TEST_API_CHECK(bu_cv_w_cookie(output_64, bu_cv_cookie("nsl"),
	sizeof(output_64), &signed_value, bu_cv_cookie("hsl"), 1) == 1,
	"bu_cv_w_cookie did not convert signed host long to network 64");
    TEST_API_CHECK(memcmp(output_64, signed_64, sizeof(output_64)) == 0,
	"signed host long to network 64 conversion produced incorrect bytes");

    TEST_API_CHECK(bu_cv_w_cookie(output_64, bu_cv_cookie("ns64"),
	sizeof(output_64), (void *)signed_16, bu_cv_cookie("ns16"), 1) == 1,
	"bu_cv_w_cookie did not widen signed network 16 to network 64");
    TEST_API_CHECK(memcmp(output_64, signed_64, sizeof(output_64)) == 0,
	"signed network 16 to network 64 conversion produced incorrect bytes");

    TEST_API_CHECK(bu_cv_w_cookie(output_16, bu_cv_cookie("ns16"),
	sizeof(output_16), (void *)signed_64, bu_cv_cookie("ns64"), 1) == 1,
	"bu_cv_w_cookie did not narrow signed network 64 to network 16");
    TEST_API_CHECK(memcmp(output_16, signed_16, sizeof(output_16)) == 0,
	"signed network 64 to network 16 conversion produced incorrect bytes");

    return errors ? BRLCAD_ERROR : BRLCAD_OK;
}


int
main(int UNUSED(argc), char *argv[])
{
    int result = BRLCAD_OK;

    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    if (test_cookie_signedness() != BRLCAD_OK)
	result = BRLCAD_ERROR;
    if (test_network_signed_conversion() != BRLCAD_OK)
	result = BRLCAD_ERROR;
    if (test_64_bit_conversion() != BRLCAD_OK)
	result = BRLCAD_ERROR;

    return result;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
