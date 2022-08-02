/*                        U U I D . H
 * BRL-CAD
 *
 * Copyright (c) 2016-2022 United States Government as represented by
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

#ifndef BU_UUID_H
#define BU_UUID_H

#include "common.h"

#include <stdint.h> /* for uint8_t */
#include <stdlib.h> /* for size_t */

#include "bu/defines.h"

/*----------------------------------------------------------------------*/
/** @addtogroup bu_uuid
 *
 * @brief
 * Routines to generate and work with universally unique identifiers.
 */
/** @{ */
/** @file bu/uuid.h */


#ifdef HAVE_STATIC_ARRAYS
#  define STATIC_ARRAY(x) static (x)
#else
#  define STATIC_ARRAY(x) (x)
#endif


__BEGIN_DECLS

/**
 * Create a UUID (a 128-bit identifier) that conforms with RFC4122,
 * version 4 or 5, in big endian network order.  Version 4 UUIDs are
 * suitable for a random object identifier (with 122 bits of random
 * entropy).  Providing a byte array and namespace will create a (SHA1-based)
 * version 5 UUID suitable for repeatable identifier hashing.
 */
BU_EXPORT int
bu_uuid_create(uint8_t uuid[STATIC_ARRAY(16)], size_t nbytes, const uint8_t *bytes, const uint8_t namespace_uuid[STATIC_ARRAY(16)]);

/**
 * This is a convenience UUID comparison routine compatible with
 * stdlib sorting functions (e.g., bu_sort()).  The function expects
 * both left and right UUIDs to be uint8_t[16] arrays.
 *
 * Returns:
 *
 * <0 if a < b
 *  0 if a == b
 * >0 if a > b
 */
BU_EXPORT int
bu_uuid_compare(const void *uuid_left, const void *uuid_right);

/**
 * Converts a UUID into a string representation of the following
 * style: "00112233-4455-6677-8899-AABBCCDDEEFF"
 *
 * The caller must provide a 36-byte + 1-byte (for nul) array to write
 * the result and will need to manually convert this into a string or
 * add braces as desired by the calling application.  For example:
 *
 * @code
 * uint8_t uuid[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
 * 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
 * char uuidstr[39] = {0};
 * (void)bu_uuid_encode(uuid, (uint8_t *)(uuidstr+1));
 * uuidstr[0] = '{';
 * uuidstr[sizeof(uuidstr)-2] = '}';
 * uuidstr[sizeof(uuidstr)-1] = '\0';
 * printf("UUID is %s\n", uuidstr);
 * @endcode
 */
BU_EXPORT int
bu_uuid_encode(const uint8_t uuid[STATIC_ARRAY(16)], uint8_t cp[STATIC_ARRAY(37)]);

/**
 * Converts a string (e.g., "{12B01234-f543-39d9-BFE0-0098765432F1}")
 * into a UUID.  The input string must be nul-terminated.  Brackets
 * are optional and are ignored.  Hyphens can appear anywhere or be
 * missing.  The hex digits can be any mixture of upper or lower case.
 */
BU_EXPORT int
bu_uuid_decode(const char *cp, uint8_t uuid[STATIC_ARRAY(16)]);


__END_DECLS

/** @} */

#endif  /* BU_UUID_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
