/*                         B U R S T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file burst.h
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066

	$Header$ (BRL)
 */
#ifndef __BURST_H__
#define __BURST_H__

#include "common.h"

/* for genptr_t */
#include "machine.h"


/* NSIG not always defined in <signal.h> */
#ifndef NSIG
#define NSIG 64
#endif

/* ANSI C definitions */
typedef int boolean;
typedef genptr_t pointer;			/* generic pointer */

#ifndef BURST_EXIT_SUCCESS
#  define BURST_EXIT_SUCCESS	0
#endif
#ifndef BURST_EXIT_FAILURE
#  define BURST_EXIT_FAILURE	1
#endif

#define DEGRAD	57.2957795130823208767981548141051703324054724665642

/* allow for SGI screw-up, the single-precision math libraries */
#if defined( sgi ) && ! defined( mips )
#  define SINGLE_PRECISION 1
#else
#  define SINGLE_PRECISION 0
#endif

/* allow for SGI screw-up, winclose() dumps core on some systems */
#if defined(SGI4D_Rel2)
#  define SGI_WINCLOSE_BUG 1
#else
#  define SGI_WINCLOSE_BUG 0
#endif

/* menu configuration */
#define MENU_LFT	1
#define MENU_TOP	2
#define MENU_MAXVISITEMS	10

/* prompt line configuration */
#define PROMPT_X	HmXPROMPT
#define PROMPT_Y	HmYPROMPT

/* banner (border) configuration */
#define BORDER_CHR	'_'
#define BORDER_Y	(PROMPT_Y+1)

/* grid offset printing window */
#define GRID_X		55	/* where grid indices are printed */
#define GRID_Y		BORDER_Y

/* scroll region configuration */
#define SCROLL_TOP	(BORDER_Y+1)
#define SCROLL_BTM	(ScLI-1) /* bottom line causes scroll */

/* timer (cpu statistics) window configuration */
#define TIMER_X		1
#define TIMER_Y		1

/* buffer sizes */
#define LNBUFSZ		1330	/* buffer for one-line messages */
#define MAXDEVWID	10000	/* maximum width of frame buffer */

#define CHAR_COMMENT	'#'
#define CMD_COMMENT	"comment"

/* default parameters */
#define DFL_AZIMUTH	0.0
#define DFL_BARRIERS	100
#define DFL_BDIST	0.0
#define DFL_CELLSIZE	101.6
#define DFL_CONEANGLE	(45.0/DEGRAD)
#define DFL_DEFLECT	0
#define DFL_DITHER	0
#define DFL_ELEVATION	0.0
#define DFL_NRAYS	200
#define DFL_OVERLAPS	1
#define DFL_RIPLEVELS	1
#define DFL_UNITS	U_MILLIMETERS

/* firing mode bit definitions */
#define ASNBIT(w,b)	(w = (b))
#define SETBIT(w,b)	(w |= (b))
#define CLRBIT(w,b)	(w &= ~(b))
#define TSTBIT(w,b)	((w)&(b))
#define FM_GRID  0	/* generate grid of shotlines */
#define FM_DFLT	 FM_GRID
#define FM_PART  (1)	/* bit 0: ON = partial envelope, OFF = full */
#define FM_SHOT	 (1<<1)	/* bit 1: ON = discrete shots, OFF = gridding */
#define FM_FILE	 (1<<2)	/* bit 2: ON = file input, OFF = direct input */
#define FM_3DIM	 (1<<3)	/* bit 3: ON = 3-D coords., OFF = 2-D coords */
#define FM_BURST (1<<4) /* bit 4: ON = discrete burst points, OFF = shots */

/* flags for notify() */
#define	NOTIFY_APPEND	1
#define NOTIFY_DELETE	2
#define NOTIFY_ERASE	4

#define NOTIFY_DELIM	':'

#define PB_ASPECT_INIT		'1'
#define PB_CELL_IDENT		'2'
#define PB_RAY_INTERSECT	'3'
#define PB_RAY_HEADER		'4'
#define PB_REGION_HEADER	'5'

#define PS_ASPECT_INIT		'1'
#define PS_CELL_IDENT		'2'
#define PS_SHOT_INTERSECT	'3'

#define TITLE_LEN	72
#define TIMER_LEN	72

#define U_INCHES	0
#define U_FEET		1
#define U_MILLIMETERS	2
#define U_CENTIMETERS	3
#define U_METERS	4
#define U_BAD		-1

#define UNITS_INCHES		"inches"
#define UNITS_FEET		"feet"
#define UNITS_MILLIMETERS	"millimeters"
#define UNITS_CENTIMETERS	"centimeters"
#define UNITS_METERS		"meters"

/* white space in input tokens */
#define WHITESPACE	" \t"

/* colors for UNIX plot files */
#define R_GRID		255	/* grid - yellow */
#define G_GRID		255
#define B_GRID		0

#define R_BURST		255	/* burst cone - red */
#define G_BURST		0
#define B_BURST		0

#define R_OUTAIR	100	/* outside air - light blue */
#define G_OUTAIR	100
#define B_OUTAIR	255

#define R_INAIR		100	/* inside air - light green */
#define G_INAIR		255
#define B_INAIR		100

#define R_COMP		0	/* component (default) - blue */
#define G_COMP		0
#define B_COMP		255

#define R_CRIT		255	/* critical component (default) - purple */
#define G_CRIT		0
#define B_CRIT		255

#define C_MAIN		0
#define C_CRIT		1

#define TWO_PI		6.28318530717958647692528676655900576839433879875022

#define COS_TOL		0.01
#define LOS_TOL		0.1
#define VEC_TOL		0.001
#define OVERLAP_TOL	0.25	/* thinner overlaps not reported */
#define EXIT_AIR	9	/* exit air is called 09 air */
#define OUTSIDE_AIR	1	/* outside air is called 01 air */

#define Air(rp)		((rp)->reg_aircode > 0)
#define DiffAir(rp,sp)	((rp)->reg_aircode != (sp)->reg_aircode)
#define SameAir(rp,sp)	((rp)->reg_aircode == (sp)->reg_aircode)
#define SameCmp(rp,sp)	((rp)->reg_regionid == (sp)->reg_regionid)
#define OutsideAir(rp)	((rp)->reg_aircode == OUTSIDE_AIR)
#define InsideAir(rp)	(Air(rp)&& !OutsideAir(rp))

#define Malloc_Bomb( _bytes_ ) \
                brst_log( "\"%s\"(%d) : allocation of %d bytes failed.\n",\
                                __FILE__, __LINE__, _bytes_ )

#define Swap_Doubles( a_, b_ ) \
		{	fastf_t	f_ = a_; \
		a_ = b_; \
		b_ = f_; \
		}
#define Toggle(f)	(f) = !(f)

typedef struct ids	Ids;
struct ids
	{
	short	i_lower;
	short	i_upper;
	Ids	*i_next;
	};
#define IDS_NULL	(Ids *) 0

typedef struct colors	Colors;
struct colors
	{
	short	c_lower;
	short	c_upper;
	unsigned char	c_rgb[3];
	Colors	*c_next;
	};
#define COLORS_NULL	(Colors *) 0

typedef struct pt_queue	Pt_Queue;
struct pt_queue
	{
	struct partition	*q_part;
	Pt_Queue		*q_next;
	};

#define PT_Q_NULL	(Pt_Queue *) 0

#endif  /* __BURST_H__ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
