/*
 *			U S E P E N . C
 *
 * Functions -
 *	usepen		Use x,y data from data tablet
 *	buildHrot	Generate rotation matrix
 *	wrt_view	Modify xform matrix with respect to current view
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
#include "externs.h"
#include "./ged.h"
#include "./titles.h"
#include "./solid.h"
#include "./menu.h"
#include "./dm.h"
#include "./sedit.h"

/*	Degree <-> Radian conversion factors	*/
double	degtorad =  0.01745329251994329573;
double	radtodeg = 57.29577951308232098299;

int	sedraw;			/* apply solid editing changes */
struct solid	*illump;	/* == 0 if none, else points to ill. solid */
int		ipathpos;	/* path index of illuminated element */
				/* set by e9.c, cleared here */
void		wrt_view(), wrt_point();
static void	illuminate();

/*
 *			F _ M O U S E
 *
 * X and Y are expected to be in -2048 <= x,y <= +2047 range.
 * The "up" flag is 1 on the not-pressed to pressed transition,
 * and 0 on the pressed to not-pressed transition.
 *
 * Note -
 *  The mouse is the focus of much of the editing activity in GED.
 *  The editor operates in one of seven basic editing states, recorded
 *  in the variable called "state".  When no editing is taking place,
 *  the editor is in state ST_VIEW.  There are two paths out of ST_VIEW:
 *  
 *  BE_S_ILLUMINATE, when pressed, takes the editor into ST_S_PICK,
 *  where the mouse is used to pick a solid to edit, using our
 *  unusual "illuminate" technique.  Moving the mouse varies the solid
 *  being illuminated.  When the mouse is pressed, the editor moves into
 *  state ST_S_EDIT, and solid editing may begin.  Solid editing is
 *  terminated via BE_ACCEPT and BE_REJECT.
 *  
 *  BE_O_ILLUMINATE, when pressed, takes the editor into ST_O_PICK,
 *  again performing the illuminate procedure.  When the mouse is pressed,
 *  the editor moves into state ST_O_PATH.  Now, moving the mouse allows
 *  the user to choose the portion of the path relation to be edited.
 *  When the mouse is pressed, the editor moves into state ST_O_EDIT,
 *  and object editing may begin.  Object editing is terminated via
 *  BE_ACCEPT and BE_REJECT.
 *  
 *  The only way to exit the intermediate states (non-VIEW, non-EDIT)
 *  is by completing the sequence, or pressing BE_REJECT.
 */
void
f_mouse( argc, argv )
int	argc;
char	**argv;
{
	fastf_t			scale;
	register struct solid	*sp;
	vect_t	pos_view;	 	/* Unrotated view space pos */
	vect_t	pos_model;	/* Rotated screen space pos */
	vect_t	tr_temp;		/* temp translation vector */
	vect_t	mousevec;		/* float pt -1..+1 mouse pos vect */
	vect_t	temp;
	int	isave;
	int	up = atoi(argv[1]);
	int	xpos = atoi(argv[2]);
	int	ypos = atoi(argv[3]);

	/* Build floating point mouse vector, -1 to +1 */
	mousevec[X] =  xpos / 2047.0;
	mousevec[Y] =  ypos / 2047.0;
	mousevec[Z] = 0;

	/*
	 * If mouse press is in scroll area, see if scrolling, and if so,
	 * divert this mouse press.
	 */
	if( (xpos >= MENUXLIM) && up )  {
		register int i;

		if( (i = scroll_select(xpos, ypos )) < 0 )  {
			(void)printf("mouse press outside valid scroll area\n");
			return;
		} 
		if( i > 0 )  {
			/* Scroller bars claimed button press */
			return;
		}
		/* Otherwise, fall through */
	}

	/*
	 * If menu is active, and mouse press is in menu area,
	 * divert this mouse press for menu purposes.
	 */
	if( xpos < MENUXLIM && up )  {
		register int i;
		if( (i = mmenu_select( ypos )) < 0 )  {
			(void)printf("mouse press outside valid menu\n");
			return;
		}
		if( i > 0 )  {
			/* Menu claimed button press */
			return;
		}
		/* Otherwise, fall through */
	}
	
	/*
	 *  In the best of all possible worlds, nothing should happen
	 *  when the mouse is not pressed;  this would relax the requirement
	 *  for the host being informed when the mouse changes position.
	 *  However, for now, illuminate mode makes this impossible.
	 */
	if( up == 0 )  switch( state )  {

	case ST_VIEW:
	case ST_S_EDIT:
	case ST_O_EDIT:
	default:
		return;		/* Take no action in these states */

	case ST_O_PICK:
	case ST_S_PICK:
		/*
		 * Use the mouse for illuminating a solid
		 */
		illuminate( ypos );
		return;

	case ST_O_PATH:
		/*
		 * Convert DT position to path element select
		 */
		isave = ipathpos;
		ipathpos = illump->s_last - (
			(ypos+2048L) * (illump->s_last+1) / 4096);
		if( ipathpos != isave )
			dmaflag++;
		return;

	} else switch( state )  {

	case ST_VIEW:
		/*
		 * Use the DT for moving view center.
		 * Make indicated point be new view center (NEW).
		 */
		slewview( mousevec );
		return;

	case ST_O_PICK:
		ipathpos = 0;
		(void)chg_state( ST_O_PICK, ST_O_PATH, "mouse press");
		dmaflag = 1;
		return;

	case ST_S_PICK:
		/* Check details, Init menu, set state */
		init_sedit();		/* does chg_state */
		dmaflag = 1;
		return;

	case ST_S_EDIT:
		/*
		 *  Solid Edit
		 */
		if( es_edflag <= 0 )  return;
		switch( es_edflag )  {

		case SSCALE:
		case PSCALE:
			/* use mouse to get a scale factor */
			es_scale = 1.0 + 0.25 * ((fastf_t)
				(ypos > 0 ?
					ypos :
					-ypos) / 2047);
			if ( ypos <= 0 )
				es_scale = 1.0 / es_scale;

			/* accumulate scale factor */
			acc_sc_sol *= es_scale;

			sedraw = 1;
			return;
		case STRANS:
			/* 
			 * Use mouse to change solid's location.
			 * Project solid's V point into view space,
			 * replace X,Y (but NOT Z) components, and
			 * project result back to model space.
			 */
			/* XXX this makes bad assumptions about format of es_rec !! */
			MAT4X3PNT( temp, es_mat, es_rec.s.s_values );
			MAT4X3PNT( pos_view, model2view, temp );
			pos_view[X] = xpos / 2047.0;
			pos_view[Y] = ypos / 2047.0;
			MAT4X3PNT( temp, view2model, pos_view );
			MAT4X3PNT( es_rec.s.s_values, es_invmat, temp );
			sedraw = 1;
			return;
		case MOVEH:
		case MOVEHH:
			/* Use mouse to change location of point V+H */
			VADD2( temp, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_H );
			MAT4X3PNT(pos_model, es_mat, temp);
			MAT4X3PNT( pos_view, model2view, pos_model );
			pos_view[X] = xpos / 2047.0;
			pos_view[Y] = ypos / 2047.0;
			/* Do NOT change pos_view[Z] ! */
			MAT4X3PNT( temp, view2model, pos_view );
			MAT4X3PNT( tr_temp, es_invmat, temp );
			VSUB2( &es_rec.s.s_tgc_H, tr_temp, &es_rec.s.s_tgc_V );
			sedraw = 1;
			return;
		case PTARB:
			/* move an arb point to indicated point */
			/* point is located at es_values[es_menu*3] */
			VADD2(temp, es_rec.s.s_values, &es_rec.s.s_values[es_menu*3]);
			MAT4X3PNT(pos_model, es_mat, temp);
			MAT4X3PNT(pos_view, model2view, pos_model);
			pos_view[X] = xpos / 2047.0;
			pos_view[Y] = ypos / 2047.0;
			MAT4X3PNT(temp, view2model, pos_view);
			MAT4X3PNT(pos_model, es_invmat, temp);
			editarb( pos_model );
			sedraw = 1;
			return;
		case EARB:
			/* move arb edge, through indicated point */
			mousevec[X] = xpos / 2047.0;
			mousevec[Y] = ypos / 2047.0;
			mousevec[Z] = 0;
			MAT4X3PNT( temp, view2model, mousevec );
			/* apply inverse of es_mat */
			MAT4X3PNT( pos_model, es_invmat, temp );
			editarb( pos_model );
			sedraw = 1;
			return;
		case MVFACE:
			/* move arb face, through  indicated  point */
			mousevec[X] = xpos / 2047.0;
			mousevec[Y] = ypos / 2047.0;
			mousevec[Z] = 0;
			MAT4X3PNT( temp, view2model, mousevec );
			/* apply inverse of es_mat */
			MAT4X3PNT( pos_model, es_invmat, temp );
			/* change D of planar equation */
			es_peqn[es_menu][3]=VDOT(&es_peqn[es_menu][0], pos_model);
			/* calculate new vertices, put in record as vectors */
			calc_pnts( &es_rec.s, es_rec.s.s_cgtype );
			sedraw = 1;
			return;
			
		default:
			(void)printf("mouse press undefined in this solid edit mode\n");
			break;
		}
		return;

	case ST_O_PATH:
		/*
		 * Set combination "illuminate" mode.  This code
		 * assumes that the user has already illuminated
		 * a single solid, and wishes to move a collection of
		 * objects of which the illuminated solid is a part.
		 * The whole combination will not illuminate (to save
		 * vector drawing time), but all the objects should
		 * move/scale in unison.
		 */
		dmp->dmr_light( LIGHT_ON, BE_ACCEPT );
		dmp->dmr_light( LIGHT_ON, BE_REJECT );
		dmp->dmr_light( LIGHT_OFF, BE_O_ILLUMINATE );

		/* Include all solids with same tree top */
		FOR_ALL_SOLIDS( sp )  {
			register int j;

			for( j = 0; j <= ipathpos; j++ )  {
				if( sp->s_path[j] != illump->s_path[j] )
					break;
			}
			/* Only accept if top of tree is identical */
			if( j == ipathpos+1 )
				sp->s_iflag = UP;
		}
		(void)chg_state( ST_O_PATH, ST_O_EDIT, "mouse press" );
		chg_l2menu(ST_O_EDIT);

		/* begin object editing - initialize */
		init_objedit();

		dmaflag++;
		return;

	case ST_O_EDIT:
		/*
		 *  Object Edit
		 */
		mat_idn( incr_change );
		scale = 1;
		if( movedir & SARROW )  {
			/* scaling option is in effect */
			scale = 1.0 + (fastf_t)(ypos > 0 ?
				ypos : -ypos) / (2047);
			if ( ypos <= 0 )
				scale = 1.0 / scale;

			/* switch depending on scaling option selected */
			switch( edobj ) {

				case BE_O_SCALE:
					/* global scaling */
					incr_change[15] = 1.0 / scale;
				break;

				case BE_O_XSCALE:
					/* local scaling ... X-axis */
					incr_change[0] = scale;
					/* accumulate the scale factor */
					acc_sc[0] *= scale;
				break;

				case BE_O_YSCALE:
					/* local scaling ... Y-axis */
					incr_change[5] = scale;
					/* accumulate the scale factor */
					acc_sc[1] *= scale;
				break;

				case BE_O_ZSCALE:
					/* local scaling ... Z-axis */
					incr_change[10] = scale;
					/* accumulate the scale factor */
					acc_sc[2] *= scale;
				break;
			}

			/* Have scaling take place with respect to a point,
			 * NOT the view center.
			 */
			/* XXX should have an es_keypoint for this */
			MAT4X3PNT(temp, es_mat, es_rec.s.s_values);
			MAT4X3PNT(pos_model, modelchanges, temp);
			wrt_point(modelchanges, incr_change, modelchanges, pos_model);
		}  else if( movedir & (RARROW|UARROW) )  {
			mat_t oldchanges;	/* temporary matrix */

			/* Vector from object center to cursor */
			/* XXX should have an es_keypoint for this */
			MAT4X3PNT( temp, es_mat, es_rec.s.s_values );
			MAT4X3PNT( pos_view, model2objview, temp );
			if( movedir & RARROW )
				pos_view[X] = xpos / 2047.0;
			if( movedir & UARROW )
				pos_view[Y] = ypos / 2047.0;

			MAT4X3PNT( pos_model, view2model, pos_view );/* NOT objview */
			MAT4X3PNT( tr_temp, modelchanges, temp );
			VSUB2( tr_temp, pos_model, tr_temp );
			MAT_DELTAS(incr_change,
				tr_temp[X], tr_temp[Y], tr_temp[Z]);
			mat_copy( oldchanges, modelchanges );
			mat_mul( modelchanges, incr_change, oldchanges );
		}  else  {
			(void)printf("No object edit mode selected;  mouse press ignored\n");
			return;
		}
		mat_idn( incr_change );
		new_mats();
		return;

	default:
		state_err( "mouse press" );
		return;
	}
	/* NOTREACHED */
}

/*
 *			I L L U M I N A T E
 */
static void
illuminate( y )  {
	register int count;
	register struct solid *sp;
	register struct solid *saveillump;

	saveillump = illump;

	/*
	 * Divide the mouse into 'ndrawn' VERTICAL zones, and use the
	 * zone number as a sequential position among solids
	 * which are drawn.
	 */
	count = ( (fastf_t) y + 2048.0 ) * ndrawn / 4096.0;

	FOR_ALL_SOLIDS( sp )  {
		if( sp->s_flag == UP )
			if( count-- == 0 && illump != sp )  {
				sp->s_iflag = UP;
				dmp->dmr_viewchange( DM_CHGV_ILLUM, sp );
				illump = sp;
			}  else
				sp->s_iflag = DOWN;
	}
	if( saveillump != illump )
		dmaflag++;
}

/*
 *			B U I L D H R O T
 *
 * This routine builds a Homogeneous rotation matrix, given
 * alpha, beta, and gamma as angles of rotation.
 *
 * NOTE:  Only initialize the rotation 3x3 parts of the 4x4
 * There is important information in dx,dy,dz,s .
 */
void
buildHrot( mat, alpha, beta, ggamma )
register matp_t mat;
double alpha, beta, ggamma;
{
	static fastf_t calpha, cbeta, cgamma;
	static fastf_t salpha, sbeta, sgamma;

	calpha = cos( alpha );
	cbeta = cos( beta );
	cgamma = cos( ggamma );

	salpha = sin( alpha );
	sbeta = sin( beta );
	sgamma = sin( ggamma );

	/*
	 * compute the new rotation to apply to the previous
	 * viewing rotation.
	 * Alpha is angle of rotation about the X axis, and is done third.
	 * Beta is angle of rotation about the Y axis, and is done second.
	 * Gamma is angle of rotation about Z axis, and is done first.
	 */
#ifdef m_RZ_RY_RX
	/* view = model * RZ * RY * RX (Neuman+Sproul, premultiply) */
	mat[0] = cbeta * cgamma;
	mat[1] = -cbeta * sgamma;
	mat[2] = -sbeta;

	mat[4] = -salpha * sbeta * cgamma + calpha * sgamma;
	mat[5] = salpha * sbeta * sgamma + calpha * cgamma;
	mat[6] = -salpha * cbeta;

	mat[8] = calpha * sbeta * cgamma + salpha * sgamma;
	mat[9] = -calpha * sbeta * sgamma + salpha * cgamma;
	mat[10] = calpha * cbeta;
#endif
	/* This is the correct form for this version of GED */
	/* view = RX * RY * RZ * model (Rodgers, postmultiply) */
	/* Point thumb along axis of rotation.  +Angle as hand closes */
	mat[0] = cbeta * cgamma;
	mat[1] = -cbeta * sgamma;
	mat[2] = sbeta;

	mat[4] = salpha * sbeta * cgamma + calpha * sgamma;
	mat[5] = -salpha * sbeta * sgamma + calpha * cgamma;
	mat[6] = -salpha * cbeta;

	mat[8] = -calpha * sbeta * cgamma + salpha * sgamma;
	mat[9] = calpha * sbeta * sgamma + salpha * cgamma;
	mat[10] = calpha * cbeta;
}


/*
 *  			W R T _ V I E W
 *  
 *  Given a model-space transformation matrix "change",
 *  return a matrix which applies the change with-respect-to
 *  the view center.
 */
void
wrt_view( out, change, in )
register matp_t out, change, in;
{
	static mat_t t1, t2;

	mat_mul( t1, toViewcenter, in );
	mat_mul( t2, change, t1 );

	/* Build "fromViewcenter" matrix */
	mat_idn( t1 );
	MAT_DELTAS( t1, -toViewcenter[MDX], -toViewcenter[MDY], -toViewcenter[MDZ] );
	mat_mul( out, t1, t2 );
}

/*
 *  			W R T _ P O I N T
 *  
 *  Given a model-space transformation matrix "change",
 *  return a matrix which applies the change with-respect-to
 *  "point".
 */
void
wrt_point( out, change, in, point )
register matp_t out, change, in;
register vect_t point;
{
	static mat_t t1, t2, pt_to_origin, origin_to_pt;

	/* build "point to origin" matrix */
	mat_idn( pt_to_origin );
	MAT_DELTAS(pt_to_origin, -point[X], -point[Y], -point[Z]);

	/* build "origin to point" matrix */
	mat_idn( origin_to_pt );
	MAT_DELTAS(origin_to_pt, point[X], point[Y], point[Z]);

	/* t1 = pt_to_origin * in */
	mat_mul( t1, pt_to_origin, in );

	/* apply change matrix: t2 = change * pt_to_origin * in */
	mat_mul( t2, change, t1 );

	/* apply origin_to_pt matrix:
	 *	out = origin_to_pt * change * pt_to_origin * in
	 */
	mat_mul( out, origin_to_pt, t2 );
}
