/*
 *			P I P E T E S T . C
 *
 *  Program to generate test pipes and particles.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright 
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "rtlist.h"
#include "wdb.h"

struct wdb_pipeseg  pipe1[] = {
	{
		{(long)WDB_PIPESEG_MAGIC, 0, 0},
		0, 1, 0,
		0, 0, 0,
		0.05, 0.1, WDB_PIPESEG_TYPE_LINEAR
	},


	{
		{(long)WDB_PIPESEG_MAGIC, 0, 0},
		0, 5, 0,
		4, 5, 0,
		0.05, 0.1, WDB_PIPESEG_TYPE_BEND
	},

	{
		{(long)WDB_PIPESEG_MAGIC, 0, 0},
		4, 9, 0,
		0, 0, 0,
		0.05, 0.1, WDB_PIPESEG_TYPE_LINEAR
	},
	{
		{(long)WDB_PIPESEG_MAGIC, 0, 0},
		9, 9, 0,
		0, 0, 0,
		0.05, 0.1, WDB_PIPESEG_TYPE_END
	}
};

#define Q	0.05	/* inset from borders of enclsing cube */
#define R	0.05	/* pushout factor */

#define A	0+Q
#define B	1-Q

point_t	pipe2[] = {
	/* Front face, in Y= A-R plane */
	A-R, A-R, A+0.5,
	A-R, A-R, B,
	B, A-R, B,
	B, A-R, A-R,
	A, A-R, A-R,
	/* Bottom face, in Z= A-R plane */
	A, B, A-R,
	B+R, B, A-R,
	B+R, A, A-R,
	/* Right face, in X= B+R plane */
	B+R, A, B,
	B+R, B+R, B,
	B+R, B+R, A,
	/* Rear face, in Y= B+R plane */
	A, B+R, A,
	A, B+R, B+R,
	B, B+R, B+R,
	/* Top face, in Z= B+R plane */
	B, A, B+R,
	A-R, A, B+R,
	A-R, B, B+R,
	/* Left face, in X= A-R plane */
	A-R, B, A,
	A-R, A-R, A,
	A-R, A-R, A+0.2		/* "repeat" of first point */
};
int	pipe2_npts = sizeof(pipe2)/sizeof(point_t);

main(argc, argv)
char	**argv;
{
	point_t	vert;
	vect_t	h;
	int	i;
	struct wdb_pipeseg head;

	mk_conversion("meters");
	mk_id( stdout, "Pipe & Particle Test" );

	/* Spherical part */
	VSET( vert, 1, 0, 0 );
	VSET( h, 0, 0, 0 );
	mk_particle( stdout, "p1", vert, h, 0.5, 0.5 );

	/* Cylindrical part */
	VSET( vert, 3, 0, 0 );
	VSET( h, 2, 0, 0 );
	mk_particle( stdout, "p2", vert, h, 0.5, 0.5 );

	/* Conical particle */
	VSET( vert, 7, 0, 0 );
	VSET( h, 2, 0, 0 );
	mk_particle( stdout, "p3", vert, h, 0.5, 1.0 );

	/* Make a piece of pipe */
	RT_LIST_INIT( &head.l );
	for( i=0; ; i++ )  {
		RT_LIST_INSERT( &head.l, &pipe1[i].l );
		if( pipe1[i].ps_type == WDB_PIPESEG_TYPE_END )
			break;
	}
	pr_pipe( "pipe1", &head );
	if( (i = mk_pipe( stdout, "pipe1", &head )) < 0 )
		fprintf(stderr,"mk_pipe(%s) returns %d\n", "pipe1", i);

	do_bending( stdout, "pipe2", pipe2, pipe2_npts, 0.1, 0.05 );
}

do_bending( fp, name, pts, npts, bend, od )
FILE	*fp;
char	*name;
point_t	pts[];
int	npts;
double	bend;
double	od;
{
	struct wdb_pipeseg	head;
	struct wdb_pipeseg	*ps;
	vect_t			prev, next;
	point_t			my_end, next_start;
	int			i;

	RT_LIST_INIT( &head.l );
	ps = (struct wdb_pipeseg *)calloc(1,sizeof(struct wdb_pipeseg));
	ps->ps_type = WDB_PIPESEG_TYPE_LINEAR;
	ps->l.magic = WDB_PIPESEG_MAGIC;
	ps->ps_id = 0;
	ps->ps_od = od;
	VMOVE( ps->ps_start, pts[0] );
	RT_LIST_INSERT( &head.l, &ps->l );

	for( i=1; i < npts-1; i++ )  {
		VSUB2( prev, pts[i-1], pts[i] );
		VSUB2( next, pts[i+1], pts[i] );
		VUNITIZE( prev );
		VUNITIZE( next );
		VJOIN1( my_end, pts[i], bend, prev );
		VJOIN1( next_start, pts[i], bend, next );
		/* End the linear segment by starting the bend */
		ps = (struct wdb_pipeseg *)calloc(1,sizeof(struct wdb_pipeseg));
		ps->ps_type = WDB_PIPESEG_TYPE_BEND;
		ps->l.magic = WDB_PIPESEG_MAGIC;
		ps->ps_id = 0;
		ps->ps_od = od;
		VMOVE( ps->ps_start, my_end );
		VJOIN1( ps->ps_bendcenter, my_end, bend, next );
		RT_LIST_INSERT( &head.l, &ps->l );

		/* End the bend by starting the next linear section */
		ps = (struct wdb_pipeseg *)calloc(1,sizeof(struct wdb_pipeseg));
		ps->ps_type = WDB_PIPESEG_TYPE_LINEAR;
		ps->l.magic = WDB_PIPESEG_MAGIC;
		ps->ps_id = 0;
		ps->ps_od = od;
		VMOVE( ps->ps_start, next_start );
		RT_LIST_INSERT( &head.l, &ps->l );
	}

	ps = (struct wdb_pipeseg *)calloc(1,sizeof(struct wdb_pipeseg));
	ps->ps_type = WDB_PIPESEG_TYPE_END;
	ps->l.magic = WDB_PIPESEG_MAGIC;
	ps->ps_id = 0;
	ps->ps_od = od;
	VMOVE( ps->ps_start, pts[npts-1] );
	RT_LIST_INSERT( &head.l, &ps->l );

	pr_pipe( name, &head );

	if( ( i = mk_pipe( fp, name, &head ) ) < 0 )
		fprintf(stderr,"mk_pipe(%s) error %d\n", name, i );

	/* free the storage */
	mk_pipe_free( &head );
}

pr_pipe( name, head )
char	*name;
struct wdb_pipeseg *head;
{
	register struct wdb_pipeseg	*psp;

	fprintf(stderr,"\n--- %s:\n", name);
	for( RT_LIST_FOR( psp, wdb_pipeseg, &head->l ) )  {
		switch( psp->ps_type )  {
		case WDB_PIPESEG_TYPE_END:
			fprintf(stderr,"END	id=%g od=%g, start=(%g,%g,%g)\n",
				psp->ps_id, psp->ps_od,
				psp->ps_start[X],
				psp->ps_start[Y],
				psp->ps_start[Z] );
			break;
		case WDB_PIPESEG_TYPE_LINEAR:
			fprintf(stderr,"LINEAR	id=%g od=%g, start=(%g,%g,%g)\n",
				psp->ps_id, psp->ps_od,
				psp->ps_start[X],
				psp->ps_start[Y],
				psp->ps_start[Z] );
			break;
		case WDB_PIPESEG_TYPE_BEND:
			fprintf(stderr,"BEND	id=%g od=%g, start=(%g,%g,%g)\n",
				psp->ps_id, psp->ps_od,
				psp->ps_start[X],
				psp->ps_start[Y],
				psp->ps_start[Z] );
			fprintf(stderr,"		bendcenter=(%g,%g,%g)\n",
				psp->ps_bendcenter[X],
				psp->ps_bendcenter[Y],
				psp->ps_bendcenter[Z] );
			break;
		default:
			fprintf(stderr," *** unknown ps_type=%d psp=x%x ***\n",
				psp->ps_type, psp );
			break;
		}
	}
}
