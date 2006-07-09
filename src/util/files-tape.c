/*                    F I L E S - T A P E . C
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
/** @file files-tape.c
 *
 *  Take a collection of files, and write them to tape.
 *  Each file is padded out to an integral number of tape records,
 *  and is written with a fixed block size (default is 24k).
 *  This program is preferred over dd(1) in several small but
 *  important ways:
 *
 *  1)	Multiple files may be written to tape in a single operation,
 *	ie, with a single open() of the output device.
 *  2)	Files are padded to full length records.
 *  3)  Input files are read using bu_mread(), so operation will
 *	proceed even with pipe input (where DD can read and write
 *	short records on a random basis).
 *
 *  This program is probably most often useful to
 *  take a collection of pix(5) or bw(5) files, and write them to
 *  tape using 24k byte records.
 *
 *  There is no requirement that the different files be the same size,
 *  although it is unlikely to be useful.  This could be a potential
 *  source of problems if some files have not been finished yet.
 *
 *  At 6250, one reel of tape holds roughly 6144 records of 24k bytes
 *  each.  This program will warn when that threshold is reached,
 *  but will take no action.
 *
 *  UNIX system calls are used, not to foil portability, but in the
 *  name of efficiency.
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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#include "machine.h"
#include "bu.h"


#define	TSIZE	(6144*24*1024)	/* # of bytes on 2400' 6250bpi reel */
long	byteswritten = 0;	/* Number of bytes written */

int	bufsize = 24*1024;	/* Default buffer size */
char	*buf;

void	fileout(register int fd, char *name);


static char usage[] = "\
Usage: files-tape [-b bytes] [-k Kbytes] [files]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "b:k:" )) != EOF )  {
		switch( c )  {
		case 'b':
			bufsize = atoi( bu_optarg );	/* bytes */
			break;
		case 'k':
			bufsize = atoi( bu_optarg ) * 1024; /* Kbytes */
			break;

		default:		/* '?' */
			return(0);	/* BAD */
		}
	}

	if( isatty(fileno(stdout)) )
		return(0);		/* BAD */

	return(1);			/* OK */
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	register int	fd;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit(1);
	}

	/* Obtain output buffer */
	if( (buf = (char *)malloc( bufsize )) == NULL )  {
		perror("malloc");
		exit(1);
	}

	if( bu_optind >= argc )  {
		/* Perform operation once, from stdin */
		fileout( 0, "-" );
		exit(0);
	}

	/* Perform operation on each argument */
	for( ; bu_optind < argc; bu_optind++ )  {
		if( (fd = open( argv[bu_optind], 0 )) < 0 )  {
			perror( argv[bu_optind] );
			/*
			 *  It is unclear whether an exit(1),
			 *  or continuing with the next file
			 *  is really the right thing here.
			 *  If the intended size was known,
			 *  a null "file" could be written to tape,
			 *  to preserve the image numbering.
			 *  For now, punt.
			 */
			exit(1);
		}
		fileout( fd, argv[bu_optind] );
		(void)close(fd);
	}
	exit(0);
}

/*
 *			F I L E O U T
 */
void
fileout(register int fd, char *name)
{
	register int	count, out;

	while( (count = bu_mread( fd, buf, bufsize )) > 0 )  {
		if( count < bufsize )  {
			/* Short read, zero rest of buffer */
			bzero( buf+count, bufsize-count );
		}
		if( (out = write( 1, buf, bufsize )) != bufsize )  {
			perror("files-tape: write");
			fprintf(stderr, "files-tape:  %s, write ret=%d\n", name, out);
			exit(1);
		}
		if( byteswritten < TSIZE && byteswritten+bufsize > TSIZE )
			fprintf(stderr, "files-tape: WARNING:  Tape capacity reached in file %s\n", name);
		byteswritten += bufsize;
	}
	if( count == 0 )
	    return;

	perror("READ ERROR");

	exit(1);
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
