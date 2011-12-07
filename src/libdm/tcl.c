/*                           T C L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file libdm/tcl.c
 *
 * LIBDM's tcl interface.
 *
 */

#include "common.h"

#include <math.h>
#include "bio.h"
#include "tcl.h"
#include "bu.h"
#include "vmath.h"
#include "dm.h"
#include "cmd.h"

/* private headers */
#include "brlcad_version.h"


/* from libdm/query.c */
extern int dm_validXType();
extern char *dm_bestXType();

/* from libdm/dm_obj.c */
extern int Dmo_Init(Tcl_Interp *interp);

/* TODO: this doesn't belong in here, move to a globals.c or eliminate */
int vectorThreshold = 100000;


HIDDEN int
dm_validXType_tcl(void *clientData, int argc, const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    struct bu_vls vls;
    Tcl_Obj *obj;

    bu_vls_init(&vls);

    if (argc != 3) {
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
dm_bestXType_tcl(void *clientData, int argc, const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    Tcl_Obj *obj;
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
    snprintf(buffer, 256, "%s", argv[1]);
    best_dm = dm_bestXType(buffer);
    if (best_dm) {
	Tcl_AppendStringsToObj(obj, best_dm, (char *)NULL);
	Tcl_SetObjResult(interp, obj);
	return TCL_OK;
    }
    return TCL_ERROR;
}


static int
wrapper_func(ClientData data, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct bu_cmdtab *ctp = (struct bu_cmdtab *)data;;

    return ctp->ct_func(interp, argc, argv);
}


static void
register_cmds(Tcl_Interp *interp, struct bu_cmdtab *cmds)
{
    struct bu_cmdtab *ctp = NULL;

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	(void)Tcl_CreateCommand(interp, ctp->ct_name, wrapper_func, (ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
    }
}


int
Dm_Init(void *interp)
{
    static struct bu_cmdtab cmdtab[] = {
	{"dm_validXType",	dm_validXType_tcl},
	{"dm_bestXType",	dm_bestXType_tcl},
	{(char *)0,		(int (*)())0}
    };

    struct bu_vls vls = BU_VLS_INIT_ZERO;

    /* register commands */
    register_cmds(interp, cmdtab);

    bu_vls_strcpy(&vls, "vectorThreshold");
    Tcl_LinkVar(interp, bu_vls_addr(&vls), (char *)&vectorThreshold,
		TCL_LINK_INT);
    bu_vls_free(&vls);

    /* initialize display manager object code */
    Dmo_Init(interp);

    Tcl_PkgProvide(interp,  "Dm", brlcad_version());

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
