/*
 *			C O N S T . C
 *
 *  Constants used by the ray tracing library.
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSmat[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

CONST double rt_pi	= 3.14159265358979323846;	/* pi */
CONST double rt_twopi	= 6.28318530717958647692;	/* pi*2 */
CONST double rt_halfpi	= 1.57079632679489661923;	/* pi/2 */
CONST double rt_quarterpi=0.78539816339744830961;	/* pi/4 */
CONST double rt_invpi	= 0.318309886183790671538;	/* 1/pi */
CONST double rt_inv2pi	= 0.159154943091895335769;	/* 1/(pi*2) */
CONST double rt_inv4pi	= 0.07957747154594766788;	/* 1/(pi*4) */

CONST double rt_inv255 = 1.0/255.0;

CONST double rt_degtorad =  0.0174532925199433;		/* (pi*2)/360 */
CONST double rt_radtodeg = 57.29577951308230698802;	/* 360/(pi*2) */

CONST mat_t	rt_identity = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};

#ifdef ipsc860
/* Hack to work around the broken iPSC/860 loader.  It doesn't
 * see the definition in global.c, probably because there is no
 * .text or .data there that it needs.
 */
struct rt_g rt_g;
#endif
