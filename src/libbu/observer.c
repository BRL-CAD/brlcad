/*                      O B S E R V E R . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2011 United States Government as represented by
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
#include "cmd.h"                  /* includes bu.h */


/**
 * Attach observer.
 *
 * Usage:
 * attach observer [cmd]
 *
 */
HIDDEN int
observer_attach_tcl(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    struct bu_observer *headp = (struct bu_observer *)clientData;
    struct bu_observer *op;

    if (argc < 2 || 3 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib bu_observer_attach");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* see if it already exists, if so, modify it */
    for (BU_LIST_FOR(op, bu_observer, &headp->l))
	if (BU_STR_EQUAL(bu_vls_addr(&op->observer), argv[1])) {
	    if (argc == 2)
		/* clobber cmd */
		bu_vls_init(&op->cmd);
	    else
		/* overwrite cmd */
		bu_vls_strcpy(&op->cmd, argv[2]);

	    return TCL_OK;
	}

    /* acquire bu_observer struct */
    BU_GETSTRUCT(op, bu_observer);

    /* initialize observer */
    bu_vls_init(&op->observer);
    bu_vls_strcpy(&op->observer, argv[1]);
    bu_vls_init(&op->cmd);

    if (argc == 3)
	bu_vls_strcpy(&op->cmd, argv[2]);

    /* append to list of bu_observer's */
    BU_LIST_APPEND(&headp->l, &op->l);

    return TCL_OK;
}


/**
 * Detach observer.
 *
 * Usage:
 * detach observer
 *
 */
HIDDEN int
observer_detach_tcl(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    struct bu_observer *headp = (struct bu_observer *)clientData;
    struct bu_observer *op;

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib bu_observer_attach");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* search for observer and remove from list */
    for (BU_LIST_FOR(op, bu_observer, &headp->l))
	if (BU_STR_EQUAL(bu_vls_addr(&op->observer), argv[1])) {
	    BU_LIST_DEQUEUE(&op->l);
	    bu_vls_free(&op->observer);
	    bu_vls_free(&op->cmd);
	    bu_free((genptr_t)op, "observer_detach_tcl: op");

	    return TCL_OK;
	}

    Tcl_AppendResult(interp, "detach: ", argv[1], " not found", (char *)NULL);
    return TCL_ERROR;
}


/**
 * Show/list observers.
 *
 * Usage:
 * show
 *
 */
HIDDEN int
observer_show_tcl(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    struct bu_observer *headp = (struct bu_observer *)clientData;
    struct bu_observer *op;

    if (argc != 1) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    for (BU_LIST_FOR(op, bu_observer, &headp->l)) {
	Tcl_AppendResult(interp, bu_vls_addr(&op->observer), " - ",
			 bu_vls_addr(&op->cmd), "\n", (char *)NULL);
    }

    return TCL_OK;
}


void
bu_observer_notify(Tcl_Interp *interp, struct bu_observer *headp, char *self)
{
    struct bu_observer *op;
    struct bu_vls vls;

    bu_vls_init(&vls);
    for (BU_LIST_FOR(op, bu_observer, &headp->l)) {
	if (bu_vls_strlen(&op->cmd) > 0) {
	    /* Execute cmd */
	    bu_vls_strcpy(&vls, bu_vls_addr(&op->cmd));
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	} else {
	    /* Assume that observer is some object that has an update method */
	    bu_vls_trunc(&vls, 0);
	    bu_vls_printf(&vls, "%s update %s", bu_vls_addr(&op->observer), self);
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	}
    }
    bu_vls_free(&vls);
}


void
bu_observer_free(struct bu_observer *headp)
{
    struct bu_observer *op;
    struct bu_observer *nop;

    op = BU_LIST_FIRST(bu_observer, &headp->l);
    while (BU_LIST_NOT_HEAD(op, &headp->l)) {
	nop = BU_LIST_PNEXT(bu_observer, op);
	BU_LIST_DEQUEUE(&op->l);
	bu_vls_free(&op->observer);
	bu_vls_free(&op->cmd);
	bu_free((genptr_t)op, "bu_observer_free: op");
	op = nop;
    }
}


/**
 * observer commands for libdm and librt's dg_obj, view_obj, and
 * wdb_obj interfaces.
 */
static struct bu_cmdtab bu_observer_cmds[] = {
    {"attach",	observer_attach_tcl},
    {"detach",	observer_detach_tcl},
    {"show",	observer_show_tcl},
    {(char *)0,	CMD_NULL}
};


int
bu_observer_cmd(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    return bu_cmd(clientData, interp, argc, argv, bu_observer_cmds, 0);
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
