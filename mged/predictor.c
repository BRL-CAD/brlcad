/*
 *			P R E D I C T O R . C
 *
 *  Put a predictor frame into view, as an aid to velocity-based
 *  navigation through an MGED model.
 *
 *  Inspired by the paper "Manipulating the Future:  Predictor Based
 *  Feedback for Velocity Control in Virtual Environment Navigation"
 *  by Dale Chapman and Colin Ware, <cware@unb.ca>, in
 *  ACM SIGGRAPH Computer Graphics Special Issue on 1992 Symposium
 *  on Interactive 3D Graphics.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1992 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <math.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "./ged.h"
#include "./cmd.h"
#include "./mged_dm.h"

extern mat_t	ModelDelta;		/* Changed to Viewrot this frame */

#define MAX_TRAIL	32
struct trail {
	int	cur_index;	/* index of first free entry */
	int	nused;		/* max index in use */
	point_t	pt[MAX_TRAIL];
};

/*
 *			I N I T _ T R A I L
 */
static void
init_trail(tp)
struct trail	*tp;
{
	tp->cur_index = 0;
	tp->nused = 0;
}

/*
 *			P U S H _ T R A I L
 *
 *  Add a new point to the end of the trail.
 */
static void
push_trail(tp, pt)
struct trail	*tp;
point_t		pt;
{
	VMOVE( tp->pt[tp->cur_index], pt );
	if( tp->cur_index >= tp->nused )  tp->nused++;
	tp->cur_index++;
	if( tp->cur_index >= MAX_TRAIL )  tp->cur_index = 0;
}

#if 0
/*
 *			D R A W _ T R A I L
 *
 *  Draw from the most recently added point, backwards, as vectors.
 */
static void
draw_trail(vhead, tp)
struct bu_list	*vhead;
struct trail	*tp;
{
	int	i;
	int	todo = tp->nused;

	BU_LIST_INIT( vhead );
	if( tp->nused <= 0 )  return;
	if( (i = tp->cur_index-1) < 0 )  i = tp->nused-1;
	for( ; todo > 0; todo-- )  {
		if( todo == tp->nused )  {
			RT_ADD_VLIST( vhead, tp->pt[i], RT_VLIST_LINE_MOVE );
		}  else  {
			RT_ADD_VLIST( vhead, tp->pt[i], RT_VLIST_LINE_DRAW );
		}
		if( (--i) < 0 )  i = tp->nused-1;
	}
}
#endif

/*
 *			P O L Y _ T R A I L
 *
 *  Draw from the most recently added points in two trails, as polygons.
 *  Proceeds backwards.
 *  t1 should be below (lower screen Y) t2.
 */
static void
poly_trail(vhead, t1, t2)
struct bu_list	*vhead;
struct trail	*t1;
struct trail	*t2;
{
	int	i1, i2;
	int	todo = t1->nused;
	fastf_t	*s1, *s2;
	vect_t	right, up;
	vect_t	norm;

	if( t2->nused < todo )  todo = t2->nused;

	BU_LIST_INIT( vhead );
	if( t1->nused <= 0 || t1->nused <= 0 )  return;

	if( (i1 = t1->cur_index-1) < 0 )  i1 = t1->nused-1;
	if( (i2 = t2->cur_index-1) < 0 )  i2 = t2->nused-1;

	/* Get starting points, next to frame. */
	s1 = t1->pt[i1];
	s2 = t2->pt[i2];
	if( (--i1) < 0 )  i1 = t1->nused-1;
	if( (--i2) < 0 )  i2 = t2->nused-1;
	todo--;

	for( ; todo > 0; todo-- )  {
		/* Go from s1 to s2 to t2->pt[i2] to t1->pt[i1] */
		VSUB2( up, s1, s2 );
		VSUB2( right, t1->pt[i1], s1 );
		VCROSS( norm, right, up );

		RT_ADD_VLIST( vhead, norm, RT_VLIST_POLY_START );
		RT_ADD_VLIST( vhead, s1, RT_VLIST_POLY_MOVE );
		RT_ADD_VLIST( vhead, s2, RT_VLIST_POLY_DRAW );
		RT_ADD_VLIST( vhead, t2->pt[i2], RT_VLIST_POLY_DRAW );
		RT_ADD_VLIST( vhead, t1->pt[i1], RT_VLIST_POLY_DRAW );
		RT_ADD_VLIST( vhead, s1, RT_VLIST_POLY_END );

		s1 = t1->pt[i1];
		s2 = t2->pt[i2];

		if( (--i1) < 0 )  i1 = t1->nused-1;
		if( (--i2) < 0 )  i2 = t2->nused-1;
	}
}

static struct trail	tA, tB, tC, tD;
static struct trail	tE, tF, tG, tH;

#define PREDICTOR_NAME	"_PREDIC_FRAME_"

/*
 *			P R E D I C T O R _ K I L L
 *
 *  Don't use mged "kill" command, just use "d".
 */
void
predictor_kill()
{
  char *av[3];

  av[0] = "d";
  av[2] = NULL;

  av[1] = PREDICTOR_NAME;
  (void)f_erase_all((ClientData)NULL, interp, 2, av);

  av[1] = "_PREDIC_TRAIL_LL_";
  (void)f_erase_all((ClientData)NULL, interp, 2, av);

  av[1] = "_PREDIC_TRAIL_LR_";
  (void)f_erase_all((ClientData)NULL, interp, 2, av);

  av[1] = "_PREDIC_TRAIL_UR_";
  (void)f_erase_all((ClientData)NULL, interp, 2, av);

  av[1] = "_PREDIC_TRAIL_UL_";
  (void)f_erase_all((ClientData)NULL, interp, 2, av);

  init_trail( &tA );
  init_trail( &tB );
  init_trail( &tC );
  init_trail( &tD );

  init_trail( &tE );
  init_trail( &tF );
  init_trail( &tG );
  init_trail( &tH );
}

#define TF_BORD	0.01
#define TF_X	0.14
#define TF_Y	0.07
#define TF_Z	(1.0-0.15)	/* To prevent Z clipping of TF_X */

#define TF_VL( _m, _v ) \
	{ vect_t edgevect_m; \
	MAT4X3VEC( edgevect_m, predictorXv2m, _v ); \
	VADD2( _m, framecenter_m, edgevect_m ); }

/*
 *			P R E D I C T O R _ F R A M E
 *
 *  Draw the frame itself as four polygons:
 *	ABFE, HGCD, EILH, and JFGK.
 *  The streamers will attach at edges AE, BF, GC, and HD.
 *	
 *		D --------------- C
 *		|                 |
 *		H -L-----------K- G
 *		|  |           |  |
 *		|  |           |  |
 *		|  |           |  |
 *		E -I-----------J- F
 *		|                 |
 *		A --------------- B
 */
void
predictor_frame()
{
	int	i;
	int	nframes;
	mat_t	predictor;
	mat_t	predictorXv2m;
	point_t	m;		/* model coords */
	point_t	mA,mB,mC,mD,mE,mF,mG,mH,mI,mJ,mK,mL;
	struct bu_list	vhead;
	struct bu_list	trail;
	point_t	framecenter_m;
#if 0
	point_t	framecenter_v;
#endif
	point_t	center_m;
	vect_t	delta_v;
	vect_t	right, up;
	vect_t	norm;

	if( rateflag_rotate == 0 && rateflag_slew == 0 && rateflag_zoom == 0 )  {
		/* If no motion, and predictor is drawn, get rid of it */
		if( db_lookup( dbip, PREDICTOR_NAME, LOOKUP_QUIET ) != DIR_NULL )  {
			predictor_kill();
			dmaflag = 1;
		}
		return;
	}

	/* Advance into the future */
	nframes = (int)(mged_variables.predictor_advance / frametime);
	if( nframes < 1 )  nframes = 1;

	/* Build view2model matrix for the future time */
	bn_mat_idn( predictor );
	for( i=0; i < nframes; i++ )  {
		bn_mat_mul2( ModelDelta, predictor );
	}
	bn_mat_mul( predictorXv2m, predictor, view2model );

	MAT_DELTAS_GET_NEG( center_m, toViewcenter );
	MAT4X3PNT( framecenter_m, predictor, center_m );
#if 0
	MAT4X3PNT( framecenter_v, model2view, framecenter_m );
#endif

	/*
	 * Draw the frame around the point framecenter_v.
	 */
	BU_LIST_INIT( &vhead );

	/* Centering dot */
	VSETALL( delta_v, 0 );
	TF_VL( m, delta_v );
	RT_ADD_VLIST( &vhead, m, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( &vhead, m, RT_VLIST_LINE_DRAW );

	/* The exterior rectangle */
	VSET( delta_v, -TF_X, -TF_Y, 0 );
	TF_VL( mA, delta_v );

	VSET( delta_v,  TF_X, -TF_Y, 0 );
	TF_VL( mB, delta_v );

	VSET( delta_v,  TF_X,  TF_Y, 0 );
	TF_VL( mC, delta_v );

	VSET( delta_v, -TF_X,  TF_Y, 0 );
	TF_VL( mD, delta_v );

	/* The EFGH rectangle */
	VSET( delta_v, -TF_X, -TF_Y+TF_BORD, 0 );
	TF_VL( mE, delta_v );

	VSET( delta_v,  TF_X, -TF_Y+TF_BORD, 0 );
	TF_VL( mF, delta_v );

	VSET( delta_v,  TF_X,  TF_Y-TF_BORD, 0 );
	TF_VL( mG, delta_v );

	VSET( delta_v, -TF_X,  TF_Y-TF_BORD, 0 );
	TF_VL( mH, delta_v );

	/* The IJKL rectangle */
	VSET( delta_v, -TF_X+TF_BORD, -TF_Y+TF_BORD, 0 );
	TF_VL( mI, delta_v );

	VSET( delta_v,  TF_X-TF_BORD, -TF_Y+TF_BORD, 0 );
	TF_VL( mJ, delta_v );

	VSET( delta_v,  TF_X-TF_BORD,  TF_Y-TF_BORD, 0 );
	TF_VL( mK, delta_v );

	VSET( delta_v, -TF_X+TF_BORD,  TF_Y-TF_BORD, 0 );
	TF_VL( mL, delta_v );

	VSUB2( right, mB, mA );
	VSUB2( up, mD, mA );
	VCROSS( norm, right, up );
	VUNITIZE(norm);

#if 0
	RT_ADD_VLIST( &vhead, mA, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( &vhead, mB, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( &vhead, mF, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( &vhead, mE, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( &vhead, mA, RT_VLIST_LINE_DRAW );

	RT_ADD_VLIST( &vhead, mE, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( &vhead, mI, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( &vhead, mL, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( &vhead, mH, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( &vhead, mE, RT_VLIST_LINE_DRAW );

	RT_ADD_VLIST( &vhead, mH, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( &vhead, mG, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( &vhead, mC, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( &vhead, mD, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( &vhead, mH, RT_VLIST_LINE_DRAW );

	RT_ADD_VLIST( &vhead, mJ, RT_VLIST_LINE_MOVE );
	RT_ADD_VLIST( &vhead, mF, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( &vhead, mG, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( &vhead, mK, RT_VLIST_LINE_DRAW );
	RT_ADD_VLIST( &vhead, mJ, RT_VLIST_LINE_DRAW );
#else
	RT_ADD_VLIST( &vhead, norm, RT_VLIST_POLY_START );
	RT_ADD_VLIST( &vhead, mA, RT_VLIST_POLY_MOVE );
	RT_ADD_VLIST( &vhead, mB, RT_VLIST_POLY_DRAW );
	RT_ADD_VLIST( &vhead, mF, RT_VLIST_POLY_DRAW );
	RT_ADD_VLIST( &vhead, mE, RT_VLIST_POLY_DRAW );
	RT_ADD_VLIST( &vhead, mA, RT_VLIST_POLY_END );

	RT_ADD_VLIST( &vhead, norm, RT_VLIST_POLY_START );
	RT_ADD_VLIST( &vhead, mE, RT_VLIST_POLY_MOVE );
	RT_ADD_VLIST( &vhead, mI, RT_VLIST_POLY_DRAW );
	RT_ADD_VLIST( &vhead, mL, RT_VLIST_POLY_DRAW );
	RT_ADD_VLIST( &vhead, mH, RT_VLIST_POLY_DRAW );
	RT_ADD_VLIST( &vhead, mE, RT_VLIST_POLY_END );

	RT_ADD_VLIST( &vhead, norm, RT_VLIST_POLY_START );
	RT_ADD_VLIST( &vhead, mH, RT_VLIST_POLY_MOVE );
	RT_ADD_VLIST( &vhead, mG, RT_VLIST_POLY_DRAW );
	RT_ADD_VLIST( &vhead, mC, RT_VLIST_POLY_DRAW );
	RT_ADD_VLIST( &vhead, mD, RT_VLIST_POLY_DRAW );
	RT_ADD_VLIST( &vhead, mH, RT_VLIST_POLY_END );

	RT_ADD_VLIST( &vhead, norm, RT_VLIST_POLY_START );
	RT_ADD_VLIST( &vhead, mJ, RT_VLIST_POLY_MOVE );
	RT_ADD_VLIST( &vhead, mF, RT_VLIST_POLY_DRAW );
	RT_ADD_VLIST( &vhead, mG, RT_VLIST_POLY_DRAW );
	RT_ADD_VLIST( &vhead, mK, RT_VLIST_POLY_DRAW );
	RT_ADD_VLIST( &vhead, mJ, RT_VLIST_POLY_END );
#endif

	invent_solid( PREDICTOR_NAME, &vhead, 0x00FFFFFFL, 0 );

	push_trail( &tA, mA );
	push_trail( &tB, mB );
	push_trail( &tC, mC );
	push_trail( &tD, mD );

	push_trail( &tE, mE );
	push_trail( &tF, mF );
	push_trail( &tG, mG );
	push_trail( &tH, mH );

	/* Draw the trails */

#if 0
	draw_trail( &trail, &tA );
	invent_solid( "_PREDIC_TRAIL_LL_", &trail, 0x00FF00FFL, 0 );

	draw_trail( &trail, &tB );
	invent_solid( "_PREDIC_TRAIL_LR_", &trail, 0x0000FFFFL, 0 );

	draw_trail( &trail, &tC );
	invent_solid( "_PREDIC_TRAIL_UR_", &trail, 0x00FF00FFL, 0 );

	draw_trail( &trail, &tD );
	invent_solid( "_PREDIC_TRAIL_UL_", &trail, 0x0000FFFFL, 0 );
#else
	poly_trail( &trail, &tA, &tE );
	invent_solid( "_PREDIC_TRAIL_LL_", &trail, 0x00FF00FFL, 0 );

	poly_trail( &trail, &tB, &tF );
	invent_solid( "_PREDIC_TRAIL_LR_", &trail, 0x0000FFFFL, 0 );

	poly_trail( &trail, &tG, &tC );
	invent_solid( "_PREDIC_TRAIL_UR_", &trail, 0x00FF00FFL, 0 );

	poly_trail( &trail, &tH, &tD );
	invent_solid( "_PREDIC_TRAIL_UL_", &trail, 0x0000FFFFL, 0 );
#endif

	/* Done */
	bn_mat_idn( ModelDelta );
}

/*
 *			P R E D I C T O R _ H O O K
 *
 *  Called from set.c when the predictor variables are modified.
 */
void
predictor_hook()
{
	if( mged_variables.predictor > 0 )  {
		/* Allocate storage? */
	} else {
		/* Release storage? */
		predictor_kill();
	}
	dmaflag = 1;
}
