/*                       H I S T O R Y . C
 * BRL-CAD
 *
 * Copyright (C) 1995-2005 United States Government as represented by
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
 */
/** @file history.c
 *
 *  Authors -
 *	Glenn Durfee
 *	Bob Parker
 *
 *  Functions -
 *      history_record - 
 *
 *  Source -
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */

#include "common.h"



#include <stdio.h>
#include <signal.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#include <time.h>

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mgedtcl.h"

#if 0
struct mged_hist {
    struct bu_list l;
    struct bu_vls command;
    struct timeval start, finish;
    int status;
} mged_hist_head, *cur_hist;
#else
struct mged_hist mged_hist_head;
#endif

FILE *journalfp;
int firstjournal;
int journal_delay = 0;

void history_journalize(struct mged_hist *hptr);

/*
 *	H I S T O R Y _ R E C O R D
 *
 *	Stores the given command with start and finish times in the
 *	  history vls'es.
 */

void
history_record(
	struct bu_vls *cmdp,
	struct timeval *start,
	struct timeval *finish,
	int status)			   /* Either CMD_OK or CMD_BAD */
{
    struct mged_hist *new_hist;

    if (strcmp(bu_vls_addr(cmdp), "\n") == 0)
	return;

    new_hist = (struct mged_hist *)bu_malloc(sizeof(struct mged_hist),
					     "mged history");
    bu_vls_init(&(new_hist->mh_command));
    bu_vls_vlscat(&(new_hist->mh_command), cmdp);
    new_hist->mh_start = *start;
    new_hist->mh_finish = *finish;
    new_hist->mh_status = status;
    BU_LIST_INSERT(&(mged_hist_head.l), &(new_hist->l));

    /* As long as this isn't our first command to record after setting
       up the journal (which would be "journal", which we don't want
       recorded!)... */

    if (journalfp != NULL && !firstjournal)
	history_journalize(new_hist);

    curr_cmd_list->cl_cur_hist = &mged_hist_head;
    firstjournal = 0;
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

void
history_journalize(struct mged_hist *hptr)
{
    struct timeval tvdiff;
    struct mged_hist *lasthptr;

    lasthptr = BU_LIST_PREV(mged_hist, &(hptr->l));

    if (journal_delay && timediff(&tvdiff, &(lasthptr->mh_finish), &(hptr->mh_start)) >= 0)
	fprintf(journalfp, "delay %ld %ld\n", (long)tvdiff.tv_sec, (long)tvdiff.tv_usec);

    if (hptr->mh_status == CMD_BAD)
	fprintf(journalfp, "# ");
    fprintf(journalfp, "%s", bu_vls_addr(&hptr->mh_command));

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
f_journal(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
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
f_delay(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
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

/*
 *	F _ H I S T O R Y
 *
 *	Prints out the command history, either to bu_log or to a file.
 */

int
f_history(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FILE *fp;
    int with_delays = 0;
    struct mged_hist *hp, *hp_prev;
    struct bu_vls str;
    struct timeval tvdiff;

    if(argc < 1 || 4 < argc){
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help history");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    fp = NULL;
    while( argc >= 2 ) {
	if( strcmp(argv[1], "-delays") == 0 )
	    with_delays = 1;
	else if( strcmp(argv[1], "-outfile") == 0 ) {
	    if( fp != NULL ) {
	      Tcl_AppendResult(interp, "history: -outfile option given more than once\n",
			       (char *)NULL);
	      return TCL_ERROR;
	    } else if( argc < 3 || strcmp(argv[2], "-delays") == 0 ) {
	      Tcl_AppendResult(interp, "history: I need a file name\n", (char *)NULL);
	      return TCL_ERROR;
	    } else {
		fp = fopen( argv[2], "a+" );
		if( fp == NULL ) {
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
    for (BU_LIST_FOR(hp, mged_hist, &(mged_hist_head.l))) {
	bu_vls_trunc(&str, 0);
	hp_prev = BU_LIST_PREV(mged_hist, &(hp->l));
	if (with_delays && BU_LIST_NOT_HEAD(hp_prev, &(mged_hist_head.l))) {
	    if (timediff(&tvdiff, &(hp_prev->mh_finish), &(hp->mh_start)) >= 0)
		bu_vls_printf(&str, "delay %d %d\n", tvdiff.tv_sec,
			      tvdiff.tv_usec);
	}

	if (hp->mh_status == CMD_BAD)
	    bu_vls_printf(&str, "# ");
	bu_vls_vlscat(&str, &(hp->mh_command));

	if (fp != NULL)
	    bu_vls_fwrite(fp, &str);
	else
	  Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
    }

    if (fp != NULL)
	fclose(fp);

    return TCL_OK;
}

/*      H I S T O R Y _ P R E V
 */
struct bu_vls *
history_prev(void)
{
    struct mged_hist *hp;

    hp = BU_LIST_PREV(mged_hist, &(curr_cmd_list->cl_cur_hist->l));
    if (BU_LIST_IS_HEAD(hp, &(mged_hist_head.l)))
	return NULL;
    else {
	curr_cmd_list->cl_cur_hist = hp;
	return &(hp->mh_command);
    }
}

/*      H I S T O R Y _ C U R
 */
struct bu_vls *
history_cur(void)
{
    if (BU_LIST_IS_HEAD(curr_cmd_list->cl_cur_hist, &(mged_hist_head.l)))
	return NULL;
    else
	return &(curr_cmd_list->cl_cur_hist->mh_command);
}

/*      H I S T O R Y _ N E X T
 */
struct bu_vls *
history_next(void)
{
    struct mged_hist *hp;

    if (BU_LIST_IS_HEAD(curr_cmd_list->cl_cur_hist, &(mged_hist_head.l))) {
	return 0;
    }
    
    hp = BU_LIST_NEXT(mged_hist, &(curr_cmd_list->cl_cur_hist->l));
    if (BU_LIST_IS_HEAD(hp, &(mged_hist_head.l))) {
	curr_cmd_list->cl_cur_hist = hp;
	return 0;
    } else {
	curr_cmd_list->cl_cur_hist = hp;
	return &(hp->mh_command);
    }
}

int
cmd_hist(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
  struct bu_vls *vp;
  struct bu_vls vls;

  bu_vls_init(&vls);

  if(argc < 2){
    bu_vls_printf(&vls, "helpdevel hist");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if(strcmp(argv[1], "add") == 0){
    struct timeval zero;

    if(argc != 3){
      bu_vls_printf(&vls, "helpdevel hist");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    if (argv[2][0] == '\n' || argv[2][0] == '\0')
	return TCL_OK;

    bu_vls_strcpy(&vls, argv[2]);
    if (argv[2][strlen(argv[2])-1] != '\n')
	bu_vls_putc(&vls, '\n');

    zero.tv_sec = zero.tv_usec = 0L;
    history_record(&vls, &zero, &zero, CMD_OK);

    bu_vls_free(&vls);
    return TCL_OK;
  }

  if(strcmp(argv[1], "next") == 0){
    if(argc != 2){
      bu_vls_printf(&vls, "helpdevel hist");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
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
      bu_vls_printf(&vls, "helpdevel hist");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    vp = history_prev();
    if (vp == NULL)
      return TCL_ERROR;

    Tcl_AppendResult(interp, bu_vls_addr(vp), (char *)NULL);
    bu_vls_free(&vls);
    return TCL_OK;
  }

  bu_vls_printf(&vls, "helpdevel hist");
  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
  return TCL_ERROR;
}

void
history_setup(void)
{
    BU_LIST_INIT(&(mged_hist_head.l));
    curr_cmd_list->cl_cur_hist = &mged_hist_head;
    bu_vls_init(&(mged_hist_head.mh_command));
    mged_hist_head.mh_start.tv_sec = mged_hist_head.mh_start.tv_usec =
	mged_hist_head.mh_finish.tv_sec = mged_hist_head.mh_finish.tv_usec = 0L;
    mged_hist_head.mh_status = CMD_OK;
    journalfp = NULL;
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
