/*
 *  			D E B U G . H
 *  
 *  Various definitions permitting segmented debugging to
 *  be independently controled.
 *
 *  For programs based on the "RT" program, these flags follow
 *  the "-x" (lower case x) option.
 *  
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 *  
 *  $Revision$
 */
#define DEBUG_OFF	0	/* No debugging */

/* These definitions are each for one bit */

/* Options useful for debugging applications */
#define DEBUG_ALLRAYS	0x00000001	/* 1 Print calls to shootray() */
#define DEBUG_ALLHITS	0x00000002	/* 2 Print partitions passed to a_hit() */
#define DEBUG_SHOOT	0x00000004	/* 3 Info about shootray() processing */
#define DEBUG_INSTANCE	0x00000008	/* 4 regionid instance revectoring */

/* Options useful for debugging the database */
#define DEBUG_DB	0x00000010	/* 5 Database debugging */
#define DEBUG_SOLIDS	0x00000020	/* 6 Print prep'ed solids */
#define DEBUG_REGIONS	0x00000040	/* 7 Print regions & boolean trees */
#define DEBUG_ARB8	0x00000080	/* 8 Print voluminus ARB8 details */
#define DEBUG_SPLINE	0x00000100	/* 9 Splines */
#define DEBUG_ANIM	0x00000200	/* 10 Animation */
#define DEBUG_ANIM_FULL	0x00000400	/* 11 Animation matrices */
#define DEBUG_VOL	0x00000800	/* 12 Volume solids */

/* Options useful for debugging the library */
#define DEBUG_ROOTS	0x00001000	/* 13 Print rootfinder details */
#define DEBUG_PARTITION	0x00002000	/* 14 Info about bool_weave() */
#define DEBUG_CUT	0x00004000	/* 15 Print space cutting statistics */
#define DEBUG_BOXING	0x00008000	/* 16 Object/box checking details */
#define DEBUG_MEM	0x00010000	/* 17 -->> BU_DEBUG_MEM_LOG */
#define DEBUG_MEM_FULL	0x00020000	/* 18 -->> BU_DEBUG_MEM_CHECK */
#define DEBUG_FDIFF	0x00040000	/* 19 bool/fdiff debugging */
#define DEBUG_PARALLEL	0x00080000	/* 20 -->> BU_DEBUG_PARALLEL */
#define DEBUG_CUTDETAIL	0x00100000	/* 21 Print space cutting details */
#define DEBUG_TREEWALK	0x00200000	/* 22 Database tree traversal */
#define DEBUG_TESTING	0x00400000	/* 23 One-shot debugging flag */
#define DEBUG_ADVANCE	0x00800000	/* 24 Cell-to-cell space partitioning */
#define DEBUG_MATH	0x01000000	/* 25 Fundamental math routines (plane.c, mat.c) */
#define DEBUG_EBM	0x02000000	/* 26 Extruded bit-map solids */
#define DEBUG_HF	0x04000000	/* 27 Height Field solids */

/* These will cause binary debugging output */
#define DEBUG_PLOTBOX	0x80000000	/* 32 Plot(3) bounding boxes and cuts */

/* Format string for rt_printb() */
#define DEBUG_FORMAT	\
"\020\040PLOTBOX\
\033HF\032EBM\031MATH\030ADVANCE\
\027TESTING\026TREEWALK\025CUTDETAIL\024PARALLEL\023FDIFF\022MEM_FULL\
\021MEM\020BOXING\017CUTING\016PARTITION\015ROOTS\014VOL\
\013ANIM_FULL\012ANIM\011SPLINE\010ARB8\7REGIONS\6SOLIDS\5DB\
\4INSTANCE\3SHOOT\2ALLHITS\1ALLRAYS"
