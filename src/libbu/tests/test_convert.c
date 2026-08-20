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

#include "test_api.h"


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


int
main(int UNUSED(argc), char *argv[])
{
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    return test_network_signed_conversion();
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
