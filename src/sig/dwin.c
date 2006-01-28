/*                          D W I N . C
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
/** @file dwin.c
 *
 *  Extract sliding windows of double values.
 *  Apply window functions if desired.
 */
#include "common.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <stdio.h>
#include <stdlib.h> /* for atof() */
#include <math.h>

#include "machine.h"

/*
 * Buffering stuff
 */
#define	BSIZE	16*1024		/* Must be AT LEAST 2*Points in spectrum */
double	buf[BSIZE];		/* input data buffer */
double	temp[BSIZE];		/* windowed data buffer */
int	input_sample = 0;	/* The *next* input sample ("file pointer") */

int	buf_start = 0;		/* sample number in buf[0] */
int	buf_num = 0;		/* number of samples currently in buffer */
int	buf_index = 0;		/* buffer offset for current window */

int	xform_start = 0;	/* current window start/end */
int	xform_end = 0;

#define	START_IN_BUFFER		(xform_start < buf_start+buf_num)
#define	END_NOT_IN_BUFFER	(xform_end >= buf_start+buf_num)

int	window = 0;
int	hamming = 0;
int	bias = 0;
int	bartlett = 0;
static int	endwin = 0;
int	midwin = 0;

void	fill_buffer(void);
void	seek_sample(int n);
void	biaswin(double *data, int L);
void	bartwin(double *data, int L);
void	hamwin(double *data, int length);
void	coswin(double *data, int length, double percent);

static char usage[] = "\
Usage: dwin [options] [width (1024)] [step (width)] [start]\n\
  -w  apply window (80%% split Cosine)\n\
  -h  apply Hamming window\n\
  -b  apply Bartlett window (triangle)\n\
  -B  apply bias window (half triangle)\n\
  -e  start first sample at end of buffer\n\
  -m  start first sample at middle of buffer\n\
";

int main(int argc, char **argv)
{
	int	L, step;

	if( isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	while( argc > 1 ) {
		if( strcmp(argv[1], "-w") == 0 ) {
			window++;
		} else if( strcmp(argv[1], "-h") == 0 ) {
			window++;
			hamming++;
		} else if( strcmp(argv[1], "-B") == 0 ) {
			window++;
			bias++;
		} else if( strcmp(argv[1], "-b") == 0 ) {
			window++;
			bartlett++;
		} else if( strcmp(argv[1], "-e") == 0 ) {
			endwin++;
		} else if( strcmp(argv[1], "-m") == 0 ) {
			midwin++;
		} else
			break;
		argc--;
		argv++;
	}

	L = (argc > 1) ? atoi(argv[1]) : 1024;
	if( argc > 2 ) {
		double	f;
		f = atof(argv[2]);
		if( f < 1.0 )
			step = f * L;
		else
			step = f;
	} else
		step = L;

	/* compute xform start/end */
	if( endwin )
		xform_start = -L + 1;	/* one sample at end */
	else if( midwin )
		xform_start = -L/2;	/* odd - center, even - just after */
	else
		xform_start = 0;
	xform_end = xform_start + L-1;

	/* initialize data buffer */
	bzero( (char *)buf, BSIZE*sizeof(*buf) );
	buf_start = -BSIZE;
	buf_num = BSIZE;
	buf_index = 0;

	while( !feof( stdin ) ) {
#ifdef DEBUG
		fprintf(stderr,"\nWant to xform [%d %d]\n", xform_start, xform_end );
		fprintf(stderr,"Buffer contains %d samples, from [%d (%d)]\n", buf_num, buf_start, buf_start+buf_num-1 );
#endif /* DEBUG */
		if( START_IN_BUFFER ) {
			buf_index = xform_start - buf_start;
			if( END_NOT_IN_BUFFER ) {
#ifdef DEBUG
				fprintf(stderr,"\tend isn't in buffer.\n");
#endif /* DEBUG */
				/* Move start to origin */
				bcopy( &buf[buf_index], &buf[0], (buf_num-buf_index)*sizeof(*buf) );
				buf_start = xform_start;
				buf_num -= buf_index;
				buf_index = 0;
				fill_buffer();
			}
		} else {
#ifdef DEBUG
			fprintf(stderr,"\tstart isn't in buffer.\n");
#endif /* DEBUG */
			if( input_sample != xform_start )
				seek_sample( xform_start );
			buf_start = xform_start;
			buf_num = 0;
			buf_index = 0;
			fill_buffer();
			if( feof( stdin ) )
				break;
		}

#ifdef DEBUG
		fprintf(stderr, "Did samples %d to %d (buf_index = %d)\n", xform_start, xform_end, buf_index );
#endif /* DEBUG */
		if( window ) {
			bcopy( &buf[buf_index], temp, L*sizeof(*temp) );
			if( hamming )
				hamwin( temp, L ); /* Hamming window */
			else if( bartlett )
				bartwin( temp, L ); /* Bartlett window */
			else if( bias )
				biaswin( temp, L ); /* Bias window */
			else
				coswin( temp, L, 0.80 ); /* 80% cosine window */
			fwrite( temp, sizeof(*temp), L, stdout );
		} else {
			fwrite( &buf[buf_index], sizeof(*buf), L, stdout );
		}

		/* Bump out pointers */
		xform_start += step;
		xform_end = xform_start + L-1;
	}

	return 0;
}

/*
 * Move input pointer to sample n.
 * Since we may be reading from a pipe, we actually
 * read and discard the samples.
 * Can only seek forward.
 */
void
seek_sample(int n)
{
	double	foo;

	fprintf(stderr,"seeking sample %d\n", n );
	while( input_sample < n ) {
		fread( &foo, sizeof(foo), 1, stdin );
		input_sample++;
	}
}

/*
 * Fill the data buffer from the current input location.
 */
void
fill_buffer(void)
{
	int	n, num_to_read;

	num_to_read = BSIZE - buf_num;

#ifdef DEBUG
fprintf(stderr, "fillbuffer: buf_start = %d, buf_num = %d, numtoread = %d, buf_index = %d\n",
buf_start, buf_num, num_to_read, buf_index );
#endif /* DEBUG */
	n = fread( &buf[buf_num], sizeof(*buf), num_to_read, stdin );
	if( n == 0 ) {
		/*fprintf( stderr, "EOF\n" );*/
		bzero( (char *)&buf[buf_num],
			sizeof(*buf)*num_to_read );
		return;
	}
	input_sample += n;
	buf_num += n;
	if( n < num_to_read ) {
		bzero( (char *)&buf[buf_num],
			sizeof(*buf)*(num_to_read-n) );
		clearerr(stdin);	/* XXX HACK */
	}

#ifdef DEBUG
	fprintf(stderr,"filled buffer now has %d samples, [%d (%d)].  Input at %d\n", buf_num, buf_start, buf_start+buf_num-1, input_sample );
#endif /* DEBUG */
}

/* Bias window (half triangle) */
void
biaswin(double *data, int L)
{
	int	i;

	for (i = 0; i < L; i++) {
		data[i] *= (double)(L-i)/(double)L;
	}
}

/* Bartlett window (triangle) */
void
bartwin(double *data, int L)
{
	int	i;

	for (i = 0; i < L/2; i++) {
		data[i] *= (double)i/(L/2.0);
	}
	for (i = L/2; i < L; i++) {
		data[i] *= (double)(L-i)/(L/2.0);
	}
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
