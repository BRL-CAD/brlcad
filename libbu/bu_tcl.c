/*
 *			B U _ T C L . C
 *
 *  Tcl interfaces to all the LIBBU Basic BRL-CAD Utility routines.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif


#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"

/*
 *			B U _ T C L _ M E M _ B A R R I E R C H E C K
 */
int
bu_tcl_mem_barriercheck(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int	ret;

	ret = bu_mem_barriercheck();
	if( ret < 0 )  {
		Tcl_AppendResult(interp, "bu_mem_barriercheck() failed\n", NULL);
		return TCL_ERROR;
	}
	return TCL_OK;
}

/*
 *			B U _ T C L _ C K _ M A L L O C _ P T R
 */
int
bu_tcl_ck_malloc_ptr(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	if( argc != 3 )  {
		Tcl_AppendResult( interp, "Usage: bu_ck_malloc_ptr ascii-ptr description\n");
		return TCL_ERROR;
	}
	bu_ck_malloc_ptr( (void *)atoi(argv[1]), argv[2] );
	return TCL_OK;
}

/*
 *			B U _ T C L _ M A L L O C _ L E N _ R O U N D U P
 */
int
bu_tcl_malloc_len_roundup(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int	val;

	if( argc != 2 )  {
		Tcl_AppendResult(interp, "Usage: bu_malloc_len_roundup nbytes\n", NULL);
		return TCL_ERROR;
	}
	val = bu_malloc_len_roundup(atoi(argv[1]));
	sprintf(interp->result, "%d", val);
	return TCL_OK;
}

/*
 *			B U _ T C L _ P R M E M
 *
 *  Print map of memory currently in use, to stderr.
 */
int
bu_tcl_prmem(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	if( argc != 2 )  {
		Tcl_AppendResult(interp, "Usage: bu_prmem title\n");
		return TCL_ERROR;
	}
	bu_prmem(argv[1]);
	return TCL_OK;
}

/*
 *			B U _ T C L _ P R I N T B
 */
int
bu_tcl_printb(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct bu_vls	str;

	if( argc != 4 )  {
		Tcl_AppendResult(interp, "Usage: bu_printb title integer-to-format bit-format-string\n", NULL);
		return TCL_ERROR;
	}
	bu_vls_init(&str);
	bu_vls_printb( &str, argv[1], atoi(argv[2]), argv[3] );
	Tcl_SetResult( interp, bu_vls_addr(&str), TCL_VOLATILE );
	bu_vls_free(&str);
}

/*
 *			B U _ T C L _ S E T U P
 *
 *  Add all the supported Tcl interfaces to LIBBU routines to
 *  the list of commands known by the given interpreter.
 */
void
bu_tcl_setup(interp)
Tcl_Interp *interp;
{
	(void)Tcl_CreateCommand(interp,
		"bu_mem_barriercheck",	bu_tcl_mem_barriercheck,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_ck_malloc_ptr",	bu_tcl_ck_malloc_ptr,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_malloc_len_roundup",bu_tcl_malloc_len_roundup,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_prmem",		bu_tcl_prmem,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp,
		"bu_printb",		bu_tcl_printb,
		(ClientData)0, (Tcl_CmdDeleteProc *)NULL);

	Tcl_SetVar(interp, "bu_version", (char *)bu_version+5, TCL_GLOBAL_ONLY);	/* from vers.c */
	Tcl_setVar(interp, "BU_DEBUG_FORMAT", BU_DEBUG_FORMAT, TCL_GLOBAL_ONLY);
	Tcl_LinkVar(interp, "bu_debug", (char *)&bu_debug, TCL_INT );
}
