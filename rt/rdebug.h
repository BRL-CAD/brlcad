/*
 *  			R D E B U G . H
 *  
 *  Debugging flags for thr RT program itself.
 *  These flags follow the "-X" (cap X) option to the RT program.
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
#define RDEBUG_HITS	0x00000001	/* 1 Print hits used by view() */
#define RDEBUG_MATERIAL	0x00000002	/* 2 Material properties */
#define RDEBUG_SHOWERR	0x00000004	/* 3 Colorful markers on errors */
#define RDEBUG_RTMEM	0x00000008	/* 4 Debug librt mem after startup */
#define RDEBUG_10	0x00000010	/* 5 unused */
#define RDEBUG_PARSE	0x00000020	/* 6 Command parsing */
#define RDEBUG_LIGHT	0x00000040	/* 7 Debug lighting */
#define RDEBUG_REFRACT	0x00000080	/* 8 Debug reflection & refraction */
#define RDEBUG_INSTANCE	0x00000100	/* 9 Debug instance revectoring */

/* These will cause binary debugging output */
#define RDEBUG_RAYWRITE	0x40000000	/* 31 Ray(5V) view rays to stdout */
#define RDEBUG_RAYPLOT	0x80000000	/* 32 plot(5) rays to stdout */

/* Format string for rt_printb() */
#define RDEBUG_FORMAT	\
"\020\040RAYPLOT\037RAYWRITE\
\1HITS\2MATERIAL\3SHOWERR\4RTMEM\6PARSE\7LIGHT\010REFRACT\011INSTANCE"
