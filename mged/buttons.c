/*
 *			B U T T O N S . C
 *
 * Functions -
 *	buttons		Process button-box functions
 */

#include	<math.h>
#include "ged_types.h"
#include "ged.h"
#include "dir.h"
#include "solid.h"
#include "menu.h"
#include "dm.h"
#include "3d.h"
#include "sedit.h"

extern void	perror();
extern int	printf(), read();
extern long	lseek();

int	adcflag;	/* angle/distance cursor in use */

/* This flag indicates that SOLID editing is in effect.
 * edobj may not be set at the same time.
 * It is set to the 0 if off, or the value of the button function
 * that is currently in effect (eg, BE_S_SCALE).
 */
static	edsol;

/* This flag indicates that OBJECT editing is in effect.
 * edsol may not be set at the same time.
 * Value is 0 if off, or the value of the button function currently
 * in effect (eg, BE_O_XY).
 */
static	edobj;		/* object editing */
int	movedir;	/* RARROW | UARROW | SARROW | ROTARROW */

static	sliceflag;	/* 0 = depth cue mode, !0 = "slice" mode */

/*
 *			B U T T O N
 */
void
button( bnum )
register long bnum;
{
	static mat_t sav_viewrot, sav_toviewcenter;
	static float sav_viewscale;
	static int	vsaved = 0;	/* set iff view saved */
	register struct solid *sp;

	/*
	 * Consistency checks
	 */
	if( edsol && edobj )
		(void)printf("WARNING: State error: edsol=%x, edobj=%x\n", edsol, edobj );

	/*
	 * Process the button function requested.
	 */
	switch( bnum )  {

	default:
		(void)printf("button(%d):  Not a defined operation\n", bnum);
		break;

	case BV_TOP:
		/* Top view */
		setview( 0, 0, 0 );
		break;

	case BV_BOTTOM:
		/* Bottom view */
		setview( 180, 0, 0 );
		break;

	case BV_RIGHT:
		/* Right view */
		setview( 270, 0, 0 );
		break;

	case BV_LEFT:
		/* Left view */
		setview( 270, 0, 180 );
		break;

	case BV_FRONT:
		/* Front view */
		setview( 270, 0, 270 );
		break;

	case BV_REAR:
		/* Rear view */
		setview( 270, 0, 90 );
		break;

	case BV_VRESTORE:
		/* restore to saved view */
		if ( vsaved )  {
			Viewscale = sav_viewscale;
			mat_copy( Viewrot, sav_viewrot );
			mat_copy( toViewcenter, sav_toviewcenter );
			new_mats();
			dmaflag++;
		}
		break;

	case BV_VSAVE:
		/* save current view */
		sav_viewscale = Viewscale;
		mat_copy( sav_viewrot, Viewrot );
		mat_copy( sav_toviewcenter, toViewcenter );
		vsaved = 1;
		dmp->dmr_light( LIGHT_ON, BV_VRESTORE );
		break;
			
	case BV_ADCURSOR:
		if (adcflag)  {
			/* Was on, turn off */
			adcflag = 0;
			dmp->dmr_light( LIGHT_OFF, BV_ADCURSOR );
		}  else  {
			/* Was off, turn on */
			adcflag = 1;
			dmp->dmr_light( LIGHT_ON, BV_ADCURSOR );
		}
		dmaflag = 1;
		break;

	case BV_RESET:
		/* Reset to initial viewing conditions */
		mat_idn( toViewcenter );
		Viewscale = maxview;
		setview( 270, 0, 0 );
		break;

	case BV_90_90:
		/* azm 90   elev 90 */
		setview( 360, 0, 180 );
		break;

	case BV_35_25:
		/* Use Azmuth=35, Elevation=25 in Keith's backwards space */
		setview( 295, 0, 235 );
		break;

	case BE_O_ILLUMINATE:
		if( state != ST_VIEW )  {
			state_err( "Object Illuminate" );
			break;
		}
		state = ST_O_PICK;
		dmp->dmr_light( LIGHT_ON, BE_O_ILLUMINATE );
		goto ill_common;

	case BE_S_ILLUMINATE:
		if( state != ST_VIEW )  {
			state_err( "Solid Illuminate" );
			break;
		}
		state = ST_S_PICK;
		dmp->dmr_light( LIGHT_ON, BE_S_ILLUMINATE );
ill_common:
		dmp->dmr_light( LIGHT_ON, BE_REJECT );
		if( HeadSolid.s_forw == &HeadSolid )  {
			(void)printf("no solids in view\n");
			state = ST_VIEW;
			break;
		}
		illump = HeadSolid.s_forw;/* any valid solid would do */
		edobj = 0;		/* sanity */
		edsol = 0;		/* sanity */
		movedir = 0;		/* No edit modes set */
		mat_idn( modelchanges );	/* No changes yet */
		new_mats();

		dmaflag++;
		break;

	case BE_O_SCALE:
		if( state != ST_O_EDIT )  {
			state_err( "Object Scale" );
			break;
		}
		dmp->dmr_light( LIGHT_OFF, edobj );
		dmp->dmr_light( LIGHT_ON, edobj = BE_O_SCALE );
		movedir = SARROW;
		dmaflag++;
		break;

	case BE_O_X:
		if( state != ST_O_EDIT )  {
			state_err( "Object X Motion" );
			break;
		}
		dmp->dmr_light( LIGHT_OFF, edobj );
		dmp->dmr_light( LIGHT_ON, edobj = BE_O_X );
		movedir = RARROW;
		dmaflag++;
		break;

	case BE_O_Y:
		if( state != ST_O_EDIT )  {
			state_err( "Object Y Motion" );
			break;
		}
		dmp->dmr_light( LIGHT_OFF, edobj );
		dmp->dmr_light( LIGHT_ON, edobj = BE_O_Y );
		movedir = UARROW;
		dmaflag++;
		break;

	case BE_O_XY:
		if( state != ST_O_EDIT )  {
			state_err( "Object XY Motion" );
			break;
		}
		dmp->dmr_light( LIGHT_OFF, edobj );
		dmp->dmr_light( LIGHT_ON, edobj = BE_O_XY );
		movedir = UARROW | RARROW;
		dmaflag++;
		break;

	case BE_O_ROTATE:
		if( state != ST_O_EDIT )  {
			state_err( "Object Rotation" );
			break;
		}
		dmp->dmr_light( LIGHT_OFF, edobj );
		dmp->dmr_light( LIGHT_ON, edobj = BE_O_ROTATE );
		movedir = ROTARROW;
		dmaflag++;
		break;

	case BE_ACCEPT:
		/* Accept edit */
		if( state != ST_O_EDIT && state != ST_S_EDIT )  {
			state_err( "Edit Accept" );
			break;
		}
		dmp->dmr_light( LIGHT_OFF, BE_ACCEPT );
		dmp->dmr_light( LIGHT_OFF, BE_REJECT );

		if( state == ST_S_EDIT )  {
			/* Accept a solid edit */
			/* write editing changes out to disc */
			db_putrec( illump->s_path[illump->s_last], &es_rec, 0 );

			dmp->dmr_light( LIGHT_OFF, edsol );
			edsol = 0;
			MENU_ON(FALSE);
			MENU_INSTALL( (struct menu_item *)NULL );
			dmp->dmr_light( LIGHT_OFF, BE_S_EDIT );
			es_edflag = -1;
			menuflag = 0;

			FOR_ALL_SOLIDS( sp )
				sp->s_iflag = DOWN;
		}  else  {
			/* Accept an object edit */
			dmp->dmr_light( LIGHT_OFF, edobj );
			edobj = 0;
			movedir = 0;	/* No edit modes set */
			if( ipathpos == 0 )  {
				moveHobj( illump->s_path[ipathpos], modelchanges );
			}  else  {
				moveHinstance(
					illump->s_path[ipathpos-1],
					illump->s_path[ipathpos],
					modelchanges
				);
			}

			/*
			 *  Redraw all solids affected by this edit.
			 *  Regenerate a new control list which does not
			 *  include the solids about to be replaced,
			 *  so we can safely fiddle the displaylist.
			 */
			modelchanges[15] = 1000000000;	/* => small ratio */
			dmaflag=1;
			refresh();

			/* Now, recompute new chunks of displaylist */
			FOR_ALL_SOLIDS( sp )  {
				if( sp->s_iflag == DOWN )
					continue;
				/* Rip off es_mat and es_rec for redraw() */
				pathHmat( sp, es_mat );
				db_getrec(sp->s_path[sp->s_last], &es_rec, 0);
				illump = sp;	/* flag to drawHsolid! */
				redraw( sp, &es_rec );
				sp->s_iflag = DOWN;
			}
			mat_idn( modelchanges );
		}

		illump = SOLID_NULL;
		state = ST_VIEW;
		dmaflag = 1;		/* show completion */
		break;

	case BE_REJECT:
		/* Reject edit */
		/* Reject is special -- it is OK in any edit state */
		if( state == ST_VIEW ) {
			state_err( "Edit Reject" );
			break;
		}
		dmp->dmr_light( LIGHT_OFF, BE_ACCEPT );
		dmp->dmr_light( LIGHT_OFF, BE_REJECT );

		if( state == ST_S_EDIT )  {
			/* Reject a solid edit */
			if( edsol )
				dmp->dmr_light( LIGHT_OFF, edsol );
			MENU_ON( FALSE );
			MENU_INSTALL( (struct menu_item *)NULL );

			/* Restore the saved original solid */
			illump = redraw( illump, &es_orig );
		}
		if( state == ST_O_EDIT && edobj )
			dmp->dmr_light( LIGHT_OFF, edobj );

		dmp->dmr_light( LIGHT_OFF, BE_O_ILLUMINATE );
		dmp->dmr_light( LIGHT_OFF, BE_S_ILLUMINATE );
		menuflag = 0;
		movedir = 0;
		edsol = 0;
		edobj = 0;
		es_edflag = -1;
		illump = SOLID_NULL;		/* None selected */
		dmaflag = 1;

		/* Clear illumination flags */
		FOR_ALL_SOLIDS( sp )
			sp->s_iflag = DOWN;
		state = ST_VIEW;
		break;

	case BV_SLICEMODE:
		if( sliceflag )  {
			/* depth cue mode */
			sliceflag = 0;
			inten_scale = 0x7FF0;
			dmp->dmr_light( LIGHT_OFF, BV_SLICEMODE );
		} else {
			/* slice mode */
			sliceflag = 1;
			inten_scale = 0xFFF0;
			dmp->dmr_light( LIGHT_ON, BV_SLICEMODE );
		}
		break;

	case BE_S_EDIT:
		/* solid editing */
		if( state != ST_S_EDIT )  {
			state_err( "Solid Edit (Menu)" );
			break;
		}
		if( edsol )
			dmp->dmr_light( LIGHT_OFF, edsol );
		dmp->dmr_light( LIGHT_ON, edsol = BE_S_EDIT );
		es_edflag = MENU;
		menuflag = 0;		/* No menu item selected yet */
		MENU_ON(TRUE);
		dmaflag++;
		break;

	case BE_S_ROTATE:
		/* rotate solid */
		if( state != ST_S_EDIT )  {
			state_err( "Solid Rotate" );
			break;
		}
		dmp->dmr_light( LIGHT_OFF, edsol );
		dmp->dmr_light( LIGHT_ON, edsol = BE_S_ROTATE );
		menuflag = 0;
		MENU_ON( FALSE );
		es_edflag = SROT;
		dmaflag++;
		break;

	case BE_S_TRANS:
		/* translate solid */
		if( state != ST_S_EDIT )  {
			state_err( "Solid Translate" );
			break;
		}
		dmp->dmr_light( LIGHT_OFF, edsol );
		dmp->dmr_light( LIGHT_ON, edsol = BE_S_TRANS );
		menuflag = 0;
		es_edflag = STRANS;
		movedir = UARROW | RARROW;
		MENU_ON( FALSE );
		dmaflag++;
		break;

	case BE_S_SCALE:
		/* scale solid */
		if( state != ST_S_EDIT )  {
			state_err( "Solid Scale" );
			break;
		}
		dmp->dmr_light( LIGHT_OFF, edsol );
		dmp->dmr_light( LIGHT_ON, edsol = BE_S_SCALE );
		menuflag = 0;
		es_edflag = SSCALE;
		MENU_ON( FALSE );
		dmaflag++;
		break;
	}
}

state_err( str )
char *str;
{
	(void)printf("Unable to do <%s> from %s state.\n",
		str, state_str[state] );
}
