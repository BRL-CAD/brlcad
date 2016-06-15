/*                        D E B U G . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2016 United States Government as represented by
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

__BEGIN_DECLS

/**
 * Each type of debugging support is independently controlled, by a
 * separate bit in the word RT_G_DEBUG
 *
 * For programs based on the "RT" program, these flags follow the "-x"
 * (lower case x) option.
 */
#define DEBUG_OFF	0	/**< @brief No debugging */

/* These definitions are each for one bit */

/* Options useful for debugging applications */
#define DEBUG_ALLRAYS	0x00000001	/**< @brief 1 Print calls to rt_shootray() */
#define DEBUG_ALLHITS	0x00000002	/**< @brief 2 Print partitions passed to a_hit() */
#define DEBUG_SHOOT	0x00000004	/**< @brief 3 Info about rt_shootray() processing */
#define DEBUG_INSTANCE	0x00000008	/**< @brief 4 regionid instance revectoring */

/* Options useful for debugging the database */
#define DEBUG_DB	0x00000010	/**< @brief 5 Database debugging */
#define DEBUG_SOLIDS	0x00000020	/**< @brief 6 Print prep'ed solids */
#define DEBUG_REGIONS	0x00000040	/**< @brief 7 Print regions & boolean trees */
#define DEBUG_ARB8	0x00000080	/**< @brief 8 Print voluminous ARB8 details */
#define DEBUG_SPLINE	0x00000100	/**< @brief 9 Splines */
#define DEBUG_ANIM	0x00000200	/**< @brief 10 Animation */
#define DEBUG_ANIM_FULL	0x00000400	/**< @brief 11 Animation matrices */
#define DEBUG_VOL	0x00000800	/**< @brief 12 Volume & opaque Binary solid */

/* Options useful for debugging the library */
#define DEBUG_ROOTS	0x00001000	/**< @brief 13 Print rootfinder details */
#define DEBUG_PARTITION	0x00002000	/**< @brief 14 Info about bool_weave() */
#define DEBUG_CUT	0x00004000	/**< @brief 15 Print space cutting statistics */
#define DEBUG_BOXING	0x00008000	/**< @brief 16 Object/box checking details */
#define DEBUG_MEM	0x00010000	/**< @brief 17 -->> BU_DEBUG_MEM_LOG */
#define DEBUG_MEM_FULL	0x00020000	/**< @brief 18 -->> BU_DEBUG_MEM_CHECK */
#define DEBUG_FDIFF	0x00040000	/**< @brief 19 bool/fdiff debugging */
#define DEBUG_PARALLEL	0x00080000	/**< @brief 20 -->> BU_DEBUG_PARALLEL */
#define DEBUG_CUTDETAIL	0x00100000	/**< @brief 21 Print space cutting details */
#define DEBUG_TREEWALK	0x00200000	/**< @brief 22 Database tree traversal */
#define DEBUG_TESTING	0x00400000	/**< @brief 23 One-shot debugging flag */
#define DEBUG_ADVANCE	0x00800000	/**< @brief 24 Cell-to-cell space partitioning */
#define DEBUG_MATH	0x01000000	/**< @brief 25 nmg math routines */

/* Options for debugging particular solids */
#define DEBUG_EBM	0x02000000	/**< @brief 26 Extruded bit-map solids */
#define DEBUG_HF	0x04000000	/**< @brief 27 Height Field solids */

#define DEBUG_UNUSED1	0x08000000	/**< @brief 28 unused */
#define DEBUG_UNUSED2	0x10000000	/**< @brief 29 unused */
#define DEBUG_UNUSED3	0x20000000	/**< @brief 30 unused */

/* Options which will cause the library to write binary debugging output */
#define DEBUG_PL_SOLIDS 0x40000000	/**< @brief 31 plot all solids */
#define DEBUG_PL_BOX	0x80000000	/**< @brief 32 Plot(3) bounding boxes and cuts */

/** Format string for bu_printb() */
#define DEBUG_FORMAT	\
    "\020\040PLOTBOX\
\037PLOTSOLIDS\
\033HF\032EBM\031MATH\030ADVANCE\
\027TESTING\026TREEWALK\025CUTDETAIL\024PARALLEL\023FDIFF\022MEM_FULL\
\021MEM\020BOXING\017CUTTING\016PARTITION\015ROOTS\014VOL\
\013ANIM_FULL\012ANIM\011SPLINE\010ARB8\7REGIONS\6SOLIDS\5DB\
\4INSTANCE\3SHOOT\2ALLHITS\1ALLRAYS"

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
