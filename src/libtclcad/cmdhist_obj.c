/*                   C M D H I S T _ O B J . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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

#include "common.h"

#include <string.h>
#include "bio.h"

#include "tcl.h"
#include "cmd.h"


/* FIXME: this is apparently used by src/tclscripts/lib/Command.tcl so
 * it needs to migrate elsewhere if mged/archer is to continue using
 * it as it doesn't belong in libbu.  if it can be fully decoupled
 * from tcl (ideal), it would belong in libged.  otherwise, it belongs
 * in libtclcad.
 */


static struct bu_cmdhist_obj HeadCmdHistObj;		/* head of command history object list */


HIDDEN int
cho_cmd(ClientData clientData, Tcl_Interp *UNUSED(interp), int argc, const char **argv)
{
    int ret;

    static struct bu_cmdtab cho_cmds[] = {
	{"add",		bu_cmdhist_add},
	{"curr",	bu_cmdhist_curr},
	{"history",	bu_cmdhist_history},
	{"next",	bu_cmdhist_next},
	{"prev",	bu_cmdhist_prev},
	{(char *)NULL,	BU_CMD_NULL}
    };

    if (bu_cmd(cho_cmds, argc, argv, 1, clientData, &ret) == BRLCAD_OK)
	return ret;

    bu_log("ERROR: '%s' command not found\n", argv[1]);
    return BRLCAD_ERROR;
}


HIDDEN void
cho_deleteProc(ClientData clientData)
{
    struct bu_cmdhist_obj *chop = (struct bu_cmdhist_obj *)clientData;
    struct bu_cmdhist *curr, *next;

    /* free list of commands */
    curr = BU_LIST_NEXT(bu_cmdhist, &chop->cho_head.l);
    while (BU_LIST_NOT_HEAD(curr, &chop->cho_head.l)) {
	curr = BU_LIST_NEXT(bu_cmdhist, &chop->cho_head.l);
	next = BU_LIST_PNEXT(bu_cmdhist, curr);

	bu_vls_free(&curr->h_command);

	BU_LIST_DEQUEUE(&curr->l);
	bu_free((genptr_t)curr, "cho_deleteProc: curr");
	curr = next;
    }

    bu_vls_free(&chop->cho_name);
    bu_vls_free(&chop->cho_head.h_command);

    BU_LIST_DEQUEUE(&chop->l);
    bu_free((genptr_t)chop, "cho_deleteProc: chop");
}


HIDDEN struct bu_cmdhist_obj *
cho_open(ClientData UNUSED(clientData), Tcl_Interp *interp, const char *name)
{
    struct bu_cmdhist_obj *chop;

    /* check to see if command history object exists */
    for (BU_LIST_FOR(chop, bu_cmdhist_obj, &HeadCmdHistObj.l)) {
	if (BU_STR_EQUAL(name, bu_vls_addr(&chop->cho_name))) {
	    Tcl_AppendResult(interp, "ch_open: ", name,
			     " exists.\n", (char *)NULL);
	    return BU_CMDHIST_OBJ_NULL;
	}
    }

    BU_GET(chop, struct bu_cmdhist_obj);
    bu_vls_init(&chop->cho_name);
    bu_vls_strcpy(&chop->cho_name, name);
    BU_LIST_INIT(&chop->cho_head.l);
    bu_vls_init(&chop->cho_head.h_command);
    chop->cho_head.h_start.tv_sec = chop->cho_head.h_start.tv_usec =
	chop->cho_head.h_finish.tv_sec = chop->cho_head.h_finish.tv_usec = 0L;
    chop->cho_head.h_status = TCL_OK;
    chop->cho_curr = &chop->cho_head;

    BU_LIST_APPEND(&HeadCmdHistObj.l, &chop->l);
    return chop;
}


int
cho_open_tcl(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    struct bu_cmdhist_obj *chop;
    struct bu_vls vls;

    if (argc == 1) {
	/* get list of command history objects */
	for (BU_LIST_FOR(chop, bu_cmdhist_obj, &HeadCmdHistObj.l))
	    Tcl_AppendResult(interp, bu_vls_addr(&chop->cho_name), " ", (char *)NULL);

	return TCL_OK;
    }

    if (argc == 2) {
	if ((chop = cho_open(clientData, interp, argv[1])) == BU_CMDHIST_OBJ_NULL)
	    return TCL_ERROR;

	(void)Tcl_CreateCommand(interp,
				bu_vls_addr(&chop->cho_name),
				(Tcl_CmdProc *)cho_cmd,
				(ClientData)chop,
				cho_deleteProc);

	/* Return new function name as result */
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, bu_vls_addr(&chop->cho_name), (char *)NULL);
	return TCL_OK;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib ch_open");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


int
Cho_Init(Tcl_Interp *interp)
{
    BU_LIST_INIT(&HeadCmdHistObj.l);
    (void)Tcl_CreateCommand(interp, "ch_open", (Tcl_CmdProc *)cho_open_tcl,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

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
