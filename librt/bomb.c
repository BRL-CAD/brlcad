/*
 *			B O M B . C
 *
 *  Checks LIBRT-specific error flags, then
 *  hands the error off to LIBBU.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998-2004 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"

#undef rt_bomb		/* in case compat4.h defines one */

/*
 *			R T _ B O M B
 *
 *  Compatibility routine
 *  If an RT program is going to dump core, make sure we check
 *  our debug flags too.
 */
void
rt_bomb(const char *s)
{
	if(RT_G_DEBUG || rt_g.NMG_debug )
		bu_debug |= BU_DEBUG_COREDUMP;
	bu_bomb(s);
}
