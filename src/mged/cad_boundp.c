/*                    C A D _ B O U N D P . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file cad_boundp.c
 *
 *	cad_boundp -- bounding polygon of two-dimensional view
 *
 *  Author -
 *	D A Gwyn
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 * NOTES FOR MAINTAINER:
 *
 *	This program is somewhat slow when operating on large input
 *	sets.  The following ideas should improve the speed, possibly
 *	at the expense of maximum data set size:
 *
 *	(1)  Merge the Chop() and Build() phases.  Since Chop() examines
 *	segment endpoints anyway, it is in a good position to build the
 *	linked lists used by Search().
 *
 *	(2)  Sort the input endpoints into a double tree on X and Y.
 *	This should cut Chop() from order N ^ 2 to N * log( N ).
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <math.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef HAVE_STDARG_H
#include	<stdarg.h>
#else
#include	<varargs.h>
#endif

#include "machine.h"
#include "bu.h"

#include "./vld_std.h"

typedef struct
	{
	/* The following are float instead of double, to save space: */
	float		x;		/* X coordinate */
	float		y;		/* Y coordinate */
}	coords; 		/* view-coordinates of point */

typedef struct segment
{
	struct segment	*links; 	/* segment list thread */
	coords		sxy;		/* (X,Y) coordinates of start */
	coords		exy;		/* (X,Y) coordinates of end */
}	segment;		/* entry in list of segments */

struct queue;				/* (to clear from name space) */

typedef struct point
{
	struct point	*linkp; 	/* point list thread */
	struct queue	*firstq;	/* NULL once output */
	coords		xy;		/* (X,Y) coordinates of point */
}	point;			/* entry in list of points */

typedef struct queue
{
	struct queue	*nextq; 	/* endpoint list thread */
	point		*endpoint;	/* -> endpt of ray from point */
}	queue;			/* entry in list of endpoints */

#ifdef HAVE_STDARG_H
static bool	Mess(char *fmt, ...);
#else
static bool	Mess();
#endif
static bool	Build(void), Chop(void), EndPoint(register coords *p, register segment *segp), GetArgs(int argc, char **argv), Input(register segment *inp),
Near(register coords *ap, register coords *bp), Search(void), Split(coords *p, register segment *oldp, register segment *listh), Usage(void);
static coords	*Intersect(register segment *a, register segment *b);
static point	*LookUp(register coords *coop), *NewPoint(register coords *coop), *PutList(register coords *coop);
static pointer	Alloc(unsigned int size);
static queue	*Enqueue(register point *addp, register point *startp);
static void	Output(register coords *coop), Toss(register pointer ptr);

static bool	initial = true; 	/* false after first Output */
static bool	vflag = false;		/* set if "-v" option found */
static double	tolerance = 0.0;	/* point matching slop */
static double	tolsq = 0.0;		/* `tolerance' ^ 2 */
static point	*headp = NULL;		/* head of list of points */
static segment	seghead = {
	&seghead };	/* segment list head */


static bool
Usage(void) 				/* print usage message */
{
	return
	    Mess( "usage: cad_boundp[ -i input][ -o output][ -t tolerance][ -v]" );
}


int
main(int argc, char **argv)			/* "cad_boundp" entry point */
   	     			/* argument count */
    	        		/* argument strings */
{
	if ( !GetArgs( argc, argv ) )	/* process command arguments */
		return 1;

	if ( !Chop() )			/* input; chop into segments */
		return 2;

	if ( !Build() ) 		/* build linked lists */
		return 3;

	if ( !Search() )		/* output bounding polygon */
		return 4;

	return 0;			/* success! */
}


static bool
GetArgs(int argc, char **argv)			/* process command arguments */
   		     		/* argument count */
    		        	/* argument strings */
{
	static bool	iflag = false;	/* set if "-i" option found */
	static bool	oflag = false;	/* set if "-o" option found */
	static bool	tflag = false;	/* set if "-t" option found */
	int		c;		/* option letter */

#ifdef	DEBUG
	(void)Mess( "\n\t\tGetArgs\n" );
#endif
	while ( (c = bu_getopt( argc, argv, "i:o:t:v" )) != EOF )
		switch ( c )
		{
		case 'i':
			if ( iflag )
				return
				    Mess( "too many -i options" );
			iflag = true;

			if ( strcmp( bu_optarg, "-" ) != 0
			    && freopen( bu_optarg, "r", stdin ) == NULL
			    )
				return
				    Mess( "can't open \"%s\"", bu_optarg );
			break;

		case 'o':
			if ( oflag )
				return
				    Mess( "too many -o options" );
			oflag = true;

			if ( strcmp( bu_optarg, "-" ) != 0
			    && freopen( bu_optarg, "w", stdout ) == NULL
			    )
				return
				    Mess( "can't create \"%s\"", bu_optarg );
			break;

		case 't':
			if ( tflag )
				return
				    Mess( "too many -t options" );
			tflag = true;

			if ( sscanf( bu_optarg, "%le", &tolerance ) != 1 )
				return
				    Mess( "bad tolerance: %s", bu_optarg );
			tolsq = tolerance * tolerance;
			break;

		case 'v':
			vflag = true;
			break;

		case '?':
			return Usage(); /* print usage message */
		}

	return true;
}


/*
	The following phase is required to chop up line segments that
	intersect; this guarantees that all polygon vertices will be
	found at segment endpoints.  Unfortunately, this is an N^2
	process.  It is also hard to do right; thus the elaborateness.
*/

static bool
Chop(void)					/* chop vectors into segments */
{
	segment *inp;			/* -> input whole segment */

#ifdef	DEBUG
	(void)Mess( "\n\t\tChop\n" );
#endif
	/* Although we try to Alloc() when no input remains, it is more
	   efficient than repeatedly copying the input into *inp.     */
	while ( (inp = (segment *)Alloc( (unsigned)sizeof(segment) )) != NULL
	    && Input( inp )
	    ) {
		register segment	*segp;	/* segment list entry */
		segment 		piecehead;
		/* list of inp pieces */
		register segment	*pp;	/* -> pieces of `inp' */

		/* Reverse the segment if necessary to get endpoints
		   in left-to-right order; speeds up range check. */

		if ( inp->sxy.x > inp->exy.x )
		{
			coords	temp;		/* store for swapping */

#ifdef	DEBUG
			(void)Mess( "endpoints swapped" );
#endif
			temp = inp->sxy;
			inp->sxy = inp->exy;
			inp->exy = temp;
		}

		/* Split input and segment list entries into pieces. */

		inp->links = &piecehead;
		piecehead.links = inp;	/* initially, one piece */

		/* Note:  Although the split-off pieces of `segp' are
		   linked in front of `segp', that's okay since they
		   cannot intersect `inp' again!		      */
		for ( segp = seghead.links; segp != &seghead;
		    segp = segp->links
		    )
			/* Similarly, once a piece of `inp' intersects
			   `segp', we can stop looking at the pieces. */
			for ( pp = piecehead.links; pp != &piecehead;
			    pp = pp->links
			    )	{
				register coords *i;	/* intersectn */

				/* Be careful; `i' -> static storage
				   internal to Intersect().	      */
				i = Intersect( pp, segp );
				if ( i != NULL )
				{
					if ( ( !EndPoint( i, pp  )
					    && !Split( i, pp, &piecehead ) )
					    || ( !EndPoint( i, segp )
					    && !Split( i, segp, &seghead ) )
					    )	/* out of memory */
						return false;

					break;	/* next `segp' */
				}
			}
#ifdef	DEBUG
		(void)Mess( "new input pieces:" );
		for ( pp = piecehead.links; pp != &piecehead;
		    pp = pp->links
		    )
			(void)Mess( "\t(%g,%g)->(%g,%g)",
			    (double)pp->sxy.x,
			    (double)pp->sxy.y,
			    (double)pp->exy.x,
			    (double)pp->exy.y
			    );
		(void)Mess( "other segments:" );
		for ( segp = seghead.links; segp != &seghead;
		    segp = segp->links
		    )
			(void)Mess( "\t(%g,%g)->(%g,%g)",
			    (double)segp->sxy.x,
			    (double)segp->sxy.y,
			    (double)segp->exy.x,
			    (double)segp->exy.y
			    );
#endif

		/* Put input segment pieces into segment list. */

		/* Note:  It is better to scan the pieces to find the
		   tail link than to waste storage on double links. */
		for ( pp = piecehead.links; pp->links != &piecehead;
		    pp = pp->links
		    )
			;
		pp->links = seghead.links;	/* last-piece link */
		seghead.links = piecehead.links;	/* add pieces */
	}
	Toss( (pointer)inp );		/* unused segment at EOF */

	return inp != NULL;
}


static bool
Split(coords *p, register segment *oldp, register segment *listh) 		/* split segment in two */
      			   	/* -> break point */
                	      	/* -> segment to be split */
                	        /* -> list to attach tail to */
{
	register segment	*newp;	/* -> new list entry */

#ifdef	DEBUG
	(void)Mess( "split (%g,%g)->(%g,%g) at (%g,%g)",
	    (double)oldp->sxy.x, (double)oldp->sxy.y,
	    (double)oldp->exy.x, (double)oldp->exy.y,
	    (double)p->x, (double)p->y
	    );
#endif
	if ( (newp = (segment *)Alloc( (unsigned)sizeof(segment) )) == NULL )
		return false;		/* out of heap space */
	newp->links = listh->links;
	newp->sxy = *p;
	newp->exy = oldp->exy;
	oldp->exy = *p; 		/* old entry, new endpoint */
	listh->links = newp;		/* attach to list head */

	return true;
}


static bool
Build(void) 				/* build linked lists */
{
	register segment	*listp; /* -> segment list entry */
	segment 		*deadp; /* -> list entry to be freed */

#ifdef	DEBUG
	(void)Mess( "\n\t\tBuild\n" );
#endif
	/* When we are finished, `seghead' will become invalid. */
	for ( listp = seghead.links; listp != &seghead;
	    deadp = listp, listp = listp->links, Toss( (pointer)deadp )
	    )	{
		register point	*startp, *endp; /* -> segment endpts */

		if ( (startp = PutList( &listp->sxy )) == NULL
		    || (endp   = PutList( &listp->exy )) == NULL
		    || Enqueue( startp, endp ) == NULL
		    || Enqueue( endp, startp ) == NULL
		    )
			return false;	/* out of heap space */
	}

	return	true;
}


static bool
Search(void)				/* output bounding polygon */
{
	double		from;		/* backward edge direction */
	register point	*currentp;	/* -> current (start) point */
	point		*previousp;	/* -> previous vertex point */

#ifdef	DEBUG
	(void)Mess( "\n\t\tSearch\n" );
#endif
	if ( headp == NULL )
		return true;		/* trivial case */

	/* Locate the lowest point; this is the first polygon vertex. */
	{
		float		miny;		/* smallest Y coordinate */
		register point	*listp; 	/* -> next point in list */

		currentp = listp = headp;
		miny = currentp->xy.y;
		while ( (listp = listp->linkp) != NULL )
			if ( listp->xy.y < miny )
			{
				miny = listp->xy.y;
				currentp = listp;
			}
	}
	previousp = NULL;		/* differs from currentp! */
	from = 270.0;

	/* Output point and look for next CCW point on periphery. */

	for ( ; ; )
	{
		coords		first;	/* first output if -v */
		double		mindir; /* smallest from->to angle */
		register point	*nextp = (point *)NULL; /* -> next perimeter point */
		queue		*endq;	/* -> endpoint queue entry */

#ifdef	DEBUG
		(void)Mess( "from %g", from );
#endif
		endq = currentp->firstq;
		if ( endq == NULL )
		{
			if ( vflag && !initial )
				Output( &first );	/* closure */

			return true;	/* been here before => done */
		}

		Output( &currentp->xy );	/* found vertex */
		if ( vflag && initial )
		{
			initial = false;
			first = currentp->xy;	/* save for closure */
		}

		/* Find the rightmost forward edge endpoint. */

		mindir = 362.0;
		do	{
			double		to;	/* forward edge dir */
			double		diff;	/* angle from->to */
			register point	*endp;	/* -> endpoint */

			endp = endq->endpoint;
			if ( endp == previousp	/* don't double back! */
			|| endp == currentp	/* don't stay here! */
			)
				continue;

			/* Note: it would be possible to save some calls
			   to atan2 by being clever about quadrants.  */
			if ( endp->xy.y == currentp->xy.y
			    && endp->xy.x == currentp->xy.x
			    )
				to = 0.0;	/* not supposed to happen */
			else
				to = atan2( (double)
				    (endp->xy.y - currentp->xy.y),
				    (double)
				    (endp->xy.x - currentp->xy.x)
				    ) * DEGRAD;
#ifdef	DEBUG
			(void)Mess( "to %g", to );
#endif
			diff = to - from;
			/* Note: Exact 360 (0) case is not supposed to
			   happen, but this algorithm copes with it.  */
			while ( diff <= 0.0 )
				diff += 360.0;

			if ( diff < mindir )
			{
#ifdef	DEBUG
				(void)Mess( "new min %g", diff );
#endif
				mindir = diff;
				nextp = endp;
			}
		} while ( (endq = endq->nextq) != NULL );

		if ( mindir > 361.0 )
			return Mess( "degenerate input" );

		currentp->firstq = NULL;	/* "visited" */
		previousp = currentp;
		currentp = nextp;

		from += mindir + 180.0; /* reverse of saved "to" */
		/* The following is needed only to improve accuracy: */
		while ( from > 360.0 )
			from -= 360.0;	/* reduce to range [0,360) */
	}
}


static point *
PutList(register coords *coop) 			/* return -> point in list */
                      		/* -> coordinates */
{
	register point	*p;		/* -> list entry */

	p = LookUp( coop );		/* may already be there */
	if ( p == NULL )		/* not yet in list */
	{			/* start new point group */
#ifdef	DEBUG
		(void)Mess( "new point group (%g,%g)",
		    (double)coop->x, (double)coop->y
		    );
#endif
		p = NewPoint( coop );
#if 0
			if ( p == NULL )
			return NULL;	/* out of heap space */
#endif
	}

	return p;			/* -> point list entry */
}


static point *
LookUp(register coords *coop)				/* find point group in list */
                      		/* -> coordinates */
{
	register point	*p;		/* -> list members */

	for ( p = headp; p != NULL; p = p->linkp )
		if ( Near( coop, &p->xy ) )
		{
#ifdef	DEBUG
			(void)Mess( "found (%g,%g) in list",
			    (double)coop->x, (double)coop->y
			    );
#endif
			return p;	/* found a match */
		}

	return NULL;			/* not yet in list */
}


static point *
NewPoint(register coords *coop)			/* add point to list */
                      		/* -> coordinates */
{
	register point	*newp;		/* newly allocated point */

	newp = (point *)Alloc( (unsigned)sizeof(point) );
	if ( newp == NULL )
		return NULL;

#ifdef	DEBUG
	(void)Mess( "add point (%g,%g)",
	    (double)coop->x, (double)coop->y
	    );
#endif
	newp->linkp = headp;
	newp->firstq = NULL;		/* empty endpoint queue */
	newp->xy = *coop;		/* coordinates */
	return headp = newp;
}


static queue *
Enqueue(register point *addp, register point *startp) 		/* add to endpoint queue */
              	      		/* -> point being queued */
              	        	/* -> point owning queue */
{
	register queue	*newq;		/* new queue element */

	newq = (queue *)Alloc( (unsigned)sizeof(queue) );
	if ( newq == NULL )
		return NULL;

#ifdef	DEBUG
	(void)Mess( "enqueue (%g,%g) on (%g,%g)",
	    (double)addp->xy.x, (double)addp->xy.y,
	    (double)startp->xy.x, (double)startp->xy.y
	    );
#endif
	newq->nextq = startp->firstq;
	newq->endpoint = addp;
	return startp->firstq = newq;
}


static coords *
Intersect(register segment *a, register segment *b)			/* determine intersection */
                	        /* segments being tested */
{
	double			det;	/* determinant, 0 if parallel */
	double			xaeas, xbebs, yaeas, ybebs;
	/* coordinate differences */

	/* First perform range check, to eliminate most cases.	Note
	   that segments point left-to-right even after splitting. */

	if ( a->sxy.x > 		/* a left */
	b->exy.x + tolerance	/* b right */
	|| a->exy.x < 		/* a right */
	b->sxy.x - tolerance	/* b left */
	|| Min( a->sxy.y, a->exy.y ) >		/* a bottom */
	Max( b->sxy.y, b->exy.y ) + tolerance	/* b top */
	|| Max( a->sxy.y, a->exy.y ) <		/* a top */
	Min( b->sxy.y, b->exy.y ) - tolerance	/* b bottom */
	)	{
#ifdef	DEBUG
		(void)Mess( "ranges don't intersect" );
#endif
		return NULL;		/* can't intersect */
	}

	/* Passed quick check, now comes the hard part. */

	xaeas = (double)a->exy.x - (double)a->sxy.x;
	xbebs = (double)b->exy.x - (double)b->sxy.x;
	yaeas = (double)a->exy.y - (double)a->sxy.y;
	ybebs = (double)b->exy.y - (double)b->sxy.y;

	det = xbebs * yaeas - xaeas * ybebs;

	{
		double	norm;			/* norm of coefficient matrix */
		double	t;			/* test value for norm */

		norm = 0.0;
		if ( (t = Abs( xaeas )) > norm )
			norm = t;
		if ( (t = Abs( xbebs )) > norm )
			norm = t;
		if ( (t = Abs( yaeas )) > norm )
			norm = t;
		if ( (t = Abs( ybebs )) > norm )
			norm = t;

#define EPSILON 1.0e-06 		/* relative `det' size thresh */
		if ( Abs( det ) <= EPSILON * norm * norm )
		{
#ifdef	DEBUG
			(void)Mess( "parallel: det=%g, norm=%g", det, norm );
#endif
			return NULL;		/* parallels don't intersect */
		}
#undef	EPSILON
	}
	{
		/* `p' must be static; Intersect returns a pointer to it! */
		static coords	p;		/* point of intersection */
		double		lambda, mu;	/* segment parameters */
		double		onemmu; 	/* 1.0 - mu, for efficiency */
		double		xbsas, ybsas;	/* more coord differences */

		xbsas = (double)b->sxy.x - (double)a->sxy.x;
		ybsas = (double)b->sxy.y - (double)a->sxy.y;

		mu = (xbebs * ybsas - xbsas * ybebs) / det;
		onemmu = 1.0 - mu;
		p.x = onemmu * a->sxy.x + mu * a->exy.x;
		p.y = onemmu * a->sxy.y + mu * a->exy.y;
		if ( (onemmu < 0.0 || mu < 0.0) && !EndPoint( &p, a ) )
		{
#ifdef	DEBUG
			(void)Mess( "intersect off (%g,%g)->(%g,%g): mu=%g",
			    (double)a->sxy.x, (double)a->sxy.y,
			    (double)a->exy.x, (double)a->exy.y,
			    mu
			    );
#endif
			return NULL;		/* not in segment *a */
		}

		lambda = (xaeas * ybsas - xbsas * yaeas) / det;
		if ( (lambda > 1.0 || lambda < 0.0) && !EndPoint( &p, b ) )
		{
#ifdef	DEBUG
			(void)Mess( "intersect off (%g,%g)->(%g,%g): lambda=%g",
			    (double)b->sxy.x, (double)b->sxy.y,
			    (double)b->exy.x, (double)b->exy.y,
			    lambda
			    );
#endif
			return NULL;		/* not in segment *b */
		}

#ifdef	DEBUG
		(void)Mess( "intersection is (%g,%g): mu=%g lambda=%g",
		    (double)p.x, (double)p.y, mu, lambda
		    );
#endif
		return &p;
	}
}


static bool
EndPoint(register coords *p, register segment *segp)			/* check for segment endpoint */
                	   	/* -> point being tested */
                	      	/* -> segment */
{
#ifdef	DEBUG
	if ( Near( p, &segp->sxy ) || Near( p, &segp->exy ) )
		(void)Mess( "(%g,%g) is endpt of (%g,%g)->(%g,%g)",
		    (double)p->x, (double)p->y,
		    (double)segp->sxy.x, (double)segp->sxy.y,
		    (double)segp->exy.x, (double)segp->exy.y
		    );
#endif
	return Near( p, &segp->sxy ) || Near( p, &segp->exy );
}


static bool
Near(register coords *ap, register coords *bp)				/* check if within tolerance */
                         	/* -> points being checked */
{
	double		xsq, ysq;	/* dist between coords ^ 2 */

	/* Originally this was an abs value test; this is neater. */

	xsq = ap->x - bp->x;
	xsq *= xsq;
	ysq = ap->y - bp->y;
	ysq *= ysq;

#ifdef	DEBUG
	if ( xsq + ysq <= tolsq )
		(void)Mess( "(%g,%g) is near (%g,%g)",
		    (double)ap->x, (double)ap->y,
		    (double)bp->x, (double)bp->y
		    );
#endif
	return xsq + ysq <= tolsq;
}


static pointer
Alloc(unsigned int size)				/* allocate storage from heap */
        		     	/* # bytes required */
{
	register pointer	ptr;	/* -> allocated storage */

	if ( (ptr = malloc( size * sizeof(char) )) == NULL )
		(void)Mess( "out of memory" );

	return ptr;			/* (may be NULL) */
}


static void
Toss(register pointer ptr)				/* return storage to heap */
                	    	/* -> allocated storage */
{
	if ( ptr != NULL )
		free( ptr );
}


/*VARARGS*/
static bool
#ifdef HAVE_STDARG_H
Mess( char *fmt, ... )			/* print error message */
#else
Mess( va_alist )			/* print error message */
va_dcl					/* format, optional arguments */
#endif
{
	va_list		ap;		/* for accessing arguments */
#ifndef HAVE_STDARG_H
	register char	*fmt;		/* format */

	va_start( ap );
	fmt = va_arg( ap, char * );
#else
	va_start( ap, fmt );
#endif
	(void)fflush( stdout );
	(void)fputs( "cad_boundp: ", stderr );
#ifdef HAVE_VPRINTF
	(void)vprintf( fmt, ap );
#else
	(void) _doprnt( fmt, ap, stderr );
#endif
	(void)fputc( '\n', stderr );

	va_end( ap );

	return false;
}


static bool
Input(register segment *inp)				/* input stroke record */
                	     	/* -> input segment */
{
	char			inbuf[82];	/* record buffer */

	while ( fgets( inbuf, (int)sizeof inbuf, stdin ) != NULL )
	{			/* scan input record */
		register int	cvt;	/* # fields converted */

#ifdef	DEBUG
		(void)Mess( "input: %s", inbuf );
#endif
		cvt = sscanf( inbuf, " %e %e %e %e",
		    &inp->sxy.x, &inp->sxy.y,
		    &inp->exy.x, &inp->exy.y
		    );

		if ( cvt == 0 )
			continue;	/* skip color, comment, etc. */

		if ( cvt == 4 )
			return true;	/* successfully converted */

		(void)Mess( "bad input: %s", inbuf );
		exit( 5 );		/* return false insufficient */
	}

	return false;			/* EOF */
}


static void
Output(register coords *coop)				/* dump polygon vertex coords */
                      		/* -> coords to be output */
{
	static coords	last;		/* previous *coop */

	if ( vflag )
	{
		if ( !initial )
			printf( "%g %g %g %g\n",
			    (double)last.x, (double)last.y,
			    (double)coop->x, (double)coop->y
			    );

		last = *coop;		/* save for next start point */
	}
	else
		printf( "%g %g\n",
		    (double)coop->x, (double)coop->y
		    );
#ifdef	DEBUG
	(void)Mess( "output: %g %g",
	    (double)coop->x, (double)coop->y
	    );
#endif
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
