/*                          D B C P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file dbcp.c
 *
 *	Double-buffered copy program for UNIX
 *
 *	Usage:    dbcp {nblocks} < inputfile > outputfile
 *
 *  Author -
 *	Doug Kingston
 *
 *  Source -
 *	Davis, Polk, and Wardwell
 *	Chase Manhattan Building
 *	New York, NY
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>

#include "machine.h"
#include "bu.h"


#define	STOP	0170
#define	GO	0017

#define P_RD	0
#define P_WR	1

typedef int pipefds[2];

static int	pid;
static long	count;

static int	verbose;

static char	errbuf[BUFSIZ];

static char	usage[] = "\
Usage:  dbcp [-v] blocksize < input > output\n\
	(blocksize = number of 512 byte 'blocks' per record)\n";

void prs();

/*
 *			M A I N
 */
int
main(argc, argv)
int	argc;
char	**argv;
{
	register char	*buffer;
	register unsigned int	size;
	register unsigned int	nread;
	int	rfd;		/* pipe to read message from */
	int	wfd;		/* pipe to write message to */
	int	exitval=0;
	int	saverrno=0;
	int	waitcode;
	char	msgchar;
	pipefds par2chld, chld2par;
	int	c;

	while ( (c = bu_getopt( argc, argv, "v" )) != EOF )  {
		switch( c )  {
		case 'v':
			verbose++;
			break;
		default:
			(void)fputs(usage, stderr);
			exit(1);
		}
	}

	if( bu_optind >= argc )  {
		(void)fputs(usage, stderr);
		exit(2);
	}
	size = 512 * atoi(argv[bu_optind]);

	setbuf (stderr, errbuf);
	if ((buffer = (char *)malloc(size)) == NULL) {
		fprintf(stderr, "dbcp: Insufficient buffer memory\n");
		exit (88);
	}
	if (pipe (par2chld) < 0 || pipe (chld2par) < 0) {
		perror ("dbcp: Can't pipe");
		exit (89);
	}

	/*
	 * Ignore SIGPIPE, which may occur sometimes when the parent
	 * goes to send a token to an already dead child on last buffer.
	 */
	(void)signal(SIGPIPE, SIG_IGN);

	switch (pid = fork()) {
	case -1:
		perror ("dbcp: Can't fork");
		exit (99);

	case 0:
		/*  Child  */
		close (par2chld[P_WR]);
		close (chld2par[P_RD]);
		wfd = chld2par[P_WR];
		rfd = par2chld[P_RD];
		msgchar = GO;		/* Prime the pump, so to speak */
		goto childstart;

	default:
		/*  Parent  */
		close (par2chld[P_RD]);
		close (chld2par[P_WR]);
		wfd = par2chld[P_WR];
		rfd = chld2par[P_RD];
		break;
	}

	exitval = 0;
	count = 0L;
	while (1) {
		if ((nread = bu_mread (0, buffer, size)) != size) {
			saverrno = errno;
			msgchar = STOP;
		} else
			msgchar = GO;
		if(write (wfd, &msgchar, 1) != 1) {
			perror("dbcp: message send");
			prs("Can't send READ message\n");
		}
		if ((int)nread == (-1)) {
			errno = saverrno;
			perror ("input read");
			prs("read error on input\n");
			break;
		}
		if(nread == 0) {
			if(verbose) prs("EOF on input\n");
			break;
		}
		if(nread != size)
			prs("partial read (nread = %u)\n", nread);
		if (read(rfd, &msgchar, 1) != 1) {
			perror("dbcp: WRITE message error");
			exitval = 69;
			break;
		}
		if (msgchar == STOP) {
			prs("Got STOP WRITE with %u left\n", nread);
			break;
		} else if (msgchar != GO) {
			prs("Got bad WRITE message 0%o\n", msgchar&0377);
			exitval = 19;
			break;
		}
		if (write(1, buffer, nread) != nread) {
			perror("output write");
			msgchar = STOP;
		} else {
			count++;
			msgchar = GO;
		}
		if(verbose>1) prs("wrote %d\n", nread);
		if (nread != size)
			break;
childstart:
		if (write (wfd, &msgchar, 1) != 1) {
			perror("dbcp: message send");
			prs("Can't send WRITE message\n");
			break;
		}
		if (msgchar == STOP) {
			prs ("write error on output\n");
			break;
		}
		if (read(rfd, &msgchar, 1) != 1) {
			perror("dbcp: READ message error");
			exitval = 79;
			break;
		}
		if (msgchar == STOP) {
			if(verbose) prs("Got STOP READ\n");
			break;
		} else if (msgchar != GO) {
			prs("Got bad READ message 0%o\n", msgchar&0377);
			exitval = 39;
			break;
		}
	}

	if(verbose) prs ("%ld records copied\n", count);
	if(pid)
		while (wait(&waitcode) > 0);
	exit(exitval);
}

void
prs (fmt, a, b, c)
char	*fmt, *a, *b, *c;
{
	fprintf(stderr, "dbcp: (%s) ", pid ? "PARENT" : "CHILD");
	fprintf(stderr, fmt, a, b, c);
	fflush(stderr);
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
