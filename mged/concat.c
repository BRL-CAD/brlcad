/*
 *			C O N C A T . C
 *
 *  Functions -
 *	f_dup()		checks for dup names before cat'ing of two files
 *	f_concat()	routine to cat another GED file onto end of current file
 *
 *  Authors -
 *	Michael John Muuss
 *	Keith A. Applin
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
static const char RCSconcat[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <pwd.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./sedit.h"

char	new_name[NAMESIZE];
char	prestr[NAMESIZE];
int	ncharadd;

int invoke_db_wrapper(Tcl_Interp *interp, int argc, char **argv);

/*
 *
 *			F _ D U P ( )
 *
 *  Check for duplicate names in preparation for cat'ing of files
 *
 *  Usage:  dup file.g [prefix]
 *  becomes: db dup file.g [prefix]
 */
int
f_dup(clientData, interp, argc, argv )
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
{
	CHECK_DBI_NULL;

	return invoke_db_wrapper(interp, argc, argv);
}

/*
 *			I N V O K E _ D B _ W R A P P E R
 *
 *  This is generally useful for all MGED commands
 *  that have been subsumed by "db" object methods.
 */
int
invoke_db_wrapper(Tcl_Interp	*interp,
		  int		argc,
		  char		**argv)
{
	register int	i;
	struct bu_vls	vls;
	int		ret;

	bu_vls_init(&vls);

	bu_vls_strcpy(&vls, MGED_DB_NAME);
	for (i = 0; i < argc; ++i)
		bu_vls_printf(&vls, " %s", argv[i]);

	if (bu_debug)
		bu_log("%s\n", bu_vls_addr(&vls));

	ret = Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	return ret;
}

/*
 *			F _ C O N C A T
 *
 *  Concatenate another GED file into the current file.
 *  Interrupts are not permitted during this function.
 *
 *  Usage:  dbconcat file.g [prefix]
 *  becomes: db concat file.g prefix
 *
 *  NOTE:  If a prefix is not given on the command line,
 *  then the users insist that they be prompted for the prefix,
 *  to prevent inadvertently sucking in a non-prefixed file.
 *  Slash ("/") specifies no prefix.
 */
int
f_concat(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
{
	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	/* get any prefix */
	if (argc < 3) {
		Tcl_AppendResult(interp, MORE_ARGS_STR,
				 "concat: Enter prefix string or / for no prefix: ",
				 (char *)NULL);
		return TCL_ERROR;
	}

	return invoke_db_wrapper(interp, argc, argv);
}
