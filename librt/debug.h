/*
 *  			D E B U G . H
 *  
 *  Various definitions permitting segmented debugging to
 *  be independently controled.
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
#define DEBUG_SHOOT	0x00000002	/* 2 Info about shootray() processing */

/* Options useful for debugging the database */
#define DEBUG_DB	0x00000010	/* 5 Database debugging */
#define DEBUG_SOLIDS	0x00000020	/* 6 Print prep'ed solids */
#define DEBUG_REGIONS	0x00000040	/* 7 Print regions & boolean trees */
#define DEBUG_ARB8	0x00000080	/* 8 Print voluminus ARB8 details */
#define DEBUG_SPLINE	0x00000100	/* 9 Splines */
#define DEBUG_ANIM	0x00000200	/* 10 Animation */

/* Options useful for debugging the library */
#define DEBUG_ROOTS	0x00001000	/* 13 Print rootfinder details */
#define DEBUG_PARTITION	0x00002000	/* 14 Info about bool_weave() */
#define DEBUG_CUT	0x00004000	/* 15 Print space cutting details */
#define DEBUG_BOXING	0x00008000	/* 16 Object/box checking details */
#define DEBUG_MEM	0x00010000	/* 17 Debug dynamic memory operations */
#define DEBUG_TESTING	0x00020000	/* 18 One-shot debugging flag */
#define DEBUG_FDIFF	0x00040000	/* 19 bool/fdiff debugging */
#define DEBUG_PARALLEL	0x00080000	/* 20 machine.c parallel support */

/* These will cause binary debugging output */
#define DEBUG_PLOTBOX	0x80000000	/* 32 Plot(3) bounding boxes to stdout */

/* Format string for rt_printb() */
#define DEBUG_FORMAT	\
"\020\040PLOTBOX\
\024PARALLEL\023FDIFF\022TEST\
\021MEM\020BOXING\017CUTING\016PARTITION\015ROOTS\
\012ANIM\011SPLINE\010ARB8\7REGIONS\6SOLIDS\5DB\
\2SHOOT\1ALLRAYS"
