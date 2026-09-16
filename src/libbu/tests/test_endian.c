/*                   T E S T _ E N D I A N . C
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

#include "common.h"

#ifdef HAVE_INTTYPES_H
#  if defined(__cplusplus)
#    define __STDC_FORMAT_MACROS
#  endif
#  include <inttypes.h>
#else
#  include "pinttypes.h"
#endif

#include "test_api.h"


static int
endian_native_byteorder(void)
{
    union {
	uint32_t value;
	unsigned char bytes[sizeof(uint32_t)];
    } order;

    order.value = 0x01020304u;

    if (order.bytes[0] == 0x04 && order.bytes[1] == 0x03 && order.bytes[2] == 0x02 && order.bytes[3] == 0x01) {
	return BU_LITTLE_ENDIAN;
    }
    if (order.bytes[0] == 0x01 && order.bytes[1] == 0x02 && order.bytes[2] == 0x03 && order.bytes[3] == 0x04) {
	return BU_BIG_ENDIAN;
    }
    if (order.bytes[0] == 0x02 && order.bytes[1] == 0x01 && order.bytes[2] == 0x04 && order.bytes[3] == 0x03) {
	return BU_PDP_ENDIAN;
    }

    return 0;
}


static int
test_endian_api(void)
{
    int errors = 0;
    bu_endian_t detected = bu_byteorder();
    int expected = endian_native_byteorder();

    TEST_API_CHECK(detected == BU_LITTLE_ENDIAN || detected == BU_BIG_ENDIAN || detected == BU_PDP_ENDIAN,
		   "bu_byteorder returned an unexpected enum value: %d", (int)detected);
    TEST_API_CHECK(expected != 0, "independent byte-order probe failed");
    TEST_API_CHECK((int)detected == expected,
		   "byte-order detection mismatch: got %d expected %d", (int)detected, expected);
    TEST_API_CHECK(bu_byteorder() == detected, "byte-order detection should be stable across calls");

    return errors ? BRLCAD_ERROR : BRLCAD_OK;
}


int
main(int UNUSED(argc), char *argv[])
{
    if (bu_getprogname()[0] == '\0') {
	bu_setprogname(argv[0]);
    }

    return test_endian_api();
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
