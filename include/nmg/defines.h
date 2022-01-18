/*                        D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup nmg_defines
 * @brief
 * Common definitions for the headers used in nmg.h (i.e. the headers found in include/nmg)
 */
/** @{ */
/** @file nmg/defines.h */

#ifndef NMG_DEFINES_H
#define NMG_DEFINES_H

#include "common.h"

#ifndef NMG_EXPORT
#  if defined(NMG_DLL_EXPORTS) && defined(NMG_DLL_IMPORTS)
#    error "Only NMG_DLL_EXPORTS or NMG_DLL_IMPORTS can be defined, not both."
#  elif defined(NMG_DLL_EXPORTS)
#    define NMG_EXPORT COMPILER_DLLEXPORT
#  elif defined(NMG_DLL_IMPORTS)
#    define NMG_EXPORT COMPILER_DLLIMPORT
#  else
#    define NMG_EXPORT
#  endif
#endif

/* Boolean operations */
#define NMG_BOOL_SUB   1        /**< @brief subtraction */
#define NMG_BOOL_ADD   2        /**< @brief addition/union */
#define NMG_BOOL_ISECT 4        /**< @brief intersection */

/* Boolean classifications */
#define NMG_CLASS_Unknown   -1
#define NMG_CLASS_AinB       0
#define NMG_CLASS_AonBshared 1
#define NMG_CLASS_AonBanti   2
#define NMG_CLASS_AoutB      3
#define NMG_CLASS_BinA       4
#define NMG_CLASS_BonAshared 5
#define NMG_CLASS_BonAanti   6
#define NMG_CLASS_BoutA      7

/* orientations available.  All topological elements are orientable. */
#define OT_NONE      0 /**< @brief no orientation (error) */
#define OT_SAME      1 /**< @brief orientation same */
#define OT_OPPOSITE  2 /**< @brief orientation opposite */
#define OT_UNSPEC    3 /**< @brief orientation unspecified */
#define OT_BOOLPLACE 4 /**< @brief object is intermediate data for boolean ops */

/**
 * macros to check/validate a structure pointer
 */
#define NMG_CKMAG(_ptr, _magic, _str)   BU_CKMAG(_ptr, _magic, _str)
#define NMG_CK2MAG(_ptr, _magic1, _magic2, _str) \
    if (!(_ptr) || (*((uint32_t *)(_ptr)) != (_magic1) && *((uint32_t *)(_ptr)) != (_magic2))) { \
        bu_badmagic((uint32_t *)(_ptr), _magic1, _str, __FILE__, __LINE__); \
    }

#define NMG_CK_LIST(_p)               BU_CKMAG(_p, BU_LIST_HEAD_MAGIC, "bu_list")

#endif  /* NMG_DEFINES_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
