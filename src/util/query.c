/*                         Q U E R Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2009 United States Government as represented by
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
 */
/** @file query.c
 *
 * By default, query reads a line from standard input and echoes it
 * to standard output.
 *
 *  entry:
 *	-t	seconds to wait for user response.
 *	-r	default response
 *	-v	toggle verbose option (Default "r" in "t" seconds)
 *	-l	loop rather than respond.
 *
 *  Exit:
 *	<stdout>	The line read
 *		or	y
 *		or	response
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "bio.h"

#include "bu.h"


char	Yes_Response[] = "y";
int	Verbose=0;
char	*Response= Yes_Response;
int	Timeout=0;
int	Loop=0;
int	Done = 0;

static const char usage[] = "\
Usage: %s [-v] [-t seconds] [-r response ] [-l]\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ( (c = bu_getopt( argc, argv, "t:r:vl" )) != EOF )  {
	switch ( c )  {
	    case 't':
		Timeout = atoi(bu_optarg);
		break;
	    case 'r':
		Response = bu_optarg;
		break;
	    case 'v':
		Verbose = 1 - Verbose;
		break;
	    case 'l':
		Loop = 1;
		break;
	    default:		/* '?' */
		return(0);
	}
    }
    if (Timeout < 0) Timeout = 0;
    if ( (Loop & Timeout) <= 0) Timeout=5;

    if (Loop) Verbose = 0;

    return(1);
}

void handler(int);

int
main(int argc, char **argv)
{
    char line[80];
    char *eol;
    char *flag;

    if ( !get_args( argc, argv ) )  {
	bu_exit(1, usage, argv[0]);
    }

    (void) signal(SIGALRM, handler);
    Done = 0;

    for (;;) {
	(void) signal(SIGALRM, handler);
	if (Verbose) {
	    if (Timeout) {
		(void) fprintf(stderr,
			       "(Default: %s in %d sec)", Response,
			       Timeout);
	    } else {
		(void) fprintf(stderr,
			       "(Default: %s)", Response);
	    }
	}

	if (Loop) {
	    (void) fprintf(stderr,
			   "(Default: %s, loop in %d sec)", Response, Timeout);
	}
	if (Timeout) alarm(Timeout);

	flag = bu_fgets(line, 80, stdin);
	if (Done) {
	    if (Loop) {
		(void) fputs("\n\007", stderr);
		Done = 0;
	    } else {
		(void) fputs(Response, stdout);
		(void) putchar('\n');
		break;
	    }
	} else if (flag == NULL) {
	    (void) fputs(Response, stdout);
	    break;
	} else {
	    /* good response */
	    eol = strlen(line) + line;
	    if (*(eol-1) == '\n') {
		--eol;
		*eol = '\0';
	    }
	    if (eol == line) {
		(void) fputs(Response, stdout);
	    } else {
		(void) fputs(line, stdout);
	    }
	    (void)putchar('\n');
	    break;
	}
    }
    bu_exit (0, NULL);
}
void
handler(int sig)
{
    Done = 1;
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
