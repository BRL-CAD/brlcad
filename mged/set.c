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
#include "./mged_solid.h"
#include "./mged_dm.h"

#include "tcl.h"

extern void	predictor_hook();		/* in ged.c */

struct _mged_variables default_mged_variables = {
/* autosize */			1,
/* rateknobs */			0,
/* adcflag */                   0,
/* slidersflag */               0,
/* sgi_win_size */		0,
/* sgi_win_origin */		{ 0, 0 },
/* faceplate */			1,
/* orig_gui */                  1,
/* m_axes */    	        0,
/* v_axes */    	        0,
/* v_axes_pos */                0,
/* e_axes */            	0,
/* linewidth */                 1,
/* linestyle */                 0,
/* send_key */                  0,
/* hot_key */                   0,
/* context */                   1,
/* dlist */                     1,
/* nirt_behavior */             0,
/* mouse_nirt */                0,
/* use_air */			0,
/* echo_nirt_cmd */		0,
/* coords */                    'v',
/* ecoords */                   'o',
/* rotate_about */              'v',
/* erotate_about */             'k',
/* transform */                 'v',
/* predictor */			0,
/* predictor_advance */		1.0,
/* predictor_length */		2.0,
/* perspective */		-1,
/* perspective_mode */           0,
/* toggle_perspective */         1,
/* nmg_eu_dist */		0.05,
/* eye_sep_dist */		0.0,
/* union lexeme */		"u",
/* intersection lexeme */	"n",
/* difference lexeme */		"-"
};

static int perspective_table[] = { 90, 30, 45, 60 };

static void set_dlist();
static void set_perspective();
static void establish_perspective();
static void toggle_perspective();
void set_dirty_flag();
void set_scroll();
void set_absolute_tran();
void set_absolute_view_tran();
void set_absolute_model_tran();

/*
 *  Cause screen to be refreshed when all cmds have been processed.
 */
static void
refresh_hook()
{
	dmaflag = 1;
}

void
set_dirty_flag()
{
  struct dm_list *dmlp;

  FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l)
    if(dmlp->_mged_variables == mged_variables)
      dmlp->_dirty = 1;
}

static void
nmg_eu_dist_set()
{
  extern double nmg_eue_dist;
  struct bu_vls tmp_vls;

  nmg_eue_dist = mged_variables->nmg_eu_dist;

  bu_vls_init(&tmp_vls);
  bu_vls_printf(&tmp_vls, "New nmg_eue_dist = %g\n", nmg_eue_dist);
  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
  bu_vls_free(&tmp_vls);
}

#define MV_O(_m)	offsetof(struct _mged_variables, _m)
struct bu_structparse mged_vparse[] = {
	{"%d",	1, "autosize",		MV_O(autosize),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "rateknobs",		MV_O(rateknobs),	set_scroll },
	{"%d",	1, "adcflag",		MV_O(adcflag),          set_scroll },
	{"%d",	1, "slidersflag",	MV_O(slidersflag),      set_scroll },
	{"%d",	1, "sgi_win_size",	MV_O(sgi_win_size),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	2, "sgi_win_origin",	MV_O(sgi_win_origin[0]),BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "faceplate",		MV_O(faceplate),	set_dirty_flag },
	{"%d",	1, "orig_gui",		MV_O(orig_gui),	        set_dirty_flag },
	{"%d",  1, "m_axes",            MV_O(m_axes),           set_dirty_flag },
	{"%d",  1, "v_axes",            MV_O(v_axes),           set_dirty_flag },
	{"%d",  1, "v_axes_pos",        MV_O(v_axes_pos),       set_dirty_flag },
	{"%d",  1, "e_axes",            MV_O(e_axes),           set_dirty_flag },
	{"%d",	1, "linewidth",		MV_O(linewidth),	set_dirty_flag },
	{"%d",	1, "linestyle",		MV_O(linestyle),	set_dirty_flag },
	{"%d",  1, "send_key",          MV_O(send_key),         BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "hot_key",           MV_O(hot_key),          BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "context",           MV_O(context),          BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "dlist",             MV_O(dlist),            set_dlist },
	{"%d",  1, "nirt_behavior",     MV_O(nirt_behavior),    BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "mouse_nirt",        MV_O(mouse_nirt),       BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "use_air",		MV_O(use_air),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "echo_nirt_cmd",	MV_O(echo_nirt_cmd),	BU_STRUCTPARSE_FUNC_NULL },
	{"%c",  1, "coords",            MV_O(coords),           BU_STRUCTPARSE_FUNC_NULL },
	{"%c",  1, "ecoords",           MV_O(ecoords),          BU_STRUCTPARSE_FUNC_NULL },
	{"%c",  1, "rotate_about",      MV_O(rotate_about),     BU_STRUCTPARSE_FUNC_NULL },
	{"%c",  1, "erotate_about",     MV_O(erotate_about),    BU_STRUCTPARSE_FUNC_NULL },
	{"%c",  1, "transform",         MV_O(transform),        BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "predictor",		MV_O(predictor),	predictor_hook },
	{"%f",	1, "predictor_advance",	MV_O(predictor_advance),predictor_hook },
	{"%f",	1, "predictor_length",	MV_O(predictor_length),	predictor_hook },
	{"%f",	1, "perspective",	MV_O(perspective),	set_perspective },
	{"%d",  1, "perspective_mode",  MV_O(perspective_mode),establish_perspective },
	{"%d",  1, "toggle_perspective",MV_O(toggle_perspective),toggle_perspective },
	{"%f",  1, "nmg_eu_dist",	MV_O(nmg_eu_dist),	nmg_eu_dist_set },
	{"%f",  1, "eye_sep_dist",	MV_O(eye_sep_dist),	set_dirty_flag },
	{"%s",  MAXLINE, "union_op",	MV_O(union_lexeme[0]),	BU_STRUCTPARSE_FUNC_NULL },
	{"%s",  MAXLINE, "intersection_op",MV_O(intersection_lexeme[0]),	BU_STRUCTPARSE_FUNC_NULL },
	{"%s",  MAXLINE, "difference_op",	MV_O(difference_lexeme[0]),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0,  (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
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
    register int i;

    /* Ask the libbu structparser for the value of the variable */

    bu_vls_init( &str );
    bu_vls_struct_item( &str, sp, (CONST char *)mged_variables, ' ');

    /* Next, set the Tcl variable to this value */
    (void)Tcl_SetVar(interp, sp->sp_name, bu_vls_addr(&str),
		     (flags&TCL_GLOBAL_ONLY)|TCL_LEAVE_ERR_MSG);

    bu_vls_free(&str);

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
    if( bu_struct_parse( &str, mged_vparse, (char *)mged_variables ) < 0) {
      Tcl_AppendResult(interp, "ERROR OCCURED WHEN SETTING ", name1,
		       " TO ", newvalue, "\n", (char *)NULL);
    }

    bu_vls_free(&str);
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

  if(argc < 1 || 2 < argc){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help vars");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  bu_vls_init(&vls);
  if (argc == 1) {
    start_catching_output(&vls);
    bu_struct_print("mged variables", mged_vparse, (CONST char *)mged_variables);
    stop_catching_output(&vls);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  } else if (argc == 2) {
    bu_vls_strcpy(&vls, argv[1]);
    bu_struct_parse(&vls, mged_vparse, (char *)mged_variables);
  }
  bu_vls_free(&vls);

  return TCL_OK;
}

void
set_scroll()
{
  struct bu_vls vls;
  struct bu_vls save_result1_vls;
  struct bu_vls save_result2_vls;

  if(!strcmp("nu", bu_vls_addr(&pathName)))
    return;

  bu_vls_init(&vls);
  bu_vls_init(&save_result1_vls);
  bu_vls_init(&save_result2_vls);
  bu_vls_strcpy(&save_result1_vls, interp->result);

  if( mged_variables->slidersflag )
    bu_vls_printf(&vls, "sliders on");
  else
    bu_vls_printf(&vls, "sliders off");

  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_strcpy(&save_result2_vls, interp->result);
  Tcl_SetResult(interp, bu_vls_addr(&save_result1_vls), TCL_VOLATILE);
  Tcl_AppendResult(interp, bu_vls_addr(&save_result2_vls), (char *)NULL);
  
  bu_vls_free(&vls);
  bu_vls_free(&save_result1_vls);
  bu_vls_free(&save_result2_vls);
}

void
set_absolute_tran()
{
  /* calculate absolute_tran */
  set_absolute_view_tran();

  /* calculate absolute_model_tran */
  set_absolute_model_tran();
}

void
set_absolute_view_tran()
{
  /* calculate absolute_tran */
  MAT4X3PNT(absolute_tran, model2view, orig_pos);
  /* This is used in f_knob()  ---- needed in case absolute_tran is set from Tcl */
  VMOVE(last_absolute_tran, absolute_tran);
}

void
set_absolute_model_tran()
{
  point_t new_pos;
  point_t diff;

  /* calculate absolute_model_tran */
  MAT_DELTAS_GET_NEG(new_pos, toViewcenter);
  VSUB2(diff, orig_pos, new_pos);
  VSCALE(absolute_model_tran, diff, 1/Viewscale);
  /* This is used in f_knob()  ---- needed in case absolute_model_tran is set from Tcl */
  VMOVE(last_absolute_model_tran, absolute_model_tran);
}

static void
set_dlist()
{
  struct dm_list *dmlp;
  struct dm_list *save_dmlp;

  save_dmlp = curr_dm_list;

#ifdef DO_DISPLAY_LISTS
  if(mged_variables->dlist){
    /* create display lists */
    FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l){
      if(dmlp->_mged_variables != save_dmlp->_mged_variables)
	continue;

      curr_dm_list = dmlp;

      /* if display manager supports display lists */
      if(displaylist){
	dirty = 1;
#ifdef DO_SINGLE_DISPLAY_LIST
	createDList(&HeadSolid);
#else
	createDLists(&HeadSolid); 
#endif
      }
    }
  }else{
    /* free display lists */
    FOR_ALL_DISPLAYS(dmlp, &head_dm_list.l){
      if(dmlp->_mged_variables != save_dmlp->_mged_variables)
	continue;

      curr_dm_list = dmlp;

      /* if display manager supports display lists */
      if(displaylist){
	dirty = 1;
#ifdef DO_SINGLE_DISPLAY_LIST
	DM_FREEDLISTS(dmp, HeadSolid.s_dlist, 1);
#else
	DM_FREEDLISTS(dmp, HeadSolid.s_dlist,
			   BU_LIST_LAST(solid, &HeadSolid.l)->s_dlist -
			   HeadSolid.s_dlist + 1);
#endif
      }
    }
  }
#endif

  /* restore */
  curr_dm_list = save_dmlp;
}

static void
set_perspective()
{
  if(mged_variables->perspective > 0)
    mged_variables->perspective_mode = 1;
  else
    mged_variables->perspective_mode = 0;

  set_dirty_flag();
}

static void
establish_perspective()
{
  mged_variables->perspective = mged_variables->perspective_mode ?
    perspective_table[perspective_angle] : -1;

  set_dirty_flag();
}

/*
   This routine toggles the perspective_angle if the
   toggle_perspective value is 0 or less. Otherwise, the
   perspective_angle is set to the value of (toggle_perspective - 1).
*/
static void
toggle_perspective()
{
  /* set perspective matrix */
  if(mged_variables->toggle_perspective > 0)
    perspective_angle = mged_variables->toggle_perspective <= 4 ?
      mged_variables->toggle_perspective - 1: 3;
  else if (--perspective_angle < 0) /* toggle perspective matrix */
    perspective_angle = 3;

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  mged_variables->toggle_perspective = 1;

  if(!mged_variables->perspective_mode)
    return;

  mged_variables->perspective = perspective_table[perspective_angle];

  set_dirty_flag();
}
