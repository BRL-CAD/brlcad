/*                        D O Z O O M . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file dozoom.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"

#include "./ged.h"
#include "./mged_solid.h"
#include "./sedit.h"
#include "./mged_dm.h"

mat_t	perspective_mat;
mat_t	incr_change;
mat_t	modelchanges;
mat_t	identity;

/* This is a holding place for the current display managers default wireframe color */
extern unsigned char geometry_default_color[];		/* defined in dodraw.c */

/* Screen coords of actual eye position.  Usually it is at (0,0,+1),
 * but in head-tracking and VR applications, it can move.
 */
#if 0
point_t	eye_pos_scr = { 0, 0, 1 };
#endif

struct solid	FreeSolid;	/* Head of freelist */
struct solid	HeadSolid;	/* Head of solid table */

/*
 *			P E R S P _ M A T
 *
 *  Compute a perspective matrix for a right-handed coordinate system.
 *  Reference: SGI Graphics Reference Appendix C
 *  (Note:  SGI is left-handed, but the fix is done in the Display Manger).
 */
static void
persp_mat(mat_t m, fastf_t fovy, fastf_t aspect, fastf_t near1, fastf_t far1, fastf_t backoff)
{
	mat_t	m2, tran;

	fovy *= 3.1415926535/180.0;

	MAT_IDN( m2 );
	m2[5] = cos(fovy/2.0) / sin(fovy/2.0);
	m2[0] = m2[5]/aspect;
	m2[10] = (far1+near1) / (far1-near1);
	m2[11] = 2*far1*near1 / (far1-near1);	/* This should be negative */

	m2[14] = -1;		/* XXX This should be positive */
	m2[15] = 0;

	/* Move eye to origin, then apply perspective */
	MAT_IDN( tran );
	tran[11] = -backoff;
	bn_mat_mul(m, m2, tran );
}

/*
 *
 *  Create a perspective matrix that transforms the +/1 viewing cube,
 *  with the acutal eye position (not at Z=+1) specified in viewing coords,
 *  into a related space where the eye has been sheared onto the Z axis
 *  and repositioned at Z=(0,0,1), with the same perspective field of view
 *  as before.
 *
 *  The Zbuffer clips off stuff with negative Z values.
 *
 *  pmat = persp * xlate * shear
 */
static void
mike_persp_mat(fastf_t *pmat, const fastf_t *eye)
{
	mat_t	shear;
	mat_t	persp;
	mat_t	xlate;
	mat_t	t1, t2;
	point_t	sheared_eye;
#if 0
	fastf_t	near1, far1;
	point_t	a,b;
#endif

	if( eye[Z] < SMALL )  {
		VPRINT("mike_persp_mat(): ERROR, z<0, eye", eye);
		return;
	}

	/* Shear "eye" to +Z axis */
	MAT_IDN(shear);
	shear[2] = -eye[X]/eye[Z];
	shear[6] = -eye[Y]/eye[Z];

	MAT4X3VEC( sheared_eye, shear, eye );
	if( !NEAR_ZERO(sheared_eye[X], .01) || !NEAR_ZERO(sheared_eye[Y], .01) )  {
		VPRINT("ERROR sheared_eye", sheared_eye);
		return;
	}
#if 0
VPRINT("sheared_eye", sheared_eye);
#endif

	/* Translate along +Z axis to put sheared_eye at (0,0,1). */
	MAT_IDN(xlate);
	/* XXX should I use MAT_DELTAS_VEC_NEG()?  X and Y should be 0 now */
	MAT_DELTAS( xlate, 0, 0, 1-sheared_eye[Z] );

	/* Build perspective matrix inline, substituting fov=2*atan(1,Z) */
	MAT_IDN( persp );
	/* From page 492 of Graphics Gems */
	persp[0] = sheared_eye[Z];	/* scaling: fov aspect term */
	persp[5] = sheared_eye[Z];	/* scaling: determines fov */

	/* From page 158 of Rogers Mathematical Elements */
	/* Z center of projection at Z=+1, r=-1/1 */
	persp[14] = -1;

	bn_mat_mul( t1, xlate, shear );
	bn_mat_mul( t2, persp, t1 );
#if 0
	/* t2 has perspective matrix, with Z ranging from -1 to +1.
	 * In order to control "near" and "far clipping planes,
	 * need to scale and translate in Z.
	 * For example, to get Z effective Z range of -1 to +11,
	 * divide Z by 12/2, then xlate by (6-1).
	 */
	t2[10] /= 6;		/* near+far/2 */
	MAT_DELTAS( xlate, 0, 0, -5 );
#else
	/* Now, move eye from Z=1 to Z=0, for clipping purposes */
	MAT_DELTAS( xlate, 0, 0, -1 );
#endif
	bn_mat_mul( pmat, xlate, t2 );
#if 0
bn_mat_print("pmat",pmat);

	/* Some quick checking */
	VSET( a, 0.0, 0.0, -1.0 );
	MAT4X3PNT( b, pmat, a );
	VPRINT("0,0,-1 ->", b);

	VSET( a, 1.0, 1.0, -1.0 );
	MAT4X3PNT( b, pmat, a );
	VPRINT("1,1,-1 ->", b);

	VSET( a, 0.0, 0.0, 0.0 );
	MAT4X3PNT( b, pmat, a );
	VPRINT("0,0,0 ->", b);

	VSET( a, 1.0, 1.0, 0.0 );
	MAT4X3PNT( b, pmat, a );
	VPRINT("1,1,0 ->", b);

	VSET( a, 1.0, 1.0, 1.0 );
	MAT4X3PNT( b, pmat, a );
	VPRINT("1,1,1 ->", b);

	VSET( a, 0.0, 0.0, 1.0 );
	MAT4X3PNT( b, pmat, a );
	VPRINT("0,0,1 ->", b);
#endif
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
 *
 *  The gist of the algorithm is to translate the display plate to the
 *  view center, shear the eye point to (0,0,1), translate back,
 *  then apply an off-axis perspective projection.
 *
 *  Another (partial) reference is "A comparison of stereoscopic cursors
 *  for the interactive manipulation of B-splines" by Barham & McAllister,
 *  SPIE Vol 1457 Stereoscopic Display & Applications, 1991, pg 19.
 */
static void
deering_persp_mat(fastf_t *m, const fastf_t *l, const fastf_t *h, const fastf_t *eye)
             	  	/* lower left corner of screen */
             	  	/* upper right (high) corner of screen */
             	    	/* eye location.  Traditionally at (0,0,1) */
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

	/* Multiplied by -1, to do right-handed Z coords */
	m[8] = 0;
	m[9] = 0;
	m[10] = -( sum[Z] - 2 * eye[Z] ) / diff[Z];
	m[11] = -(-eye[Z] + 2 * h[Z] * eye[Z]) / diff[Z];

	m[12] = 0;
	m[13] = 0;
	m[14] = -1;
	m[15] = eye[Z];

/* XXX May need to flip Z ? (lefthand to righthand?) */
}

static void
drawSolid(register struct solid *sp,
	  short r,
	  short g,
	  short b) {

  if(sp->s_cflag){
    if(!DM_SAME_COLOR(r,g,b,
		      (short)geometry_default_color[0],
		      (short)geometry_default_color[1],
		      (short)geometry_default_color[2])){
      DM_SET_FGCOLOR(dmp,
		     (short)geometry_default_color[0],
		     (short)geometry_default_color[1],
		     (short)geometry_default_color[2],
		     0,
		     sp->s_transparency);
      DM_COPY_COLOR(r,g,b,
		    (short)geometry_default_color[0],
		    (short)geometry_default_color[1],
		    (short)geometry_default_color[2]);
    }
  } else {
    if(!DM_SAME_COLOR(r,g,b,
		      (short)sp->s_color[0],
		      (short)sp->s_color[1],
		      (short)sp->s_color[2])){
      DM_SET_FGCOLOR(dmp,
		     (short)sp->s_color[0],
		     (short)sp->s_color[1],
		     (short)sp->s_color[2],
		     0,
		     sp->s_transparency);
      DM_COPY_COLOR(r,g,b,
		    (short)sp->s_color[0],
		    (short)sp->s_color[1],
		    (short)sp->s_color[2]);
    }
  }


#ifdef DO_DISPLAY_LISTS
  if (displaylist && mged_variables->mv_dlist) {
    DM_DRAWDLIST(dmp, sp->s_dlist);
    sp->s_flag = UP;
    ndrawn++;
  } else {
    if (DM_DRAW_VLIST(dmp, (struct rt_vlist *)&sp->s_vlist) == TCL_OK) {
      sp->s_flag = UP;
      ndrawn++;
    }
  }
#else
  if (DM_DRAW_VLIST(dmp, (struct rt_vlist *)&sp->s_vlist) == TCL_OK) {
    sp->s_flag = UP;
    ndrawn++;
  }
#endif
}

/*
 *			D O Z O O M
 *
 *	This routine reviews all of the solids in the solids table,
 * to see if they are visible within the current viewing
 * window.  If they are, the routine computes the scale and appropriate
 * screen position for the object.
 */
void
dozoom(int which_eye)
{
	register struct solid	*sp;
	FAST fastf_t		ratio;
	fastf_t			inv_viewsize;
	mat_t			new;
	matp_t			mat = new;
	int 			linestyle = -1;  /* not dashed */
	short 			r = -1;
	short 			g = -1;
	short 			b = -1;

	/*
	 * The vectorThreshold stuff in libdm may turn the
	 * Tcl-crank causing curr_dm_list to change.
	 */
	struct dm_list *save_dm_list = curr_dm_list;

	ndrawn = 0;
	inv_viewsize = view_state->vs_vop->vo_invSize;

	/*
	 * Draw all solids not involved in an edit.
	 */
	if( view_state->vs_vop->vo_perspective <= 0 && view_state->vs_vop->vo_eye_pos[Z] == 1.0 )  {
		mat = view_state->vs_vop->vo_model2view;
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
		to_eye_scr = 1 / tan(view_state->vs_vop->vo_perspective * bn_degtorad * 0.5);

#define SCR_WIDTH_PHYS	330	/* Assume a 330 mm wide screen */

		eye_delta_scr = mged_variables->mv_eye_sep_dist * 0.5 / SCR_WIDTH_PHYS;

		VSET( l, -1.0, -1.0, -1.0 );
		VSET( h, 1.0, 1.0, 200.0 );
		if(which_eye) {
		  printf("d=%gscr, d=%gmm, delta=%gscr\n", to_eye_scr, to_eye_scr * SCR_WIDTH_PHYS, eye_delta_scr);
		  VPRINT("l", l);
		  VPRINT("h", h);
		}
		VSET( eye, 0.0, 0.0, to_eye_scr );
#if 0
		MAT_IDN(tmat);
		tmat[11] = -1.0;
		bn_mat_mul( tvmat, tmat, view_state->vs_model2view );
#endif
		switch(which_eye)  {
		  case 0:
		    /* Non-stereo case */
		    mat = view_state->vs_vop->vo_model2view;
		    /* XXX hack */
#define HACK 0
#if !HACK
		    if( 1 ) {
#else
		      if( mged_variables->mv_faceplate > 0 )  {
#endif
			if( view_state->vs_vop->vo_eye_pos[Z] == 1.0 )  {
				/* This way works, with reasonable Z-clipping */
				persp_mat( perspective_mat, view_state->vs_vop->vo_perspective,
					(fastf_t)1.0f, (fastf_t)0.01f, (fastf_t)1.0e10f, (fastf_t)1.0f );
			} else {
				/* This way does not have reasonable Z-clipping,
				 * but includes shear, for GDurf's testing.
				 */
			  deering_persp_mat( perspective_mat, l, h, view_state->vs_vop->vo_eye_pos );
			}
		      } else {
			/* New way, should handle all cases */
			mike_persp_mat( perspective_mat, view_state->vs_vop->vo_eye_pos );
		      }
#if HACK
		      bn_mat_print("perspective_mat", perspective_mat);
#endif
		      break;
		case 1:
			/* R */
			mat = view_state->vs_vop->vo_model2view;
			eye[X] = eye_delta_scr;
			deering_persp_mat( perspective_mat, l, h, eye );
			break;
		case 2:
			/* L */
			mat = view_state->vs_vop->vo_model2view;
			eye[X] = -eye_delta_scr;
			deering_persp_mat( perspective_mat, l, h, eye );
			break;
		    }
		    bn_mat_mul( new, perspective_mat, mat );
		    mat = new;
		}

		DM_LOADMATRIX( dmp, mat, which_eye );

		if (dmp->dm_transparency) {
		  /* First, draw opaque stuff */
		  FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)  {
		    sp->s_flag = DOWN;		/* Not drawn yet */

		    /* If part of object edit, will be drawn below */
		    if( sp->s_iflag == UP )
		      continue;

		    if (sp->s_transparency < 1.0)
		      continue;

		    /*
		     * The vectorThreshold stuff in libdm may turn the
		     * Tcl-crank causing curr_dm_list to change.
		     */
		    if (curr_dm_list != save_dm_list)
		      curr_dm_list = save_dm_list;

		    if (dmp->dm_boundFlag) {
		      ratio = sp->s_size * inv_viewsize;

		      /*
		       * Check for this object being bigger than
		       * dmp->dm_bound * the window size, or smaller than a speck.
		       */
		      if (ratio < 0.001)
			continue;
		    }

		    if(linestyle != sp->s_soldash){
		      linestyle = sp->s_soldash;
		      DM_SET_LINE_ATTR(dmp, mged_variables->mv_linewidth, linestyle);
		    }

		    drawSolid(sp, r, g, b);
		  }

		  /* disable write to depth buffer */
		  DM_SET_DEPTH_MASK(dmp, 0);

		  /* Second, draw transparent stuff */
		  FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)  {

		    /* If part of object edit, will be drawn below */
		    if( sp->s_iflag == UP )
		      continue;

		    /* already drawn above */
		    if (sp->s_transparency == 1.0)
		      continue;

		    /*
		     * The vectorThreshold stuff in libdm may turn the
		     * Tcl-crank causing curr_dm_list to change.
		     */
		    if (curr_dm_list != save_dm_list)
		      curr_dm_list = save_dm_list;

		    if (dmp->dm_boundFlag) {
		      ratio = sp->s_size * inv_viewsize;

		      /*
		       * Check for this object being bigger than
		       * dmp->dm_bound * the window size, or smaller than a speck.
		       */
		      if (ratio < 0.001)
			continue;
		    }

		    if(linestyle != sp->s_soldash){
		      linestyle = sp->s_soldash;
		      DM_SET_LINE_ATTR(dmp, mged_variables->mv_linewidth, linestyle);
		    }

		    drawSolid(sp, r, g, b);
		  }

		  /* re-enable write of depth buffer */
		  DM_SET_DEPTH_MASK(dmp, 1);
		} else {

		  FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)  {
		    sp->s_flag = DOWN;		/* Not drawn yet */
		    /* If part of object edit, will be drawn below */
		    if( sp->s_iflag == UP )
		      continue;

		    /*
		     * The vectorThreshold stuff in libdm may turn the
		     * Tcl-crank causing curr_dm_list to change.
		     */
		    if (curr_dm_list != save_dm_list)
		      curr_dm_list = save_dm_list;

		    if (dmp->dm_boundFlag) {
		      ratio = sp->s_size * inv_viewsize;

		      /*
		       * Check for this object being smaller than a speck.
		       */
		      if (ratio < 0.001)
			continue;
		    }

		    if(linestyle != sp->s_soldash){
		      linestyle = sp->s_soldash;
		      DM_SET_LINE_ATTR(dmp, mged_variables->mv_linewidth, linestyle);
		    }

		    drawSolid(sp, r, g, b);
		  }
		}

		/*
		 * The vectorThreshold stuff in libdm may turn the
		 * Tcl-crank causing curr_dm_list to change.
		 */
		if (curr_dm_list != save_dm_list)
		  curr_dm_list = save_dm_list;

		/* draw predictor vlist */
		if(mged_variables->mv_predictor){
		  DM_SET_FGCOLOR(dmp,
				 color_scheme->cs_predictor[0],
				 color_scheme->cs_predictor[1],
				 color_scheme->cs_predictor[2], 1, 1.0);
		  DM_DRAW_VLIST(dmp, (struct rt_vlist *)&curr_dm_list->dml_p_vlist);
		}

		/*
		 *  Draw all solids involved in editing.
		 *  They may be getting transformed away from the other solids.
		 */
		if( state == ST_VIEW )
		  return;

		if( view_state->vs_vop->vo_perspective <= 0 )  {
		  mat = view_state->vs_model2objview;
		} else {
		  bn_mat_mul( new, perspective_mat, view_state->vs_model2objview );
		  mat = new;
		}
		DM_LOADMATRIX( dmp, mat, which_eye );
		inv_viewsize /= modelchanges[15];
		DM_SET_FGCOLOR(dmp,
			       color_scheme->cs_geo_hl[0],
			       color_scheme->cs_geo_hl[1],
			       color_scheme->cs_geo_hl[2], 1, 1.0);

		FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)  {
		  /* Ignore all objects not being edited */
		  if (sp->s_iflag != UP)
		    continue;

		  /*
		   * The vectorThreshold stuff in libdm may turn the
		   * Tcl-crank causing curr_dm_list to change.
		   */
		  if (curr_dm_list != save_dm_list)
		    curr_dm_list = save_dm_list;

		  if (dmp->dm_boundFlag) {
		    ratio = sp->s_size * inv_viewsize;
		    /*
		     * Check for this object being smaller than a speck.
		     */
		    if (ratio < 0.001)
		      continue;
		  }

		  if (linestyle != sp->s_soldash) {
		    linestyle = sp->s_soldash;
		    DM_SET_LINE_ATTR(dmp, mged_variables->mv_linewidth, linestyle);
		  }

#ifdef DO_DISPLAY_LISTS
		  if (displaylist && mged_variables->mv_dlist) {
		    DM_DRAWDLIST(dmp, sp->s_dlist);
		    sp->s_flag = UP;
		    ndrawn++;
		  } else {
		    /* draw in immediate mode */
		    if (DM_DRAW_VLIST(dmp, (struct rt_vlist *)&sp->s_vlist) == TCL_OK) {
		      sp->s_flag = UP;
		      ndrawn++;
		    }
		  }
#else
		  if (DM_DRAW_VLIST(dmp, (struct rt_vlist *)&sp->s_vlist) == TCL_OK) {
		    sp->s_flag = UP;
		    ndrawn++;
		  }
#endif
	}

	/*
	 * The vectorThreshold stuff in libdm may turn the
	 * Tcl-crank causing curr_dm_list to change.
	 */
	if (curr_dm_list != save_dm_list)
		curr_dm_list = save_dm_list;
}

#ifdef DO_DISPLAY_LISTS
/*
 * Create Display List
 */
void
createDList(struct solid *sp)
{
  DM_BEGINDLIST(dmp, sp->s_dlist);
  DM_DRAW_VLIST(dmp, (struct rt_vlist *)&sp->s_vlist);
  DM_ENDDLIST(dmp);
}

/*
 * Create Display Lists
 */
void
createDLists(struct bu_list *hsp)
{
  register struct solid *sp;

  FOR_ALL_SOLIDS(sp, hsp){
    createDList(sp);
  }
}

/*
 * Create a display list for "sp" for every display manager
 * manager that:
 *		1 - supports display lists
 *		2 - is actively using display lists
 *		3 - has not already been created (i.e. sharing with a
 *			display manager that has already created the display list)
 */
void
createDListALL(struct solid *sp)
{
  struct dm_list *dlp;
  struct dm_list *save_dlp;

  save_dlp = curr_dm_list;

  FOR_ALL_DISPLAYS(dlp, &head_dm_list.l)
    dlp->dml_dlist_state->dl_flag = 1;

  FOR_ALL_DISPLAYS(dlp, &head_dm_list.l){
    if(dlp->dml_dmp->dm_displaylist &&
       dlp->dml_mged_variables->mv_dlist){
      if(dlp->dml_dlist_state->dl_flag){
	curr_dm_list = dlp;
	createDList(sp);
      }
    }

    dlp->dml_dirty = 1;
    dlp->dml_dlist_state->dl_flag = 0;
  }

  curr_dm_list = save_dlp;
}

/*
 * Call createDListALL for all solids. See createDListALL above
 * for a description.
 */
void
createDListsAll(struct bu_list *hsp)
{
  struct solid *sp;

  FOR_ALL_SOLIDS(sp, hsp){
    createDListALL(sp);
  }
}

/*
 * Free the range of display lists for all display managers
 * that support display lists and have them activated.
 */
void
freeDListsAll(unsigned int dlist, int range)
{
  struct dm_list *dlp;

  FOR_ALL_DISPLAYS(dlp, &head_dm_list.l)
    dlp->dml_dlist_state->dl_flag = 1;

  FOR_ALL_DISPLAYS(dlp, &head_dm_list.l){
    if(dlp->dml_dmp->dm_displaylist &&
       dlp->dml_mged_variables->mv_dlist){
      if(dlp->dml_dlist_state->dl_flag)
	DM_FREEDLISTS(dlp->dml_dmp, dlist, range);
    }

    dlp->dml_dirty = 1;
    dlp->dml_dlist_state->dl_flag = 0;
  }
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
