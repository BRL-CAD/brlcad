/*
 *			T I T L E S . C
 *
 * Functions -
 *	dotitles	Output GED "faceplate" & titles.
 *
 *  Author -
 *	Michael John Muuss
 *	Kieth A. Applin
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
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./titles.h"
#include "./solid.h"
#include "./sedit.h"
#include "./dm.h"

int	state;
char	*state_str[] = {
	"-ZOT-",
	"VIEWING",
	"SOL PICK",
	"SOL EDIT",
	"OBJ PICK",
	"OBJ PATH",
	"OBJ EDIT",
	"UNKNOWN",
};

extern struct rt_db_internal	es_int;


/*
 *			C R E A T E _ T E X T _ O V E R L A Y
 *
 *  Prepare the numerical display of the currently edited solid/object.
 */
void
create_text_overlay( vp )
register struct rt_vls	*vp;
{
	auto char linebuf[512];
	register int	i;

	RT_VLS_CHECK(vp);

	/*
	 * Set up for character output.  For the best generality, we
	 * don't assume that the display can process a CRLF sequence,
	 * so each line is written with a separate call to dmp->dmr_puts().
	 */

	/* print solid info at top of screen */
	if( es_edflag >= 0 ) {
		rt_vls_strcat( vp, "** SOLID -- " );
		rt_vls_strcat( vp, es_name );
		rt_vls_strcat( vp, ": ");

		vls_solid( vp, &es_rec.s, rt_identity );

		if(illump->s_last) {
			rt_vls_strcat( vp, "\n** PATH --  ");
			for(i=0; i <= illump->s_last; i++) {
				rt_vls_strcat( vp, "/" );
				rt_vls_strcat( vp, illump->s_path[i]->d_namep);
			}
			rt_vls_strcat( vp, ": " );

			/* print the evaluated (path) solid parameters */
			vls_solid( vp, &es_rec.s, es_mat );
		}
	}

	/* display path info for object editing also */
	if( state == ST_O_EDIT ) {
		rt_vls_strcat( vp, "** PATH --  ");
		for(i=0; i <= illump->s_last; i++) {
			rt_vls_strcat( vp, "/" );
			rt_vls_strcat( vp, illump->s_path[i]->d_namep);
		}
		rt_vls_strcat( vp, ": " );

		/* print the evaluated (path) solid parameters */
		if( state == ST_O_EDIT && illump->s_Eflag == 0 ) {
			mat_t	new_mat;
			/* NOT an evaluated region */
			/* object edit option selected */
			mat_mul(new_mat, modelchanges, es_mat);

			vls_solid( vp, &es_rec.s, new_mat );
		}

		if( state == ST_O_EDIT && illump->s_Eflag ) {
			point_t	work;
			/* region has been evaluated */
			/* XXX should have an es_keypoint for this */
			MAT4X3PNT(work, modelchanges, es_rec.s.s_values);
			(void)sprintf( &linebuf[0],
				"CENTER : %.4f %.4f %.4f\n",
				work[0]*base2local, work[1]*base2local, work[2]*base2local );
			rt_vls_strcat( vp, &linebuf[0] );
		}

	}
}

/*
 *			S C R E E N _ V L S
 *
 *  Output a vls string to the display manager,
 *  as a text overlay on the graphics area (ugh).
 *
 * Set up for character output.  For the best generality, we
 * don't assume that the display can process a CRLF sequence,
 * so each line is written with a separate call to dmp->dmr_puts().
 */
void
screen_vls( xbase, ybase, vp )
int	xbase;
int	ybase;
register struct rt_vls	*vp;
{
	register char	*start;
	register char	*end;
	register int	y;

	RT_VLS_CHECK( vp );
	y = ybase;

	start = rt_vls_addr( vp );
	while( *start != '\0' )  {
		if( (end = strchr( start, '\n' )) == NULL )  return;

		*end = '\0';
		dmp->dmr_puts( start, xbase, y, 0, DM_YELLOW );
		start = end+1;
		y += TEXT0_DY;
	}
}

/*
 *			D O T I T L E S
 *
 * Produce titles, etc, on the screen.
 * NOTE that this routine depends on being called AFTER dozoom();
 */
void
dotitles()
{
	register int i;
	register char	*cp;		/* char pointer - for sprintf() */
	register int y;			/* for menu computations */
	static vect_t work;		/* work vector */
	static vect_t temp;
	mat_t new_mat;
	register int yloc, xloc;
	register float y_val;
	auto fastf_t	az, el;
	auto char linebuf[512];
	struct rt_vls	vls;

	RT_VLS_INIT( &vls );

	/* Enclose window in decorative box.  Mostly for alignment. */
	dmp->dmr_2d_line( XMIN, YMIN, XMAX, YMIN, 0 );
	dmp->dmr_2d_line( XMAX, YMIN, XMAX, YMAX, 0 );
	dmp->dmr_2d_line( XMAX, YMAX, XMIN, YMAX, 0 );
	dmp->dmr_2d_line( XMIN, YMAX, XMIN, YMIN, 0 );

	/* Line across the bottom, above two bottom status lines */
	dmp->dmr_2d_line( XMIN, TITLE_YBASE-TEXT1_DY, XMAX, TITLE_YBASE-TEXT1_DY, 0 );

	/* Display scroll bars */
	y = scroll_display( SCROLLY ); 
	y = MENUY;

	/* Display state and local unit in upper right corner, boxed */
	dmp->dmr_puts(state_str[state], MENUX, MENUY - MENU_DY, 1, DM_WHITE );
#define YPOS	(MENUY - MENU_DY - 75 )
	dmp->dmr_2d_line(MENUXLIM, YPOS, MENUXLIM, YMAX, 0);	/* vert. */
#undef YPOS

	/*
	 * Print information about object illuminated
	 */
	if( illump != SOLID_NULL &&
	    (state==ST_O_PATH || state==ST_O_PICK || state==ST_S_PICK) )  {
		for( i=0; i <= illump->s_last; i++ )  {
			if( i == ipathpos  &&  state == ST_O_PATH )  {
				dmp->dmr_puts( "[MATRIX]", MENUX, y, 0, DM_WHITE );
				y += MENU_DY;
			}
			dmp->dmr_puts( illump->s_path[i]->d_namep, MENUX, y, 0, DM_YELLOW );
			y += MENU_DY;
		}
	}

	/*
	 * The top of the menu (if any) begins at the Y value specified.
	 */
	mmenu_display( y );

	/* print parameter locations on screen */
	if( state == ST_O_EDIT && illump->s_Eflag ) {
		/* region is a processed region */
		/* XXX should have an es_keypoint for this ??? */
		MAT4X3PNT(temp, model2objview, es_rec.s.s_values);
		xloc = (int)(temp[X]*2048);
		yloc = (int)(temp[Y]*2048);
		dmp->dmr_2d_line(xloc-TEXT0_DY, yloc+TEXT0_DY, xloc+TEXT0_DY, yloc-TEXT0_DY, 0);
		dmp->dmr_2d_line(xloc-TEXT0_DY, yloc-TEXT0_DY, xloc+TEXT0_DY, yloc+TEXT0_DY, 0);
		dmp->dmr_2d_line(xloc+TEXT0_DY, yloc+TEXT0_DY, xloc-TEXT0_DY, yloc+TEXT0_DY, 0);
		dmp->dmr_2d_line(xloc+TEXT0_DY, yloc-TEXT0_DY, xloc-TEXT0_DY, yloc-TEXT0_DY, 0);
		dmp->dmr_2d_line(xloc+TEXT0_DY, yloc+TEXT0_DY, xloc+TEXT0_DY, yloc-TEXT0_DY, 0);
		dmp->dmr_2d_line(xloc-TEXT0_DY, yloc+TEXT0_DY, xloc-TEXT0_DY, yloc-TEXT0_DY, 0);
	}

	/* Label the vertices of the edited solid */
	if(es_edflag >= 0 || (state == ST_O_EDIT && illump->s_Eflag == 0))  {
		mat_t			xform;
		struct rt_point_labels	pl[8+1];

		mat_mul( xform, model2objview, es_mat );

		label_edited_solid( pl, 8+1, xform, &es_int );

		for( i=0; i<8+1; i++ )  {
			if( pl[i].str[0] == '\0' )  break;
			dmp->dmr_puts( pl[i].str,
				((int)(pl[i].pt[X]*2048))+15,
				((int)(pl[i].pt[Y]*2048))+15,
				0, DM_WHITE );
		}
	}

	/*
	 * Prepare the numerical display of the currently edited solid/object.
	 */
	create_text_overlay( &vls );
	screen_vls( SOLID_XBASE, SOLID_YBASE, &vls );
	rt_vls_free( &vls );

	/*
	 * General status information on the next to last line
	 */
	(void)sprintf( &linebuf[0],
		" cent=(%.3f, %.3f, %.3f), sz=%.3f %s, ",
		-toViewcenter[MDX]*base2local,
		-toViewcenter[MDY]*base2local,
		-toViewcenter[MDZ]*base2local,
		VIEWSIZE*base2local,
		rt_units_string(dbip->dbi_local2base) );

	cp = &linebuf[0];
#define FINDNULL(p)	while(*p++); p--;	/* leaves p at NULL */
	FINDNULL(cp);
	/* az/el 0,0 is when screen +Z is model +X */
	VSET( work, 0, 0, 1 );
	MAT3X3VEC( temp, view2model, work );
	ae_vec( &az, &el, temp );
	(void)sprintf( cp, "az=%3.2f el=%2.2f ang=(%.2f, %.2f, %.2f)",
		az, el,
		dm_values.dv_xjoy * 6,
		dm_values.dv_yjoy * 6,
		dm_values.dv_zjoy * 6 );
	dmp->dmr_puts( &linebuf[0], TITLE_XBASE, TITLE_YBASE, 1, DM_WHITE );

	/*
	 * Second status line
	 */

	/* Priorities for what to display:
	 *	1.  adc info
	 *	2.  illuminated path
	 *	3.  title
	 *
	 * This way the adc info will be displayed during editing
	 */

	if( adcflag ) {
		/* Angle/Distance cursor */
		point_t	pt1, pt2, pt3;
		point_t	center_model;

		VSET(pt1, 
		    (curs_x / 2047.0) *Viewscale,
		    (curs_y / 2047.0) *Viewscale, 0.0);
		VSET(center_model, 
		    -toViewcenter[MDX], -toViewcenter[MDY],
		    -toViewcenter[MDZ]);
		MAT4X3VEC(pt2, Viewrot, center_model);
		VADD2(pt3, pt1, pt2);
		(void)sprintf( &linebuf[0],
" curs:  a1=%.1f,  a2=%.1f,  dst=%.3f,  cent=(%.3f, %.3f),  delta=(%.3f, %.3f)",
			angle1 * radtodeg, angle2 * radtodeg,
			(c_tdist / 2047.0) *Viewscale*base2local,
			pt3[X]*base2local, pt3[Y]*base2local,
			(curs_x / 2047.0) *Viewscale*base2local,
			(curs_y / 2047.0) *Viewscale*base2local );
		dmp->dmr_puts( &linebuf[0], TITLE_XBASE, TITLE_YBASE + TEXT1_DY, 1, DM_YELLOW );
	}
	else if( illump != SOLID_NULL )  {
		/* Illuminated path */
		(void)sprintf( linebuf, " Path: ");
		for( i=0; i <= illump->s_last; i++ )  {
			if( i == ipathpos  &&
			    (state == ST_O_PATH || state == ST_O_EDIT) )
				(void)strcat( &linebuf[0], "/__MATRIX__" );
			cp = &linebuf[0];
			FINDNULL(cp);
			(void)sprintf(cp, "/%s", illump->s_path[i]->d_namep );
		}
		dmp->dmr_puts( &linebuf[0], TITLE_XBASE, TITLE_YBASE + TEXT1_DY, 1, DM_YELLOW );
	}
}

/*
 *			L A B E L _ E D I T E D _ S O L I D
 *
 *  Put labels on the vertices of the currently edited solid.
 *  XXX This really should use import/export interface!!!  Or be part of it.
 */
label_edited_solid( pl, max_pl, xform, ip )
struct rt_point_labels	pl[];
int			max_pl;
CONST mat_t		xform;
struct rt_db_internal	*ip;
{
	register int	i;
	union record	temp_rec;	/* copy of es_rec record */
	point_t		work;
	point_t		pos_view;
	int		npl = 0;

	RT_CK_DB_INTERNAL( ip );

	switch( ip->idb_type )  {

#define	POINT_LABEL( _pt, _char )	{ \
	VMOVE( pl[npl].pt, _pt ); \
	pl[npl].str[0] = _char; \
	pl[npl++].str[1] = '\0'; }

	case ID_ARB8:
		MAT4X3PNT( pos_view, xform, es_rec.s.s_values );
		POINT_LABEL( pos_view, '1' );
		temp_rec.s = es_rec.s;
		if(es_type == ARB4) {
			VMOVE(&temp_rec.s.s_values[9], &temp_rec.s.s_values[12]);
		}
		if(es_type == ARB6) {
			VMOVE(&temp_rec.s.s_values[15], &temp_rec.s.s_values[18]);
		}
		for(i=1; i<es_type; i++) {
			VADD2( work, es_rec.s.s_values, &temp_rec.s.s_values[i*3] );
			MAT4X3PNT(pos_view, xform, work);
			POINT_LABEL( pos_view, i + '1' );
		}
		break;
	case ID_TGC:
		MAT4X3PNT( pos_view, xform, &es_rec.s.s_tgc_V );
		POINT_LABEL( pos_view, 'V' );

		VADD2( work, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_A );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'A' );

		VADD2( work, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_B );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'B' );

		VADD3( work, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_H, &es_rec.s.s_tgc_C );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'C' );

		VADD3( work, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_H, &es_rec.s.s_tgc_D );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'D' );
		break;

	case ID_ELL:
		MAT4X3PNT( pos_view, xform, &es_rec.s.s_ell_V );
		POINT_LABEL( pos_view, 'V' );

		VADD2( work, &es_rec.s.s_ell_V, &es_rec.s.s_ell_A );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'A' );

		VADD2( work, &es_rec.s.s_ell_V, &es_rec.s.s_ell_B );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'B' );

		VADD2( work, &es_rec.s.s_ell_V, &es_rec.s.s_ell_C );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'C' );
		break;

	case ID_TOR:
		MAT4X3PNT( pos_view, xform, &es_rec.s.s_tor_V );
		POINT_LABEL( pos_view, 'V' );

		VADD2( work, &es_rec.s.s_tor_V, &es_rec.s.s_tor_C );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'I' );

		VADD2( work, &es_rec.s.s_tor_V, &es_rec.s.s_tor_E );
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'O' );

		VADD3( work, &es_rec.s.s_tor_V, &es_rec.s.s_tor_A, &es_rec.s.s_tor_H);
		MAT4X3PNT(pos_view, xform, work);
		POINT_LABEL( pos_view, 'H' );
		break;

	case ID_ARS:
		MAT4X3PNT(pos_view, xform, es_rec.s.s_values);
		POINT_LABEL( pos_view, 'V' );
		break;
	}

	pl[npl].str[0] = '\0';	/* Mark ending */
}
