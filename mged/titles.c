/*
 *			T I T L E S . C
 *
 * Functions -
 *	dotitles	Output GED "faceplate" & titles.
 */
#include "ged_types.h"
#include "ged.h"
#include "solid.h"
#include "dir.h"
#include "db.h"
#include "sedit.h"
#include <math.h>
#include "dm.h"
#include "vmath.h"

extern int	printf(), sprintf();

int	state;
char	*state_str[] = {
	"-ZOT-",
	"VIEW",
	"SOLID PICK",
	"SOLID EDIT",
	"OBJECT PICK",
	"OBJECT PATH",
	"OBJECT EDIT",
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
	register int yloc;
	auto char linebuf[512];

	/* Enclose window in decorative box.  Mostly for alignment. */
	dmp->dmr_2d_line( -2048, -2048,  2047, -2048, 0 );
	dmp->dmr_2d_line(  2047, -2048,  2047,  2047, 0 );
	dmp->dmr_2d_line(  2047,  2047, -2048,  2047, 0 );
	dmp->dmr_2d_line( -2048,  2047, -2048, -2048, 0 );
	if( illump != SOLID_NULL )  {
		dmp->dmr_2d_line(  XLIM,  2047,  XLIM,  TITLE_YBASE-TEXT1_DY, 0 );
		dmp->dmr_2d_line( -2047, TITLE_YBASE-TEXT1_DY, XLIM, TITLE_YBASE-TEXT1_DY, 0 );
	}

	/* Display current state in upper right corner */
	dmp->dmr_puts( state_str[state], MENUX, MENUY - MENU_DY, 1, DM_YELLOW );

	/*
	 * Print information about object illuminated
	 */
	y = MENUY;

	if( illump != SOLID_NULL )  {
		for( i=0; i <= illump->s_last; i++ )  {
			dmp->dmr_puts( illump->s_path[i]->d_namep, MENUX, y, 0, DM_YELLOW );
			if( state == ST_O_PATH && i == ipathpos )  {
				dmp->dmr_puts( illump->s_path[i]->d_namep, MENUX, y, 0, DM_WHITE );
				dmp->dmr_puts( illump->s_path[i]->d_namep, MENUX, y, 0, DM_WHITE );
			}
			y += MENU_DY;
		}
	}
	/*
	 * The "y" value is passed so that the menu can be presented
	 * even when in illuminate path mode.
	 * This is probably unlikely, but is there if needed.
	 */
	menu_display( y );

	/* print parameter locations on screen */
	if(es_edflag >= 0)  switch( es_gentype )  {

	case GENARB8:
		MAT4X3PNT( work, es_mat, es_rec.s.s_values );
		MAT4X3PNT( pos_view, model2objview, work );
		dmp->dmr_puts( "1", ((int)(pos_view[X]*2048))+15, ((int)(pos_view[Y]*2048))+15, 0, DM_WHITE );
		for(i=1; i<8; i++) {
			static char kvt[4] = "X\0";/* Cvts chars to strings */
			VADD2( work, es_rec.s.s_values, &es_rec.s.s_values[i*3] );
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
	}

	/* prefix item selected with "==>" to let user know it is selected */
	if( es_edflag >= 0 && menuflag ) {
			dmp->dmr_puts("==>", MENUX-114, menuyy, 0, DM_WHITE);
			dmp->dmr_puts("==>", MENUX-114, menuyy, 0, DM_WHITE);
			dmp->dmr_puts("==>", MENUX-114, menuyy, 0, DM_WHITE);
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
				TITLE_XBASE+15,
				SOLID_YBASE,
				1,
				DM_YELLOW);
		for( i=0; i<es_nlines; i++ )  {
			dmp->dmr_puts( &es_display[i*ES_LINELEN],
				TITLE_XBASE,
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

			dmp->dmr_puts(	&linebuf[0],
					TITLE_XBASE+15,
					SOLID_YBASE+TEXT1_DY+(TEXT0_DY*(es_nlines+2)),
					1,
					DM_RED);
			/* print the evaluated (path) solid parameters */
			temp_rec = es_rec;
			MAT4X3PNT( &temp_rec.s.s_values[0], es_mat,
					&es_rec.s.s_values[0] );
			for(i=1; i<8; i++) {
				MAT4X3VEC( &temp_rec.s.s_values[i*3], es_mat,
						&es_rec.s.s_values[i*3] );
			}

			yloc = SOLID_YBASE+(2*TEXT1_DY)+(TEXT0_DY*(es_nlines+2));
			pr_solid( &temp_rec.s );

			for(i=0; i<es_nlines; i++) {
				dmp->dmr_puts(	&es_display[i*ES_LINELEN],
						TITLE_XBASE,
						yloc+(TEXT0_DY*i),
						0,
						DM_RED );
			}
			pr_solid( &es_rec.s );

		}
	}

	/*
	 * General status information on the last line
	 */
	(void)sprintf( &linebuf[0],
		"\r view:  cent=(%.3f, %.3f, %.3f), size=%.3f, ",
		-toViewcenter[MDX], -toViewcenter[MDY], -toViewcenter[MDZ], VIEWSIZE );

	cp = &linebuf[0];
	FINDNULL(cp);
	(void)sprintf( cp, "angles=(%.2f, %.2f, %.2f)",
		dm_values.dv_xjoy * 6,
		dm_values.dv_yjoy * 6,
		dm_values.dv_zjoy * 6 );
	dmp->dmr_puts( &linebuf[0], TITLE_XBASE, TITLE_YBASE, 1, DM_WHITE );

	/*
	 * Angle/Distance cursor above status line.
	 */
	if (adcflag)  {
		(void)sprintf( &linebuf[0],
"\r cursor:  angle1=%.1f,  angle2=%.1f,  dist=%.3f,  cent=(%.3f, %.3f)",
				angle1 * radtodeg, angle2 * radtodeg,
				(c_tdist / 2047.0) / VIEWFACTOR,
				(curs_x / 2047.0) / VIEWFACTOR,
				(curs_y / 2047.0) / VIEWFACTOR
			     );
		dmp->dmr_puts( &linebuf[0], TITLE_XBASE, TITLE_YBASE - TEXT1_DY, 1, DM_CYAN );
	}
}
