/*                          D B C P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file util/dbcp.c
 *
 * Double-buffered copy program for UNIX
 *
 * Usage:    dbcp {nblocks} < inputfile > outputfile
 *
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include "bio.h"

#include "bu.h"


#define STOP 0170
#define GO 0017

#define P_RD 0
#define P_WR 1

typedef int pipefds[2];

static int pid;
static long count;

static int verbose;

static char errbuf[BUFSIZ] = {0};

static const char usage[] = "\
Usage:  dbcp [-v] blocksize < input > output\n\
	(blocksize = number of 512 byte 'blocks' per record)\n";


/*
 * M A I N
 */
int
main(int argc, char **argv)
{
    char *buffer;
    size_t size;
    long nread;
    int rfd;		/* pipe to read message from */
    int wfd;		/* pipe to write message to */
    int exitval=0;
    int saverrno=0;
    int waitcode;
    char msgchar;
    pipefds par2chld, chld2par;
    int c;
    int deprecated = 1;

    while ((c = bu_getopt(argc, argv, "v")) != -1) {
	switch (c) {
	    case 'v':
		verbose++;
		break;
	    case 'D':
		deprecated=0;
	    default:
		bu_exit(1, "%s", usage);
	}
    }

    if (deprecated)
	bu_log("DEPRECATED: dbcp is no longer being maintained.  Please contact the developers if you use this tool.  Use -D to suppress this message.\n");

    if (bu_optind >= argc) {
	bu_exit(2, "%s", usage);
    }
    size = 512 * atoi(argv[bu_optind]);

    setbuf (stderr, errbuf);
    if ((buffer = (char *)malloc(size)) == NULL)
	bu_exit(88, "dbcp: Insufficient buffer memory\n");
    if (pipe (par2chld) < 0 || pipe (chld2par) < 0) {
	perror("pipe");
	bu_exit(89, "dbcp: Can't pipe\n");
    }

    /*
     * Ignore SIGPIPE, which may occur sometimes when the parent
     * goes to send a token to an already dead child on last buffer.
     */
    (void)signal(SIGPIPE, SIG_IGN);

    switch (pid = fork()) {
	case -1:
	    perror("fork");
	    bu_exit(99, "dbcp: Can't fork\n");

	case 0:
	    /* Child */
	    close (par2chld[P_WR]);
	    close (chld2par[P_RD]);
	    wfd = chld2par[P_WR];
	    rfd = par2chld[P_RD];
	    msgchar = GO;		/* Prime the pump, so to speak */
	    goto childstart;

	default:
	    /* Parent */
	    close (par2chld[P_RD]);
	    close (chld2par[P_WR]);
	    wfd = par2chld[P_WR];
	    rfd = chld2par[P_RD];
	    break;
    }

    exitval = 0;
    count = 0L;
    while (1) {
	nread = bu_mread (0, buffer, size);
	if ((size_t)nread != size) {
	    saverrno = errno;
	    msgchar = STOP;
	} else
	    msgchar = GO;
	if (write (wfd, &msgchar, 1) != 1) {
	    perror("dbcp write:");
	    bu_log("dbcp: (%s) ", pid ? "PARENT" : "CHILD");
	    bu_log("Can't send READ message\n");
	}
	if (nread == -1) {
	    errno = saverrno;
	    perror("dbcp input read:");
	    bu_log("dbcp: (%s) ", pid ? "PARENT" : "CHILD");
	    bu_log("read error on input\n");
	    break;
	}
	if (nread == 0) {
	    if (verbose) {
		bu_log("dbcp: (%s) ", pid ? "PARENT" : "CHILD");
		bu_log("EOF on input\n");
	    }
	    break;
	}
	if ((size_t)nread != size) {
	    bu_log("dbcp: (%s) ", pid ? "PARENT" : "CHILD");
	    bu_log("partial read (nread = %ld)\n", nread);
	}
	if (read(rfd, &msgchar, 1) != 1) {
	    perror("dbcp: WRITE message error");
	    exitval = 69;
	    break;
	}
	if (msgchar == STOP) {
	    bu_log("dbcp: (%s) ", pid ? "PARENT" : "CHILD");
	    bu_log("Got STOP WRITE with %ld left\n", nread);
	    break;
	} else if (msgchar != GO) {
	    bu_log("dbcp: (%s) ", pid ? "PARENT" : "CHILD");
	    bu_log("Got bad WRITE message 0%o\n", msgchar&0377);
	    exitval = 19;
	    break;
	}
	if (write(1, buffer, nread) != nread) {
	    perror("dbcp output write:");
	    msgchar = STOP;
	} else {
	    count++;
	    msgchar = GO;
	}
	if (verbose>1) {
	    bu_log("dbcp: (%s) ", pid ? "PARENT" : "CHILD");
	    bu_log("wrote %ld\n", nread);
	}
	if ((size_t)nread != size) {
	    break;
	}
    childstart:
	if (write (wfd, &msgchar, 1) != 1) {
	    perror("dbcp message send:");
	    bu_log("dbcp: (%s) ", pid ? "PARENT" : "CHILD");
	    bu_log("Can't send WRITE message\n");
	    break;
	}
	if (msgchar == STOP) {
	    bu_log("dbcp: (%s) ", pid ? "PARENT" : "CHILD");
	    bu_log("write error on output\n");
	    break;
	}
	if (read(rfd, &msgchar, 1) != 1) {
	    perror("dbcp: READ message error");
	    exitval = 79;
	    break;
	}
	if (msgchar == STOP) {
	    if (verbose) {
		bu_log("dbcp: (%s) ", pid ? "PARENT" : "CHILD");
		bu_log("Got STOP READ\n");
	    }
	    break;
	} else if (msgchar != GO) {
	    bu_log("dbcp: (%s) ", pid ? "PARENT" : "CHILD");
	    bu_log("Got bad READ message 0%o\n", msgchar&0377);
	    exitval = 39;
	    break;
	}
    }

    if (verbose) {
	bu_log("dbcp: (%s) ", pid ? "PARENT" : "CHILD");
	bu_log("%ld records copied\n", count);
    }
    if (pid) {
	while (wait(&waitcode) > 0)
	    ;
    }

    return exitval;
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
