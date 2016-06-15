/*                         E N D I A N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/** @defgroup data Data Management */
/**   @defgroup endian Endian Support */

/** @file endian.h
 *
 */
#ifndef BU_ENDIAN_H
#define BU_ENDIAN_H

#include "common.h"
#include "bu/defines.h"

/** @addtogroup endian */
/** @{ */
/** @file libbu/endian.c
 *
 * Run-time byte order detection.
 *
 */

typedef enum {
    BU_LITTLE_ENDIAN = 1234, /* LSB first: i386, VAX order */
    BU_BIG_ENDIAN    = 4321, /* MSB first: 68000, IBM, network order */
    BU_PDP_ENDIAN    = 3412  /* LSB first in word, MSW first in long */
} bu_endian_t;


/**
 * returns the platform byte ordering (e.g., big-/little-endian)
 */
BU_EXPORT extern bu_endian_t bu_byteorder(void);

/** @} */

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
