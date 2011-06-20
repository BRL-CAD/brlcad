/*                           C M D . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
 *
 */
/** @file bwish/cmd.c
 *
 * This is the place where BWISH/BTCLSH's commands live.  The history
 * routines were borrowed from mged/history.c and modified for use in
 * this application.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#ifdef BWISH
#  include "tk.h"
#else
#  include "tcl.h"
#endif

#include "cmd.h"
#include "libtermio.h"


/* defined in tcl.c */
extern void Cad_Exit(int status);

HIDDEN struct bu_cmdhist histHead;
HIDDEN struct bu_cmdhist *currHist;


/***************************** BWISH/BTCLSH COMMANDS *****************************/

HIDDEN int
cmd_quit(ClientData UNUSED(clientData),
	 Tcl_Interp *UNUSED(interp),
	 int argc,
	 char **argv)
{
    int status;

    if (argc == 2)
	status = atoi(argv[1]);
    else
	status = 0;

    Cad_Exit(status);

    /* NOT REACHED */
    return TCL_OK;
}


/***************************** BWISH/BTCLSH COMMAND HISTORY *****************************/

HIDDEN int historyInitialized=0;
HIDDEN void
historyInit(void)
{
    BU_LIST_INIT(&(histHead.l));
    bu_vls_init(&(histHead.h_command));
    histHead.h_start.tv_sec = histHead.h_start.tv_usec =
	histHead.h_finish.tv_sec = histHead.h_finish.tv_usec = 0L;
    histHead.h_status = TCL_OK;
    currHist = &histHead;
    historyInitialized=1;
}


/*
 * H I S T O R Y _ R E C O R D
 *
 * Stores the given command with start and finish times in the history
 * vls'es.
 *
 * status is either TCL_OK or TCL_ERROR.
 */
void
history_record_priv(struct bu_vls *cmdp, struct timeval *start, struct timeval *finish, int status)
{
    struct bu_cmdhist *new_hist;

    if (BU_STR_EQUAL(bu_vls_addr(cmdp), "\n"))
	return;

    new_hist = (struct bu_cmdhist *)bu_malloc(sizeof(struct bu_cmdhist),
					      "mged history");
    bu_vls_init(&(new_hist->h_command));
    bu_vls_vlscat(&(new_hist->h_command), cmdp);
    new_hist->h_start = *start;
    new_hist->h_finish = *finish;
    new_hist->h_status = status;

    /* make sure list is initialized before attempting to add entry */
    if (!historyInitialized) {
	historyInit();
    }

    BU_LIST_INSERT(&(histHead.l), &(new_hist->l));

    /* As long as this isn't our first command to record after setting
     * up the journal (which would be "journal", which we don't want
     * recorded!)...
     */


    currHist = &histHead;
}


HIDDEN int
timediff(struct timeval *tvdiff, struct timeval *start, struct timeval *finish)
{
    if (finish->tv_sec == 0 && finish->tv_usec == 0)
	return -1;
    if (start->tv_sec == 0 && start->tv_usec == 0)
	return -1;

    tvdiff->tv_sec = finish->tv_sec - start->tv_sec;
    tvdiff->tv_usec = finish->tv_usec - start->tv_usec;
    if (tvdiff->tv_usec < 0) {
	--tvdiff->tv_sec;
	tvdiff->tv_usec += 1000000L;
    }

    return 0;
}


/*
 * F _ H I S T O R Y
 *
 * Prints out the command history, either to bu_log or to a file.
 */
int
cmd_history(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    FILE *fp;
    int with_delays = 0;
    struct bu_cmdhist *hp, *hp_prev;
    struct bu_vls str;
    struct timeval tvdiff;

    if (argc < 1 || 4 < argc) {
	Tcl_AppendResult(interp, "history [-delays] [-outfile file]\n\tlist command history", (char *)0);
	return TCL_ERROR;
    }

    fp = NULL;
    while (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-delays"))
	    with_delays = 1;
	else if (BU_STR_EQUAL(argv[1], "-outfile")) {
	    if (fp != NULL) {
		fclose(fp);
		Tcl_AppendResult(interp, "history: -outfile option given more than once\n",
				 (char *)NULL);
		return TCL_ERROR;
	    } else if (argc < 3 || BU_STR_EQUAL(argv[2], "-delays")) {
		Tcl_AppendResult(interp, "history: I need a file name\n", (char *)NULL);
		return TCL_ERROR;
	    } else {
		fp = fopen(argv[2], "a+");
		if (fp == NULL) {
		    Tcl_AppendResult(interp, "history: error opening file", (char *)NULL);
		    return TCL_ERROR;
		}
		--argc;
		++argv;
	    }
	} else {
	    Tcl_AppendResult(interp, "Invalid option ", argv[1], "\n", (char *)NULL);
	}

	--argc;
	++argv;
    }

    bu_vls_init(&str);
    for (BU_LIST_FOR(hp, bu_cmdhist, &(histHead.l))) {
	bu_vls_trunc(&str, 0);
	hp_prev = BU_LIST_PREV(bu_cmdhist, &(hp->l));
	if (with_delays && BU_LIST_NOT_HEAD(hp_prev, &(histHead.l))) {
	    if (timediff(&tvdiff, &(hp_prev->h_finish), &(hp->h_start)) >= 0)
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


/**
 * H I S T O R Y _ P R E V
 */
struct bu_vls *
history_prev(void)
{
    struct bu_cmdhist *hp;

    hp = BU_LIST_PREV(bu_cmdhist, &(currHist->l));
    if (BU_LIST_IS_HEAD(hp, &(histHead.l)))
	return NULL;
    else {
	currHist = hp;
	return &(hp->h_command);
    }
}


/**
 * H I S T O R Y _ C U R
 */
struct bu_vls *
history_cur(void)
{
    if (BU_LIST_IS_HEAD(currHist, &(histHead.l)))
	return NULL;
    else
	return &(currHist->h_command);
}


/**
 * H I S T O R Y _ N E X T
 */
struct bu_vls *
history_next(void)
{
    struct bu_cmdhist *hp;

    if (BU_LIST_IS_HEAD(currHist, &(histHead.l))) {
	return 0;
    }

    hp = BU_LIST_NEXT(bu_cmdhist, &(currHist->l));
    if (BU_LIST_IS_HEAD(hp, &(histHead.l))) {
	currHist = hp;
	return 0;
    } else {
	currHist = hp;
	return &(hp->h_command);
    }
}


int
cmd_hist(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls *vp;
    struct bu_vls vls;

    bu_vls_init(&vls);

    if (argc < 2) {
	Tcl_AppendResult(interp, "hist command\n\troutine for maintaining command history", (char *)0);
	return TCL_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "add")) {
	struct timeval zero;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "hist add command\n\tadd command to history", (char *)0);
	    return TCL_ERROR;
	}

	if (argv[2][0] == '\n' || argv[2][0] == '\0')
	    return TCL_OK;

	bu_vls_strcpy(&vls, argv[2]);
	if (argv[2][strlen(argv[2])-1] != '\n')
	    bu_vls_putc(&vls, '\n');

	zero.tv_sec = zero.tv_usec = 0L;
	history_record_priv(&vls, &zero, &zero, TCL_OK);

	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "next")) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "hist next\n\treturn next command in history", (char *)0);
	    return TCL_ERROR;
	}

	vp = history_next();
	if (vp == NULL)
	    return TCL_ERROR;

	Tcl_AppendResult(interp, bu_vls_addr(vp), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[1], "prev")) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "hist prev\n\treturn previous command in history", (char *)0);
	    return TCL_ERROR;
	}

	vp = history_prev();
	if (vp == NULL)
	    return TCL_ERROR;

	Tcl_AppendResult(interp, bu_vls_addr(vp), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
    }

    Tcl_AppendResult(interp, "hist command\n\troutine for maintaining command history", (char *)0);
    return TCL_ERROR;
}


HIDDEN struct bu_cmdtab bwish_cmds[] =
{
    {"exit",		cmd_quit},
    {"history",		cmd_history},
    {"hist",		cmd_hist},
    {"q",		cmd_quit},
    {(char *)NULL,	CMD_NULL}
};


#ifdef BWISH
/* structure provided in libtclcad. provides -format pix-n-w support.
 * doesn't really seem to be used anywhere except here.
 */
extern Tk_PhotoImageFormat tkImgFmtPIX;
#endif

int
cmdInit(Tcl_Interp *interp)
{
    /* Register bwish/btclsh commands */
    bu_register_cmds(interp, bwish_cmds);

#ifdef BWISH
    /* Add pix format for images */
    Tk_CreatePhotoImageFormat(&tkImgFmtPIX);
#endif

    /* initialize command history */
    historyInit();
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
