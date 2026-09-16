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

#include "bu/endian.h"
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
test_32_bit_cookie_conversion(void)
{
    const unsigned char network_signed[] = {
	0, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xfe
    };
    const unsigned char network_unsigned[] = {
	0, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01
    };
    const unsigned char expected_network[] = {
	0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xfe
    };
    const int signed_input[] = {1, -2};
    const double double_input[] = {1.0, -2.0};
    unsigned char network_output[sizeof(expected_network)] = {0};
    unsigned int unsigned_output[2] = {0};
    int signed_output[2] = {0};
    double double_output[2] = {0.0};
    struct {
	int values[2];
	uint64_t guard;
    } bounded_output = {{0}, UINT64_C(0x0123456789abcdef)};
    const uint64_t expected_guard = bounded_output.guard;
    int errors = 0;

    TEST_API_CHECK(sizeof(int) == sizeof(uint32_t),
	"CV_32 host int is %zu bytes, expected %zu", sizeof(int), sizeof(uint32_t));

    TEST_API_CHECK(bu_cv_w_cookie(signed_output, bu_cv_cookie("hsi"),
	sizeof(signed_output), (void *)(network_signed + 1),
	bu_cv_cookie("nsi"), 2) == 2,
	"bu_cv_w_cookie did not convert all signed network 32 inputs");
    TEST_API_CHECK(signed_output[0] == 1 && signed_output[1] == -2,
	"signed network 32 conversion returned {%d, %d}, expected {1, -2}",
	signed_output[0], signed_output[1]);

    TEST_API_CHECK(bu_cv_w_cookie(unsigned_output, bu_cv_cookie("hui"),
	sizeof(unsigned_output), (void *)(network_unsigned + 1),
	bu_cv_cookie("nui"), 2) == 2,
	"bu_cv_w_cookie did not convert all unsigned network 32 inputs");
    TEST_API_CHECK(unsigned_output[0] == (unsigned int)UINT32_MAX &&
	unsigned_output[1] == 1,
	"unsigned network 32 conversion returned {%u, %u}, expected {%u, 1}",
	unsigned_output[0], unsigned_output[1], (unsigned int)UINT32_MAX);

    TEST_API_CHECK(bu_cv_w_cookie(network_output, bu_cv_cookie("nsi"),
	sizeof(network_output), (void *)signed_input, bu_cv_cookie("hsi"), 2) == 2,
	"bu_cv_w_cookie did not convert all signed host 32 inputs");
    TEST_API_CHECK(memcmp(network_output, expected_network,
	sizeof(network_output)) == 0,
	"signed host 32 to network conversion produced incorrect bytes");

    TEST_API_CHECK(bu_cv_w_cookie(double_output, bu_cv_cookie("hd"),
	sizeof(double_output), (void *)signed_input, bu_cv_cookie("hsi"), 2) == 2,
	"bu_cv_w_cookie did not widen all host 32 inputs to double");
    TEST_API_CHECK(test_api_close_enough(double_output[0], 1.0, 0.0, 0.0) &&
	test_api_close_enough(double_output[1], -2.0, 0.0, 0.0),
	"host 32 to double conversion returned {%g, %g}, expected {1, -2}",
	double_output[0], double_output[1]);

    double_output[0] = double_output[1] = 0.0;
    TEST_API_CHECK(bu_cv_w_cookie(double_output, bu_cv_cookie("hd"),
	sizeof(double_output), (void *)(network_signed + 1),
	bu_cv_cookie("nsi"), 2) == 2,
	"bu_cv_w_cookie did not widen all network 32 inputs to double");
    TEST_API_CHECK(test_api_close_enough(double_output[0], 1.0, 0.0, 0.0) &&
	test_api_close_enough(double_output[1], -2.0, 0.0, 0.0),
	"network 32 to double conversion returned {%g, %g}, expected {1, -2}",
	double_output[0], double_output[1]);

    TEST_API_CHECK(bu_cv_w_cookie(bounded_output.values, bu_cv_cookie("hsi"),
	sizeof(bounded_output.values), (void *)double_input, bu_cv_cookie("hd"), 2) == 2,
	"bu_cv_w_cookie did not narrow all doubles to host 32");
    TEST_API_CHECK(bounded_output.values[0] == 1 && bounded_output.values[1] == -2,
	"double to host 32 conversion returned {%d, %d}, expected {1, -2}",
	bounded_output.values[0], bounded_output.values[1]);
    TEST_API_CHECK(bounded_output.guard == expected_guard,
	"double to host 32 conversion wrote beyond the output buffer");

    memset(network_output, 0, sizeof(network_output));
    TEST_API_CHECK(bu_cv_w_cookie(network_output, bu_cv_cookie("nsi"),
	sizeof(network_output), (void *)double_input, bu_cv_cookie("hd"), 2) == 2,
	"bu_cv_w_cookie did not narrow all doubles to network 32");
    TEST_API_CHECK(memcmp(network_output, expected_network,
	sizeof(network_output)) == 0,
	"double to network 32 conversion produced incorrect bytes");

    return errors ? BRLCAD_ERROR : BRLCAD_OK;
}


static int
test_double_cookie_conversion(void)
{
    const unsigned char network_values[] = {
	0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    const double host_values[] = {1.0, -2.0};
    const int integer_values[] = {1, -2};
    unsigned char network_output[sizeof(network_values)] = {0};
    double host_output[2] = {0.0};
    int integer_output[2] = {0};
    struct {
	unsigned char value[SIZEOF_NETWORK_DOUBLE];
	uint64_t guard;
    } bounded_output = {{0}, UINT64_C(0x0123456789abcdef)};
    const uint64_t expected_guard = bounded_output.guard;
    const int optimized = bu_cv_optimize(bu_cv_cookie("nd"));
    const int expected_host = sizeof(double) == SIZEOF_NETWORK_DOUBLE &&
	bu_byteorder() == BU_BIG_ENDIAN;
    int errors = 0;

    TEST_API_CHECK(!!(optimized & CV_HOST_MASK) == expected_host,
	"bu_cv_optimize treated network double as %s on this host",
	(optimized & CV_HOST_MASK) ? "host format" : "network format");

    TEST_API_CHECK(bu_cv_w_cookie(host_output, bu_cv_cookie("hd"),
	sizeof(host_output), (void *)network_values, bu_cv_cookie("nd"), 2) == 2,
	"bu_cv_w_cookie did not convert all network doubles to host doubles");
    TEST_API_CHECK(test_api_close_enough(host_output[0], 1.0, 0.0, 0.0) &&
	test_api_close_enough(host_output[1], -2.0, 0.0, 0.0),
	"network double conversion returned {%g, %g}, expected {1, -2}",
	host_output[0], host_output[1]);

    TEST_API_CHECK(bu_cv_w_cookie(network_output, bu_cv_cookie("nd"),
	sizeof(network_output), (void *)host_values, bu_cv_cookie("hd"), 2) == 2,
	"bu_cv_w_cookie did not convert all host doubles to network doubles");
    TEST_API_CHECK(memcmp(network_output, network_values,
	sizeof(network_output)) == 0,
	"host double conversion produced incorrect network bytes");

    TEST_API_CHECK(bu_cv_w_cookie(integer_output, bu_cv_cookie("hsi"),
	sizeof(integer_output), (void *)network_values, bu_cv_cookie("nd"), 2) == 2,
	"bu_cv_w_cookie did not convert all network doubles to host integers");
    TEST_API_CHECK(integer_output[0] == 1 && integer_output[1] == -2,
	"network double to host integer conversion returned {%d, %d}, expected {1, -2}",
	integer_output[0], integer_output[1]);

    memset(network_output, 0, sizeof(network_output));
    TEST_API_CHECK(bu_cv_w_cookie(network_output, bu_cv_cookie("nd"),
	sizeof(network_output), (void *)integer_values, bu_cv_cookie("hsi"), 2) == 2,
	"bu_cv_w_cookie did not convert all host integers to network doubles");
    TEST_API_CHECK(memcmp(network_output, network_values,
	sizeof(network_output)) == 0,
	"host integer to network double conversion produced incorrect bytes");

    TEST_API_CHECK(bu_cv_w_cookie(bounded_output.value, bu_cv_cookie("nd"),
	sizeof(bounded_output.value), (void *)host_values, bu_cv_cookie("hd"), 2) == 1,
	"host double conversion did not honor the network output size");
    TEST_API_CHECK(memcmp(bounded_output.value, network_values,
	sizeof(bounded_output.value)) == 0,
	"bounded host double conversion produced incorrect network bytes");
    TEST_API_CHECK(bounded_output.guard == expected_guard,
	"host double conversion wrote beyond the network output buffer");

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
    if (test_32_bit_cookie_conversion() != BRLCAD_OK)
	result = BRLCAD_ERROR;
    if (test_double_cookie_conversion() != BRLCAD_OK)
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
