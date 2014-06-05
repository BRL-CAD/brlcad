/*                         S I M D . H
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

/** @file simd.h
 *
 */
#ifndef BU_SIMD_H
#define BU_SIMD_H

#include "common.h"

#include "bu/defines.h"

__BEGIN_DECLS

/** @addtogroup simd */
/** @{ */

/** @file libbu/simd.c
 * Detect SIMD type at runtime.
 */

#define BU_SIMD_SSE4_2 7
#define BU_SIMD_SSE4_1 6
#define BU_SIMD_SSE3 5
#define BU_SIMD_ALTIVEC 4
#define BU_SIMD_SSE2 3
#define BU_SIMD_SSE 2
#define BU_SIMD_MMX 1
#define BU_SIMD_NONE 0
/**
 * Detect SIMD capabilities at runtime.
 */
BU_EXPORT extern int bu_simd_level(void);

/**
 * Detect if requested SIMD capabilities are available at runtime.
 * Returns 1 if they are, 0 if they are not.
 */
BU_EXPORT extern int bu_simd_supported(int level);

/** @} */

__END_DECLS

#endif  /* BU_SIMD_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
