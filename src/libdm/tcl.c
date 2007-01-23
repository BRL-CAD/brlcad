/*                           T C L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file tcl.c
 *
 * LIBDM's tcl interface.
 *
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *	Robert G. Parker
 */
#include "common.h"


#include <math.h>
#include "tcl.h"
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "dm.h"
#include "cmd.h"

/* from libdm/query.c */
extern int dm_validXType();
extern char *dm_bestXType();

/* from libdm/dm_obj.c */
extern int Dmo_Init(Tcl_Interp *interp);

HIDDEN int dm_validXType_tcl();
HIDDEN int dm_bestXType_tcl();

int vectorThreshold = 100000;

HIDDEN struct bu_cmdtab cmdtab[] = {
	{"dm_validXType",	dm_validXType_tcl},
	{"dm_bestXType",	dm_bestXType_tcl},
	{(char *)0,		(int (*)())0}
};

int
#ifdef BRLCAD_DEBUG
Dm_d_Init(Tcl_Interp *interp)
#else
Dm_Init(Tcl_Interp *interp)
#endif
{
	const char		*version_number;
	struct bu_vls	vls;

	/* register commands */
	bu_register_cmds(interp, cmdtab);

	bu_vls_init(&vls);
	bu_vls_strcpy(&vls, "vectorThreshold");
	Tcl_LinkVar(interp, bu_vls_addr(&vls), (char *)&vectorThreshold,
		    TCL_LINK_INT);
	bu_vls_free(&vls);

	/* initialize display manager object code */
	Dmo_Init(interp);

	Tcl_SetVar(interp, "dm_version", (char *)dm_version+5, TCL_GLOBAL_ONLY);
	Tcl_Eval(interp, "lindex $dm_version 2");
	version_number = Tcl_GetStringResult(interp);
	Tcl_PkgProvide(interp,  "Dm", version_number);

	return TCL_OK;
}

HIDDEN int
dm_validXType_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct bu_vls	vls;
	Tcl_Obj		*obj;

	bu_vls_init(&vls);

	if(argc != 3){
		bu_vls_printf(&vls, "helplib dm_validXType");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_printf(&vls, "%d", dm_validXType(argv[1], argv[2]));
	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);
	Tcl_AppendStringsToObj(obj, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	Tcl_SetObjResult(interp, obj);
	return TCL_OK;
}

HIDDEN int
dm_bestXType_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	Tcl_Obj		*obj;
	const char *best_dm;
	char buffer[256] = {0};

	if (argc != 2) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib dm_bestXType");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
		obj = Tcl_DuplicateObj(obj);
	snprintf(buffer, 256, argv[1]);
	best_dm = dm_bestXType(buffer);
	if (best_dm) {
	    Tcl_AppendStringsToObj(obj, best_dm, (char *)NULL);
	    Tcl_SetObjResult(interp, obj);
	    return TCL_OK;
	}
	return TCL_ERROR;
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
