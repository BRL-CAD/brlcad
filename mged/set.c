/*
 *			S E T . C
 *  Authors -
 *	Lee A. Butler
 *      Glenn Durfee
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./sedit.h"
#include "./ged.h"
#include "./mged_dm.h"

#include "tcl.h"

extern void	predictor_hook();		/* in ged.c */
extern void	reattach();			/* in attach.c */

struct _mged_variables default_mged_variables = {
/* autosize */			1,
/* rateknobs */			1,
/* adcflag */                   0,
/* scroll_enabled */            0,
/* sgi_win_size */		0,
/* sgi_win_origin */		{ 0, 0 },
/* faceplate */			1,
/* show_menu */                 1,
/* w_axes */    	        0,
/* v_axes */    	        0,
/* e_axes */            	0,
/* send_key */                  0,
/* hot_key */                   0,
/* view */                      0,
/* edit */                      0,
/* predictor */			0,
/* predictor_advance */		1.0,
/* predictor_length */		2.0,
/* perspective */		-1,
/* nmg_eu_dist */		0.05,
/* eye_sep_dist */		0.0,
/* union lexeme */		"u",
/* intersection lexeme */	"n",
/* difference lexeme */		"-"
};

static void set_view();
static void set_scroll();


/*
 *  Cause screen to be refreshed when all cmds have been processed.
 */
static void
refresh_hook()
{
	dmaflag = 1;
}

static void
nmg_eu_dist_set()
{
  extern double nmg_eue_dist;
  struct bu_vls tmp_vls;

  nmg_eue_dist = mged_variables.nmg_eu_dist;

  bu_vls_init(&tmp_vls);
  bu_vls_printf(&tmp_vls, "New nmg_eue_dist = %g\n", nmg_eue_dist);
  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
  bu_vls_free(&tmp_vls);
}

#define MV_O(_m)	offsetof(struct _mged_variables, _m)
struct bu_structparse mged_vparse[] = {
	{"%d",	1, "autosize",		MV_O(autosize),		FUNC_NULL },
	{"%d",	1, "rateknobs",		MV_O(rateknobs),	set_scroll },
	{"%d",	1, "adcflag",		MV_O(adcflag),          set_scroll },
	{"%d",	1, "scroll_enabled",	MV_O(scroll_enabled),   set_scroll },
	{"%d",	1, "sgi_win_size",	MV_O(sgi_win_size),	FUNC_NULL },
	{"%d",	2, "sgi_win_origin",	MV_O(sgi_win_origin[0]),FUNC_NULL },
	{"%d",	1, "faceplate",		MV_O(faceplate),	refresh_hook },
	{"%d",	1, "show_menu",		MV_O(show_menu),	refresh_hook },
	{"%d",  1, "w_axes",            MV_O(w_axes),           refresh_hook },
	{"%d",  1, "v_axes",            MV_O(v_axes),           refresh_hook },
	{"%d",  1, "e_axes",            MV_O(e_axes),           refresh_hook },
	{"%d",  1, "send_key",          MV_O(send_key),         FUNC_NULL },
	{"%d",  1, "hot_key",           MV_O(hot_key),         FUNC_NULL },
	{"%d",  1, "view",              MV_O(view),             set_view },
	{"%d",  1, "edit",              MV_O(edit),             set_scroll },
	{"%d",	1, "predictor",		MV_O(predictor),	predictor_hook },
	{"%f",	1, "predictor_advance",	MV_O(predictor_advance),predictor_hook },
	{"%f",	1, "predictor_length",	MV_O(predictor_length),	predictor_hook },
	{"%f",	1, "perspective",	MV_O(perspective),	refresh_hook },
	{"%f",  1, "nmg_eu_dist",	MV_O(nmg_eu_dist),	nmg_eu_dist_set },
	{"%f",  1, "eye_sep_dist",	MV_O(eye_sep_dist),	reattach },
	{"%s",  MAXLINE, "union_op",	MV_O(union_lexeme[0]),	FUNC_NULL },
	{"%s",  MAXLINE, "intersection_op",MV_O(intersection_lexeme[0]),	FUNC_NULL },
	{"%s",  MAXLINE, "difference_op",	MV_O(difference_lexeme[0]),	FUNC_NULL },
	{"",	0,  (char *)0,		0,			FUNC_NULL }
};

/**
 **            R E A D _ V A R
 **
 ** Callback used when an MGED variable is read with either the Tcl "set"
 ** command or the Tcl dereference operator '$'.
 **
 **/

char *
read_var(clientData, interp, name1, name2, flags)
ClientData clientData;       /* Contains pointer to bu_struct_parse entry */
Tcl_Interp *interp;
char *name1, *name2;
int flags;
{
    struct bu_structparse *sp = (struct bu_structparse *)clientData;
    struct bu_vls str;

    /* Ask the libbu structparser for the value of the variable */

    bu_vls_init( &str );
    bu_vls_struct_item( &str, sp, (CONST char *)&mged_variables, ' ');

    /* Next, set the Tcl variable to this value */

    (void)Tcl_SetVar(interp, sp->sp_name, bu_vls_addr(&str),
		     (flags&TCL_GLOBAL_ONLY)|TCL_LEAVE_ERR_MSG);
    return NULL;
}

/**
 **            W R I T E _ V A R
 **
 ** Callback used when an MGED variable is set with the Tcl "set" command.
 **
 **/

char *
write_var(clientData, interp, name1, name2, flags)
ClientData clientData;
Tcl_Interp *interp;
char *name1, *name2;
int flags;
{
    struct bu_structparse *sp = (struct bu_structparse *)clientData;
    struct bu_vls str;
    char *newvalue;

    newvalue = Tcl_GetVar(interp, sp->sp_name,
			  (flags&TCL_GLOBAL_ONLY)|TCL_LEAVE_ERR_MSG);
    bu_vls_init( &str );
    bu_vls_printf( &str, "%s=\"%s\"", name1, newvalue );
    if( bu_struct_parse( &str, mged_vparse, (char *)&mged_variables ) < 0) {
      Tcl_AppendResult(interp, "ERROR OCCURED WHEN SETTING ", name1,
		       " TO ", newvalue, "\n", (char *)NULL);
    }
    return read_var(clientData, interp, name1, name2,
		    (flags&(~TCL_TRACE_WRITES))|TCL_TRACE_READS);
}

/**
 **            U N S E T _ V A R
 **
 ** Callback used when an MGED variable is unset.  This function undoes that.
 **
 **/

char *
unset_var(clientData, interp, name1, name2, flags)
ClientData clientData;
Tcl_Interp *interp;
char *name1, *name2;
int flags;
{
    struct bu_structparse *sp = (struct bu_structparse *)clientData;

    if( flags & TCL_INTERP_DESTROYED )
	return NULL;

    Tcl_AppendResult(interp, "mged variables cannot be unset\n", (char *)NULL);
    Tcl_TraceVar( interp, sp->sp_name, TCL_TRACE_READS, read_var,
		  (ClientData)sp );
    Tcl_TraceVar( interp, sp->sp_name, TCL_TRACE_WRITES, write_var,
		  (ClientData)sp );
    Tcl_TraceVar( interp, sp->sp_name, TCL_TRACE_UNSETS, unset_var,
 		  (ClientData)sp );
    read_var(clientData, interp, name1, name2,
	     (flags&(~TCL_TRACE_UNSETS))|TCL_TRACE_READS);
    return NULL;
}

/**
 **           M G E D _ V A R I A B L E _ S E T U P
 **
 ** Sets the variable traces for each of the MGED variables so they can be
 ** accessed with the Tcl "set" and "$" operators.
 **
 **/

void
mged_variable_setup(interp)
Tcl_Interp *interp;    
{
    register struct bu_structparse *sp;
    register int i;

    for( sp = &mged_vparse[0]; sp->sp_name != NULL; sp++ ) {
	read_var( (ClientData)sp, interp, sp->sp_name, (char *)NULL, 0 );
	Tcl_TraceVar( interp, sp->sp_name, TCL_TRACE_READS|TCL_GLOBAL_ONLY,
		      read_var, (ClientData)sp );
	Tcl_TraceVar( interp, sp->sp_name, TCL_TRACE_WRITES|TCL_GLOBAL_ONLY,
		      write_var, (ClientData)sp );
	Tcl_TraceVar( interp, sp->sp_name, TCL_TRACE_UNSETS|TCL_GLOBAL_ONLY,
		      unset_var, (ClientData)sp );
    }
}

int
f_set(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char *argv[];
{
	struct bu_vls vls;
	int bad = 0;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	bu_vls_init(&vls);

	if (argc == 1) {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  start_catching_output(&tmp_vls);
	  bu_struct_print("mged variables", mged_vparse, (CONST char *)&mged_variables);
	  bu_log("%s", bu_vls_addr(&vls) );
	  stop_catching_output(&tmp_vls);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	} else if (argc == 2) {
		bu_vls_strcpy(&vls, argv[1]);
		bu_struct_parse(&vls, mged_vparse, (char *)&mged_variables);
	}

	bu_vls_free(&vls);

	dmaflag = 1;
	return bad ? TCL_ERROR : TCL_OK;
}

static void
set_view()
{
  point_t model_pos;
  point_t new_pos;

  /* save current view */
  mat_copy(viewrot_table[current_view], Viewrot);

  /* save current Viewscale */
  viewscale_table[current_view] = Viewscale;

  /* toggle forward */
  if(mged_variables.view){
    if(++current_view > VIEW_TABLE_SIZE - 1)
      current_view = 0;
  }else{
    /* toggle backward */
    if(--current_view < 0)
            current_view = VIEW_TABLE_SIZE - 1;
  }

#if 0
  if(EDIT_TRAN)
    MAT4X3PNT(model_pos, view2model, edit_absolute_tran);
#endif
  /* restore previously saved view and Viewscale */
  mat_copy(Viewrot, viewrot_table[current_view]);
  Viewscale = viewscale_table[current_view];
  new_mats();
#if 0
  if(EDIT_TRAN)
    MAT4X3PNT(edit_absolute_tran, model2view, model_pos);
#endif
  VSET(new_pos, -orig_pos[X], -orig_pos[Y], -orig_pos[Z]);
  MAT4X3PNT(absolute_slew, model2view, new_pos);
}

static void
set_scroll()
{
  if( mged_variables.scroll_enabled )
    Tcl_Eval( interp, "sliders on");
}
