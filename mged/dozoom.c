/*
 *			D O Z O O M . C
 *
 * Functions -
 *	dozoom		Compute new zoom/rotation perspectives
 *  
 * Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "ged_types.h"
#include "ged.h"
#include "solid.h"
#include "3d.h"
#include "sedit.h"
#include <math.h>
#include "dm.h"
#include "vmath.h"

/* Variables for dozoom() */
float	Viewscale;
mat_t	Viewrot, toViewcenter;
mat_t	model2view, view2model;
mat_t	model2objview, objview2model;
mat_t	incr_change;
mat_t	modelchanges;
float	maxview = 0.5;
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
	static float ratio;

	ndrawn = 0;

	/*
	 * Compute translation vector & scale factor for all solids
	 */
	FOR_ALL_SOLIDS( sp )  {
		/* If part of object rotation, will be drawn below */
		if( sp->s_iflag == UP )
			continue;

		ratio = sp->s_size / VIEWSIZE;
		sp->s_flag = DOWN;		/* Not drawn yet */

		/*
		 * Check for this object being bigger than 
		 * dmp->dmr_bound * the window size, or smaller than a speck.
		 */
		 if( ratio >= dmp->dmr_bound || ratio < 0.001 )
		 	continue;

		if( dmp->dmr_object( sp->s_center, model2view, ratio, sp->s_addr, sp==illump ) )  {
			sp->s_flag = UP;
			ndrawn++;
		}
	}

	/*
	 * Compute translation vector & scale factor for solids involved in
	 * Object edits.
	 */
	if( 0 )			/* later, check flag here */
		return;

	/* Output special Rotation matrix, when needed */
	dmp->dmr_newrot( model2objview );

	FOR_ALL_SOLIDS( sp )  {
		/* Ignore all objects not being rotated */
		if( sp->s_iflag != UP )
			continue;

		ratio = (sp->s_size / modelchanges[15]) / VIEWSIZE;
		sp->s_flag = DOWN;		/* Not drawn yet */

		/*
		 * Check for this object being bigger than 
		 * dmp->dmr_bound * the window size, or smaller than a speck.
		 */
		 if( ratio >= dmp->dmr_bound || ratio < 0.001 )
		 	continue;

		if( dmp->dmr_object( sp->s_center, model2objview, ratio, sp->s_addr, 1 ) )  {
			sp->s_flag = UP;
			ndrawn++;
		}
	}
}
