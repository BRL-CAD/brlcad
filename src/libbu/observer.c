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
observer_attach(void *clientData, int argc, const char **argv)
{
    struct bu_observer *headp = (struct bu_observer *)clientData;
    struct bu_observer *op;

    if (argc < 2 || 3 < argc) {
	bu_log("ERROR: expecting only three arguments\n");
	return BRLCAD_ERROR;
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

	    return BRLCAD_OK;
	}

    /* acquire bu_observer struct */
    BU_GET(op, struct bu_observer);

    /* initialize observer */
    bu_vls_init(&op->observer);
    bu_vls_strcpy(&op->observer, argv[1]);
    bu_vls_init(&op->cmd);

    if (argc == 3)
	bu_vls_strcpy(&op->cmd, argv[2]);

    /* append to list of bu_observer's */
    BU_LIST_APPEND(&headp->l, &op->l);

    return BRLCAD_OK;
}


/**
 * Detach observer.
 *
 * Usage:
 * detach observer
 *
 */
HIDDEN int
observer_detach(void *clientData, int argc, const char **argv)
{
    struct bu_observer *headp = (struct bu_observer *)clientData;
    struct bu_observer *op;

    if (argc != 2) {
	bu_log("ERROR: expecting two arguments\n");
	return BRLCAD_ERROR;
    }

    /* search for observer and remove from list */
    for (BU_LIST_FOR(op, bu_observer, &headp->l))
	if (BU_STR_EQUAL(bu_vls_addr(&op->observer), argv[1])) {
	    BU_LIST_DEQUEUE(&op->l);
	    bu_vls_free(&op->observer);
	    bu_vls_free(&op->cmd);
	    bu_free((genptr_t)op, "observer_detach: op");

	    return BRLCAD_OK;
	}

    bu_log("detach: %s not found", argv[1]);
    return BRLCAD_ERROR;
}


/**
 * Show/list observers.
 *
 * Usage:
 * show
 *
 */
HIDDEN int
observer_show(void *clientData, int argc, const char **UNUSED(argv))
{
    struct bu_observer *headp = (struct bu_observer *)clientData;
    struct bu_observer *op;

    if (argc != 1) {
	bu_log("ERROR: expecting only one argument\n");
	return BRLCAD_ERROR;
    }

    for (BU_LIST_FOR(op, bu_observer, &headp->l)) {
	bu_log("%s - %s\n", bu_vls_addr(&op->observer), bu_vls_addr(&op->cmd));
    }

    return BRLCAD_OK;
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
    {"attach",	observer_attach},
    {"detach",	observer_detach},
    {"show",	observer_show},
    {(char *)0,	CMD_NULL}
};


int
bu_observer_cmd(void *clientData, int argc, const char **argv)
{
    int ret;
    if (bu_cmd(bu_observer_cmds, argc, argv, 0, clientData, &ret) == BRLCAD_OK)
	return ret;

    bu_log("ERROR: '%s' command not found\n", argv[0]);
    return BRLCAD_ERROR;
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
