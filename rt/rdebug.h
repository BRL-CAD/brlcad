/*
 *  			R D E B U G . H
 *  
 *  Debugging flags for thr RT program itself.
 *  librt debugging is separately controlled.
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
#define RDEBUG_OFF	0	/* No debugging */

extern int	rdebug;

/* These definitions are each for one bit */
/* Should be reogranized to put most useful ones first */
#define RDEBUG_HITS	0x00000001	/* Print hits used by view() */
#define RDEBUG_MATERIAL	0x00000002	/* Material properties */
#define RDEBUG_SHOWERR	0x00000004	/* Colorful markers on errors */
#define RDEBUG_RTMEM	0x00000008	/* Debug librt mem after startup */
#define RDEBUG_PARALLEL	0x00000010	/* Debug parallelism */

/* These will cause binary debugging output */
#define RDEBUG_RAYWRITE	0x80000000	/* Ray(5V) view rays to stdout */
