/*                        B U F F E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file buffer.c
 *
 *  This program is intended to be use as part of a complex pipeline.
 *  It serves somewhat the same purpose as the Prolog "cut" operator.
 *  Data from stdin is read and buffered until EOF is detected, and then
 *  all the buffered data is written to stdout.  An arbitrary amount of
 *  data may need to be buffered, so a combination of a 1 Mbyte memory buffer
 *  and a temporary file is used.
 *
 *  The use of read() and write() is prefered over fread() and fwrite()
 *  for reasons of efficiency, given the large buffer size in use.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

int mread(int fd, char *bufp, int n );

char	template[] = "/usr/tmp/bufferXXXXXX";

#define	SIZE	(1024*1024)

char	buf[SIZE];

int
main(void)
{
	register int	count;
	register int	tfd;

	if( (count = mread(0, buf, sizeof(buf))) < sizeof(buf) )  {
		if( count < 0 )  {
			perror("buffer: mem read");
			exit(1);
		}
		/* Entire input sequence fit into buf */
		if( write(1, buf, count) != count )  {
			perror("buffer: stdout write 1");
			exit(1);
		}
		exit(0);
	}

	/* Create temporary file to hold data, get r/w file descriptor */
	(void)mkstemp( template );
	if( (tfd = creat( template, 0600 )) < 0 )  {
		perror(template);
		exit(1);
	}
	(void)close(tfd);
	if( (tfd = open( template, 2 )) < 0 )  {
		perror(template);
		goto err;
	}

	/* Stash away first buffer full */
	if( write(tfd, buf, count) != count )  {
		perror("buffer: tmp write1");
		goto err;
	}

	/* Continue reading and writing additional buffer loads to temp file */
	while( (count = mread(0, buf, sizeof(buf))) > 0 )  {
		if( write(tfd, buf, count) != count )  {
			perror("buffer: tmp write2");
			goto err;
		}
	}
	if( count < 0 )  {
		perror("buffer: read");
		goto err;
	}

	/* All input read, regurgitate it all on stdout */
	if( lseek( tfd, 0L, 0 ) < 0 )  {
		perror("buffer: lseek");
		goto err;
	}
	while( (count = mread(tfd, buf, sizeof(buf))) > 0 )  {
		if( write(1, buf, count) != count )  {
			perror("buffer: stdout write 2");
			goto err;
		}
	}
	if( count < 0 )  {
		perror("buffer: tmp read");
		goto err;
	}
	(void)unlink(template);
	exit(0);

err:
	(void)unlink(template);
	exit(1);
}

/*
 *			M R E A D
 *
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This can be necessary because pipes
 * and network connections don't deliver data with the same
 * grouping as it is written with.  Written by Robert S. Miles, BRL.
 */
int
mread(int fd, register char *bufp, int n)
{
	register int	count = 0;
	register int	nread;

	do {
		nread = read(fd, bufp, (unsigned)n-count);
		if(nread < 0)  {
			perror("buffer: mread");
			return(-1);
		}
		if(nread == 0)
			return((int)count);
		count += (unsigned)nread;
		bufp += nread;
	 } while(count < n);

	return((int)count);
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
