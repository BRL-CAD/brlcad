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

/** values for the "allhits" argument to mg_class_pt_fu_except() */
#define NMG_FPI_FIRST   0       /**< @brief return after finding first
                                 * touch
                                 */
#define NMG_FPI_PERGEOM 1       /**< @brief find all touches, call
                                 * user funcs once for each geometry
                                 * element touched.
                                 */
#define NMG_FPI_PERUSE  2       /**< @brief find all touches, call
                                 * user funcs once for each use of
                                 * geom elements touched.
                                 */

/**
 * storage allocation/deallocation support
 */
#define NMG_GETSTRUCT(p, str) p = (struct str *)bu_calloc(1, sizeof(struct str), "NMG_GETSTRUCT")
#define NMG_FREESTRUCT(p, str) bu_free(p, "NMG_FREESTRUCT")
#define NMG_ALLOC(_ptr, _type) _ptr = (_type *)bu_calloc(1, sizeof(_type), #_type " (NMG_ALLOC) " CPP_FILELINE)

/**
 * macros to check/validate a structure pointer
 */
#define NMG_CKMAG(_ptr, _magic, _str)   BU_CKMAG(_ptr, _magic, _str)
#define NMG_CK2MAG(_ptr, _magic1, _magic2, _str) \
    if (!(_ptr) || (*((uint32_t *)(_ptr)) != (_magic1) && *((uint32_t *)(_ptr)) != (_magic2))) { \
        bu_badmagic((uint32_t *)(_ptr), _magic1, _str, __FILE__, __LINE__); \
    }

#define NMG_CK_LIST(_p)               BU_CKMAG(_p, BU_LIST_HEAD_MAGIC, "bu_list")
#define NMG_CK_RADIAL(_p) NMG_CKMAG(_p, NMG_RADIAL_MAGIC, "nmg_radial")
#define NMG_CK_INTER_STRUCT(_p) NMG_CKMAG(_p, NMG_INTER_STRUCT_MAGIC, "nmg_inter_struct")

/*
 * Macros to create and destroy storage for the NMG data structures.
 * Since nmg_mk.c and g_nmg.c are the only source file which should
 * perform these most fundamental operations, the macros do not belong
 * in nmg.h In particular, application code should NEVER do these
 * things.  Any need to do so should be handled by extending nmg_mk.c
 */
#define NMG_INCR_INDEX(_p, _m)  \
    NMG_CK_MODEL(_m); (_p)->index = ((_m)->maxindex)++


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
