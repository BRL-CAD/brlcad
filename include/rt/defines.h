/*                       D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2011-2022 United States Government as represented by
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
/** @addtogroup rt_defines
 *
 * Common definitions for LIBRT
 *
 */

/** @{ */
/** @file rt/defines.h */

#ifndef RT_DEFINES_H
#define RT_DEFINES_H

#ifndef RT_EXPORT
#  if defined(RT_DLL_EXPORTS) && defined(RT_DLL_IMPORTS)
#    error "Only RT_DLL_EXPORTS or RT_DLL_IMPORTS can be defined, not both."
#  elif defined(RT_DLL_EXPORTS)
#    define RT_EXPORT COMPILER_DLLEXPORT
#  elif defined(RT_DLL_IMPORTS)
#    define RT_EXPORT COMPILER_DLLIMPORT
#  else
#    define RT_EXPORT
#  endif
#endif

#include "common.h"

#ifdef USE_OPENCL
#include <limits.h>
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS

#define CL_TARGET_OPENCL_VERSION 120

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#ifndef CL_VERSION_1_2
#  error "OpenCL 1.2 required."
#endif

#ifdef CLT_SINGLE_PRECISION
#define cl_double cl_float
#define cl_double3 cl_float3
#define cl_double4 cl_float4
#define cl_double16 cl_float16
#endif
#endif

#include "bg/defines.h"

/*
 * Non-geometric objects (see bg/defines.h for geometric object types)
 */
#define ID_COMBINATION  31      /**< @brief Combination Record */
#define ID_UNUSED1      32      /**< @brief UNUSED (placeholder)  */
#define ID_BINUNIF      33      /**< @brief Uniform-array binary */
#define ID_UNUSED2      34      /**< @brief UNUSED (placeholder) */
#define ID_CONSTRAINT   39      /**< @brief Constraint object */
#define ID_ANNOT        42      /**< @brief Annotation */
#define ID_DATUM        44      /**< @brief Datum references */
#define ID_SCRIPT       45      /**< @brief Script */
#define ID_MATERIAL     46      /**< @brief Material object */
/* NOTE - be sure to check bg/defines.h for additional non-geometric numbers
 * currently in use before adding new numerical ID types, as well as adjusting
 * the ID_MAX_SOLID and ID_MAXIMUM values in that header if needed */

/**
 * DEPRECATED: external applications should use other LIBRT API to
 * access database objects.
 *
 * The directory is organized as forward linked lists hanging off of
 * one of RT_DBNHASH headers in the db_i structure.
 *
 * FIXME: this should not be public API, push container and iteration
 * down into LIBRT.  External applications should not use this.
 */
#define RT_DBNHASH              8192    /**< @brief hash table is an
					 * array of linked lists with
					 * this many array pointer
					 * elements (Memory use for
					 * 32-bit: 32KB, 64-bit: 64KB)
					 */

#if     ((RT_DBNHASH)&((RT_DBNHASH)-1)) != 0
/**
 * DEPRECATED: external applications should use other LIBRT API to
 * access database objects.
 */
#define RT_DBHASH(sum)  ((size_t)(sum) % (RT_DBNHASH))
#else
/**
 * DEPRECATED: external applications should use other LIBRT API to
 * access database objects.
 */
#define RT_DBHASH(sum)  ((size_t)(sum) & ((RT_DBNHASH)-1))
#endif

/* Used to set globals declared in bot.c */
#define RT_DEFAULT_MINPIECES            32
#define RT_DEFAULT_TRIS_PER_PIECE       4
#define RT_DEFAULT_MINTIE               0       /* TODO: find the best value */


#define BACKING_DIST    (-2.0)          /**< @brief  mm to look behind start point */
#define OFFSET_DIST     0.01            /**< @brief  mm to advance point into box */

/**
 * FIXME: These should probably be vmath macros
 */
#define RT_BADNUM(n)    (!((n) >= -INFINITY && (n) <= INFINITY))
#define RT_BADVEC(v)    (RT_BADNUM((v)[X]) || RT_BADNUM((v)[Y]) || RT_BADNUM((v)[Z]))

/* FIXME: this is a dubious define that should be removed */
#define RT_MAXLINE              10240

#define RT_PART_NUBSPT  0

#endif /* RT_DEFINES_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
