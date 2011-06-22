/*                       C M D H I S T . C
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

#include "bu.h"
#include "cmd.h"


/**
 * H I S T O R Y _ R E C O R D
 *
 * Stores the given command with start and finish times in the
 * history vls'es. 'status' is either TCL_OK or TCL_ERROR.
 */
HIDDEN void
_bu_history_record(struct bu_cmdhist_obj *chop, struct bu_vls *cmdp, struct timeval *start, struct timeval *finish, int status)
{
    struct bu_cmdhist *new_hist;
    const char *eol = "\n";

    if (UNLIKELY(BU_STR_EQUAL(bu_vls_addr(cmdp), eol)))
	return;

    new_hist = (struct bu_cmdhist *)bu_malloc(sizeof(struct bu_cmdhist),
					      "mged history");
    bu_vls_init(&new_hist->h_command);
    bu_vls_vlscat(&new_hist->h_command, cmdp);
    new_hist->h_start = *start;
    new_hist->h_finish = *finish;
    new_hist->h_status = status;
    BU_LIST_INSERT(&chop->cho_head.l, &new_hist->l);

    chop->cho_curr = &chop->cho_head;
}


HIDDEN int
_bu_timediff(struct timeval *tvdiff, struct timeval *start, struct timeval *finish)
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


int
bu_cmdhist_history(ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct bu_cmdhist_obj *chop = (struct bu_cmdhist_obj *)clientData;
    FILE *fp;
    int with_delays = 0;
    struct bu_cmdhist *hp, *hp_prev;
    struct bu_vls str;
    struct timeval tvdiff;

    if (argc < 2 || 5 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help history");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
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
		Tcl_AppendResult(interp, "history: -outfile option given more than once\n",
				 (char *)NULL);
		return TCL_ERROR;
	    } else if (argc < 4 || BU_STR_EQUAL(argv[3], delays)) {
		Tcl_AppendResult(interp, "history: I need a file name\n", (char *)NULL);
		return TCL_ERROR;
	    } else {
		fp = fopen(argv[3], "ab+");
		if (UNLIKELY(fp == NULL)) {
		    Tcl_AppendResult(interp, "history: error opening file", (char *)NULL);
		    return TCL_ERROR;
		}
		--argc;
		++argv;
	    }
	} else {
	    Tcl_AppendResult(interp, "Invalid option ", argv[2], "\n", (char *)NULL);
	}
	--argc;
	++argv;
    }

    bu_vls_init(&str);
    for (BU_LIST_FOR(hp, bu_cmdhist, &chop->cho_head.l)) {
	bu_vls_trunc(&str, 0);
	hp_prev = BU_LIST_PREV(bu_cmdhist, &hp->l);
	if (with_delays && BU_LIST_NOT_HEAD(hp_prev, &chop->cho_head.l)) {
	    if (_bu_timediff(&tvdiff, &(hp_prev->h_finish), &(hp->h_start)) >= 0)
		bu_vls_printf(&str, "delay %ld %ld\n", (long)tvdiff.tv_sec,
			      (long)tvdiff.tv_usec);

	}

	if (hp->h_status == TCL_ERROR)
	    bu_vls_printf(&str, "# ");
	bu_vls_vlscat(&str, &(hp->h_command));

	if (fp != NULL)
	    bu_vls_fwrite(fp, &str);
	else
	    Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
    }

    if (fp != NULL)
	fclose(fp);

    return TCL_OK;
}


int
bu_cmdhist_add(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    struct bu_cmdhist_obj *chop = (struct bu_cmdhist_obj *)clientData;
    struct bu_vls vls;
    struct timeval zero;

    if (argc != 3) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib cmdhist_add");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (UNLIKELY(argv[2][0] == '\n' || argv[2][0] == '\0'))
	return TCL_OK;

    bu_vls_init(&vls);
    bu_vls_strcpy(&vls, argv[2]);
    if (argv[2][strlen(argv[2])-1] != '\n')
	bu_vls_putc(&vls, '\n');

    zero.tv_sec = zero.tv_usec = 0L;
    _bu_history_record(chop, &vls, &zero, &zero, TCL_OK);

    bu_vls_free(&vls);
    return TCL_OK;
}


int
bu_cmdhist_prev(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    struct bu_cmdhist_obj *chop = (struct bu_cmdhist_obj *)clientData;
    struct bu_cmdhist *hp;

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    hp = BU_LIST_PLAST(bu_cmdhist, chop->cho_curr);
    if (BU_LIST_NOT_HEAD(hp, &chop->cho_head.l))
	chop->cho_curr = hp;

    Tcl_AppendResult(interp, bu_vls_addr(&chop->cho_curr->h_command), (char *)NULL);
    return TCL_OK;
}


int
bu_cmdhist_curr(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    struct bu_cmdhist_obj *chop = (struct bu_cmdhist_obj *)clientData;

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (BU_LIST_NOT_HEAD(chop->cho_curr, &chop->cho_head.l))
	Tcl_AppendResult(interp, bu_vls_addr(&chop->cho_curr->h_command), (char *)NULL);

    return TCL_OK;
}


int
bu_cmdhist_next(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv)
{
    struct bu_cmdhist_obj *chop = (struct bu_cmdhist_obj *)clientData;

    if (argc != 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib %s", argv[0]);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if (BU_LIST_IS_HEAD(chop->cho_curr, &chop->cho_head.l))
	return TCL_ERROR;

    chop->cho_curr = BU_LIST_PNEXT(bu_cmdhist, chop->cho_curr);
    if (BU_LIST_IS_HEAD(chop->cho_curr, &chop->cho_head.l))
	return TCL_ERROR;

    Tcl_AppendResult(interp, bu_vls_addr(&chop->cho_curr->h_command), (char *)NULL);
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
