/*
 *			C A D _ P A R E A
 *
 *	cad_parea -- area of polygon
 *
 *  Author -
 *	D A Gwyn
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
static char	sccsid[] = "@(#)cad_parea.c	1.6";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"

#include	"./vld_std.h"


typedef struct
	{
	double	x;			/* X coordinate */
	double	y;			/* Y coordinate */
}	point;			/* polygon vertex */

static bool	GetArgs(), Input();
static void	Output(), Usage();


static void
Usage() 				/* print usage message */
{
	(void)printf( "usage: cad_parea[ -i input][ -o output]\n"
	    );
}


main( argc, argv )			/* "cad_parea" entry point */
int		argc;		/* argument count */
char		*argv[];	/* argument strings */
{
	point		previous;	/* previous point */
	point		current;	/* current point */
	point		first;		/* saved first point */
	register bool	saved;		/* "`first' valid" flag */
	double		sum;		/* accumulator */

	if ( !GetArgs( argc, argv ) )	/* process command arguments */
	{
		Output( 0.0 );
		return 1;
	}

	saved = false;
	sum = 0.0;

	while ( Input( &current ) )
	{			/* scan input record */
		if ( !saved )
		{		/* first input only */
			first = current;
			saved = true;
		}
		else	/* accumulate cross-product */
			sum += previous.x * current.y -
			    previous.y * current.x;

		previous = current;
	}

	if ( saved )			/* normally true */
		sum += previous.x * first.y - previous.y * first.x;

	Output( sum / 2.0 );
	return 0;			/* success! */
}


static bool
GetArgs( argc, argv )			/* process command arguments */
int		argc;		/* argument count */
char		*argv[];	/* argument strings */
{
	static bool	iflag = false;	/* set if "-i" option found */
	static bool	oflag = false;	/* set if "-o" option found */
	int		c;		/* option letter */

	while ( (c = bu_getopt( argc, argv, "i:o:" )) != EOF )
		switch ( c )
		{
		case 'i':
			if ( iflag )
			{
				(void)printf(
				    "cad_parea: too many -i options\n"
				    );
				return false;
			}
			iflag = true;

			if ( strcmp( bu_optarg, "-" ) != 0
			    && freopen( bu_optarg, "r", stdin ) == NULL
			    )	{
				(void)printf(
				    "cad_parea: can't open \"%s\"\n",
				    bu_optarg
				    );
				return false;
			}
			break;

		case 'o':
			if ( oflag )
			{
				(void)printf(
				    "cad_parea: too many -o options\n"
				    );
				return false;
			}
			oflag = true;

			if ( strcmp( bu_optarg, "-" ) != 0
			    && freopen( bu_optarg, "w", stdout ) == NULL
			    )	{
				(void)printf(
				    "cad_parea: can't create \"%s\"\n",
				    bu_optarg
				    );
				return false;
			}
			break;

		case '?':
			Usage();	/* print usage message */
			return false;
		}

	return true;
}


static bool
Input( coop )				/* input a coordinate record */
register point	*coop;		/* -> input coordinates */
{
	char		inbuf[82];	/* input record buffer */

	while ( fgets( inbuf, (int)sizeof inbuf, stdin ) != NULL )
	{			/* scan input record */
		register int	cvt;	/* # converted fields */

		cvt = sscanf( inbuf, " %le %le", &coop->x, &coop->y );

		if ( cvt == 0 )
			continue;	/* skip color, comment, etc. */

		if ( cvt == 2 )
			return true;	/* successfully converted */

		(void)printf( "cad_parea: bad input:\n%s\n", inbuf
		    );
		Output( 0.0 );
		exit( 2 );		/* return false insufficient */
	}

	return false;			/* EOF */
}


static void
Output( result )			/* output computed area */
double	result; 		/* computed area */
{
	printf( "%g\n", result );
}
