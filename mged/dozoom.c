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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./sedit.h"
#include "./mged_dm.h"

#ifdef DM_X
extern int X_drawString2D();
#endif

#define M_AXES 0
#define V_AXES 1
#define E_AXES 2

extern point_t e_axes_pos;
extern point_t curr_e_axes_pos;
static void draw_axes();

mat_t	perspective_mat;
mat_t	incr_change;
mat_t	modelchanges;
mat_t	identity;

/* Screen coords of actual eye position.  Usually it is at (0,0,+1),
 * but in head-tracking and VR applications, it can move.
 */
point_t	eye_pos_scr = { 0, 0, 1 };

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
persp_mat( m, fovy, aspect, near, far, backoff )
mat_t	m;
fastf_t	fovy, aspect, near, far, backoff;
{
	mat_t	m2, tran;

	fovy *= 3.1415926535/180.0;

	bn_mat_idn( m2 );
	m2[5] = cos(fovy/2.0) / sin(fovy/2.0);
	m2[0] = m2[5]/aspect;
	m2[10] = (far+near) / (far-near);
	m2[11] = 2*far*near / (far-near);	/* This should be negative */

	m2[14] = -1;		/* XXX This should be positive */
	m2[15] = 0;

	/* Move eye to origin, then apply perspective */
	bn_mat_idn( tran );
	tran[11] = -backoff;
	bn_mat_mul( m, m2, tran );
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
mike_persp_mat( pmat, eye )
mat_t		pmat;
CONST point_t	eye;
{
	mat_t	shear;
	mat_t	persp;
	mat_t	xlate;
	mat_t	t1, t2;
	point_t	sheared_eye;
#if 0
	fastf_t	near, far;
	point_t	a,b;
#endif

	if( eye[Z] < SMALL )  {
		VPRINT("mike_persp_mat(): ERROR, z<0, eye", eye);
		return;
	}

	/* Shear "eye" to +Z axis */
	bn_mat_idn(shear);
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
	bn_mat_idn(xlate);
	/* XXX should I use MAT_DELTAS_VEC_NEG()?  X and Y should be 0 now */
	MAT_DELTAS( xlate, 0, 0, 1-sheared_eye[Z] );

	/* Build perspective matrix inline, substituting fov=2*atan(1,Z) */
	bn_mat_idn( persp );
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
	VSET( a, 0, 0, -1 );
	MAT4X3PNT( b, pmat, a );
	VPRINT("0,0,-1 ->", b);

	VSET( a, 1, 1, -1 );
	MAT4X3PNT( b, pmat, a );
	VPRINT("1,1,-1 ->", b);

	VSET( a, 0, 0, 0 );
	MAT4X3PNT( b, pmat, a );
	VPRINT("0,0,0 ->", b);

	VSET( a, 1, 1, 0 );
	MAT4X3PNT( b, pmat, a );
	VPRINT("1,1,0 ->", b);

	VSET( a, 1, 1, 1 );
	MAT4X3PNT( b, pmat, a );
	VPRINT("1,1,1 ->", b);

	VSET( a, 0, 0, 1 );
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
	mat_t		tmat, tvmat;
	mat_t		new;
	matp_t		mat;
	int linestyle = 0;  /* not dashed */
	short r = -1;
	short g = -1;
	short b = -1;

	ndrawn = 0;
	inv_viewsize = 1 / VIEWSIZE;

	/*
	 * Draw all solids not involved in an edit.
	 */
	if( mged_variables.perspective <= 0 && eye_pos_scr[Z] == 1.0 )  {
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
		to_eye_scr = 1 / tan(mged_variables.perspective * bn_degtorad * 0.5);

#define SCR_WIDTH_PHYS	330	/* Assume a 330 mm wide screen */

		eye_delta_scr = mged_variables.eye_sep_dist * 0.5 / SCR_WIDTH_PHYS;

		VSET( l, -1, -1, -1 );
		VSET( h, 1, 1, 200.0 );
if(which_eye) {
printf("d=%gscr, d=%gmm, delta=%gscr\n", to_eye_scr, to_eye_scr * SCR_WIDTH_PHYS, eye_delta_scr);
VPRINT("l", l);
VPRINT("h", h);
}
		VSET( eye, 0, 0, to_eye_scr );
#if 0
		bn_mat_idn(tmat);
		tmat[11] = -1;
		bn_mat_mul( tvmat, tmat, model2view );
#endif
		switch(which_eye)  {
		case 0:
			/* Non-stereo case */
			mat = model2view;
/* XXX hack */
#define HACK 0
#if !HACK
if( 1 ) {
#else
if( mged_variables.faceplate > 0 )  {
#endif
			if( eye_pos_scr[Z] == 1.0 )  {
				/* This way works, with reasonable Z-clipping */
				persp_mat( perspective_mat, mged_variables.perspective,
					1.0, 0.01, 1.0e10, 1.0 );
			} else {
				/* This way does not have reasonable Z-clipping,
				 * but includes shear, for GDurf's testing.
				 */
				deering_persp_mat( perspective_mat, l, h, eye_pos_scr );
			}
} else {
			/* New way, should handle all cases */
			mike_persp_mat( perspective_mat, eye_pos_scr );
}
#if HACK
bn_mat_print("perspective_mat", perspective_mat);
#endif
			break;
		case 1:
			/* R */
			mat = model2view;
			eye[X] = eye_delta_scr;
			deering_persp_mat( perspective_mat, l, h, eye );
			break;
		case 2:
			/* L */
			mat = model2view;
			eye[X] = -eye_delta_scr;
			deering_persp_mat( perspective_mat, l, h, eye );
			break;
		}
		bn_mat_mul( new, perspective_mat, mat );
		mat = new;
	}

	dmp->dm_newrot( dmp, mat, which_eye );

	dmp->dm_setLineAttr(dmp, 1, linestyle); /* linewidth - 0, not dashed */
	FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
	  sp->s_flag = DOWN;		/* Not drawn yet */
	  /* If part of object rotation, will be drawn below */
	  if( sp->s_iflag == UP )
	    continue;

	  ratio = sp->s_size * inv_viewsize;

	  /*
	   * Check for this object being bigger than 
	   * dmp->dm_bound * the window size, or smaller than a speck.
	   */
	  if( ratio >= dmp->dm_bound || ratio < 0.001 )
	    continue;

	  if(linestyle != sp->s_soldash){
	    linestyle = sp->s_soldash;
	    dmp->dm_setLineAttr(dmp, 1, linestyle);
	  }

	  if(!DM_SAME_COLOR(r,g,b,
			    (short)sp->s_color[0],
			    (short)sp->s_color[1],
			    (short)sp->s_color[2])){
	    dmp->dm_setColor(dmp,
			     (short)sp->s_color[0],
			     (short)sp->s_color[1],
			     (short)sp->s_color[2], 0);
	    DM_COPY_COLOR(r,g,b,
			  (short)sp->s_color[0],
			  (short)sp->s_color[1],
			  (short)sp->s_color[2]);
	  }
	  if(dmp->dm_drawVList( dmp, (struct rt_vlist *)&sp->s_vlist, mat ) == TCL_OK) {
	    sp->s_flag = UP;
	    ndrawn++;
	  }
	}

	if(mged_variables.m_axes)
	  draw_axes(M_AXES);  /* draw world view axis */

	if(mged_variables.v_axes)
	  draw_axes(V_AXES);  /* draw view axis */

	/*
	 *  Draw all solids involved in editing.
	 *  They may be getting transformed away from the other solids.
	 */
	if( state == ST_VIEW )
		return;

	if( mged_variables.perspective <= 0 )  {
		mat = model2objview;
	} else {
		bn_mat_mul( new, perspective_mat, model2objview );
		mat = new;
	}
	dmp->dm_newrot( dmp, mat, which_eye );
	inv_viewsize /= modelchanges[15];
	dmp->dm_setColor(dmp, DM_WHITE, 1);

	FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
	  /* Ignore all objects not being rotated */
	  if( sp->s_iflag != UP )
	    continue;

	  ratio = sp->s_size * inv_viewsize;

	  /*
	   * Check for this object being bigger than 
	   * dmp->dm_bound * the window size, or smaller than a speck.
	   */
	  if( ratio >= dmp->dm_bound || ratio < 0.001 )
	    continue;

	  if(linestyle != sp->s_soldash){
	    linestyle = sp->s_soldash;
	    dmp->dm_setLineAttr(dmp, 1, linestyle);
	  }

	  if( dmp->dm_drawVList( dmp, (struct rt_vlist *)&sp->s_vlist, mat ) == TCL_OK){
	    sp->s_flag = UP;
	    ndrawn++;
	  }
	}

  if(mged_variables.e_axes)
    draw_axes(E_AXES); /* draw edit axis */
}

/*
 * Draw view, edit or world axes.
 */
static void
draw_axes(axes)
int axes;
{
  short r, g, b;
  short index;
  struct rt_vlist h_vlist;
  struct rt_vlist vlist;
  int i, j;
  double ox, oy;
  point_t a1, a2;
  point_t m1, m2;
  point_t m3, m4;
  point_t r_m3, r_m4;
  point_t v1, v2;
  mat_t mr_mat;   /* model rotations */
  static char *labels[] = {"X", "Y", "Z"};

  BU_LIST_INIT(&h_vlist.l);
  BU_LIST_APPEND(&h_vlist.l, &vlist.l);

  bn_mat_idn(mr_mat);
  dmp->dm_newrot(dmp, mr_mat, 0);

  if(axes_color_hook)
    (*axes_color_hook)(axes, &r, &g, &b, &index);
  else{/* use default color */
    switch(axes){
    case E_AXES:
      r = DM_WHITE_R;
      g = DM_WHITE_G;
      b = DM_WHITE_B;
      index = 1;
      break;
    case M_AXES:
      r = 150;
      g = 230;
      b = 150;
      index = 1;
      break;
    case V_AXES:
    default:
      r = 150;
      g = 150;
      b = 230;
      index = 1;
      break;
    }
  }

  if(axes == E_AXES)
    vlist.nused = 12;
  else
    vlist.nused = 6;

  /* set the vertex label color */
  dmp->dm_setColor(dmp, DM_YELLOW, 1);

  /* load vlist with axes */
  for(i = 0; i < 3; ++i){

    for(j = 0; j < 3; ++j){
      if(i == j){
	if(axes == V_AXES){
	  a1[j] = -0.125;
	  a2[j] = 0.125;
	}else{
	  a1[j] = -0.25;
	  a2[j] = 0.25;
	}
      }else{
	a1[j] = 0.0;
	a2[j] = 0.0;
      }
    }

    if(axes == M_AXES){ /* world axes */
      m1[X] = Viewscale*a1[X];
      m1[Y] = Viewscale*a1[Y];
      m1[Z] = Viewscale*a1[Z];
      m2[X] = Viewscale*a2[X];
      m2[Y] = Viewscale*a2[Y];
      m2[Z] = Viewscale*a2[Z];
    }else if(axes == V_AXES){  /* create view axes */
      /* build axes in view coodinates */

      /* apply rotations */
      MAT4X3PNT(v1, Viewrot, a1);
      MAT4X3PNT(v2, Viewrot, a2);

      /* possibly translate */
      if(mged_variables.v_axes > 2){
	switch(mged_variables.v_axes){
	case 3:     /* lower left */
	  ox = -0.8 / dmp->dm_aspect;
	  oy = -0.8;
	  break;
	case 4:     /* upper left */
	  ox = -0.8 / dmp->dm_aspect;
	  oy = 0.8;
	  break;
	case 5:     /* upper right */
	  ox = 0.8 / dmp->dm_aspect;
	  oy = 0.8;
	  break;
	case 6:     /* lower right */
	  ox = 0.8 / dmp->dm_aspect;
	  oy = -0.8;
	  break;
	default:    /* center */
	  ox = 0;
	  oy = 0;
	  break;
	}

	v1[X] += ox;
	v1[Y] += oy;
	v2[X] += ox;
	v2[Y] += oy;
      }

      /* convert view to model coordinates */
      MAT4X3PNT(m1, view2model, v1);
      MAT4X3PNT(m2, view2model, v2);
    }else{  /* create edit axes */
      /* build edit axes in model coordinates */
      if(state == ST_S_EDIT){
	/* apply rotations */
	MAT4X3PNT(m1, acc_rot_sol, a1);
	MAT4X3PNT(m2, acc_rot_sol, a2);

	/* apply scale and translations */
	m1[X] = Viewscale*m1[X] + curr_e_axes_pos[X];
	m1[Y] = Viewscale*m1[Y] + curr_e_axes_pos[Y];
	m1[Z] = Viewscale*m1[Z] + curr_e_axes_pos[Z];
	m2[X] = Viewscale*m2[X] + curr_e_axes_pos[X];
	m2[Y] = Viewscale*m2[Y] + curr_e_axes_pos[Y];
	m2[Z] = Viewscale*m2[Z] + curr_e_axes_pos[Z];
	m3[X] = Viewscale*a1[X] + e_axes_pos[X];
	m3[Y] = Viewscale*a1[Y] + e_axes_pos[Y];
	m3[Z] = Viewscale*a1[Z] + e_axes_pos[Z];
	m4[X] = Viewscale*a2[X] + e_axes_pos[X];
	m4[Y] = Viewscale*a2[Y] + e_axes_pos[Y];
	m4[Z] = Viewscale*a2[Z] + e_axes_pos[Z];
      }else if(state == ST_O_EDIT){
	point_t delta1, delta2;
	point_t point1, point2;

	VSCALE(a1, a1, Viewscale);
	VSCALE(a2, a2, Viewscale);

	MAT4X3PNT(delta1, es_mat, es_keypoint);
	MAT4X3PNT(delta2, modelchanges, delta1);

	VADD2(m3, delta1, a1);
	VADD2(m4, delta1, a2);

	MAT4X3PNT(point1, acc_rot_sol, a1);
	MAT4X3PNT(point2, acc_rot_sol, a2);
	VADD2(m1, delta2, point1);
	VADD2(m2, delta2, point2);
      }else
	return;
    }

    /* load axes */
    if(axes == E_AXES){
      VMOVE(vlist.pt[i*4], m1);
      vlist.cmd[i*4] = RT_VLIST_LINE_MOVE;
      VMOVE(vlist.pt[i*4 + 1], m2);
      vlist.cmd[i*4 + 1] = RT_VLIST_LINE_DRAW;

      VMOVE(vlist.pt[i*4 + 2], m3);
      vlist.cmd[i*4 + 2] = RT_VLIST_LINE_MOVE;
      VMOVE(vlist.pt[i*4 + 3], m4);
      vlist.cmd[i*4 + 3] = RT_VLIST_LINE_DRAW;

      /* convert point m4 from model to view space */
      MAT4X3PNT(v2, model2view, m4);

#ifdef DM_X
      /* label axes */
      if(dmp->dm_drawString2D == X_drawString2D)
	dmp->dm_drawString2D(dmp, labels[i], ((int)(2048.0 * v2[X])) + 15,
			     ((int)(2048.0 * v2[Y])) + 15, 1, 1);
      else
#endif
	dmp->dm_drawString2D(dmp, labels[i], ((int)(2048.0 * v2[X])) + 15,
			     ((int)(2048.0 * v2[Y])) + 15, 1, 0);
    }else{
      VMOVE(vlist.pt[i*2], m1);
      vlist.cmd[i*2] = RT_VLIST_LINE_MOVE;
      VMOVE(vlist.pt[i*2 + 1], m2);
      vlist.cmd[i*2 + 1] = RT_VLIST_LINE_DRAW;
    }

    /* convert point m2 from model to view space */
    MAT4X3PNT(v2, model2view, m2);

#ifdef DM_X
    /* label axes */
    if(dmp->dm_drawString2D == X_drawString2D)
      dmp->dm_drawString2D(dmp, labels[i], ((int)(2048.0 * v2[X])) + 15,
			   ((int)(2048.0 * v2[Y])) + 15, 1, 1);
    else
#endif
      dmp->dm_drawString2D(dmp, labels[i], ((int)(2048.0 * v2[X])) + 15,
			   ((int)(2048.0 * v2[Y])) + 15, 1, 0);
  }

  dmp->dm_newrot(dmp, model2view, 0);

  /* draw axes */
  dmp->dm_setColor(dmp, r, g, b, 1);
  dmp->dm_setLineAttr(dmp, 1, 0);  /* linewidth - 1, not dashed */
  dmp->dm_drawVList(dmp, &h_vlist, model2view);
}
