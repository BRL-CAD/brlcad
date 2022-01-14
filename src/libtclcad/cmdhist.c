/*                       C M D H I S T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2022 United States Government as represented by
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

#include "bsocket.h" /* for timeval */
#include "bio.h"

#include "tcl.h"
#include "bu/cmd.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "tclcad.h"

#include "./tclcad_private.h"


/* FIXME: this is apparently used by src/tclscripts/lib/Command.tcl so
 * it needs to migrate elsewhere if mged/archer is to continue using
 * it as it doesn't belong in libbu.  if it can be fully decoupled
 * from tcl (ideal), it would belong in libged.  otherwise, it belongs
 * in libtclcad.
 */

struct tclcad_cmdhist {
    struct bu_list l;
    struct bu_vls h_command;
    struct timeval h_start;
    struct timeval h_finish;
    int h_status;
};
#define TCLCAD_CMDHIST_NULL (struct tclcad_cmdhist *)NULL

struct tclcad_cmdhist_obj {
    struct bu_list l;
    struct bu_vls cho_name;
    struct tclcad_cmdhist cho_head;
    struct tclcad_cmdhist *cho_curr;
};
#define TCLCAD_CMDHIST_OBJ_NULL (struct tclcad_cmdhist_obj *)NULL

static struct tclcad_cmdhist_obj HeadCmdHistObj;		/* head of command history object list */

/**
 * Stores the given command with start and finish times in the
 * history vls'es. 'status' is either BRLCAD_OK or BRLCAD_ERROR.
 */
HIDDEN void
_tclcad_cmdhist_record(struct tclcad_cmdhist_obj *chop, struct bu_vls *cmdp, struct timeval *start, struct timeval *finish, int status)
{
    struct tclcad_cmdhist *new_hist;
    const char *eol = "\n";

    if (UNLIKELY(BU_STR_EQUAL(bu_vls_addr(cmdp), eol)))
	return;

    BU_ALLOC(new_hist, struct tclcad_cmdhist);
    bu_vls_init(&new_hist->h_command);
    bu_vls_vlscat(&new_hist->h_command, cmdp);
    new_hist->h_start = *start;
    new_hist->h_finish = *finish;
    new_hist->h_status = status;
    BU_LIST_INSERT(&chop->cho_head.l, &new_hist->l);

    chop->cho_curr = &chop->cho_head;
}


HIDDEN int
_tclcad_cmdhist_timediff(struct timeval *tvdiff, struct timeval *start, struct timeval *finish)
{
    if (UNLIKELY(finish->tv_sec == 0 && finish->tv_usec == 0))
	return -1;
    if (UNLIKELY(start->tv_sec == 0 && start->tv_usec == 0))
	return -1;

    tvdiff->tv_sec = finish->tv_sec - start->tv_sec;
    tvdiff->tv_usec = finish->tv_usec - start->tv_usec;
    if (tvdiff->tv_usec < 0) {
	--tvdiff->tv_sec;
	tvdiff->tv_usec += 1000000L;
    }

    return 0;
}


HIDDEN int
tclcad_cmdhist_history(void *data, int argc, const char *argv[])
{
    FILE *fp;
    int with_delays = 0;
    struct tclcad_cmdhist *hp, *hp_prev;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct timeval tvdiff;
    struct tclcad_cmdhist_obj *chop = (struct tclcad_cmdhist_obj *)data;

    if (argc < 2 || 5 < argc) {
	bu_log("Usage: %s -delays\nList command history.\n", argv[0]);
	return BRLCAD_ERROR;
    }

    fp = NULL;
    while (argc >= 3) {
	const char *delays = "-delays";
	const char *outfile = "-outfile";

	if (BU_STR_EQUAL(argv[2], delays))
	    with_delays = 1;
	else if (BU_STR_EQUAL(argv[2], outfile)) {
	    if (fp != NULL) {
		fclose(fp);
		bu_log("%s: -outfile option given more than once\n", argv[0]);
		return BRLCAD_ERROR;
	    } else if (argc < 4 || BU_STR_EQUAL(argv[3], delays)) {
		bu_log("%s: I need a file name\n", argv[0]);
		return BRLCAD_ERROR;
	    } else {
		fp = fopen(argv[3], "ab+");
		if (UNLIKELY(fp == NULL)) {
		    bu_log("%s: error opening file", argv[0]);
		    return BRLCAD_ERROR;
		}
		--argc;
		++argv;
	    }
	} else {
	    bu_log("Invalid option %s\n", argv[2]);
	}
	--argc;
	++argv;
    }

    for (BU_LIST_FOR(hp, tclcad_cmdhist, &chop->cho_head.l)) {
	bu_vls_trunc(&str, 0);
	hp_prev = BU_LIST_PREV(tclcad_cmdhist, &hp->l);
	if (with_delays && BU_LIST_NOT_HEAD(hp_prev, &chop->cho_head.l)) {
	    if (_tclcad_cmdhist_timediff(&tvdiff, &(hp_prev->h_finish), &(hp->h_start)) >= 0)
		bu_vls_printf(&str, "delay %ld %ld\n", (long)tvdiff.tv_sec,
			      (long)tvdiff.tv_usec);

	}

	if (hp->h_status == BRLCAD_ERROR)
	    bu_vls_printf(&str, "# ");
	bu_vls_vlscat(&str, &(hp->h_command));

	if (fp != NULL)
	    bu_vls_fwrite(fp, &str);
	else
	    bu_log("%s\n", bu_vls_addr(&str));
    }

    if (fp != NULL)
	fclose(fp);

    return BRLCAD_OK;
}


HIDDEN int
tclcad_cmdhist_add(void *clientData, int argc, const char **argv)
{
    struct tclcad_cmdhist_obj *chop = (struct tclcad_cmdhist_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    struct timeval zero;

    if (argc != 3) {
	bu_log("ERROR: expecting only three arguments\n");
	return BRLCAD_ERROR;
    }

    if (UNLIKELY(argv[2][0] == '\n' || argv[2][0] == '\0'))
	return BRLCAD_OK;

    bu_vls_strcpy(&vls, argv[2]);
    if (argv[2][strlen(argv[2])-1] != '\n')
	bu_vls_putc(&vls, '\n');

    zero.tv_sec = zero.tv_usec = 0L;
    _tclcad_cmdhist_record(chop, &vls, &zero, &zero, BRLCAD_OK);

    bu_vls_free(&vls);

    /* newly added command is in chop->cho_curr */
    return BRLCAD_OK;
}


HIDDEN int
tclcad_cmdhist_prev(void *clientData, int argc, const char **UNUSED(argv))
{
    struct tclcad_cmdhist_obj *chop = (struct tclcad_cmdhist_obj *)clientData;
    struct tclcad_cmdhist *hp;

    if (argc != 2) {
	bu_log("ERROR: expecting only two arguments\n");
	return BRLCAD_ERROR;
    }

    hp = BU_LIST_PLAST(tclcad_cmdhist, chop->cho_curr);
    if (BU_LIST_NOT_HEAD(hp, &chop->cho_head.l))
	chop->cho_curr = hp;

    /* result is in chop->cho_curr */
    return BRLCAD_OK;
}


HIDDEN int
tclcad_cmdhist_curr(void *clientData, int argc, const char **UNUSED(argv))
{
    struct tclcad_cmdhist_obj *chop = (struct tclcad_cmdhist_obj *)clientData;

    if (argc != 2) {
	bu_log("ERROR: expecting only two arguments\n");
	return BRLCAD_ERROR;
    }

    if (BU_LIST_NOT_HEAD(chop->cho_curr, &chop->cho_head.l)) {
	/* result is in chop->cho_curr */
	return BRLCAD_OK;
    }

    /* no commands exist yet */
    return BRLCAD_ERROR;
}


HIDDEN int
tclcad_cmdhist_next(void *clientData, int argc, const char **UNUSED(argv))
{
    struct tclcad_cmdhist_obj *chop = (struct tclcad_cmdhist_obj *)clientData;

    if (argc != 2) {
	bu_log("ERROR: expecting only two arguments\n");
	return BRLCAD_ERROR;
    }

    if (BU_LIST_IS_HEAD(chop->cho_curr, &chop->cho_head.l))
	return BRLCAD_ERROR;

    chop->cho_curr = BU_LIST_PNEXT(tclcad_cmdhist, chop->cho_curr);
    if (BU_LIST_IS_HEAD(chop->cho_curr, &chop->cho_head.l))
	return BRLCAD_ERROR;

    /* result is in chop->cho_curr */
    return BRLCAD_OK;
}




HIDDEN int
cho_cmd(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    int ret;

    static struct bu_cmdtab cho_cmds[] = {
	{"add",		tclcad_cmdhist_add},
	{"curr",	tclcad_cmdhist_curr},
	{"history",	tclcad_cmdhist_history},
	{"next",	tclcad_cmdhist_next},
	{"prev",	tclcad_cmdhist_prev},
	{(const char *)NULL, BU_CMD_NULL}
    };

    if (bu_cmd(cho_cmds, argc, argv, 1, clientData, &ret) == BRLCAD_OK) {
	if (ret == BRLCAD_OK)
	    Tcl_AppendResult(interp, bu_vls_addr(&((struct tclcad_cmdhist_obj *)clientData)->cho_curr->h_command), NULL);
	return ret;
    }

    bu_log("ERROR: '%s' command not found\n", argv[1]);
    return BRLCAD_ERROR;
}


HIDDEN void
cho_deleteProc(ClientData clientData)
{
    struct tclcad_cmdhist_obj *chop = (struct tclcad_cmdhist_obj *)clientData;
    struct tclcad_cmdhist *curr, *next;

    /* free list of commands */
    curr = BU_LIST_NEXT(tclcad_cmdhist, &chop->cho_head.l);
    while (BU_LIST_NOT_HEAD(curr, &chop->cho_head.l)) {
	curr = BU_LIST_NEXT(tclcad_cmdhist, &chop->cho_head.l);
	next = BU_LIST_PNEXT(tclcad_cmdhist, curr);

	bu_vls_free(&curr->h_command);

	BU_LIST_DEQUEUE(&curr->l);
	bu_free((void *)curr, "cho_deleteProc: curr");
	curr = next;
    }

    bu_vls_free(&chop->cho_name);
    bu_vls_free(&chop->cho_head.h_command);

    BU_LIST_DEQUEUE(&chop->l);
    BU_PUT(chop, struct tclcad_cmdhist_obj);
}


HIDDEN struct tclcad_cmdhist_obj *
cho_open(ClientData UNUSED(clientData), Tcl_Interp *interp, const char *name)
{
    struct tclcad_cmdhist_obj *chop;

    /* check to see if command history object exists */
    for (BU_LIST_FOR(chop, tclcad_cmdhist_obj, &HeadCmdHistObj.l)) {
	if (BU_STR_EQUAL(name, bu_vls_addr(&chop->cho_name))) {
	    Tcl_AppendResult(interp, "ch_open: ", name,
			     " exists.\n", (char *)NULL);
	    return TCLCAD_CMDHIST_OBJ_NULL;
	}
    }

    BU_GET(chop, struct tclcad_cmdhist_obj);
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
    struct tclcad_cmdhist_obj *chop;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc == 1) {
	/* get list of command history objects */
	for (BU_LIST_FOR(chop, tclcad_cmdhist_obj, &HeadCmdHistObj.l))
	    Tcl_AppendResult(interp, bu_vls_addr(&chop->cho_name), " ", (char *)NULL);

	return TCL_OK;
    }

    if (argc == 2) {
	if ((chop = cho_open(clientData, interp, argv[1])) == TCLCAD_CMDHIST_OBJ_NULL)
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

    bu_vls_printf(&vls, "helplib ch_open");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
}


int
Cho_Init(Tcl_Interp *interp)
{
    memset(&HeadCmdHistObj, 0, sizeof(struct tclcad_cmdhist_obj));
    BU_LIST_INIT(&HeadCmdHistObj.l);
    BU_VLS_INIT(&HeadCmdHistObj.cho_name);
    /* cho_head already zero'd */
    HeadCmdHistObj.cho_curr = NULL;

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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
