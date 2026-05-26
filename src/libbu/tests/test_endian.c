/*                T E S T _ E N D I A N . C
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
 * ex: shiftwidth=4 tabstop=8
 */
