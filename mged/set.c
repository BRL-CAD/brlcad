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
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"
#include "./ged.h"

#ifndef XMGED
#include "tcl.h"
#endif

#ifdef XMGED
extern int update_views;
#endif

extern void	predictor_hook();		/* in ged.c */
extern void	reattach();			/* in attach.c */

struct mged_variables mged_variables = {
/* autosize */			1,
/* rateknobs */			1,
/* sgi_win_size */		0,
/* sgi_win_origin */		{ 0, 0 },
/* faceplate */			1,
#ifdef XMGED
/* w_axis */    	        0,
/* v_axis */    	        0,
/* e_axis */            	0,
#endif
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

	nmg_eue_dist = mged_variables.nmg_eu_dist;

	rt_log( "New nmg_eue_dist = %g\n", nmg_eue_dist);
}
#define MV_O(_m)	offsetof(struct mged_variables, _m)
struct structparse mged_vparse[] = {
	{"%d",	1, "autosize",		MV_O(autosize),		FUNC_NULL },
	{"%d",	1, "rateknobs",		MV_O(rateknobs),	FUNC_NULL },
	{"%d",	1, "sgi_win_size",	MV_O(sgi_win_size),	FUNC_NULL },
	{"%d",	2, "sgi_win_origin",	MV_O(sgi_win_origin[0]),FUNC_NULL },
	{"%d",	1, "faceplate",		MV_O(faceplate),	refresh_hook },
#ifdef XMGED
	{"%d",  1, "w_axis",            MV_O(w_axis),           refresh_hook },
	{"%d",  1, "v_axis",            MV_O(v_axis),           refresh_hook },
	{"%d",  1, "e_axis",            MV_O(e_axis),           refresh_hook },
#endif
	{"%d",	1, "predictor",		MV_O(predictor),	predictor_hook },
	{"%f",	1, "predictor_advance",	MV_O(predictor_advance),predictor_hook },
	{"%f",	1, "predictor_length",	MV_O(predictor_length),	predictor_hook },
	{"%f",	1, "perspective",	MV_O(perspective),	refresh_hook },
	{"%f",  1, "nmg_eu_dist",	MV_O(nmg_eu_dist),	nmg_eu_dist_set },
	{"%f",  1, "eye_sep_dist",	MV_O(eye_sep_dist),	reattach },
	{"%s",  MAXLINE, "union_op",	MV_O(union_lexeme),	FUNC_NULL },
	{"%s",  MAXLINE, "intersection_op",MV_O(intersection_lexeme),	FUNC_NULL },
	{"%s",  MAXLINE, "difference_op",	MV_O(difference_lexeme),	FUNC_NULL },
	{"",	0,  (char *)0,		0,			FUNC_NULL }
};

#ifndef XMGED

/**
 **            R E A D _ V A R
 **
 ** Callback used when an MGED variable is read with either the Tcl "set"
 ** command or the Tcl dereference operator '$'.
 **
 **/

char *
read_var(clientData, interp, name1, name2, flags)
ClientData clientData;       /* Contains pointer to structparse entry */
Tcl_Interp *interp;
char *name1, *name2;
int flags;
{
    struct structparse *sp = (struct structparse *)clientData;
    struct rt_vls str;
    char *curvalue;

    /* Ask the librt structparser for the value of the variable */

    rt_vls_init( &str );
    rt_vls_item_print( &str, sp, (CONST char *)&mged_variables );

    /* Next, set the Tcl variable to this value */

    (void)Tcl_SetVar(interp, sp->sp_name, rt_vls_addr(&str),
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
    struct structparse *sp = (struct structparse *)clientData;
    struct rt_vls str;
    char *newvalue;

    newvalue = Tcl_GetVar(interp, sp->sp_name,
			  (flags&TCL_GLOBAL_ONLY)|TCL_LEAVE_ERR_MSG);
    rt_vls_init( &str );
    rt_vls_printf( &str, "%s=\"%s\"", name1, newvalue );
    if( rt_structparse( &str, mged_vparse, (char *)&mged_variables ) < 0) {
	rt_log("ERROR OCCURED WHEN SETTING %s TO %s\n",
	       name1, newvalue);
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
    struct structparse *sp = (struct structparse *)clientData;

    if( flags & TCL_INTERP_DESTROYED )
	return NULL;

    rt_log( "mged variables cannot be unset\n" );
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
    register struct structparse *sp;

    for( sp = &mged_vparse[0]; sp->sp_name != NULL; sp++ ) {
	read_var( (ClientData)sp, interp, sp->sp_name, NULL, 0 );
	Tcl_TraceVar( interp, sp->sp_name, TCL_TRACE_READS|TCL_GLOBAL_ONLY,
		      read_var, (ClientData)sp );
	Tcl_TraceVar( interp, sp->sp_name, TCL_TRACE_WRITES|TCL_GLOBAL_ONLY,
		      write_var, (ClientData)sp );
	Tcl_TraceVar( interp, sp->sp_name, TCL_TRACE_UNSETS|TCL_GLOBAL_ONLY,
		      unset_var, (ClientData)sp );
    }
}

#endif

int
f_set(ac,av)
int ac;
char *av[];
{
	struct rt_vls vls;
	int bad = 0;

	rt_vls_init(&vls);

	if (ac <= 1) {
		rt_structprint("mged variables", mged_vparse, (CONST char *)&mged_variables);
		rt_log("%s", rt_vls_addr(&vls) );
	} else if (ac == 2) {
		rt_vls_strcpy(&vls, av[1]);
		rt_structparse(&vls, mged_vparse, (char *)&mged_variables);
	} else {
		rt_log("Usage: set\t\t- prints all options\n\tset opt=val\t- sets an option\n");
		bad = 1;
	}
	rt_vls_free(&vls);

#ifdef XMGED
	update_views = 1;
#endif
	return bad ? CMD_BAD : CMD_OK;
}


