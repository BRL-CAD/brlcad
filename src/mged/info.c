/*                      I N F O . C
 * BRL-CAD
 *
 * Copyright (c) 2010 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_PWD_H
#  include <pwd.h>
#endif
#include <signal.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"
#include "db.h"

#include "./mged.h"
#include "./sedit.h"
#include "./cmd.h"

int
f_list(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	Tcl_DString ds;
	int ret;
	char **av;
	int i;
	CHECK_DBI_NULL;

	if (argc >= 2) {
	    ret = ged_list(gedp, argc, (const char **)argv);
        } else {
 	   if ((argc == 1) && (state == ST_S_EDIT)) {
	      /*bu_log("%s\n",LAST_SOLID(illump)->d_namep);*/
	      argc = 2;
	      av = (char **)bu_malloc(sizeof(char *)*(argc + 1), "f_list: av");
	      av[0] = argv[0];
              av[1] = LAST_SOLID(illump)->d_namep;
              av[argc] = NULL;
	      ret = ged_list(gedp, argc, (const char **)av);
	      bu_free((genptr_t)av, "f_list: av");
	   } else 
	      ret = ged_list(gedp, argc, (const char **)argv);
        }

        Tcl_DStringInit(&ds);
        Tcl_DStringAppend(&ds, bu_vls_addr(&gedp->ged_result_str), -1);
        Tcl_DStringResult(interp, &ds);

	return TCL_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
