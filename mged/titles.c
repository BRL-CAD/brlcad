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

char	*local_unit[] = {
	"NONE",
	"MILLIMETERS",
	"CENTIMETERS",
	"METERS",
	"INCHES",
	"FEET",
	"UNKNOWN",
};


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
	static vect_t pos_view;		/* view position */
	union record temp_rec;		/* copy of es_rec record */
	mat_t new_mat;
	register int yloc, xloc;
	register float y_val;
	auto fastf_t	az, el;
	auto char linebuf[512];

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

	if(es_edflag >= 0 || (state == ST_O_EDIT && illump->s_Eflag == 0))  switch( es_gentype )  {

	case GENARB8:
		MAT4X3PNT( work, es_mat, es_rec.s.s_values );
		MAT4X3PNT( pos_view, model2objview, work );
		dmp->dmr_puts( "1", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );
		temp_rec.s = es_rec.s;
		if(es_type == ARB4) {
			VMOVE(&temp_rec.s.s_values[9], &temp_rec.s.s_values[12]);
		}
		if(es_type == ARB6) {
			VMOVE(&temp_rec.s.s_values[15], &temp_rec.s.s_values[18]);
		}
		for(i=1; i<es_type; i++) {
			static char kvt[4] = "X\0";/* Cvts chars to strings */
			VADD2( work, es_rec.s.s_values, &temp_rec.s.s_values[i*3] );
			MAT4X3PNT(temp, es_mat, work);
			MAT4X3PNT( pos_view, model2objview, temp );
			kvt[0] = i + '1';
			dmp->dmr_puts( kvt, ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );
		}
		break;
	case GENTGC:
		VADD2( work, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_A );
		MAT4X3PNT(temp, es_mat, work);
		MAT4X3PNT( pos_view, model2objview, temp );
		dmp->dmr_puts( "A", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );

		VADD2( work, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_B );
		MAT4X3PNT(temp, es_mat, work);
		MAT4X3PNT( pos_view, model2objview, temp );
		dmp->dmr_puts( "B", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );

		VADD2( temp, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_H );
		VADD2( work, temp, &es_rec.s.s_tgc_C );
		MAT4X3PNT(temp, es_mat, work);
		MAT4X3PNT( pos_view, model2objview, temp );
		dmp->dmr_puts( "C", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );

		VADD2( temp, &es_rec.s.s_tgc_V, &es_rec.s.s_tgc_H );
		VADD2( work, temp, &es_rec.s.s_tgc_D );
		MAT4X3PNT(temp, es_mat, work);
		MAT4X3PNT( pos_view, model2objview, temp );
		dmp->dmr_puts( "D", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );
		break;

	case GENELL:
		VADD2( work, &es_rec.s.s_ell_V, &es_rec.s.s_ell_A );
		MAT4X3PNT(temp, es_mat, work);
		MAT4X3PNT( pos_view, model2objview, temp );
		dmp->dmr_puts( "A", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );
		VADD2( work, &es_rec.s.s_ell_V, &es_rec.s.s_ell_B );
		MAT4X3PNT(temp, es_mat, work);
		MAT4X3PNT( pos_view, model2objview, temp );
		dmp->dmr_puts( "B", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );
		VADD2( work, &es_rec.s.s_ell_V, &es_rec.s.s_ell_C );
		MAT4X3PNT(temp, es_mat, work);
		MAT4X3PNT( pos_view, model2objview, temp );
		dmp->dmr_puts( "C", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );
		break;
	case TOR:
		VADD2( work, &es_rec.s.s_tor_V, &es_rec.s.s_tor_C );
		MAT4X3PNT(temp, es_mat, work);
		MAT4X3PNT( pos_view, model2objview, temp );
		dmp->dmr_puts( "I", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );
		VADD2( work, &es_rec.s.s_tor_V, &es_rec.s.s_tor_E );
		MAT4X3PNT(temp, es_mat, work);
		MAT4X3PNT( pos_view, model2objview, temp );
		dmp->dmr_puts( "O", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );
		VADD3( work, &es_rec.s.s_tor_V, &es_rec.s.s_tor_A, &es_rec.s.s_tor_H);
		MAT4X3PNT(temp, es_mat, work);
		MAT4X3PNT( pos_view, model2objview, temp );
		dmp->dmr_puts( "H", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );
		break;
	case ARS:
	case ARSCONT:
		MAT4X3PNT(temp, es_mat, es_rec.s.s_values);
		MAT4X3PNT( pos_view, model2objview, temp );
		dmp->dmr_puts( "V", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );
		break;
	}

	/*
	 * Set up for character output.  For the best generality, we
	 * don't assume that the display can process a CRLF sequence,
	 * so each line is written with a separate call to dmp->dmr_puts().
	 */

#define FINDNULL(p)	while(*p++); p--;	/* leaves p at NULL */
	/* print solid info at top of screen */
	if( es_edflag >= 0 ) {
		(void)sprintf(&linebuf[0], "** SOLID -- %s:", es_name);
		dmp->dmr_puts(	&linebuf[0],
				SOLID_XBASE,
				SOLID_YBASE,
				1,
				DM_YELLOW);
		for( i=0; i<es_nlines; i++ )  {
			dmp->dmr_puts( &es_display[i*ES_LINELEN],
				SOLID_XBASE,
				SOLID_YBASE+TEXT1_DY+(TEXT0_DY*i),
				0,
				DM_YELLOW );
		}

		if(illump->s_last) {
			(void)sprintf(&linebuf[0], "** PATH --  ");
			for(i=0; i <= illump->s_last; i++) {
				cp = &linebuf[0];
				FINDNULL( cp );
				(void)sprintf(cp,"/%s",illump->s_path[i]->d_namep);
			}
			cp = &linebuf[0];
			FINDNULL( cp );
			(void)sprintf(cp,":");
			
			/* This breaks on the 4D compilers if done
			 * as part of the procedure call 
			 * also if its a int so   (sigh....)
			 */
			y_val = (float) ((es_nlines + 2) *(TEXT0_DY))
	                               + SOLID_YBASE + TEXT1_DY;

			dmp->dmr_puts(	&linebuf[0],
					SOLID_XBASE,
					y_val,
					1,
					DM_RED);
			/* print the evaluated (path) solid parameters */
			temp_rec.s = es_rec.s;		/* struct copy */
			MAT4X3PNT( &temp_rec.s.s_values[0], es_mat,
					&es_rec.s.s_values[0] );
			for(i=1; i<8; i++) {
				MAT4X3VEC( &temp_rec.s.s_values[i*3], es_mat,
						&es_rec.s.s_values[i*3] );
			}

			y_val = (float) SOLID_YBASE+(2*TEXT1_DY)+(TEXT0_DY*(es_nlines+2));
			pr_solid( &temp_rec.s );

			for(i=0; i<es_nlines; i++) {
				yloc = (int) y_val + (TEXT0_DY*i);
				dmp->dmr_puts(	&es_display[i*ES_LINELEN],
						SOLID_XBASE,
						yloc,
						0,
						DM_RED );
			}
			pr_solid( &es_rec.s );

		}
	}

	/* display path info for object editing also */
	if( state == ST_O_EDIT ) {
		(void)sprintf(&linebuf[0], "** PATH --  ");
		for(i=0; i <= illump->s_last; i++) {
			cp = &linebuf[0];
			FINDNULL( cp );
			(void)sprintf(cp,"/%s",illump->s_path[i]->d_namep);
		}
		cp = &linebuf[0];
		FINDNULL( cp );
		(void)sprintf(cp,":");

		dmp->dmr_puts(	&linebuf[0],
				SOLID_XBASE,
				SOLID_YBASE,
				1,
				DM_RED);

		/* print the evaluated (path) solid parameters */
		if( state == ST_O_EDIT && illump->s_Eflag == 0 ) {
			/* NOT an evaluated region */
			/* object edit option selected */
			temp_rec.s = es_rec.s;		/* struct copy */
			mat_mul(new_mat, modelchanges, es_mat);
			MAT4X3PNT( temp_rec.s.s_values, new_mat, es_rec.s.s_values );
			for(i=1; i<8; i++) {
				MAT4X3VEC( &temp_rec.s.s_values[i*3], new_mat, &es_rec.s.s_values[i*3] );
			}

			pr_solid( &temp_rec.s );

			for(i=0; i<es_nlines; i++) {
				dmp->dmr_puts(	&es_display[i*ES_LINELEN],
						SOLID_XBASE,
						SOLID_YBASE+TEXT1_DY+(TEXT0_DY*i),
						0,
						DM_RED );
			}
			pr_solid( &es_rec.s );
		}

		if( state == ST_O_EDIT && illump->s_Eflag ) {
			/* region has been evaluated */
			/* XXX should have an es_keypoint for this */
			MAT4X3PNT(work, modelchanges, es_rec.s.s_values);
			(void)sprintf( &linebuf[0],
					"CENTER : %.4f %.4f %.4f",
					work[0]*base2local, work[1]*base2local, work[2]*base2local );
			dmp->dmr_puts( &linebuf[0],
					SOLID_XBASE,
					SOLID_YBASE+TEXT1_DY+TEXT0_DY,
					1,
					DM_RED );
		}

	}

	/*
	 * General status information on the next to last line
	 */
	(void)sprintf( &linebuf[0],
		" cent=(%.3f, %.3f, %.3f), sz=%.3f %s, ",
		-toViewcenter[MDX]*base2local,
		-toViewcenter[MDY]*base2local,
		-toViewcenter[MDZ]*base2local,
		VIEWSIZE*base2local,
		local_unit[localunit] );

	cp = &linebuf[0];
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
" curs:  a1=%.1f,  a2=%.1f,  dst=%.3f,  cent=(%.3f, %.3f)",
			angle1 * radtodeg, angle2 * radtodeg,
			(c_tdist / 2047.0) *Viewscale*base2local,
			pt3[X]*base2local, pt3[Y]*base2local);
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
	} else {
		/* Title of model */
		(void)sprintf(&linebuf[0], " %s", cur_title);
		dmp->dmr_puts( &linebuf[0], TITLE_XBASE, TITLE_YBASE + TEXT1_DY, 1, DM_YELLOW );
	}
}
