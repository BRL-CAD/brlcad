/*
 *			D O Z O O M . C
 *
 * Functions -
 *	dozoom		Compute new zoom/rotation perspectives
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
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./sedit.h"
#include "./dm.h"

/* Variables for dozoom() */
fastf_t	Viewscale;
mat_t	Viewrot, toViewcenter;
mat_t	model2view, view2model;
mat_t	model2objview, objview2model;
mat_t	incr_change;
mat_t	modelchanges;
mat_t	identity;

struct solid	*FreeSolid;	/* Head of freelist */
struct solid	HeadSolid;	/* Head of solid table */
int		ndrawn;

/*
 *			D O Z O O M
 *
 *	This routine reviews all of the solids in the solids table,
 * to see if they  visible within the current viewing
 * window.  If they are, the routine computes the scale and appropriate
 * screen position for the object.
 */
void
dozoom()
{
	register struct solid *sp;
	FAST fastf_t ratio;
	fastf_t		inv_viewsize;

	ndrawn = 0;
	inv_viewsize = 1 / VIEWSIZE;

	/*
	 * Draw all solids not involved in an edit.
	 */
	dmp->dmr_newrot( model2view );
	FOR_ALL_SOLIDS( sp )  {
		/* If part of object rotation, will be drawn below */
		if( sp->s_iflag == UP )
			continue;

		ratio = sp->s_size * inv_viewsize;
		sp->s_flag = DOWN;		/* Not drawn yet */

		/*
		 * Check for this object being bigger than 
		 * dmp->dmr_bound * the window size, or smaller than a speck.
		 */
		 if( ratio >= dmp->dmr_bound || ratio < 0.001 )
		 	continue;

		if( dmp->dmr_object( sp, model2view, ratio, sp==illump ) )  {
			sp->s_flag = UP;
			ndrawn++;
		}
	}

	/*
	 *  Draw all solids involved in editing.
	 *  They may be getting transformed away from the other solids.
	 */
	if( state == ST_VIEW )
		return;

	dmp->dmr_newrot( model2objview );

	FOR_ALL_SOLIDS( sp )  {
		/* Ignore all objects not being rotated */
		if( sp->s_iflag != UP )
			continue;

		ratio = (sp->s_size / modelchanges[15]) * inv_viewsize;
		sp->s_flag = DOWN;		/* Not drawn yet */

		/*
		 * Check for this object being bigger than 
		 * dmp->dmr_bound * the window size, or smaller than a speck.
		 */
		 if( ratio >= dmp->dmr_bound || ratio < 0.001 )
		 	continue;

		if( dmp->dmr_object( sp, model2objview, ratio, 1 ) )  {
			sp->s_flag = UP;
			ndrawn++;
		}
	}
}
