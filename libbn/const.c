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
#include "bu.h"
#include "vmath.h"
#include "bn.h"


CONST double bn_pi	= 3.14159265358979323846;	/* pi */
CONST double bn_twopi	= 6.28318530717958647692;	/* pi*2 */
CONST double bn_halfpi	= 1.57079632679489661923;	/* pi/2 */
CONST double bn_quarterpi=0.78539816339744830961;	/* pi/4 */
CONST double bn_invpi	= 0.318309886183790671538;	/* 1/pi */
CONST double bn_inv2pi	= 0.159154943091895335769;	/* 1/(pi*2) */
CONST double bn_inv4pi	= 0.07957747154594766788;	/* 1/(pi*4) */

CONST double bn_inv255 = 1.0/255.0;

CONST double bn_degtorad =  0.0174532925199433;		/* (pi*2)/360 */
CONST double bn_radtodeg = 57.29577951308230698802;	/* 360/(pi*2) */
