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

/* How should this be implemented????? */
point_t	recip_vanishing_point = { 0, 0, -1 };

struct solid	*FreeSolid;	/* Head of freelist */
struct solid	HeadSolid;	/* Head of solid table */
int		ndrawn;

/*
 *			P E R S P _ M A T
 *
 *  Compute a perspective matrix for a right-handed coordinate system.
 *  Reference: SGI Graphics Reference Appendix C
 *  (Note:  SGI is left-handed, but the fix is done in the Display Manger).
 */
static void
persp_mat( m, fovy, aspect, near, far, backoff )
mat_t	m;
fastf_t	fovy, aspect, near, far, backoff;
{
	mat_t	m2, tran;

	fovy *= 3.1415926535/180.0;

	mat_idn( m2 );
	m2[5] = cos(fovy/2.0) / sin(fovy/2.0);
	m2[0] = m2[5]/aspect;
	m2[10] = (far+near) / (far-near);
	m2[11] = 2*far*near / (far-near);

	m2[14] = -1;
	m2[15] = 0;

	mat_idn( tran );
	tran[11] = -backoff;
	mat_mul( m, m2, tran );
}

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
	mat_t		pmat;
	mat_t		new;
	matp_t		mat;

	ndrawn = 0;
	inv_viewsize = 1 / VIEWSIZE;

	/*
	 * Draw all solids not involved in an edit.
	 */
	if( mged_variables.perspective <= 0 )  {
		mat = model2view;
	} else {
		persp_mat( pmat, mged_variables.perspective,
			1.0, 0.01, 1.0e10, 1.0 );
		mat_mul( new, pmat, model2view );
		mat = new;
	}
	dmp->dmr_newrot( mat );

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

		if( dmp->dmr_object( sp, mat, ratio, sp==illump ) )  {
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

	if( mged_variables.perspective <= 0 )  {
		mat = model2objview;
	} else {
		mat_mul( new, pmat, model2objview );
		mat = new;
	}
	dmp->dmr_newrot( mat );
	inv_viewsize /= modelchanges[15];

	FOR_ALL_SOLIDS( sp )  {
		/* Ignore all objects not being rotated */
		if( sp->s_iflag != UP )
			continue;

		ratio = sp->s_size * inv_viewsize;
		sp->s_flag = DOWN;		/* Not drawn yet */

		/*
		 * Check for this object being bigger than 
		 * dmp->dmr_bound * the window size, or smaller than a speck.
		 */
		 if( ratio >= dmp->dmr_bound || ratio < 0.001 )
		 	continue;

		if( dmp->dmr_object( sp, mat, ratio, 1 ) )  {
			sp->s_flag = UP;
			ndrawn++;
		}
	}
}
