/*                          M E N U . C
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
/** @file menu.c
 *
 * Functions -
 *	mmenu_init		Clear global menu data
 *	mmenu_display		Add a list of items to the display list
 *	mmenu_select		Called by usepen() for menu pointing
 *	mmenu_pntr		Reset the pointer to a menu item
 *
 * Authors -
 *	Bob Suckling
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

#include "tcl.h"

#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./titles.h"
#include "./mged_dm.h"


extern struct menu_item second_menu[], sed_menu[];

void set_menucurrent();
int set_arrowloc();

int
cmd_mmenu_get(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    int index;

    if (argc > 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helpdevel mmenu_get");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (argc == 2) {
	register struct menu_item **m, *mptr;

	if (Tcl_GetInt(interp, argv[1], &index) != TCL_OK)
	    return TCL_ERROR;

	if (index < 0 || NMENU <= index) {
	    Tcl_AppendResult(interp, "index out of range", (char *)NULL);
	    return TCL_ERROR;
	}

	m = menu_state->ms_menus+index;
	if (*m == MENU_NULL)
	    return TCL_OK;

	for (mptr = *m; mptr->menu_string[0] != '\0'; mptr++)
	    Tcl_AppendElement(interp, mptr->menu_string);
    } else {
	register struct menu_item **m;
	struct bu_vls result;
	int status;

	bu_vls_init(&result);
	bu_vls_strcat(&result, "list");
	for (m = menu_state->ms_menus; m - menu_state->ms_menus < NMENU; m++)
	    bu_vls_printf(&result, " [%s %d]", argv[0], m-menu_state->ms_menus);

	status = Tcl_Eval(interp, bu_vls_addr(&result));
	bu_vls_free(&result);

	return status;
    }

    return TCL_OK;
}


/*
 *			M M E N U _ I N I T
 *
 * Clear global data
 */
void
mmenu_init(void)
{
	menu_state->ms_flag = 0;
	menu_state->ms_menus[MENU_L1] = MENU_NULL;
	menu_state->ms_menus[MENU_L2] = MENU_NULL;
	menu_state->ms_menus[MENU_GEN] = MENU_NULL;
#if 0
	(void)Tcl_CreateCommand(interp, "mmenu_set", cmd_nop, (ClientData)NULL,
				(Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "mmenu_get", cmd_mmenu_get,
				(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
#endif
}


/*
 *			M M E N U _ S E T
 */

void
mmenu_set(int index, struct menu_item *value)
{
    struct dm_list *dlp;

    Tcl_DString ds_menu;
    struct bu_vls menu_string;

    menu_state->ms_menus[index] = value;  /* Change the menu internally */

    bu_vls_init(&menu_string);
    Tcl_DStringInit(&ds_menu);

    bu_vls_printf(&menu_string, "mmenu_set %S %d ", &curr_cmd_list->cl_name, index);

    (void)Tcl_Eval(interp, bu_vls_addr(&menu_string));

    Tcl_DStringFree(&ds_menu);
    bu_vls_free(&menu_string);

    FOR_ALL_DISPLAYS(dlp, &head_dm_list.l){
      if(menu_state == dlp->dml_menu_state &&
	 dlp->dml_mged_variables->mv_faceplate &&
	 dlp->dml_mged_variables->mv_orig_gui)
	dlp->dml_dirty = 1;
    }
}

void
mmenu_set_all(int index, struct menu_item *value)
{
  struct dm_list *p;
  struct cmd_list *save_cmd_list;
  struct dm_list *save_dm_list;

  save_cmd_list = curr_cmd_list;
  save_dm_list = curr_dm_list;
  FOR_ALL_DISPLAYS(p, &head_dm_list.l){
    if(p->dml_tie)
      curr_cmd_list = p->dml_tie;

    curr_dm_list = p;
    mmenu_set( index, value );
  }

  curr_cmd_list = save_cmd_list;
  curr_dm_list = save_dm_list;
}

void
mged_highlight_menu_item(struct menu_item *mptr, int y)
{
  switch(mptr->menu_arg){
  case BV_RATE_TOGGLE:
    if(mged_variables->mv_rateknobs){
      DM_SET_FGCOLOR(dmp,
		     color_scheme->cs_menu_text1[0],
		     color_scheme->cs_menu_text1[1],
		     color_scheme->cs_menu_text1[2], 1, 1.0);
      DM_DRAW_STRING_2D(dmp, "Rate",
			GED2PM1(MENUX), GED2PM1(y-15), 0, 0);
      DM_SET_FGCOLOR(dmp,
		     color_scheme->cs_menu_text2[0],
		     color_scheme->cs_menu_text2[1],
		     color_scheme->cs_menu_text2[2], 1, 1.0);
      DM_DRAW_STRING_2D(dmp, "/Abs",
			GED2PM1(MENUX+4*40), GED2PM1(y-15), 0, 0);
    }else{
      DM_SET_FGCOLOR(dmp,
		     color_scheme->cs_menu_text2[0],
		     color_scheme->cs_menu_text2[1],
		     color_scheme->cs_menu_text2[2], 1, 1.0);
      DM_DRAW_STRING_2D(dmp, "Rate/",
			GED2PM1(MENUX), GED2PM1(y-15), 0, 0);
      DM_SET_FGCOLOR(dmp,
		     color_scheme->cs_menu_text1[0],
		     color_scheme->cs_menu_text1[1],
		     color_scheme->cs_menu_text1[2], 1, 1.0);
      DM_DRAW_STRING_2D(dmp, "Abs",
			GED2PM1(MENUX+5*40), GED2PM1(y-15), 0, 0);
    }
    break;
  default:
    break;
  }
}

/*
 *			M M E N U _ D I S P L A Y
 *
 *  Draw one or more menus onto the display.
 *  If "menu_state->ms_flag" is non-zero, then the last selected
 *  menu item will be indicated with an arrow.
 */
void
mmenu_display(int y_top)
{
  static int menu, item;
  register struct menu_item	**m;
  register struct menu_item	*mptr;
  register int y = y_top;

  menu_state->ms_top = y - MENU_DY / 2;
  DM_SET_FGCOLOR(dmp,
		 color_scheme->cs_menu_line[0],
		 color_scheme->cs_menu_line[1],
		 color_scheme->cs_menu_line[2], 1, 1.0);
#if 1
  DM_SET_LINE_ATTR(dmp, mged_variables->mv_linewidth, 0);
#else
  DM_SET_LINE_ATTR(dmp, 1, 0);
#endif
  DM_DRAW_LINE_2D(dmp,
		  GED2PM1(MENUXLIM), GED2PM1(menu_state->ms_top),
		  GED2PM1(XMIN), GED2PM1(menu_state->ms_top));

  for( menu=0, m = menu_state->ms_menus; m - menu_state->ms_menus < NMENU; m++,menu++ )  {
    if( *m == MENU_NULL )  continue;
    for( item=0, mptr = *m;
	 mptr->menu_string[0] != '\0' && y > TITLE_YBASE;
	 mptr++, y += MENU_DY, item++ )  {
#if 0
      if((*m == (struct menu_item *)second_menu && (mptr->menu_arg == BV_RATE_TOGGLE ||
				  mptr->menu_arg == BV_EDIT_TOGGLE))
	  || (*m == (struct menu_item *)sed_menu && mptr->menu_arg == BE_S_CONTEXT))
#else
      if((*m == (struct menu_item *)second_menu &&
	  (mptr->menu_arg == BV_RATE_TOGGLE ||
	   mptr->menu_arg == BV_EDIT_TOGGLE ||
	   mptr->menu_arg == BV_EYEROT_TOGGLE)))
#endif
	mged_highlight_menu_item(mptr, y);
      else{
	if(mptr == *m)
	  DM_SET_FGCOLOR(dmp,
			 color_scheme->cs_menu_title[0],
			 color_scheme->cs_menu_title[1],
			 color_scheme->cs_menu_title[2], 1, 1.0);
	else
	  DM_SET_FGCOLOR(dmp,
			 color_scheme->cs_menu_text2[0],
			 color_scheme->cs_menu_text2[1],
			 color_scheme->cs_menu_text2[2], 1, 1.0);
	DM_DRAW_STRING_2D(dmp, mptr->menu_string,
			  GED2PM1(MENUX), GED2PM1(y-15), 0, 0);
      }
      DM_SET_FGCOLOR(dmp,
		     color_scheme->cs_menu_line[0],
		     color_scheme->cs_menu_line[1],
		     color_scheme->cs_menu_line[2], 1, 1.0);
      DM_DRAW_LINE_2D(dmp,
		      GED2PM1(MENUXLIM), GED2PM1(y+(MENU_DY/2)),
		      GED2PM1(XMIN), GED2PM1(y+(MENU_DY/2)));
      if( menu_state->ms_cur_item == item && menu_state->ms_cur_menu == menu && menu_state->ms_flag )  {
	/* prefix item selected with "==>" */
	DM_SET_FGCOLOR(dmp,
		       color_scheme->cs_menu_arrow[0],
		       color_scheme->cs_menu_arrow[1],
		       color_scheme->cs_menu_arrow[2], 1, 1.0);
	DM_DRAW_STRING_2D(dmp, "==>",
			  GED2PM1(XMIN), GED2PM1(y-15), 0, 0);
      }
    }
  }

  if( y == y_top )
    return;	/* no active menus */

  DM_SET_FGCOLOR(dmp,
		 color_scheme->cs_menu_line[0],
		 color_scheme->cs_menu_line[1],
		 color_scheme->cs_menu_line[2], 1, 1.0);
#if 1
  DM_SET_LINE_ATTR(dmp, mged_variables->mv_linewidth, 0);
#else
  DM_SET_LINE_ATTR(dmp, 1, 0);
#endif
  DM_DRAW_LINE_2D( dmp,
		   GED2PM1(MENUXLIM), GED2PM1(menu_state->ms_top-1),
		   GED2PM1(MENUXLIM), GED2PM1(y-(MENU_DY/2)) );
}

/*
 *			M M E N U _ S E L E C T
 *
 *  Called with Y coordinate of pen in menu area.
 *
 * Returns:	1 if menu claims these pen co-ordinates,
 *		0 if pen is BELOW menu
 *		-1 if pen is ABOVE menu	(error)
 */
int
mmenu_select( int pen_y, int do_func )
{
	static int menu, item;
	struct menu_item	**m;
	register struct menu_item	*mptr;
	register int			yy;

	if( pen_y > menu_state->ms_top )
		return(-1);	/* pen above menu area */

	/*
	 * Start at the top of the list and see if the pen is
	 * above here.
	 */
	yy = menu_state->ms_top;

	for( menu=0, m=menu_state->ms_menus; m - menu_state->ms_menus < NMENU; m++,menu++ )  {
		if( *m == MENU_NULL )  continue;
		for( item=0, mptr = *m;
		     mptr->menu_string[0] != '\0';
		     mptr++, item++ )  {
			yy += MENU_DY;
			if( pen_y <= yy )
				continue;	/* pen is below this item */
			menu_state->ms_cur_item = item;
			menu_state->ms_cur_menu = menu;
			menu_state->ms_flag = 1;
		     	/* It's up to the menu_func to set menu_state->ms_flag=0
		     	 * if no arrow is desired */
			if( do_func && mptr->menu_func != ((void (*)())0) )
				(*(mptr->menu_func))(mptr->menu_arg, menu, item);

			return( 1 );		/* menu claims pen value */
		}
	}
	return( 0 );		/* pen below menu area */
}

/*
 *			M M E N U _ P N T R
 *
 *  Routine to allow user to reset the arrow to any menu & item desired.
 *  Example:  menu_pntr( MENU_L1, 3 ).
 *  The arrow can always be eliminated by setting menu_state->ms_flag=0, view_state->flag=1.
 */
void
mmenu_pntr(int menu, int item)
{
	menu_state->ms_cur_menu = menu;
	menu_state->ms_cur_item = item;
	if( menu_state->ms_cur_menu >= 0 )
		menu_state->ms_flag = 1;
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
