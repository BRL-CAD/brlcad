/*                      O B S E R V E R . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup butcl */
/*@{*/

/** @file observer.c
 * @brief Routines for implementing the observer pattern.
 *
 * @par Source
 *	SLAD CAD Team
 * @n	The U. S. Army Research Laboratory
 * @n	Aberdeen Proving Ground, Maryland  21005
 *
 * @author
 *	Robert G. Parker
 */


#include "common.h"


#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "tcl.h"
#include "machine.h"
#include "cmd.h"                  /* includes bu.h */

static int bu_observer_attach_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int bu_observer_detach_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
static int bu_observer_show_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);

struct bu_cmdtab bu_observer_cmds[] = {
	{"attach",	bu_observer_attach_tcl},
	{"detach",	bu_observer_detach_tcl},
	{"show",	bu_observer_show_tcl},
	{(char *)0,	(int (*)())0}
};

/**
 * Attach observer.
 *
 * Usage:
 *	  attach observer [cmd]
 *
 */
static int
bu_observer_attach_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
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
		if (strcmp(bu_vls_addr(&op->observer), argv[1]) == 0) {
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
 *	  detach observer
 *
 */
static int
bu_observer_detach_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
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
		if (strcmp(bu_vls_addr(&op->observer), argv[1]) == 0) {
			BU_LIST_DEQUEUE(&op->l);
			bu_vls_free(&op->observer);
			bu_vls_free(&op->cmd);
			bu_free((genptr_t)op, "bu_observer_detach_tcl: op");

			return TCL_OK;
		}

	Tcl_AppendResult(interp, "detach: ", argv[1], " not found", (char *)NULL);
	return TCL_ERROR;
}

/**
 * Show/list observers.
 *
 * Usage:
 *	  show
 *
 */
static int
bu_observer_show_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct bu_observer *headp = (struct bu_observer *)clientData;
	struct bu_observer *op;

	for (BU_LIST_FOR(op, bu_observer, &headp->l)) {
		Tcl_AppendResult(interp, bu_vls_addr(&op->observer), " - ",
				 bu_vls_addr(&op->cmd), "\n", (char *)NULL);
	}

	return TCL_OK;
}

/**
 * Notify observers.
 */
void
bu_observer_notify(Tcl_Interp *interp, struct bu_observer *headp, char *this)
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
			bu_vls_printf(&vls, "%s update %s", bu_vls_addr(&op->observer), this);
			Tcl_Eval(interp, bu_vls_addr(&vls));
		}
	}
	bu_vls_free(&vls);
}

/**
 * Free observers.
 */
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

/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
