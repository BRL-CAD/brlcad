/*
 *			D O Z O O M . C
 *
 * Functions -
 *	dozoom		Compute new zoom/rotation perspectives
 */
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
 * to see if they are visible IN ENTIRETY within the current viewing
 * window.  If they are, the routine computes the scale and appropriate
 * screen position for the object.
 *
 * NOTE that titles, etc, are now done in dotitles().
 */
void
dozoom()
{
	register struct solid *sp;
	static float ratio;
#ifdef MEGATEK
#define BOUND 15.9			/* Max magnification in Rot matrix */
	static mat_t mat_xform;		/* intermediate Megatek matrix */
	static mat_t mat_screen;	/* xform from prototype to screen */
	static mat_t mat_trnrot;	/* transposed Rot matrix */
#else	/* VG */
#define BOUND 0.95			/* Max magnification in Rot matrix */
#endif
	static vect_t pos_screen;	/* Pos, in (rotated) view space */

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
		 * BOUND * the window size, or smaller than a speck.
		 */
		 if( ratio >= BOUND || ratio < 0.001 )
		 	continue;

#ifndef MEGATEK			/* VG */

		/* Compute view-space pos of solid's center */
		MAT4X3PNT( pos_screen, model2view, sp->s_center );

		/*
		 * If the view co-ordinates of the center point fall
		 * outside the viewing window, discard this solid.
		 */
		if( pos_screen[X] >= BOUND || pos_screen[X] <= -BOUND )
			 continue;
		if( pos_screen[Y] >= BOUND || pos_screen[Y] <= -BOUND )
			continue;
		if( pos_screen[Z] >= BOUND || pos_screen[Z] <= -BOUND )
			continue;

		/*
		 * Object will be displayed.
		 */
		sp->s_flag = UP;		/* obj is drawn */
		ndrawn++;
		dm_call( sp->s_addr, pos_screen, ratio, sp==illump );
#endif
#ifdef MEGATEK

		/*
		 * Object will be displayed.
		 *
		 * Compute transfomation from model to view co-ordinates,
		 * to correctly position prototype on the screen.
		 * Take into account prototypes actual size (-1..+1) by
		 * multiplying by ratio.
		 */
		/* Compute view-space pos of solid's center */
		MAT4X3PNT( pos_screen, model2view, sp->s_center );

		/* Save rotation portion of model2view, add xlate */
		mat_copy( mat_trnrot, model2view );
		mat_trnrot[15] = 1;
		mat_trnrot[MDX] = pos_screen[X];
		mat_trnrot[MDY] = pos_screen[Y];
		mat_trnrot[MDZ] = pos_screen[Z];

		/* Also include scaling, given prototype size -vs- view */
		mat_idn(mat_xform);
		mat_xform[0] = ratio;	/* * s_size / ( 2 * m2view[15] ) */
		mat_xform[5] = ratio;
		mat_xform[10] = ratio;

		/* Glue it together.  Important to scale BEFORE rot+xlate, so
		 * as not to damage the length of the (screen space) xlate */
		mat_mul( mat_screen, mat_trnrot, mat_xform );

		if(
			mat_screen[ 0] < -BOUND ||
			mat_screen[ 0] >  BOUND ||
			mat_screen[ 5] < -BOUND ||
			mat_screen[ 5] >  BOUND ||
		 	mat_screen[MDX] < -BOUND ||
			mat_screen[MDX] >  BOUND ||
			mat_screen[MDY] < -BOUND ||
			mat_screen[MDY] >  BOUND
		)  continue;

		sp->s_flag = UP;		/* obj is drawn */
		ndrawn++;
		dm_matcall( sp->s_addr, mat_screen,
			sp==illump ? DM_WHITE
				: ((ndrawn>>2)%13)+2 );
#endif
	}

	/*
	 * Compute translation vector & scale factor for solids involved in
	 * Object edits.
	 */
	if( 1 )  {
#ifndef MEGATEK
		/* Output special Rotation matrix */
		dm_newrot( model2objview );
#endif

		FOR_ALL_SOLIDS( sp )  {
			/* Ignore all objects not being rotated */
			if( sp->s_iflag != UP )
				continue;

			ratio = (sp->s_size / modelchanges[15]) / VIEWSIZE;
			sp->s_flag = DOWN;		/* Not drawn yet */

			/*
			 * Check for this object being bigger than 
			 * BOUND * the window size, or smaller than a speck.
			 */
			 if( ratio >= BOUND || ratio < 0.001 )
			 	continue;

#ifndef MEGATEK				/* VG */
			/* Compute view-space pos of solid's center */
			MAT4X3PNT( pos_screen, model2objview, sp->s_center );

			/*
			 * If the edit view co-ordinates of the center point
			 * fall outside the viewing window, discard the solid.
			 */
			if( pos_screen[X] >= BOUND || pos_screen[X] <= -BOUND)
				continue;
			if( pos_screen[Y] >= BOUND || pos_screen[Y] <= -BOUND)
				continue;
			if( pos_screen[Z] >= BOUND || pos_screen[Z] <= -BOUND)
				continue;

			/*
			 * Object will be displayed.
			 */
			sp->s_flag = UP;		/* obj is drawn */
			ndrawn++;
			dm_call( sp->s_addr, pos_screen, ratio, 0 );
#endif

#ifdef MEGATEK
			/* Compute view-space pos of solid's center */
			MAT4X3PNT( pos_screen, model2objview, sp->s_center );

			/* Save rot portion of model2objview, add xlate */
			mat_copy( mat_trnrot, model2objview );
			mat_trnrot[15] = 1;
			mat_trnrot[MDX] = pos_screen[X];
			mat_trnrot[MDY] = pos_screen[Y];
			mat_trnrot[MDZ] = pos_screen[Z];

			/* Also include scaling, given prototype size -vs- view */
			mat_idn(mat_xform);
			mat_xform[0] = ratio;	/* * s_size / ( 2 * m2view[15] ) */
			mat_xform[5] = ratio;
			mat_xform[10] = ratio;

			/* Glue it together.  Important to scale BEFORE rot+xlate, so
			 * as not to damage the length of the (screen space) xlate */
			mat_mul( mat_screen, mat_trnrot, mat_xform );

			if(
				mat_screen[ 0] < -BOUND ||
				mat_screen[ 0] >  BOUND ||
				mat_screen[ 5] < -BOUND ||
				mat_screen[ 5] >  BOUND ||
			 	mat_screen[MDX] < -BOUND ||
				mat_screen[MDX] >  BOUND ||
				mat_screen[MDY] < -BOUND ||
				mat_screen[MDY] >  BOUND
			)  continue;

			sp->s_flag = UP;		/* obj is drawn */
			ndrawn++;
			dm_matcall( sp->s_addr, mat_screen, DM_WHITE );
#endif
		}
	}
}
