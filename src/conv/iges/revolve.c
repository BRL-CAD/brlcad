/*                       R E V O L V E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
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
/** @file revolve.c
 *  Authors -
 *	John R. Anderson
 *	Susanne L. Muuss
 *	Earl P. Weaver
 *
 *  Source -
 *	VLD/ASB Building 1065
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
/*				SOLID OF REVOLUTION	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void Addsub();

#ifdef M_PI
#define PI M_PI
#else
#define	PI	3.14159265358979
#endif

struct subtracts
{
	char *name;
	int index;
	struct subtracts *next;
};

struct trclist
{
	point_t base,top;
	fastf_t r1,r2;
	int op; /* 0 => union, 1=> subtract */
	int index;
	char name[NAMESIZE];
	struct subtracts *subtr;
	struct trclist *next,*prev;
};

int
revolve( entityno )
int entityno;
{
	struct wmember	head;			/* For region */
	char		*trcform="rev.%d.%d";	/* Format for creating TRC names */
	int		sol_num;		/* IGES solid type number */
	point_t		pt;			/* Point on axis of revolution */
	vect_t		adir;			/* Direction of axis of revolution */
	int		curve;			/* Pointer to driectory entry for curve */
	fastf_t		fract;			/* Fraction of circle for rotation (0 < fract <= 1.0) */
	vect_t		v1;			/* Vector from "pt" to any point along curve */
	fastf_t		h;			/* height of "TRC" */
	int		npts;			/* Number of points used to approximate curve */
	struct ptlist	*curv_pts,*ptr;		/* Pointer to a linked list of npts points along curve */
	int		ntrcs;			/* number of "TRC" solids used */
	vect_t		tmp;			/* temporary storage for a vector */
	struct trclist	*trcs,*trcptr,*ptr2;	/* Pointers to linked list of TRC`s */
	fastf_t		r2;			/* TRC radius */
	fastf_t		hmax,hmin;		/* Max and Min distances along axis of rotation */
	fastf_t		rmax;			/* Max radius */
	int		cutop = Intersect;	/* Operator for cutting solid */
	char		cutname[NAMESIZE];	/* Name for cutting solid */
	struct subtracts *subp;
	int		i;

	BU_LIST_INIT( &head.l );

	/* Default values */
	VSET( adir , 0.0 , 0.0 , 1.0 );
	VSET( pt , 0.0 , 0.0 , 0.0 );
	fract = 1.0;

	/* Acquire data */

	if( dir[entityno]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}
	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );

	/* Read pointer to directory entry for curve to be extruded */

	Readint( &curve , "" );

	/* Convert this to a "dir" index */

	curve = (curve-1)/2;

	Readflt( &fract , "" );
	Readflt( &pt[X] , "" );
	Readflt( &pt[Y] , "" );
	Readflt( &pt[Z] , "" );
	Readflt( &adir[X] , "" );
	Readflt( &adir[Y] , "" );
	Readflt( &adir[Z] , "" );

	/* just to be safe */
	VUNITIZE( adir );

	if( fract <= 0.0 || fract > 1.0 )
	{
		bu_log( "Illegal parameters for entity D%07d (%s)\n" ,
			dir[entityno]->direct , dir[entityno]->name );
		return( 0 );
	}

	dir[entityno]->referenced = 1;

	/* Get the curve in the form of a series of straight line segments */

	npts = Getcurve( curve , &curv_pts );
	if( npts == 0 )
	{
		bu_log( "Could not get points along curve for revovling\n" );
		bu_log( "Illegal parameters for entity D%07d (%s)\n" ,
			dir[entityno]->direct , dir[entityno]->name );
		return( 0 );
	}

/* Construct a linked list of TRC's */
	ntrcs = 0;
	trcs = NULL;
	ptr = curv_pts;

	/* Calculate radius at start of curve */
	VSUB2( v1 , ptr->pt , pt );
	VCROSS( tmp , v1 , adir );
	r2 = MAGNITUDE( tmp );
	if( r2 < TOL )
		r2 = TOL;
	rmax = r2;
	hmax = VDOT( v1 , adir );
	hmin = hmax;

	trcptr = NULL;
	while( ptr->next != NULL )
	{
		struct trclist *prev;
		fastf_t h1;

		if( trcs == NULL )
		{
			trcs = (struct trclist *)bu_malloc( sizeof( struct trclist ),
				"Revolve: trcs" );
			trcptr = trcs;
			prev = NULL;
		}
		else if( trcptr->name[0] != '\0' )
		{
			trcptr->next = (struct trclist *)bu_malloc( sizeof( struct trclist ),
				"Revolve: trcptr->next" );
			prev = trcptr;
			trcptr = trcptr->next;
		}
		else  prev = NULL;
		trcptr->next = NULL;
		trcptr->prev = prev;
		trcptr->op = 0;
		trcptr->subtr = NULL;
		trcptr->name[0] = '\0';

		/* Calculate base point of TRC */
		VSUB2( v1 , ptr->pt , pt );
		VJOIN1( trcptr->base , pt , VDOT( v1 , adir ) , adir );

		/* Height along axis of rotation */
		h1 = VDOT( v1 , adir );
		if( h1 < hmin )
			hmin = h1;
		if( h1 > hmax )
			hmax = h1;

		/* Radius at base is top radius from previous TRC */
		trcptr->r1 = r2;

		/* Calculate new top radius */
		VSUB2( v1 , ptr->next->pt , pt );
		VCROSS( tmp , v1 , adir );
		trcptr->r2 = MAGNITUDE( tmp );
		if( trcptr->r2 < TOL )
			trcptr->r2 = TOL;
		r2 = trcptr->r2;
		if( r2 > rmax )
			rmax = r2;

		/* Calculate height of TRC */
		VSUB2( v1 , ptr->next->pt , pt );
		VJOIN1( trcptr->top , pt , VDOT( v1 , adir ) , adir );
		VSUB2( v1 , trcptr->top , trcptr->base );
		h = MAGNITUDE( v1 );
		/* If height is zero, don't make a TRC */
		if( NEAR_ZERO( h , TOL ) )
		{
			ptr = ptr->next;
			continue;
		}

		/* Make a name for the TRC */
		sprintf( trcptr->name , trcform , entityno , ntrcs );

		/* Make the TRC */
		if( mk_trc_top( fdout, trcptr->name, trcptr->base,
		    trcptr->top, trcptr->r1, trcptr->r2 ) < 0 )  {
			bu_log( "Unable to write TRC for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
			return( 0 );
		}

		/* Count 'em */
		ntrcs++;
		ptr = ptr->next;
	}

	/* Eliminate last struct if not used */
	if( trcptr->name[0] == '\0' )
	{
		trcptr->prev->next = NULL;
		bu_free( (char *)trcptr, "Revolve: trcptr" );
	}

	if( dir[entityno]->form == 1 ) /* curve closed on itself */
	{
		trcptr = trcs;
		while( trcptr != NULL )
		{
			fastf_t hb1,ht1,hb2,ht2; /* distance from "pt" to bottom and top of TRC's */
			fastf_t	rtmp;	/* interpolated radii for TRC */
			fastf_t tmpp;	/* temp storage */

			/* Calculate distances to top and base */
			VSUB2( tmp , trcptr->base , pt );
			hb1 = MAGNITUDE( tmp );
			VSUB2( tmp , trcptr->top , pt );
			ht1 = MAGNITUDE( tmp );
			/* Make sure distance to base is smaller */
			if( ht1 < hb1 )
			{
				tmpp = ht1;
				ht1 = hb1;
				hb1 = tmpp;
			}

			/* Check every TRC against this one */
			ptr2 = trcs;
			while( ptr2 != NULL )
			{
				if( ptr2 == trcptr ) /* but not itself */
					ptr2 = ptr2->next;
				else
				{
					/* Calculate heights */
					VSUB2( tmp , ptr2->base , pt );
					hb2 = MAGNITUDE( tmp );
					VSUB2( tmp , ptr2->top , pt );
					ht2 = MAGNITUDE( tmp );
					/* and order them */
					if( ht2 < hb2 )
					{
						tmpp = ht2;
						ht2 = hb2;
						hb2 = tmpp;
					}
					if( hb2 < ht1 && hb2 > hb1 )
					{
						/* These TRC's overlap */
						/* Calculate radius at hb2 */
						rtmp = trcptr->r1 + (trcptr->r2 - trcptr->r1)*(hb2-hb1)/(ht1-hb1);
						if( rtmp > ptr2->r1 )
						{
							/* ptr2 must be an inside solid, so subtract it */
							Addsub( trcptr , ptr2 );
							ptr2->op = 1;
						}
						else if( rtmp < ptr2->r1 )
						{
							/* trcptr must be an inside solid */
							Addsub( ptr2 , trcptr );
							trcptr->op = 1;
						}
					}
					else if( ht2 < ht1 && ht2 > hb1 )
					{
						/* These TRC's overlap */
						/* Calculate radius at ht2 */
						rtmp = trcptr->r1 + (trcptr->r2 - trcptr->r1)*(ht2-hb1)/(ht1-hb1);
						if( rtmp > ptr2->r2 )
						{
							/* ptr2 must be an inside solid, so subtract it */
							Addsub( trcptr , ptr2 );
							ptr2->op = 1;
						}
						else if( rtmp < ptr2->r1 )
						{
							/* trcptr must be an inside solid */
							Addsub( ptr2 , trcptr );
							trcptr->op = 1;
						}
					}
					ptr2 = ptr2->next;
				}
			}
			trcptr = trcptr->next;
		}
	}

	if( fract < 1.0 )
	{
		/* Must calculate a cutting solid */
		vect_t pdir,enddir,startdir;
		fastf_t len,theta;
		point_t pts[8];

		/* Calculate direction from axis to curve */
		len = 0.0;
		ptr = curv_pts;
		while( len == 0.0 )
		{
			VSUB2( pdir , ptr->pt , pt );
			VJOIN1( startdir , pdir , -VDOT( pdir , adir ) , adir );
			len = MAGNITUDE( startdir );
			ptr = ptr->next;
		}
		VUNITIZE( startdir );

		/* Calculate direction towards solid from axis */
		VCROSS( pdir , adir , startdir );
		VUNITIZE( pdir );

		if( fract < 0.5 )
		{
			theta = 2.0*PI*fract;
			cutop = Intersect;
		}
		else if( fract > 0.5 )
		{
			theta = (-2.0*PI*(1.0-fract));
			cutop = Subtract;
		}
		else
		{
			/* XXX fract == 0.5, a dangerous comparison (roundoff) */
			theta = PI;
			cutop = Intersect;
			/* Construct vertices for cutting solid */
			VJOIN2( pts[0] , pt , hmin , adir , rmax , startdir );
			VJOIN1( pts[1] , pts[0] , (-2.0*rmax) , startdir );
			VJOIN1( pts[2] , pts[1] , rmax , pdir );
			VJOIN1( pts[3] , pts[0] , rmax , pdir );
			for( i=0 ; i<4 ; i++ )
			{
				VJOIN1( pts[i+4] , pts[i] , (hmax-hmin) , adir );
			}
		}
		if( fract != 0.5 )
		{
			/* Calculate direction to end of revolve */
			VSCALE( enddir , startdir , cos( theta ) );
			VJOIN1( enddir , enddir , sin( theta ) , pdir );
			VUNITIZE( enddir );

			/* Calculate required length of a side */
			len = rmax/cos( theta/4.0 );

			/* Construct vertices for cutting solid */
				/* Point at bottom center of revolution */
			VJOIN1( pts[0] , pt , hmin , adir );
				/* Point at bottom on curve */
			VJOIN1( pts[1] , pts[0] , len , startdir );
				/* Point at bottom at end of revolution */
			VJOIN1( pts[3] , pts[0] , len , enddir );
				/* Calculate direction to pts[2] */
			VADD2( enddir , enddir , startdir );
			VUNITIZE( enddir );
				/* Calculate pts[2] */
			VJOIN1( pts[2] , pts[0] , len , enddir );

			/* Calculate top vertices */
			for( i=0 ; i<4 ; i++ )
			{
				VJOIN1( pts[i+4] , pts[i] , (hmax-hmin) , adir );
			}
		}

		/* Make the BRL-CAD solid */
		if( mk_arb8( fdout , cutname , &pts[0][X] ) < 0 )  {
			bu_log( "Unable to write ARB8 for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
			return( 0 );
		}
	}

	/* Build region */
	trcptr = trcs;
	while( trcptr != NULL )
	{
		/* Union together all the TRC's that are not subtracts */
		if( trcptr->op != 1 )
		{
			(void)mk_addmember( trcptr->name , &head.l, NULL, operator[Union] );

			if( fract < 1.0 )
			{
				/* include cutting solid */
				(void)mk_addmember( cutname , &head.l, NULL, operator[cutop] );
			}

			subp = trcptr->subtr;
			/* Subtract the inside TRC's */
			while( subp != NULL )
			{
				(void)mk_addmember( subp->name , &head.l, NULL, operator[Subtract] );
				subp = subp->next;
			}
		}
		trcptr = trcptr->next;
	}

	/* Make the object */
	if( mk_lcomb( fdout , dir[entityno]->name , &head , 0 ,
	    (char *)0 , (char *)0 , (unsigned char *)0 , 0 ) < 0 )  {
		bu_log( "Unable to make combination for entity D%07d (%s)\n" ,
			dir[entityno]->direct , dir[entityno]->name );
		return( 0 );
	}


	/* Free the TRC structures */
	trcptr = trcs;
	while( trcptr != NULL )
	{
		bu_free( (char *)trcptr, "Revolve: trcptr" );
		trcptr = trcptr->next;
	}
	return( 1 );
}

/* Routine to add a name to the list of subtractions */
void
Addsub( trc , ptr )
struct trclist *trc,*ptr;
{
	struct subtracts *subp;

	if( trc->subtr == NULL )
	{
		trc->subtr = (struct subtracts *)bu_malloc( sizeof( struct subtracts ),
			"Revolve: trc->subtr" );
		subp = trc->subtr;
	}
	else
	{
		subp = trc->subtr;
		while( subp->next != NULL )
			subp = subp->next;
		subp->next = (struct subtracts *)bu_malloc( sizeof( struct subtracts ),
			"Revolve: subp->next" );
		subp = subp->next;
	}

	subp->next = NULL;
	subp->name = ptr->name;
	subp->index = ptr->index;
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
