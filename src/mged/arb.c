/*
 *			A R B . C
 *
 * Functions -
 *	move_arb	move an ARB8
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
 *	This software is Copyright (C) 1985-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./ged.h"

void
dbpr_arb(struct solidrec *sp, register struct directory *dp)
{
	int i;
	char *s;

	if( (i=sp->s_cgtype) < 0 )
		i = -i;
	switch( i )  {
	case ARB4:
		s="ARB4";
		break;
	case ARB5:
		s="ARB5";
		break;
	case RAW:
	case ARB6:
		s="ARB6";
		break;
	case ARB7:
		s="ARB7";
		break;
	case ARB8:
		s="ARB8";
		break;
	default:
		s="??";
		break;
	}

	bu_log("%s:  ARB8 (%s)\n", dp->d_namep, s );

	/* more in edsol.c/pr_solid, called from do_list */
}
