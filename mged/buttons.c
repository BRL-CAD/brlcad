/*
 *			B U T T O N S . C
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
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./sedit.h"

extern void stateChange();		/* defined in dm-generic.c */
extern int mged_svbase();
extern void set_e_axes_pos();
extern int mged_zoom();
extern void set_absolute_tran();	/* defined in set.c */
extern void set_scroll_private();	/* defined in set.c */
extern void adc_set_scroll();		/* defined in adc.c */

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

static void bv_zoomin(), bv_zoomout(), bv_rate_toggle();
static void bv_top(), bv_bottom(), bv_right();
static void bv_left(), bv_front(), bv_rear();
static void bv_vrestore(), bv_vsave(), bv_adcursor(), bv_reset();
static void bv_45_45(), bv_35_25();
static void be_o_illuminate(), be_s_illuminate();
static void be_o_scale(), be_o_x(), be_o_y(), be_o_xy(), be_o_rotate();
static void be_accept(), be_reject(), bv_slicemode();
static void be_s_edit(), be_s_rotate(), be_s_trans(), be_s_scale();
static void be_o_xscale(), be_o_yscale(), be_o_zscale();

struct buttons  {
	int	bu_code;	/* dm_values.dv_button */
	char	*bu_name;	/* keyboard string */
	void	(*bu_func)();	/* function to call */
}  button_table[] = {
	BV_35_25,	"35,25",	bv_35_25,
	BV_45_45,	"45,45",	bv_45_45,
	BE_ACCEPT,	"accept",	be_accept,
	BV_ADCURSOR,	"adc",		bv_adcursor,
	BV_BOTTOM,	"bottom",	bv_bottom,
	BV_FRONT,	"front",	bv_front,
	BV_LEFT,	"left",		bv_left,
	BE_O_ILLUMINATE,"oill",		be_o_illuminate,
	BE_O_ROTATE,	"orot",		be_o_rotate,
	BE_O_SCALE,	"oscale",	be_o_scale,
	BE_O_X,		"ox",		be_o_x,
	BE_O_XY,	"oxy",		be_o_xy,
	BE_O_XSCALE,	"oxscale",	be_o_xscale,
	BE_O_Y,		"oy",		be_o_y,
	BE_O_YSCALE,	"oyscale",	be_o_yscale,
	BE_O_ZSCALE,	"ozscale",	be_o_zscale,
	BV_REAR,	"rear",		bv_rear,
	BE_REJECT,	"reject",	be_reject,
	BV_RESET,	"reset",	bv_reset,
	BV_VRESTORE,	"restore",	bv_vrestore,
	BV_RIGHT,	"right",	bv_right,
	BV_VSAVE,	"save",		bv_vsave,
	BE_S_EDIT,	"sedit",	be_s_edit,
	BE_S_ILLUMINATE,"sill",		be_s_illuminate,
	BV_SLICEMODE,	"slice",	bv_slicemode,
	BE_S_ROTATE,	"srot",		be_s_rotate,
	BE_S_SCALE,	"sscale",	be_s_scale,
	BE_S_TRANS,	"sxy",		be_s_trans,
	BV_TOP,		"top",		bv_top,
	BV_ZOOM_IN,	"zoomin",	bv_zoomin,
	BV_ZOOM_OUT,	"zoomout",	bv_zoomout,
	BV_RATE_TOGGLE, "rate",		bv_rate_toggle,
	-1,		"-end-",	be_reject
};

static mat_t sav_viewrot, sav_toviewcenter;
static fastf_t sav_vscale;
static int	vsaved = 0;	/* set iff view saved */

extern void color_soltab();
extern void	sl_halt_scroll();	/* in scroll.c */
extern void	sl_toggle_scroll();

void		btn_head_menu();
void		btn_item_hit();

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
button( bnum )
register int bnum;
{
	register struct buttons *bp;

	if( edsol && edobj )
	  bu_log("WARNING: State error: edsol=%x, edobj=%x\n", edsol, edobj);

	/* Process the button function requested. */
	for( bp = button_table; bp->bu_code >= 0; bp++ )  {
		if( bnum != bp->bu_code )
			continue;

		bp->bu_func();
		return;
	}

	bu_log("button(%d):  Not a defined operation\n", bnum);
}

/*
 *  			P R E S S
 */
void
press( str )
char *str;{
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
	  return;
	}

	/* Process the button function requested. */
	for( bp = button_table; bp->bu_code >= 0; bp++ )  {
		if( strcmp( str, bp->bu_name ) != 0 )
			continue;

		bp->bu_func();
		return;
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

		    return;
		}
	}

	Tcl_AppendResult(interp, "press(", str,
			 "):  Unknown operation, type 'press help' for help\n", (char *)NULL);
}
/*
 *  			L A B E L _ B U T T O N
 *  
 *  For a given GED button number, return the "press" ID string.
 *  Useful for displays with programable button lables, etc.
 */
char *
label_button(bnum)
int bnum;
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

static void
bv_zoomin()
{
  (void)mged_zoom(2.0);
}

static void
bv_zoomout()
{
  (void)mged_zoom(0.5);
}

static void
bv_rate_toggle()
{
  mged_variables->mv_rateknobs = !mged_variables->mv_rateknobs;
  set_scroll_private();
}

static void
bv_top()
{
  /* Top view */
  setview( 0.0, 0.0, 0.0 );
}

static void
bv_bottom()
{
  /* Bottom view */
  setview( 180.0, 0.0, 0.0 );
}

static void
bv_right()
{
  /* Right view */
  setview( 270.0, 0.0, 0.0 );
}

static void
bv_left()
{
  /* Left view */
  setview( 270.0, 0.0, 180.0 );
}

static void
bv_front()
{
  /* Front view */
  setview( 270.0, 0.0, 270.0 );
}

static void
bv_rear()
{
  /* Rear view */
  setview( 270.0, 0.0, 90.0 );
}

static void
bv_vrestore()
{
  /* restore to saved view */
  if ( vsaved )  {
    view_state->vs_Viewscale = sav_vscale;
    bn_mat_copy( view_state->vs_Viewrot, sav_viewrot );
    bn_mat_copy( view_state->vs_toViewcenter, sav_toviewcenter );
    new_mats();

    (void)mged_svbase();
  }
}

static void
bv_vsave()
{
  /* save current view */
  sav_vscale = view_state->vs_Viewscale;
  bn_mat_copy( sav_viewrot, view_state->vs_Viewrot );
  bn_mat_copy( sav_toviewcenter, view_state->vs_toViewcenter );
  vsaved = 1;
}

static void
bv_adcursor()
{
  if (adc_state->adc_draw)  {
    /* Was on, turn off */
    adc_state->adc_draw = 0;
  }  else  {
    /* Was off, turn on */
    adc_state->adc_draw = 1;
  }

  adc_set_scroll();
}

static void
bv_reset()  {
  /* Reset view such that all solids can be seen */
  size_reset();
  setview( 0.0, 0.0, 0.0 );
  (void)mged_svbase();
}

static void
bv_45_45()  {
  setview( 270.0+45.0, 0.0, 270.0-45.0 );
}

static void
bv_35_25()  {
  /* Use Azmuth=35, Elevation=25 in GIFT's backwards space */
  setview( 270.0+25.0, 0.0, 270.0-35.0 );
}

/* returns 0 if error, !0 if success */
static int
ill_common()  {
	/* Common part of illumination */
	if(BU_LIST_IS_EMPTY(&HeadSolid.l)) {
	  Tcl_AppendResult(interp, "no solids in view\n", (char *)NULL);
	  return(0);	/* BAD */
	}

	illump = BU_LIST_NEXT(solid, &HeadSolid.l);/* any valid solid would do */
	illump->s_iflag = UP;
	edobj = 0;		/* sanity */
	edsol = 0;		/* sanity */
	movedir = 0;		/* No edit modes set */
	bn_mat_idn( modelchanges );	/* No changes yet */

	return(1);		/* OK */
}

static void
be_o_illuminate()  {
	if( not_state( ST_VIEW, "Matrix Illuminate" ) )
		return;

	if( ill_common() )  {
		(void)chg_state( ST_VIEW, ST_O_PICK, "Matrix Illuminate" );
	}
	/* reset accumulation local scale factors */
	acc_sc[0] = acc_sc[1] = acc_sc[2] = 1.0;

	/* reset accumulation global scale factors */
	acc_sc_obj = 1.0;
}

static void
be_s_illuminate()  {
	if( not_state( ST_VIEW, "Prim Illuminate" ) )
		return;

	if( ill_common() )  {
		(void)chg_state( ST_VIEW, ST_S_PICK, "Prim Illuminate" );
	}
}

static void
be_o_scale()  {
	if( not_state( ST_O_EDIT, "Matrix Scale" ) )
		return;

	edobj = BE_O_SCALE;
	movedir = SARROW;
	update_views = 1;
	set_e_axes_pos(1);

	edit_absolute_scale = acc_sc_obj - 1.0;
	if(edit_absolute_scale > 0.0)
	  edit_absolute_scale /= 3.0;
}

static void
be_o_xscale()  {
	if( not_state( ST_O_EDIT, "Matrix Local X Scale" ) )
		return;

	edobj = BE_O_XSCALE;
	movedir = SARROW;
	update_views = 1;
	set_e_axes_pos(1);

	edit_absolute_scale = acc_sc[0] - 1.0;
	if(edit_absolute_scale > 0.0)
	  edit_absolute_scale /= 3.0;
}

static void
be_o_yscale()  {
	if( not_state( ST_O_EDIT, "Matrix Local Y Scale" ) )
		return;

	edobj = BE_O_YSCALE;
	movedir = SARROW;
	update_views = 1;
	set_e_axes_pos(1);

	edit_absolute_scale = acc_sc[1] - 1.0;
	if(edit_absolute_scale > 0.0)
	  edit_absolute_scale /= 3.0;
}

static void
be_o_zscale()  {
	if( not_state( ST_O_EDIT, "Matrix Local Z Scale" ) )
		return;

	edobj = BE_O_ZSCALE;
	movedir = SARROW;
	update_views = 1;
	set_e_axes_pos(1);

	edit_absolute_scale = acc_sc[2] - 1.0;
	if(edit_absolute_scale > 0.0)
	  edit_absolute_scale /= 3.0;
}

static void
be_o_x()  {
	if( not_state( ST_O_EDIT, "Matrix X Motion" ) )
		return;

	edobj = BE_O_X;
	movedir = RARROW;
	update_views = 1;
	set_e_axes_pos(1);
}

static void
be_o_y()  {
	if( not_state( ST_O_EDIT, "Matrix Y Motion" ) )
		return;

	edobj = BE_O_Y;
	movedir = UARROW;
	update_views = 1;
	set_e_axes_pos(1);
}


static void
be_o_xy()  {
	if( not_state( ST_O_EDIT, "Matrix XY Motion" ) )
		return;

	edobj = BE_O_XY;
	movedir = UARROW | RARROW;
	update_views = 1;
	set_e_axes_pos(1);
}

static void
be_o_rotate()  {
	if( not_state( ST_O_EDIT, "Matrix Rotation" ) )
		return;

	edobj = BE_O_ROTATE;
	movedir = ROTARROW;
	update_views = 1;
	set_e_axes_pos(1);
}

static void
be_accept()  {
	register struct solid *sp;
	register struct dm_list *dmlp;

	if( state == ST_S_EDIT )  {
		/* Accept a solid edit */
		edsol = 0;

		sedit_accept();		/* zeros "edsol" var */

		mmenu_set_all( MENU_L1, MENU_NULL );
		mmenu_set_all( MENU_L2, MENU_NULL );

		FOR_ALL_SOLIDS(sp, &HeadSolid.l)
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
		(void)not_state( ST_S_EDIT, "Edit Accept" );
		return;
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
}

static void
be_reject()  {
	register struct solid *sp;
	register struct dm_list *dmlp;

	update_views = 1;

	/* Reject edit */

	switch( state )  {
	default:
		state_err( "Edit Reject" );
		return;

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
	FOR_ALL_SOLIDS(sp, &HeadSolid.l)
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
}

static void
bv_slicemode() {
}

static void
be_s_edit()  {
	/* solid editing */
	if( not_state( ST_S_EDIT, "Prim Edit (Menu)" ) )
		return;

	edsol = BE_S_EDIT;
	sedit_menu();		/* Install appropriate menu */
}

static void
be_s_rotate()  {
	/* rotate solid */
	if( not_state( ST_S_EDIT, "Prim Rotate" ) )
		return;

	es_edflag = SROT;
	edsol = BE_S_ROTATE;
	mmenu_set( MENU_L1, MENU_NULL );

        set_e_axes_pos(1);
}

static void
be_s_trans()  {
	/* translate solid */
	if( not_state( ST_S_EDIT, "Prim Translate" ) )
		return;

	edsol = BE_S_TRANS;
	es_edflag = STRANS;
	movedir = UARROW | RARROW;
	mmenu_set( MENU_L1, MENU_NULL );

        set_e_axes_pos(1);
}

static void
be_s_scale()  {
	/* scale solid */
	if( not_state( ST_S_EDIT, "Prim Scale" ) )
		return;

	edsol = BE_S_SCALE;
	es_edflag = SSCALE;
	mmenu_set( MENU_L1, MENU_NULL );
	acc_sc_sol = 1.0;

        set_e_axes_pos(1);
}

/*
 *			N O T _ S T A T E
 *  
 *  Returns 0 if current state is as desired,
 *  Returns !0 and prints error message if state mismatch.
 */
int
not_state( desired, str )
int desired;
char *str;
{
  if( state != desired ) {
    Tcl_AppendResult(interp, "Unable to do <", str, "> from ",
		     state_str[state], " state.\n", (char *)NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *  			C H G _ S T A T E
 *
 *  Returns 0 if state change is OK,
 *  Returns !0 and prints error message if error.
 */
int
chg_state( from, to, str )
int from, to;
char *str;
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
      bn_mat_copy(o_toViewcenter, view_state->vs_toViewcenter);
      o_Viewscale = view_state->vs_Viewscale;

      /* get new orig_pos */
      size_reset();
      MAT_DELTAS_GET_NEG(view_state->vs_orig_pos, view_state->vs_toViewcenter);

      /* restore old toViewcenter and Viewscale */
      bn_mat_copy(view_state->vs_toViewcenter, o_toViewcenter);
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
state_err( str )
char *str;
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
 btn_item_hit(arg, menu, item)  {
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
btn_head_menu(i, menu, item)  {
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
chg_l2menu(i)  {
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
