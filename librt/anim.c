/*
 *			A N I M
 *
 *  Ray Tracing program, routines to apply animation directives.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSanim[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"

/*
 *			R T _ F R _ A N I M
 *
 *  Release chain of animation structures
 *
 * XXX This routine is no longer used.
 * XXX Look in db_anim.c for the new versions.
 */
void
rt_fr_anim( rtip )
register struct rt_i *rtip;
{
	register struct animate *anp;
	register struct directory *dp;
	register int		i;

	/* Rooted animations */
	for( anp = rtip->rti_anroot; anp != ANIM_NULL; )  {
		register struct animate *nextanp = anp->an_forw;

		rt_free( (char *)anp->an_path, "animation path[]");
		rt_free( (char *)anp, "struct animate");
		anp = nextanp;
	}
	rtip->rti_anroot = ANIM_NULL;

	/* Node animations */
	for( i=0; i < RT_DBNHASH; i++ )  {
		dp = rtip->rti_dbip->dbi_Head[i];
		for( ; dp != DIR_NULL; dp = dp->d_forw )  {
			for( anp = dp->d_animate; anp != ANIM_NULL; )  {
				register struct animate *nextanp = anp->an_forw;

				rt_free( (char *)anp->an_path, "animation path[]");
				rt_free( (char *)anp, "struct animate");
				anp = nextanp;
			}
			dp->d_animate = ANIM_NULL;
		}
	}
}
