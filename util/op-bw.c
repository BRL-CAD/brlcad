/*
 *			O P - B W . C
 *
 *  Read 8-bit (.bw) images from an Optronics Scanner
 *
 *  Author -
 *	Phillip Dykstra
 *
 * Acknowledgment:
 * 	This grew out of opread.c by
 *	Doug Kingston and
 *	Charles M. Kennedy
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

# include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef vax
# include <vaxuba/opio.h>
# include <sys/ioctl.h>

char	*malloc();

int	step = 4;	/* 12.5 micron steps per scanline */
int	offset = 0;	/* pixels in "left margin" */
int	roffset = 0;	/* Rotational offset (pixels) before read starts */
int	verbose = 0;
int	width = 512;
int	nlines = 512;
char	*buf;

extern int	getopt();
extern char	*optarg;
extern int	optind;

static char usage[] = "\
Usage: op-bw [-hv] [-S step] [-x xoffset] [-y yoffset]\n\
        [-s squaresize] [-w width] [-n nlines] > file.bw\n";

/*
 *			G E T_ A R G S
 */
get_args( argc, argv )
register char **argv;
{

	register int c;

	while ( (c = getopt( argc, argv, "hvx:y:o:r:S:w:n:s:" )) != EOF )  {
		switch( c )  {
		case 'v':
			verbose++;
			break;
		case 'h':
			nlines = width = 1024;
			break;
		case 'y':
		case 'o':
			offset = atoi( optarg );
			break;
		case 'x':
		case 'r':
			roffset = atoi( optarg );
			break;
		case 'S':
			step = atoi( optarg );
			break;
		case 'w':
			width = atoi( optarg );
			break;
		case 'n':
			nlines = atoi( optarg );
			break;
		case 's':
			width = nlines = atoi( optarg );
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if ( argc > optind )
		(void)fprintf( stderr, "fbline: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main( argc, argv )
int	argc;
char	**argv;
{
	int	fd;
	int	status;
	struct opstate ops;
	struct opseek opseek;

	if ( !get_args( argc, argv ) || isatty(fileno(stdout)) ) {
		fputs( usage, stderr);
		exit( 1 );
	}

	if( width % 512 != 0 ) {
		fprintf( stderr, "op-bw: Warning, width is not a multiple of 512\n" );
	}
	if( (buf = malloc(width)) == NULL ) {
		fprintf( stderr, "op-bw: can't malloc a buffer for %d pixels\n", width );
		exit( 1 );
	}

	if (verbose)
		fprintf( stderr, "step=%d, offset=%d, rotational offset=%d\n",
			step, offset, roffset );

	if( (fd = open("/dev/op0", 0)) < 0 ) {
		perror("/dev/op0");
		exit(8);
	}
	/* Make Optronics fd stdin so dd can read from it. */
	dup2( fd, 0 );
	close( fd );

	sync();			/* Force disk update now so it will
				   not interrupt the Optronics */

	ops.op_var = OPINCR;
	ops.op_val = step;
	if( step && ioctl(0, OPIOCSET, &ops) < 0 ) {
		perror("OPIOCSET: OPINCR");
		fprintf( stderr, "op_var=%d, op_val=%d\n",
			ops.op_var, ops.op_val );
		exit(1);
	}

	ops.op_var = OPDRUM;
	ops.op_val = roffset;
	if( ioctl(0, OPIOCSET, &ops) < 0 ) {
		perror("OPIOCSET: OPDRUM");
		fprintf( stderr, "op_var=%d, op_val=%d\n",
			ops.op_var, ops.op_val );
		exit(1);
	}

	opseek.op_cmd = OPRIGHT;
	opseek.op_arg = offset;
	if( ioctl(0, OPIOSEEK, &opseek) < 0 ) {
		perror("OPIOSEEK: OPRIGHT");
		fprintf( stderr, "op_cmd=%d, op_arg=%d\n",
			opseek.op_cmd, opseek.op_arg );
		exit(1);
	}

	return( doit() );
}

doit()
{
	int	i, n;

	for( i = 0; i < nlines; i++ ) {
		if( (n = read( 0, buf, width )) != width ) {
			fprintf( stderr, "op-bw: read returned %d\n", n );
			return 1;
		}
		if( (n = write( 1, buf, width )) != width ) {
			fprintf( stderr, "op-bw: write returned %d\n", n );
			return 1;
		}
	}

	return 0;
}
#else
int
main()
{
	fprintf( stderr, "op-bw: this is a vax specific program\n" );
	exit( 1 );
}
#endif /* Not vax */
