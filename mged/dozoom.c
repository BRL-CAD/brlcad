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
 *  Map "display plate coordinates" (which can just be the screen viewing cube), 
 *  into [-1,+1] coordinates, with perspective.
 *  Per "High Resolution Virtual Reality" by Michael Deering,
 *  Computer Graphics 26, 2, July 1992, pp 195-201.
 *
 *  L is lower left corner of screen, H is upper right corner.
 *  L[Z] is the front (near) clipping plane location.
 *  H[Z] is the back (far) clipping plane location.
 *
 *  This corresponds to the SGI "window()" routine, but taking into account
 *  skew due to the eyepoint being offset parallel to the image plane.
 */
static void
deering_persp_mat( m, l, h, eye )
mat_t		m;
CONST point_t	l;	/* lower left corner of screen */
CONST point_t	h;	/* upper right (high) corner of screen */
CONST point_t	eye;	/* eye location.  Traditionally at (0,0,1) */
{
	vect_t	diff;	/* H - L */
	vect_t	sum;	/* H + L */

	VSUB2( diff, h, l );
	VADD2( sum, h, l );

	m[0] = 2 * eye[Z] / diff[X];
	m[1] = 0;
	m[2] = ( sum[X] - 2 * eye[X] ) / diff[X];
	m[3] = -eye[Z] * sum[X] / diff[X];

	m[4] = 0;
	m[5] = 2 * eye[Z] / diff[Y];
	m[6] = ( sum[Y] - 2 * eye[Y] ) / diff[Y];
	m[7] = -eye[Z] * sum[Y] / diff[Y];

	m[8] = 0;
	m[9] = 0;
	m[10] = ( sum[Z] - 2 * eye[Z] ) / diff[Z];
	m[11] = -eye[Z] + 2 * h[Z] * eye[Z] / diff[Z];

	m[12] = 0;
	m[13] = 0;
	m[14] = -1;
	m[15] = eye[Z];
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
dozoom(which_eye)
int	which_eye;
{
	register struct solid *sp;
	FAST fastf_t ratio;
	fastf_t		inv_viewsize;
	mat_t		pmat;
	mat_t		tmat, tvmat;
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
		/*
		 *  There are two strategies that could be used:
		 *  1)  Assume a standard head location w.r.t. the
		 *  screen, and fix the perspective angle.
		 *  2)  Based upon the perspective angle, compute
		 *  where the head should be to achieve that field of view.
		 *  Try strategy #2 for now.
		 */
		fastf_t	to_eye_scr;	/* screen space dist to eye */
		fastf_t eye_delta_scr;	/* scr, 1/2 inter-occular dist */
		point_t	l, h, eye;

		/* Determine where eye should be */
		to_eye_scr = 1 / tan(mged_variables.perspective * rt_degtorad * 0.5);

#define SCR_WIDTH_PHYS	330	/* Assume a 330 mm wide screen */

		eye_delta_scr = mged_variables.eye_sep_dist * 0.5 / SCR_WIDTH_PHYS;

		VSET( l, -1, -1, 0.01 );
		VSET( h, 1, 1, 200.0 );
if(which_eye) {
printf("d=%gscr, d=%gmm, delta=%gscr\n", to_eye_scr, to_eye_scr * SCR_WIDTH_PHYS, eye_delta_scr);
VPRINT("l", l);
VPRINT("h", h);
}
		VSET( eye, 0, 0, to_eye_scr );

		mat_idn(tmat);
		tmat[11] = -1;
		mat_mul( tvmat, tmat, model2view );

		switch(which_eye)  {
		case 0:
			mat = model2view;
			persp_mat( pmat, mged_variables.perspective,
				1.0, 0.01, 1.0e10, 1.0 );
			break;
		case 1:
			/* R */
			mat = model2view;
			eye[X] = eye_delta_scr;
			deering_persp_mat( pmat, l, h, eye );
			break;
		case 2:
			/* L */
			mat = model2view;
			eye[X] = -eye_delta_scr;
			deering_persp_mat( pmat, l, h, eye );
			break;
		}
		mat_mul( new, pmat, mat );
		mat = new;
	}
	dmp->dmr_newrot( mat, which_eye );

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
	dmp->dmr_newrot( mat, which_eye );
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
