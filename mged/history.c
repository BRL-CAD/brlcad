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

#define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */
#include "tcl.h"
#include "tk.h"

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "rtstring.h"
#include "externs.h"
#include "./ged.h"

struct rt_vls history;
struct rt_vls replay_history;
struct timeval lastfinish;
long int hist_index;

FILE *journalfp;
int firstjournal;

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
    static int done = 0;
    struct rt_vls timing;
    struct rt_vls command;

    rt_vls_init(&timing);
    rt_vls_init(&command);
    
    if (status == CMD_BAD) {
	rt_vls_strcpy(&command, "# ");
	rt_vls_vlscat(&command, cmdp);
    } else {
	rt_vls_vlscat(&command, cmdp);
    }

    if (done != 0) {
	if (lastfinish.tv_usec > start->tv_usec) {
	    rt_vls_printf(&timing, "delay %ld %08ld\n",
			  start->tv_sec - lastfinish.tv_sec - 1,
			  start->tv_usec - lastfinish.tv_usec + 1000000L);
	} else {
	    rt_vls_printf(&timing, "delay %ld %08ld\n",
			  start->tv_sec - lastfinish.tv_sec,
			  start->tv_usec - lastfinish.tv_usec);
	}
    }		

    /* As long as this isn't our first command to record after setting
       up the journal (which would be "journal", which we don't want
       recorded!)... */

    if (journalfp != NULL && !firstjournal) {
	rt_vls_fwrite(journalfp, &timing);
	rt_vls_fwrite(journalfp, &command);
    }
    
    rt_vls_vlscat(&replay_history, &timing);
    rt_vls_vlscat(&replay_history, &command);

    rt_vls_vlscat(&history, &command);

    lastfinish.tv_sec = finish->tv_sec;
    lastfinish.tv_usec = finish->tv_usec;
    done = 1;
    firstjournal = 0;

    rt_vls_free(&command);
    rt_vls_free(&timing);

    hist_index = rt_vls_strlen(&history) - 1;
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
	struct rt_vls *which_history;

	fp = NULL;
	which_history = &history;

	while( argc >= 2 ) {
		if( strcmp(argv[1], "-delays") == 0 ) {
			if( which_history == &replay_history ) {
				rt_log( "history: -delays option given more than once\n" );
				return CMD_BAD;
			}
			which_history = &replay_history;
		} else if( strcmp(argv[1], "-outfile") == 0 ) {
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

	if( fp == NULL ) {
		rt_log( "%s", rt_vls_addr(which_history) );
	} else {
		rt_vls_fwrite( fp, which_history );
		fclose( fp );
	}

	return CMD_OK;
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
    struct rt_vls result;
    register char *cp;

    cp = rt_vls_addr(&history) + hist_index;

    do {
	--cp;
	--hist_index;
    } while (hist_index > 0 && *cp != '\n');

    rt_vls_init(&result);
    if (*cp == '\n') ++cp;
    while (*cp && *cp != '\n')
	rt_vls_putc(&result, (int)(*cp++));

    Tcl_SetResult(interp, rt_vls_addr(&result), TCL_VOLATILE);

    rt_vls_free(&result);
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
	return TCL_OK;
}


void
history_setup()
{
    rt_vls_init(&history);
    rt_vls_init(&replay_history);
    journalfp = NULL;
    hist_index = 0;
}
    
