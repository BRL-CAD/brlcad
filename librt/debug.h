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
#define DEBUG_ALLRAYS	0x00000001	/* Print calls to shootray() */
#define DEBUG_SHOOT	0x00000002	/* Info about shootray() processing */

/* Options useful for debugging the database */
#define DEBUG_DB	0x00000010	/* Database debugging */
#define DEBUG_SOLIDS	0x00000020	/* Print prep'ed solids */
#define DEBUG_REGIONS	0x00000040	/* Print regions & boolean trees */
#define DEBUG_ARB8	0x00000080	/* Print voluminus ARB8 details */
#define DEBUG_SPLINE	0x00000100	/* Splines */
#define DEBUG_ANIM	0x00000200	/* Animation */

/* Options useful for debugging the library */
#define DEBUG_ROOTS	0x00001000	/* Print rootfinder details */
#define DEBUG_PARTITION	0x00002000	/* Info about bool_weave() */
#define DEBUG_CUT	0x00004000	/* Print space cutting details */
#define DEBUG_BOXING	0x00008000	/* Object/box checking details */
#define DEBUG_MEM	0x00010000	/* Debug dynamic memory operations */
#define DEBUG_TESTING	0x00020000	/* One-shot debugging flag */
#define DEBUG_FDIFF	0x00040000	/* bool/fdiff debugging */

/* These will cause binary debugging output */
#define DEBUG_PLOTBOX	0x80000000	/* Plot(3) bounding boxes to stdout */
