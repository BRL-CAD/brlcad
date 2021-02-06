/*                         E N D I A N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#ifndef BU_ENDIAN_H
#define BU_ENDIAN_H

#include "common.h"
#include "bu/defines.h"

__BEGIN_DECLS

/** @addtogroup bu_endian
 * @brief
 * Run-time byte order detection.
 *
 */
/** @{ */
/** @file bu/endian.h */


typedef enum {
    BU_LITTLE_ENDIAN = 1234, /**< LSB first: i386, VAX order */
    BU_BIG_ENDIAN    = 4321, /**< MSB first: 68000, IBM, network order */
    BU_PDP_ENDIAN    = 3412  /**< LSB first in word, MSW first in long */
} bu_endian_t;


/**
 * returns the platform byte ordering (e.g., big-/little-endian)
 */
BU_EXPORT extern bu_endian_t bu_byteorder(void);

/** @} */


/**
 * Get the current operating host's name.  This is usually also the
 * network name of the current host.  The name is written into the
 * provided hostname buffer of at least len size.  The hostname is
 * always null-terminated and should be sized accordingly.
 */
BU_EXPORT extern int bu_gethostname(char *hostname, size_t len);


__END_DECLS

#endif  /* BU_ENDIAN_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
