/*
 *			F I L E S - T A P E
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
 *  3)  Input files are read using mread(), so operation will
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
 *  
 *  Copyright Notice -
 *	Public Domain, Distribution Unlimited
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "externs.h"		/* For malloc and getopt */

#define	TSIZE	(6144*24*1024)	/* # of bytes on 2400' 6250bpi reel */
long	byteswritten = 0;	/* Number of bytes written */

int	bufsize = 24*1024;	/* Default buffer size */
char	*buf;

void	fileout(register int fd, char *name);

int mread(int fd, char *bufp, int n );

static char usage[] = "\
Usage: files-tape [-b bytes] [-k Kbytes] [files]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "b:k:" )) != EOF )  {
		switch( c )  {
		case 'b':
			bufsize = atoi( optarg );	/* bytes */
			break;
		case 'k':
			bufsize = atoi( optarg ) * 1024; /* Kbytes */
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
	if( (buf = malloc( bufsize )) == NULL )  {
		perror("malloc");
		exit(1);
	}

	if( optind >= argc )  {
		/* Perform operation once, from stdin */
		fileout( 0, "-" );
		exit(0);
	}

	/* Perform operation on each argument */
	for( ; optind < argc; optind++ )  {
		if( (fd = open( argv[optind], 0 )) < 0 )  {
			perror( argv[optind] );
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
		fileout( fd, argv[optind] );
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

	while( (count = mread( fd, buf, bufsize )) > 0 )  {
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

	/* mread() logged the error, die */
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
			perror("files-tape: mread");
			return(-1);
		}
		if(nread == 0)
			return((int)count);
		count += (unsigned)nread;
		bufp += nread;
	 } while(count < n);

	return((int)count);
}
