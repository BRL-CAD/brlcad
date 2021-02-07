/*                           C M D . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2021 United States Government as represented by
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

#ifndef HAVE_WINDOWS_H

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#ifdef BWISH
#  include "tk.h"
#else
#  include "tcl.h"
#endif

#include "bu/cmd.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "libtermio.h"

struct bu_cmdhist {
    struct bu_vls h_command;
    struct timeval h_start;
    struct timeval h_finish;
    int h_status;
};
#define BU_CMDHIST_NULL (struct bu_cmdhist *)NULL

struct bu_cmdhist_list {
    size_t size, capacity;
    size_t current;
    struct bu_cmdhist *cmdhist;
};

struct bu_cmdhist_obj {
    struct bu_vls cho_name;
    struct bu_cmdhist_list cmdhist;
};
#define BU_CMDHIST_OBJ_NULL (struct bu_cmdhist_obj *)NULL

#define BU_CMDHIST_LIST_INIT_CAPACITY 8

/* defined in tcl.c */
extern void Cad_Exit(int status);

HIDDEN struct bu_cmdhist_list currHist;


/***************************** BWISH/BTCLSH COMMANDS *****************************/

HIDDEN int
cmd_quit(void *UNUSED(clientData),
	 int argc,
	 const char **argv)
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
    if (currHist.capacity == 0) {
	currHist.capacity = BU_CMDHIST_LIST_INIT_CAPACITY;
	currHist.cmdhist = (struct bu_cmdhist *)bu_malloc(
		sizeof (struct bu_cmdhist) * currHist.capacity,
		"init chop");
    } else if (currHist.size == currHist.capacity) {
	currHist.capacity *= 2;
	currHist.cmdhist = (struct bu_cmdhist *)bu_realloc(
		currHist.cmdhist,
		sizeof (struct bu_cmdhist_obj) * currHist.capacity,
		"init chop");
    }
}


/*
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

    if (!historyInitialized) {
	historyInit();
    }

    new_hist = &currHist.cmdhist[currHist.size];

    bu_vls_init(&(new_hist->h_command));
    bu_vls_vlscat(&(new_hist->h_command), cmdp);
    new_hist->h_start = *start;
    new_hist->h_finish = *finish;
    new_hist->h_status = status;

    /* As long as this isn't our first command to record after setting
     * up the journal (which would be "journal", which we don't want
     * recorded!)...
     */

    currHist.current = currHist.size;
    currHist.size++;
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
 * Prints out the command history, either to bu_log or to a file.
 */
int
cmd_history(void *clientData, int argc, const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    FILE *fp;
    int with_delays = 0;
    struct bu_cmdhist *hp, *hp_prev;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct timeval tvdiff;
    size_t i;

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

    for (i = 1; i < currHist.size; i++) {
	    hp = &currHist.cmdhist[i];
	    hp_prev = &currHist.cmdhist[i - 1];
	bu_vls_trunc(&str, 0);
	if (with_delays) {
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


struct bu_vls *
history_prev(void)
{
    if (currHist.current == 0) {
	return NULL;
    } else {
	currHist.current--;
	return &(currHist.cmdhist[currHist.current].h_command);
    }
}


struct bu_vls *
history_cur(void)
{
    if (currHist.capacity == 0) {
	return NULL;
    } else {
	return &(currHist.cmdhist[currHist.current].h_command);
	}
}


struct bu_vls *
history_next(void)
{
    if (currHist.capacity == 0 || currHist.current == currHist.size - 1) {
	return NULL;
    }

    currHist.current++;
    return &(currHist.cmdhist[currHist.current].h_command);
}


int
cmd_hist(void *clientData, int argc, const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    struct bu_vls *vp;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

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


static int
wrapper_func(ClientData data, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct bu_cmdtab *ctp = (struct bu_cmdtab *)data;

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


#ifdef BWISH
/* structure provided in libtclcad. provides -format pix-n-w support.
 * doesn't really seem to be used anywhere except here.
 */
extern Tk_PhotoImageFormat tkImgFmtPIX;
#endif


int
cmdInit(Tcl_Interp *interp)
{
    static struct bu_cmdtab bwish_cmds[] = {
	{"exit",	cmd_quit},
	{"history",	cmd_history},
	{"hist",	cmd_hist},
	{"q",		cmd_quit},
	{(const char *)NULL, BU_CMD_NULL}
    };

    /* Register bwish/btclsh commands */
    register_cmds(interp, bwish_cmds);

#ifdef BWISH
    /* Add pix format for images */
    Tk_CreatePhotoImageFormat(&tkImgFmtPIX);
#endif

    /* initialize command history */
    historyInit();
    return TCL_OK;
}

#endif /* HAVE_WINDOWS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
