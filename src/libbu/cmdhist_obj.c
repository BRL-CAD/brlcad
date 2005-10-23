/*                   C M D H I S T _ O B J . C
 * BRL-CAD
 *
 * Copyright (C) 1998-2005 United States Government as represented by
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

/** \addtogroup cmd */
/*@{*/
/** @file cmdhist_obj.c
 * A cmdhist object contains the attributes and
 * methods for maintaining command history.
 *
 *
 *  Author -
 *	  Robert G. Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
/*@}*/

#include "common.h"


#include "tcl.h"
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "machine.h"
#include "cmd.h"

/* bu_cmdhist routines are defined in libbu/cmdhist.c */
extern int bu_cmdhist_history(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
extern int bu_cmdhist_add(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
extern int bu_cmdhist_curr(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
extern int bu_cmdhist_next(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
extern int bu_cmdhist_prev(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);

int cho_open_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);

static struct bu_cmdhist_obj HeadCmdHistObj;		/* head of command history object list */

static struct bu_cmdtab ch_cmds[] =
{
	{"add",		bu_cmdhist_add},
	{"curr",	bu_cmdhist_curr},
	{"next",	bu_cmdhist_next},
	{"prev",	bu_cmdhist_prev},
	{(char *)NULL,	CMD_NULL}
};

int
cho_hist(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	return bu_cmd(clientData, interp, argc, argv, ch_cmds, 1);
}

static struct bu_cmdtab cho_cmds[] =
{
	{"add",		bu_cmdhist_add},
	{"curr",	bu_cmdhist_curr},
	{"history",	bu_cmdhist_history},
	{"next",	bu_cmdhist_next},
	{"prev",	bu_cmdhist_prev},
	{(char *)NULL,	CMD_NULL}
};

static int
cho_cmd(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	return bu_cmd(clientData, interp, argc, argv, cho_cmds, 1);
}

int
Cho_Init(Tcl_Interp *interp)
{
	BU_LIST_INIT(&HeadCmdHistObj.l);
	(void)Tcl_CreateCommand(interp, "ch_open", (Tcl_CmdProc *)cho_open_tcl,
				(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	return TCL_OK;
}

static void
cho_deleteProc(ClientData clientData)
{
	struct bu_cmdhist_obj *chop = (struct  bu_cmdhist_obj *)clientData;
	struct bu_cmdhist *curr, *next;

	/* free list of commands */
	curr = BU_LIST_NEXT(bu_cmdhist, &chop->cho_head.l);
	while(BU_LIST_NOT_HEAD(curr,&chop->cho_head.l)) {
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

#if 0			/* As far as I can tell, this is not used.  CTJ */
/*
 * Close a command history object.
 *
 * USAGE:
 *        procname close
 */
static int
cho_close_tcl(clientData, interp, argc, argv)
     ClientData      clientData;
     Tcl_Interp      *interp;
     int             argc;
     char            **argv;
{
	struct bu_cmdhist_obj *chop = (struct  bu_cmdhist_obj *)clientData;
	struct bu_vls vls;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib cho_close");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* Among other things, this will call cho_deleteProc. */
	Tcl_DeleteCommand(interp, bu_vls_addr(&chop->cho_name));

	return TCL_OK;
}
#endif

static struct bu_cmdhist_obj *
cho_open(ClientData clientData, Tcl_Interp *interp, char *name)
{
	struct bu_cmdhist_obj *chop;

	/* check to see if command history object exists */
	for (BU_LIST_FOR(chop, bu_cmdhist_obj, &HeadCmdHistObj.l)) {
		if (strcmp(name,bu_vls_addr(&chop->cho_name)) == 0) {
			Tcl_AppendResult(interp, "ch_open: ", name,
					 " exists.\n", (char *)NULL);
			return CMDHIST_OBJ_NULL;
		}
	}

	BU_GETSTRUCT(chop, bu_cmdhist_obj);
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

/*
 * Open a command history object.
 *
 * USAGE:
 *        ch_open name
 */
int
cho_open_tcl(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
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
		if ((chop = cho_open(clientData, interp, argv[1])) == CMDHIST_OBJ_NULL)
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
