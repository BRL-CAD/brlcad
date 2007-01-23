/*                       B U T T O N S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file buttons.c
 *
 * Functions -
 *	buttons		Process button-box functions
 *	press		button function request
 *	state_err	state error printer
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
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"

#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./sedit.h"


extern int mged_svbase(void);
extern void set_e_axes_pos(int both);
extern int mged_zoom(double val);
extern void set_absolute_tran(void);	/* defined in set.c */
extern void set_scroll_private(void);	/* defined in set.c */
extern void adc_set_scroll(void);		/* defined in adc.c */

/*
 * This flag indicates that Primitive editing is in effect.
 * edobj may not be set at the same time.
 * It is set to the 0 if off, or the value of the button function
 * that is currently in effect (eg, BE_S_SCALE).
 */
static int	edsol;

/* This flag indicates that Matrix editing is in effect.
 * edsol may not be set at the same time.
 * Value is 0 if off, or the value of the button function currently
 * in effect (eg, BE_O_XY).
 */
int	edobj;		/* object editing */
int	movedir;	/* RARROW | UARROW | SARROW | ROTARROW */

/*
 * The "accumulation" solid rotation matrix and scale factor
 */
mat_t	acc_rot_sol;
fastf_t	acc_sc_sol;
fastf_t	acc_sc_obj;     /* global object scale factor --- accumulations */
fastf_t	acc_sc[3];	/* local object scale factors --- accumulations */

struct buttons  {
	int	bu_code;	/* dm_values.dv_button */
	char	*bu_name;	/* keyboard string */
	int	(*bu_func)(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);	/* function to call */
}  button_table[] = {
	{BV_35_25,		"35,25",	bv_35_25},
	{BV_45_45,		"45,45",	bv_45_45},
	{BE_ACCEPT,		"accept",	be_accept},
	{BV_ADCURSOR,		"adc",		bv_adcursor},
	{BV_BOTTOM,		"bottom",	bv_bottom},
	{BV_FRONT,		"front",	bv_front},
	{BV_LEFT,		"left",		bv_left},
	{BE_O_ILLUMINATE,	"oill",		be_o_illuminate},
	{BE_O_ROTATE,		"orot",		be_o_rotate},
	{BE_O_SCALE,		"oscale",	be_o_scale},
	{BE_O_X,		"ox",		be_o_x},
	{BE_O_XY,		"oxy",		be_o_xy},
	{BE_O_XSCALE,		"oxscale",	be_o_xscale},
	{BE_O_Y,		"oy",		be_o_y},
	{BE_O_YSCALE,		"oyscale",	be_o_yscale},
	{BE_O_ZSCALE,		"ozscale",	be_o_zscale},
	{BV_REAR,		"rear",		bv_rear},
	{BE_REJECT,		"reject",	be_reject},
	{BV_RESET,		"reset",	bv_reset},
	{BV_VRESTORE,		"restore",	bv_vrestore},
	{BV_RIGHT,		"right",	bv_right},
	{BV_VSAVE,		"save",		bv_vsave},
	{BE_S_EDIT,		"sedit",	be_s_edit},
	{BE_S_ILLUMINATE,	"sill",		be_s_illuminate},
	{BE_S_ROTATE,		"srot",		be_s_rotate},
	{BE_S_SCALE,		"sscale",	be_s_scale},
	{BE_S_TRANS,		"sxy",		be_s_trans},
	{BV_TOP,		"top",		bv_top},
	{BV_ZOOM_IN,		"zoomin",	bv_zoomin},
	{BV_ZOOM_OUT,		"zoomout",	bv_zoomout},
	{BV_RATE_TOGGLE,	"rate",		bv_rate_toggle},
	{-1,			"-end-",	be_reject}
};

static mat_t sav_viewrot, sav_toviewcenter;
static fastf_t sav_vscale;
static int	vsaved = 0;	/* set iff view saved */

extern void color_soltab(void);
extern void	sl_halt_scroll(void);	/* in scroll.c */
extern void	sl_toggle_scroll(void);

void		btn_head_menu(int i, int menu, int item);
void		btn_item_hit(int arg, int menu, int item);

static struct menu_item first_menu[] = {
	{ "BUTTON MENU", btn_head_menu, 1 },		/* chg to 2nd menu */
	{ "", (void (*)())NULL, 0 }
};
struct menu_item second_menu[] = {
	{ "BUTTON MENU", btn_head_menu, 0 },	/* chg to 1st menu */
	{ "REJECT Edit", btn_item_hit, BE_REJECT },
	{ "ACCEPT Edit", btn_item_hit, BE_ACCEPT },
	{ "35,25", btn_item_hit, BV_35_25 },
	{ "Top", btn_item_hit, BV_TOP },
	{ "Right", btn_item_hit, BV_RIGHT },
	{ "Front", btn_item_hit, BV_FRONT },
	{ "45,45", btn_item_hit, BV_45_45 },
	{ "Restore View", btn_item_hit, BV_VRESTORE },
	{ "Save View", btn_item_hit, BV_VSAVE },
	{ "Ang/Dist Curs", btn_item_hit, BV_ADCURSOR },
	{ "Reset Viewsize", btn_item_hit, BV_RESET },
	{ "Zero Sliders", sl_halt_scroll, 0 },
	{ "Sliders", sl_toggle_scroll, 0 },
	{ "Rate/Abs", btn_item_hit, BV_RATE_TOGGLE },
	{ "Zoom In 2X", btn_item_hit, BV_ZOOM_IN },
	{ "Zoom Out 2X", btn_item_hit, BV_ZOOM_OUT },
	{ "Prim Illum", btn_item_hit, BE_S_ILLUMINATE },
	{ "Matrix Illum", btn_item_hit, BE_O_ILLUMINATE },
	{ "", (void (*)())NULL, 0 }
};
struct menu_item sed_menu[] = {
	{ "*PRIM EDIT*", btn_head_menu, 2 },
	{ "Edit Menu", btn_item_hit, BE_S_EDIT },
	{ "Rotate", btn_item_hit, BE_S_ROTATE },
	{ "Translate", btn_item_hit, BE_S_TRANS },
	{ "Scale", btn_item_hit, BE_S_SCALE },
	{ "", (void (*)())NULL, 0 }
};

struct menu_item oed_menu[] = {
	{ "*MATRIX EDIT*", btn_head_menu, 2 },
	{ "Scale", btn_item_hit, BE_O_SCALE },
	{ "X Move", btn_item_hit, BE_O_X },
	{ "Y Move", btn_item_hit, BE_O_Y },
	{ "XY Move", btn_item_hit, BE_O_XY },
	{ "Rotate", btn_item_hit, BE_O_ROTATE },
	{ "Scale X", btn_item_hit, BE_O_XSCALE },
	{ "Scale Y", btn_item_hit, BE_O_YSCALE },
	{ "Scale Z", btn_item_hit, BE_O_ZSCALE },
	{ "", (void (*)())NULL, 0 }
};

/*
 *			B U T T O N
 */
void
button(register int bnum)
{
	register struct buttons *bp;

	if( edsol && edobj )
	  bu_log("WARNING: State error: edsol=%x, edobj=%x\n", edsol, edobj);

	/* Process the button function requested. */
	for( bp = button_table; bp->bu_code >= 0; bp++ )  {
		if( bnum != bp->bu_code )
			continue;

		bp->bu_func( (ClientData)NULL, interp, 0, NULL );
		return;
	}

	bu_log("button(%d):  Not a defined operation\n", bnum);
}

/*
 *			F _ P R E S S
 *
 * Hook for displays with no buttons
 *
 *  Given a string description of which button to press, simulate
 *  pressing that button on the button box.
 *
 *  These days, just hand off to the appropriate Tcl proc of the same name.
 */
int
f_press(ClientData clientData,
	Tcl_Interp *interp,
	int	argc,
	char	**argv)
{
	register int i;

	if(argc < 2){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help press");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	for( i = 1; i < argc; i++ )  {
		char *str = argv[i];
		register struct buttons *bp;
		struct menu_item	**m;
		int menu, item;
		register struct menu_item	*mptr;

		if( edsol && edobj ){
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "WARNING: State error: edsol=%x, edobj=%x\n", edsol, edobj );
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);
		}

		if(strcmp(str, "help") == 0) {
		  struct bu_vls vls;

		  bu_vls_init(&vls);

		  for( bp = button_table; bp->bu_code >= 0; bp++ )
		    vls_col_item(&vls, bp->bu_name);
		  vls_col_eol(&vls);

		  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		  bu_vls_free(&vls);
		  goto next;
		}

		/* Process the button function requested. */
		for( bp = button_table; bp->bu_code >= 0; bp++ )  {
			if( strcmp( str, bp->bu_name ) != 0 )
				continue;

			(void)bp->bu_func(clientData, interp, 2, argv+1);
			goto next;
		}

		for( menu=0, m=menu_state->ms_menus; m - menu_state->ms_menus < NMENU; m++,menu++ )  {
			if( *m == MENU_NULL )  continue;
			for( item=0, mptr = *m;
			     mptr->menu_string[0] != '\0';
			     mptr++, item++ )  {
			    if ( strcmp( str, mptr->menu_string ) != 0 )
				continue;

			    menu_state->ms_cur_item = item;
			    menu_state->ms_cur_menu = menu;
			    menu_state->ms_flag = 1;
			    /* It's up to the menu_func to set menu_state->ms_flag=0
			     * if no arrow is desired */
			    if( mptr->menu_func != ((void (*)())0) )
				(*(mptr->menu_func))(mptr->menu_arg, menu, item);

			    goto next;
			}
		}

		Tcl_AppendResult(interp, "press(", str,
			 "):  Unknown operation, type 'press help' for help\n", (char *)NULL);
next:		;
	}

	return TCL_OK;
}

/*
 *  			L A B E L _ B U T T O N
 *
 *  For a given GED button number, return the "press" ID string.
 *  Useful for displays with programable button lables, etc.
 */
char *
label_button(int bnum)
{
	register struct buttons *bp;

	/* Process the button function requested. */
	for( bp = button_table; bp->bu_code >= 0; bp++ )  {
		if( bnum != bp->bu_code )
			continue;
		return( bp->bu_name );
	}

	{
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "label_button(%d):  Not a defined operation\n", bnum);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	}

	return("");
}

int
bv_zoomin(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  (void)mged_zoom(2.0);
  return TCL_OK;
}

int
bv_zoomout(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  (void)mged_zoom(0.5);
  return TCL_OK;
}

int
bv_rate_toggle(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  mged_variables->mv_rateknobs = !mged_variables->mv_rateknobs;
  set_scroll_private();
  return TCL_OK;
}

int
bv_top(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	/* Top view */
	setview(0.0, 0.0, 0.0);
	return TCL_OK;
}

int
bv_bottom(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	/* Bottom view */
	setview(180.0, 0.0, 0.0);
	return TCL_OK;
}

int
bv_right(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	/* Right view */
	setview(270.0, 0.0, 0.0);
	return TCL_OK;
}

int
bv_left(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	/* Left view */
	setview(270.0, 0.0, 180.0);
	return TCL_OK;
}

int
bv_front(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	/* Front view */
	setview(270.0, 0.0, 270.0);
	return TCL_OK;
}

int
bv_rear(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	/* Rear view */
	setview(270.0, 0.0, 90.0);
	return TCL_OK;
}

int
bv_vrestore(ClientData	clientData,
	    Tcl_Interp	*interp,
	    int		argc,
	    char	**argv)
{
  /* restore to saved view */
	if (vsaved) {
		view_state->vs_vop->vo_scale = sav_vscale;
		MAT_COPY(view_state->vs_vop->vo_rotation, sav_viewrot);
		MAT_COPY(view_state->vs_vop->vo_center, sav_toviewcenter);
		new_mats();

		(void)mged_svbase();
	}

	return TCL_OK;
}

int
bv_vsave(ClientData	clientData,
	 Tcl_Interp	*interp,
	 int		argc,
	 char		**argv)
{
	/* save current view */
	sav_vscale = view_state->vs_vop->vo_scale;
	MAT_COPY(sav_viewrot, view_state->vs_vop->vo_rotation);
	MAT_COPY(sav_toviewcenter, view_state->vs_vop->vo_center);
	vsaved = 1;
	return TCL_OK;
}

/*
 *			B V _ A D C U R S O R
 *
 *  Toggle state of angle/distance cursor.
 *  "press adc"
 *  This command conflicts with existing "adc" command,
 *  can't be bound as "adc", only as "press adc".
 */
int
bv_adcursor(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  if (adc_state->adc_draw)  {
    /* Was on, turn off */
    adc_state->adc_draw = 0;
  }  else  {
    /* Was off, turn on */
    adc_state->adc_draw = 1;
  }

  adc_set_scroll();
  return TCL_OK;
}

int
bv_reset(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	/* Reset view such that all solids can be seen */
	size_reset();
	setview(0.0, 0.0, 0.0);
	(void)mged_svbase();
	return TCL_OK;
}

int
bv_45_45(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	setview(270.0+45.0, 0.0, 270.0-45.0);
	return TCL_OK;
}

int
bv_35_25(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	/* Use Azmuth=35, Elevation=25 in GIFT's backwards space */
	setview(270.0+25.0, 0.0, 270.0-35.0);
	return TCL_OK;
}

/* returns 0 if error, !0 if success */
static int
ill_common(void) {
	/* Common part of illumination */
	if(BU_LIST_IS_EMPTY(&dgop->dgo_headSolid)) {
	  Tcl_AppendResult(interp, "no solids in view\n", (char *)NULL);
	  return(0);	/* BAD */
	}

	illump = BU_LIST_NEXT(solid, &dgop->dgo_headSolid);/* any valid solid would do */
	illump->s_iflag = UP;
	edobj = 0;		/* sanity */
	edsol = 0;		/* sanity */
	movedir = 0;		/* No edit modes set */
	MAT_IDN( modelchanges );	/* No changes yet */

	return(1);		/* OK */
}

int
be_o_illuminate(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	if( not_state( ST_VIEW, "Matrix Illuminate" ) )
		return TCL_ERROR;

	if( ill_common() )  {
		(void)chg_state( ST_VIEW, ST_O_PICK, "Matrix Illuminate" );
	}
	/* reset accumulation local scale factors */
	acc_sc[0] = acc_sc[1] = acc_sc[2] = 1.0;

	/* reset accumulation global scale factors */
	acc_sc_obj = 1.0;
	return TCL_OK;
}

int
be_s_illuminate(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	if( not_state( ST_VIEW, "Prim Illuminate" ) )
		return TCL_ERROR;

	if( ill_common() )  {
		(void)chg_state( ST_VIEW, ST_S_PICK, "Prim Illuminate" );
	}
	return TCL_OK;
}

int
be_o_scale(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	if( not_state( ST_O_EDIT, "Matrix Scale" ) )
		return TCL_ERROR;

	edobj = BE_O_SCALE;
	movedir = SARROW;
	update_views = 1;
	set_e_axes_pos(1);

	edit_absolute_scale = acc_sc_obj - 1.0;
	if(edit_absolute_scale > 0.0)
	  edit_absolute_scale /= 3.0;
	return TCL_OK;
}

int
be_o_xscale(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	if( not_state( ST_O_EDIT, "Matrix Local X Scale" ) )
		return TCL_ERROR;

	edobj = BE_O_XSCALE;
	movedir = SARROW;
	update_views = 1;
	set_e_axes_pos(1);

	edit_absolute_scale = acc_sc[0] - 1.0;
	if(edit_absolute_scale > 0.0)
	  edit_absolute_scale /= 3.0;
	return TCL_OK;
}

int
be_o_yscale(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	if( not_state( ST_O_EDIT, "Matrix Local Y Scale" ) )
		return TCL_ERROR;

	edobj = BE_O_YSCALE;
	movedir = SARROW;
	update_views = 1;
	set_e_axes_pos(1);

	edit_absolute_scale = acc_sc[1] - 1.0;
	if(edit_absolute_scale > 0.0)
	  edit_absolute_scale /= 3.0;
	return TCL_OK;
}

int
be_o_zscale(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	if( not_state( ST_O_EDIT, "Matrix Local Z Scale" ) )
		return TCL_ERROR;

	edobj = BE_O_ZSCALE;
	movedir = SARROW;
	update_views = 1;
	set_e_axes_pos(1);

	edit_absolute_scale = acc_sc[2] - 1.0;
	if(edit_absolute_scale > 0.0)
	  edit_absolute_scale /= 3.0;
	return TCL_OK;
}

int
be_o_x(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	if( not_state( ST_O_EDIT, "Matrix X Motion" ) )
		return TCL_ERROR;

	edobj = BE_O_X;
	movedir = RARROW;
	update_views = 1;
	set_e_axes_pos(1);
	return TCL_OK;
}

int
be_o_y(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	if( not_state( ST_O_EDIT, "Matrix Y Motion" ) )
		return TCL_ERROR;

	edobj = BE_O_Y;
	movedir = UARROW;
	update_views = 1;
	set_e_axes_pos(1);
	return TCL_OK;
}


int
be_o_xy(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	if( not_state( ST_O_EDIT, "Matrix XY Motion" ) )
		return TCL_ERROR;

	edobj = BE_O_XY;
	movedir = UARROW | RARROW;
	update_views = 1;
	set_e_axes_pos(1);
	return TCL_OK;
}

int
be_o_rotate(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	if( not_state( ST_O_EDIT, "Matrix Rotation" ) )
		return TCL_ERROR;

	edobj = BE_O_ROTATE;
	movedir = ROTARROW;
	update_views = 1;
	set_e_axes_pos(1);
	return TCL_OK;
}

int
be_accept(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	register struct solid *sp;
	register struct dm_list *dmlp;

	if( state == ST_S_EDIT )  {
		/* Accept a solid edit */
		edsol = 0;

		sedit_accept();		/* zeros "edsol" var */

		mmenu_set_all( MENU_L1, MENU_NULL );
		mmenu_set_all( MENU_L2, MENU_NULL );

		FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)
			sp->s_iflag = DOWN;

		illump = SOLID_NULL;
		color_soltab();
		(void)chg_state( ST_S_EDIT, ST_VIEW, "Edit Accept" );
	}  else if( state == ST_O_EDIT )  {
		/* Accept an object edit */
		edobj = 0;
		movedir = 0;	/* No edit modes set */

		oedit_accept();

		mmenu_set_all( MENU_L2, MENU_NULL );

		illump = SOLID_NULL;
		color_soltab();
		(void)chg_state( ST_O_EDIT, ST_VIEW, "Edit Accept" );
	} else {
		if( not_state( ST_S_EDIT, "Edit Accept" ) )
			return TCL_ERROR;
		return TCL_OK;
	}

	FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
	  if(dmlp->dml_mged_variables->mv_transform == 'e')
	    dmlp->dml_mged_variables->mv_transform = 'v';

	{
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_strcpy(&vls, "end_edit_callback");
	  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	}
	return TCL_OK;
}

int
be_reject(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	register struct solid *sp;
	register struct dm_list *dmlp;

	update_views = 1;

	/* Reject edit */

	switch( state )  {
	default:
		state_err( "Edit Reject" );
		return TCL_ERROR;

	case ST_S_EDIT:
		/* Reject a solid edit */
		mmenu_set_all( MENU_L1, MENU_NULL );
		mmenu_set_all( MENU_L2, MENU_NULL );

		sedit_reject();
		break;

	case ST_O_EDIT:
		mmenu_set_all( MENU_L2, MENU_NULL );

		oedit_reject();
		break;
	case ST_O_PICK:
		break;
	case ST_S_PICK:
		break;
	case ST_O_PATH:
		break;
	}

	menu_state->ms_flag = 0;
	movedir = 0;
	edsol = 0;
	edobj = 0;
	es_edflag = -1;
	illump = SOLID_NULL;		/* None selected */

	/* Clear illumination flags */
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)
		sp->s_iflag = DOWN;
	color_soltab();
	(void)chg_state( state, ST_VIEW, "Edit Reject" );

	FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
	  if(dmlp->dml_mged_variables->mv_transform == 'e')
	    dmlp->dml_mged_variables->mv_transform = 'v';

	{
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_strcpy(&vls, "end_edit_callback");
	  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	}
	return TCL_OK;
}

int
be_s_edit(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	/* solid editing */
	if( not_state( ST_S_EDIT, "Prim Edit (Menu)" ) )
		return TCL_ERROR;

	edsol = BE_S_EDIT;
	sedit_menu();		/* Install appropriate menu */
	return TCL_OK;
}

int
be_s_rotate(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	/* rotate solid */
	if( not_state( ST_S_EDIT, "Prim Rotate" ) )
		return TCL_ERROR;

	es_edflag = SROT;
	edsol = BE_S_ROTATE;
	mmenu_set( MENU_L1, MENU_NULL );

        set_e_axes_pos(1);
	return TCL_OK;
}

int
be_s_trans(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	/* translate solid */
	if( not_state( ST_S_EDIT, "Prim Translate" ) )
		return TCL_ERROR;

	edsol = BE_S_TRANS;
	es_edflag = STRANS;
	movedir = UARROW | RARROW;
	mmenu_set( MENU_L1, MENU_NULL );

        set_e_axes_pos(1);
	return TCL_OK;
}

int
be_s_scale(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)  {
	/* scale solid */
	if( not_state( ST_S_EDIT, "Prim Scale" ) )
		return TCL_ERROR;

	edsol = BE_S_SCALE;
	es_edflag = SSCALE;
	mmenu_set( MENU_L1, MENU_NULL );
	acc_sc_sol = 1.0;

        set_e_axes_pos(1);
	return TCL_OK;
}

/*
 *			N O T _ S T A T E
 *
 *  Returns 0 if current state is as desired,
 *  Returns !0 and prints error message if state mismatch.
 */
int
not_state(int desired, char *str)
{
  if( state != desired ) {
    Tcl_AppendResult(interp, "Unable to do <", str, "> from ", state_str[state], " state.\n", (char *)NULL);
    Tcl_AppendResult(interp, "Expecting ", state_str[desired], " state.\n", (char *)NULL);
    return -1;
  }

  return 0;
}

/*
 *  			C H G _ S T A T E
 *
 *  Returns 0 if state change is OK,
 *  Returns !0 and prints error message if error.
 */
int
chg_state(int from, int to, char *str)
{
  register struct dm_list *p;
  struct dm_list *save_dm_list;
  struct bu_vls vls;

  if(state != from){
    bu_log("Unable to do <%s> going from %s to %s state.\n", str, state_str[from], state_str[to]);
    return(1);	/* BAD */
  }

  state = to;

  stateChange(from, to);

  save_dm_list = curr_dm_list;
  FOR_ALL_DISPLAYS(p, &head_dm_list.l){
    curr_dm_list = p;

#if 0
    if(to == ST_VIEW){
      mat_t o_toViewcenter;
      fastf_t o_Viewscale;

      /* save toViewcenter and Viewscale */
      MAT_COPY(o_toViewcenter, view_state->vs_toViewcenter);
      o_Viewscale = view_state->vs_Viewscale;

      /* get new orig_pos */
      size_reset();
      MAT_DELTAS_GET_NEG(view_state->vs_orig_pos, view_state->vs_toViewcenter);

      /* restore old toViewcenter and Viewscale */
      MAT_COPY(view_state->vs_toViewcenter, o_toViewcenter);
      view_state->vs_Viewscale = o_Viewscale;
    }
#endif
    new_mats();

#if 0
    /* recompute absolute_tran */
    set_absolute_tran();
#endif
  }

  curr_dm_list = save_dm_list;

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "%s(state)", MGED_DISPLAY_VAR);
  Tcl_SetVar(interp, bu_vls_addr(&vls), state_str[state], TCL_GLOBAL_ONLY);
  bu_vls_free(&vls);

  return(0);		/* GOOD */
}

void
state_err(char *str)
{
  Tcl_AppendResult(interp, "Unable to do <", str, "> from ", state_str[state],
		   " state.\n", (char *)NULL);
}


/*
 *			B T N _ I T E M _ H I T
 *
 *  Called when a menu item is hit
 */
void
 btn_item_hit(int arg, int menu, int item) {
	button(arg);
	if( menu == MENU_GEN &&
	    ( arg != BE_O_ILLUMINATE && arg != BE_S_ILLUMINATE) )
		menu_state->ms_flag = 0;
}

/*
 *			B T N _ H E A D _ M E N U
 *
 *  Called to handle hits on menu heads.
 *  Also called from main() with arg 0 in init.
 */
void
btn_head_menu(int i, int menu, int item)  {
	switch(i)  {
	case 0:
		mmenu_set( MENU_GEN, first_menu );
		break;
	case 1:
		mmenu_set( MENU_GEN, second_menu );
		break;
	case 2:
		/* nothing happens */
		break;
	default:
	  {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "btn_head_menu(%d): bad arg\n", i);
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	  }

	  break;
	}
}

void
chg_l2menu(int i)  {
	switch( i )  {
	case ST_S_EDIT:
		mmenu_set_all( MENU_L2, sed_menu );
		break;
	case ST_S_NO_EDIT:
		mmenu_set_all( MENU_L2, MENU_NULL );
		break;
	case ST_O_EDIT:
		mmenu_set_all( MENU_L2, oed_menu );
		break;
	default:
	  {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "chg_l2menu(%d): bad arg\n", i);
	    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	    bu_vls_free(&tmp_vls);
	  }

	  break;
	}
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
