/*
 *                           H I S T O R Y . C
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
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include <signal.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/time.h>
#include <time.h>

#include "tcl.h"

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "rtstring.h"
#include "rtlist.h"
#include "externs.h"
#include "./ged.h"
#include "./mgedtcl.h"

struct mged_hist {
    struct rt_list l;
    struct rt_vls command;
    struct timeval start, finish;
    int status;
} mged_hist_head, *cur_hist;

FILE *journalfp;
int firstjournal;

void history_journalize();

/*
 *	H I S T O R Y _ R E C O R D
 *
 *	Stores the given command with start and finish times in the
 *	  history vls'es.
 */

void
history_record(cmdp, start, finish, status)
struct rt_vls *cmdp;
struct timeval *start, *finish;
int status;   /* Either CMD_OK or CMD_BAD */
{
    struct mged_hist *new_hist;

    if (strcmp(rt_vls_addr(cmdp), "\n") == 0)
	return;

    new_hist = (struct mged_hist *)rt_malloc(sizeof(struct mged_hist),
					     "mged history");
    rt_vls_init(&(new_hist->command));
    rt_vls_vlscat(&(new_hist->command), cmdp);
    new_hist->start = *start;
    new_hist->finish = *finish;
    new_hist->status = status;
    RT_LIST_INSERT(&(mged_hist_head.l), &(new_hist->l));

    /* As long as this isn't our first command to record after setting
       up the journal (which would be "journal", which we don't want
       recorded!)... */

    if (journalfp != NULL && !firstjournal)
	history_journalize(new_hist);

    cur_hist = &mged_hist_head;
    firstjournal = 0;
}

HIDDEN int
timediff(tvdiff, start, finish)
struct timeval *tvdiff, *start, *finish;
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
history_journalize(hptr)
struct mged_hist *hptr;
{
    struct timeval tvdiff;
    struct mged_hist *lasthptr;

    lasthptr = RT_LIST_PREV(mged_hist, &(hptr->l));
    if (timediff(&tvdiff, &(lasthptr->finish), &(hptr->start)) >= 0)
	fprintf(journalfp, "delay %d %d\n", tvdiff.tv_sec, tvdiff.tv_usec);
    
    if (hptr->status == CMD_BAD)
	fprintf(journalfp, "# ");
    fprintf(journalfp, "%s", hptr->command);
}

/*
 *	F _ J O U R N A L
 *
 *	Opens the journal file, so each command and the time since the previous
 *	  one will be recorded.  Or, if called with no arguments, closes the
 *	  journal file.
 */

int
f_journal(argc, argv)
int argc;
char **argv;
{
    if (argc < 2) {
	if (journalfp != NULL)
	    fclose(journalfp);
	journalfp = NULL;
	return CMD_OK;
    } else {
	if (journalfp != NULL) {
	    rt_log("First shut off journaling with \"journal\" (no args)\n");
	    return CMD_BAD;
	} else {
	    journalfp = fopen(argv[1], "a+");
	    if (journalfp == NULL) {
		rt_log("Error opening %s for appending\n", argv[1]);
		return CMD_BAD;
	    }
	    firstjournal = 1;
	}
    }
    return CMD_OK;
}

/*
 *	F _ D E L A Y
 *
 * 	Uses select to delay for the specified amount of seconds and 
 *	  microseconds.
 */

int
f_delay(argc, argv)
int argc;
char **argv;
{
    struct timeval tv;

    tv.tv_sec = atoi(argv[1]);
    tv.tv_usec = atoi(argv[2]);
    select(0, NULL, NULL, NULL, &tv);

    return CMD_OK;
}

/*
 *	F _ H I S T O R Y
 *
 *	Prints out the command history, either to rt_log or to a file.
 */

int
f_history( argc, argv )
int argc;
char **argv;
{
    FILE *fp;
    int with_delays = 0;
    struct mged_hist *hp, *hp_prev;
    struct rt_vls str;
    struct timeval tvdiff;
    
    fp = NULL;
    while( argc >= 2 ) {
	if( strcmp(argv[1], "-delays") == 0 )
	    with_delays = 1;
	else if( strcmp(argv[1], "-outfile") == 0 ) {
	    if( fp != NULL ) {
		rt_log( "history: -outfile option given more than once\n" );
		return CMD_BAD;
	    } else if( argc < 3 || strcmp(argv[2], "-delays") == 0 ) {
		rt_log( "history: I need a file name\n" );
		return CMD_BAD;
	    } else {
		fp = fopen( argv[2], "a+" );
		if( fp == NULL ) {
		    rt_log( "history: error opening file" );
		    return CMD_BAD;
		}
		--argc;
		++argv;
	    }
	} else {
	    rt_log( "Invalid option %s\n", argv[1] );
	}
	--argc;
	++argv;
    }

    rt_vls_init(&str);
    for (RT_LIST_FOR(hp, mged_hist, &(mged_hist_head.l))) {
	rt_vls_trunc(&str, 0);
	hp_prev = RT_LIST_PREV(mged_hist, &(hp->l));
	if (with_delays && RT_LIST_NOT_HEAD(hp_prev, &(mged_hist_head.l))) {
	    if (timediff(&tvdiff, &(hp_prev->finish), &(hp->start)) >= 0)
		rt_vls_printf(&str, "delay %d %d\n", tvdiff.tv_sec,
			      tvdiff.tv_usec);
	}

	if (hp->status == CMD_BAD)
	    rt_vls_printf(&str, "# ");
	rt_vls_vlscat(&str, &(hp->command));

	if (fp != NULL)
	    rt_vls_fwrite(fp, &str);
	else
	    rt_log("%s", rt_vls_addr(&str));
    }

    if (fp != NULL)
	fclose(fp);

    return CMD_OK;
}

/*      H I S T O R Y _ P R E V
 */
struct rt_vls *
history_prev()
{
    struct mged_hist *hp;

    hp = RT_LIST_PREV(mged_hist, &(cur_hist->l));
    if (RT_LIST_IS_HEAD(hp, &(mged_hist_head.l)))
	return NULL;
    else {
	cur_hist = hp;
	return &(hp->command);
    }
}

/*      H I S T O R Y _ C U R
 */
struct rt_vls *
history_cur()
{
    if (RT_LIST_IS_HEAD(cur_hist, &(mged_hist_head.l)))
	return NULL;
    else
	return &(cur_hist->command);
}

/*      H I S T O R Y _ N E X T
 */
struct rt_vls *
history_next()
{
    struct mged_hist *hp;

    if (RT_LIST_IS_HEAD(cur_hist, &(mged_hist_head.l))) {
	return 0;
    }
    
    hp = RT_LIST_NEXT(mged_hist, &(cur_hist->l));
    if (RT_LIST_IS_HEAD(hp, &(mged_hist_head.l))) {
	cur_hist = hp;
	return 0;
    } else {
	cur_hist = hp;
	return &(hp->command);
    }
}

/*
 *	C M D _ P R E V
 *
 *      Returns the previous command, looking through the history.
 */

int
cmd_prev(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
    struct rt_vls *vp;
    vp = history_prev();
    if (vp == NULL)
	vp = &(cur_hist->command);

    Tcl_SetResult(interp, rt_vls_addr(vp), TCL_VOLATILE);
    return TCL_OK;
}

/*
 *	C M D _ N E X T
 *
 *      Returns the next command, looking through the history.
 */

int
cmd_next( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
    struct rt_vls *vp;
    vp = history_next();
    if (vp == NULL)
	vp = &(cur_hist->command);

    Tcl_SetResult(interp, rt_vls_addr(vp), TCL_VOLATILE);
    return TCL_OK;
}


int
cmd_hist_add(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
    struct timeval zero;
    struct rt_vls vls;

    if (argc < 2)
	return TCL_OK;

    if (argv[1][0] == '\n')
	return TCL_OK;

    rt_vls_init(&vls);
    rt_vls_strcat(&vls, argv[1]);
    if (argv[1][strlen(argv[1])-1] != '\n')
	rt_vls_putc(&vls, '\n');

    zero.tv_sec = zero.tv_usec = 0L;
    history_record(&vls, &zero, &zero, CMD_OK);

    rt_vls_free(&vls);
    return TCL_OK;
}

void
history_setup()
{
    RT_LIST_INIT(&(mged_hist_head.l));
    cur_hist = &mged_hist_head;
    rt_vls_init(&(mged_hist_head.command));
    mged_hist_head.start.tv_sec = mged_hist_head.start.tv_usec =
	mged_hist_head.finish.tv_sec = mged_hist_head.finish.tv_usec = 0L;
    mged_hist_head.status = CMD_OK;
    journalfp = NULL;
}
    
