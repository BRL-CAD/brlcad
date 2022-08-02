/*                        D E B U G . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @addtogroup rt_debug
 *
 * @brief librt debugging information
 *
 */
/** @{ */
/** @file rt/debug.h */

#ifndef RT_DEBUG_H
#define RT_DEBUG_H

#include "common.h"
#include "rt/defines.h"

__BEGIN_DECLS

/**
 * Each type of debugging support is independently controlled, by a
 * separate bit in the word RT_G_DEBUG
 *
 * For programs based on the "RT" program, these flags follow the "-x"
 * (lower case x) option.
 */
#define RT_DEBUG_OFF	0	/**< @brief No debugging */

/* These definitions are each for one bit */

/* Options useful for debugging applications */
#define RT_DEBUG_ALLRAYS	0x00000001	/* 1 Print calls to rt_shootray() */
#define RT_DEBUG_ALLHITS	0x00000002	/* 2 Print partitions passed to a_hit() */
#define RT_DEBUG_SHOOT		0x00000004	/* 3 Info about rt_shootray() processing */
#define RT_DEBUG_INSTANCE	0x00000008	/* 4 regionid instance revectoring */

/* Options useful for debugging the database */
#define RT_DEBUG_DB		0x00000010	/* 5 Database debugging */
#define RT_DEBUG_SOLIDS		0x00000020	/* 6 Print prep'ed solids */
#define RT_DEBUG_REGIONS	0x00000040	/* 7 Print regions & boolean trees */
#define RT_DEBUG_ARB8		0x00000080	/* 8 Print voluminous ARB8 details */

#define RT_DEBUG_SPLINE		0x00000100	/* 9 Splines */
#define RT_DEBUG_ANIM		0x00000200	/* 10 Animation */
#define RT_DEBUG_ANIM_FULL	0x00000400	/* 11 Animation matrices */
#define RT_DEBUG_VOL		0x00000800	/* 12 Volume & opaque Binary solid */

/* Options useful for debugging the library */
#define RT_DEBUG_ROOTS		0x00001000	/* 13 Print rootfinder details */
#define RT_DEBUG_PARTITION	0x00002000	/* 14 Info about bool_weave() */
#define RT_DEBUG_CUT		0x00004000	/* 15 Print space cutting statistics */
#define RT_DEBUG_BOXING		0x00008000	/* 16 Object/box checking details */

#define RT_DEBUG_UNUSED_0	0x00010000	/* 17 -->> BU_DEBUG_MEM_LOG */
#define RT_DEBUG_UNUSED_1	0x00020000	/* 18 Unassigned */
#define RT_DEBUG_FDIFF		0x00040000	/* 19 bool/fdiff debugging */
#define RT_DEBUG_PARALLEL	0x00080000	/* 20 -->> BU_DEBUG_PARALLEL */

#define RT_DEBUG_CUTDETAIL	0x00100000	/* 21 Print space cutting details */
#define RT_DEBUG_TREEWALK	0x00200000	/* 22 Database tree traversal */
#define RT_DEBUG_TESTING	0x00400000	/* 23 One-shot debugging flag */
#define RT_DEBUG_ADVANCE	0x00800000	/* 24 Cell-to-cell space partitioning */

#define RT_DEBUG_MATH		0x01000000	/* 25 nmg math routines */

/* Options for debugging particular solids */
#define RT_DEBUG_EBM		0x02000000	/* 26 Extruded bit-map solids */
#define RT_DEBUG_HF		0x04000000	/* 27 Height Field solids */

#define RT_DEBUG_MESHING	0x08000000	/* 28 Print meshing/triangulation details */
#define RT_DEBUG_UNUSED_3	0x10000000	/* 29 Unassigned */
#define RT_DEBUG_UNUSED_4	0x20000000	/* 30 Unassigned */

/* Options which will cause the library to write binary debugging output */
#define RT_DEBUG_PL_SOLIDS 	0x40000000	/* 31 plot all solids */
#define RT_DEBUG_PL_BOX		0x80000000	/* 32 Plot(3) bounding boxes and cuts */

/** Format string for bu_printb() */
#define RT_DEBUG_FORMAT	"\020" /* print hex */ \
    "\040PL_BOX" \
    "\037PL_SOLIDS" \
    "\036UNUSED_4" \
    "\035UNUSED_3" \
    "\034UNUSED_2" \
    "\033HF" \
    "\032EBM" \
    "\031MATH" \
    "\030ADVANCE" \
    "\027TESTING" \
    "\026TREEWALK" \
    "\025CUTDETAIL" \
    "\024PARALLEL" \
    "\023FDIFF" \
    "\022UNUSED_1" \
    "\021UNUSED_0" \
    "\020BOXING" \
    "\017CUT" \
    "\016PARTITION" \
    "\015ROOTS" \
    "\014VOL" \
    "\013ANIM_FULL" \
    "\012ANIM" \
    "\011SPLINE" \
    "\010ARB8" \
    "\7REGIONS" \
    "\6SOLIDS" \
    "\5DB" \
    "\4INSTANCE" \
    "\3SHOOT" \
    "\2ALLHITS" \
    "\1ALLRAYS"


/**
 * controls the librt debug level
 */
RT_EXPORT extern unsigned int rt_debug;

/* Normally set when in production mode, setting the RT_G_DEBUG define
 * to 0 will allow chucks of code to poof away at compile time (since
 * they are truth-functionally constant (false)) This can boost
 * raytrace performance considerably (~10%).
 */
#ifdef NO_DEBUG_CHECKING
#  define RT_G_DEBUG 0
#else
#  define RT_G_DEBUG rt_debug
#endif

__END_DECLS

#endif /* RT_DEBUG_H */
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
