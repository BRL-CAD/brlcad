/*                           C M D . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file cmd.c
 *
 * This is the place where BWISH/BTCLSH's commands live.
 * The history routines were borrowed from mged/history.c
 * and modified for use in this application.
 *
 *  Author -
 *	   Robert G. Parker
 *
 *  Authors of mged/history.c -
 *	   Glenn Durfee
 *	   Bob Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */

#include "common.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#ifdef BWISH
#  include "tk.h"
#else
#  include "tcl.h"
#endif

#include "machine.h"
#include "cmd.h"
#include "libtermio.h"


/* defined in tcl.c */
extern void Cad_Exit(int status);

HIDDEN void historyInit(void);
HIDDEN int cmd_history(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
HIDDEN int cmd_hist(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
HIDDEN int cmd_quit(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);

HIDDEN struct bu_cmdhist histHead;
HIDDEN struct bu_cmdhist *currHist;

HIDDEN struct bu_cmdtab bwish_cmds[] =
    {
	{"exit",		cmd_quit},
	{"history",		cmd_history},
	{"hist",		cmd_hist},
	{"q",			cmd_quit},
	{(char *)NULL,		CMD_NULL}
    };

#ifdef BWISH
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

/***************************** BWISH/BTCLSH COMMANDS *****************************/

HIDDEN int
cmd_quit(ClientData	clientData,
	 Tcl_Interp	*interp,
	 int		argc,
	 char		**argv)
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
#if 0
    journalfp = NULL;
#endif
    historyInitialized=1;
}

/*
 *	H I S T O R Y _ R E C O R D
 *
 *	Stores the given command with start and finish times in the
 *	history vls'es.
 */
void
history_record(struct bu_vls *cmdp, struct timeval *start, struct timeval *finish, int status)


     /* Either TCL_OK or TCL_ERROR */
{
    struct bu_cmdhist *new_hist;

    if (strcmp(bu_vls_addr(cmdp), "\n") == 0)
	return;

    new_hist = (struct bu_cmdhist *)bu_malloc(sizeof(struct bu_cmdhist),
					      "mged history");
    bu_vls_init(&(new_hist->h_command));
    bu_vls_vlscat(&(new_hist->h_command), cmdp);
    new_hist->h_start = *start;
    new_hist->h_finish = *finish;
    new_hist->h_status = status;

    /* make sure the list is initialized before attempting to add this entry */
    if (!historyInitialized) {
	historyInit();
    }

    BU_LIST_INSERT(&(histHead.l), &(new_hist->l));

    /* As long as this isn't our first command to record after setting
       up the journal (which would be "journal", which we don't want
       recorded!)... */

#if 0
    if (journalfp != NULL && !firstjournal)
	history_journalize(new_hist);
#endif

    currHist = &histHead;
#if 0
    firstjournal = 0;
#endif
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

#if 0
void
history_journalize(hptr)
     struct bu_cmdhist *hptr;
{
    struct timeval tvdiff;
    struct bu_cmdhist *lasthptr;

    lasthptr = BU_LIST_PREV(bu_cmdhist, &(hptr->l));

    if (journal_delay && timediff(&tvdiff, &(lasthptr->h_finish), &(hptr->h_start)) >= 0)
	fprintf(journalfp, "delay %d %ld\n", tvdiff.tv_sec, tvdiff.tv_usec);

    if (hptr->h_status == TCL_ERROR)
	fprintf(journalfp, "# ");
    fprintf(journalfp, "%s", bu_vls_addr(&hptr->h_command));

    if (journal_delay)
	fprintf(journalfp, "mged_update 1\n");
}

/*
 *	F _ J O U R N A L
 *
 *	Opens the journal file, so each command and the time since the previous
 *	  one will be recorded.  Or, if called with no arguments, closes the
 *	  journal file.
 */
int
cmd_journal(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char **argv;
{
    if(argc < 1 || 3 < argc){
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help journal");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    /* close previously open journal file */
    if (journalfp != NULL) {
	fclose(journalfp);
	journalfp = NULL;
    }
    journal_delay = 0;

    if (argc < 2)
	return TCL_OK;

    if(argv[1][0] == '-' && argv[1][1] == 'd'){
	journal_delay = 1;
	++argv;
	--argc;
    }

    if (argc < 2) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help journal");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    journalfp = fopen(argv[1], "a+");
    if (journalfp == NULL) {
	Tcl_AppendResult(interp, "Error opening ", argv[1],
			 " for appending\n", (char *)NULL);
	return TCL_ERROR;
    }
    firstjournal = 1;

    return TCL_OK;
}

/*
 *	F _ D E L A Y
 *
 * 	Uses select to delay for the specified amount of seconds and
 *	  microseconds.
 */

int
f_delay(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char **argv;
{
    struct timeval tv;

    if(argc < 3 || 3 < argc){
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help delay");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    tv.tv_sec = atoi(argv[1]);
    tv.tv_usec = atoi(argv[2]);
    select(0, NULL, NULL, NULL, &tv);

    return TCL_OK;
}
#endif

/*
 *	F _ H I S T O R Y
 *
 *	Prints out the command history, either to bu_log or to a file.
 */

int
cmd_history(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
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
	if (strcmp(argv[1], "-delays") == 0)
	    with_delays = 1;
	else if (strcmp(argv[1], "-outfile") == 0) {
	    if (fp != NULL) {
		fclose(fp);
		Tcl_AppendResult(interp, "history: -outfile option given more than once\n",
				 (char *)NULL);
		return TCL_ERROR;
	    } else if (argc < 3 || strcmp(argv[2], "-delays") == 0) {
		Tcl_AppendResult(interp, "history: I need a file name\n", (char *)NULL);
		return TCL_ERROR;
	    } else {
		fp = fopen( argv[2], "a+" );
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
		bu_vls_printf(&str, "delay %d %d\n", tvdiff.tv_sec,
			      tvdiff.tv_usec);
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

/*
 *      H I S T O R Y _ P R E V
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

/*
 *      H I S T O R Y _ C U R
 */
struct bu_vls *
history_cur(void)
{
    if (BU_LIST_IS_HEAD(currHist, &(histHead.l)))
	return NULL;
    else
	return &(currHist->h_command);
}

/*
 *      H I S T O R Y _ N E X T
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
cmd_hist(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    struct bu_vls *vp;
    struct bu_vls vls;

    bu_vls_init(&vls);

    if(argc < 2){
	Tcl_AppendResult(interp, "hist command\n\troutine for maintaining command history", (char *)0);
	return TCL_ERROR;
    }

    if(strcmp(argv[1], "add") == 0){
	struct timeval zero;

	if(argc != 3){
	    Tcl_AppendResult(interp, "hist add command\n\tadd command to history", (char *)0);
	    return TCL_ERROR;
	}

	if (argv[2][0] == '\n' || argv[2][0] == '\0')
	    return TCL_OK;

	bu_vls_strcpy(&vls, argv[2]);
	if (argv[2][strlen(argv[2])-1] != '\n')
	    bu_vls_putc(&vls, '\n');

	zero.tv_sec = zero.tv_usec = 0L;
	history_record(&vls, &zero, &zero, TCL_OK);

	bu_vls_free(&vls);
	return TCL_OK;
    }

    if(strcmp(argv[1], "next") == 0){
	if(argc != 2){
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

    if(strcmp(argv[1], "prev") == 0){
	if(argc != 2){
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
