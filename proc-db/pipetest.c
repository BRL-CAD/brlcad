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

void do_bending();
void pr_pipe();

struct wdb_pipept  pipe1[] = {
	{
		{(long)WDB_PIPESEG_MAGIC, 0, 0},
		0, 1, 0,
		0.05, 0.1, 0.1
	},


	{
		{(long)WDB_PIPESEG_MAGIC, 0, 0},
		4, 5, 0,
		0.05, 0.1, 0.1
	},

	{
		{(long)WDB_PIPESEG_MAGIC, 0, 0},
		4, 9, 0,
		0.05, 0.1, 0.1
	},
	{
		{(long)WDB_PIPESEG_MAGIC, 0, 0},
		9, 9, 0,
		0.05, 0.1, 0.1
	}
};
int	pipe1_npts = sizeof(pipe1)/sizeof(struct wdb_pipept);

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
	struct wdb_pipept head;

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
	for( i=0; i<pipe1_npts ; i++ )  {
		RT_LIST_INSERT( &head.l, &pipe1[i].l );
	}
	pr_pipe( "pipe1", &head );
	if( (i = mk_pipe( stdout, "pipe1", &head )) < 0 )
		fprintf(stderr,"mk_pipe(%s) returns %d\n", "pipe1", i);

	do_bending( stdout, "pipe2", pipe2, pipe2_npts, 0.1, 0.05 );
}

void
do_bending( fp, name, pts, npts, bend, od )
FILE	*fp;
char	*name;
point_t	pts[];
int	npts;
double	bend;
double	od;
{
	struct wdb_pipept	head;
	struct wdb_pipept	*ps;
	vect_t			prev, next;
	point_t			my_end, next_start;
	int			i;

	RT_LIST_INIT( &head.l );
	ps = (struct wdb_pipept *)calloc(1,sizeof(struct wdb_pipept));
	ps->l.magic = WDB_PIPESEG_MAGIC;
	ps->pp_id = 0;
	ps->pp_od = od;
	ps->pp_bendradius = bend;
	VMOVE( ps->pp_coord, pts[0] );
	RT_LIST_INSERT( &head.l, &ps->l );

	for( i=1; i < npts-1; i++ )  {
		ps = (struct wdb_pipept *)calloc(1,sizeof(struct wdb_pipept));
		ps->l.magic = WDB_PIPESEG_MAGIC;
		ps->pp_id = 0;
		ps->pp_od = od;
		ps->pp_bendradius = bend;
		VMOVE( ps->pp_coord, pts[i] );
		RT_LIST_INSERT( &head.l, &ps->l );

	}

	pr_pipe( name, &head );

	if( ( i = mk_pipe( fp, name, &head ) ) < 0 )
		fprintf(stderr,"mk_pipe(%s) error %d\n", name, i );

	/* free the storage */
	mk_pipe_free( &head );
}

void
pr_pipe( name, head )
char	*name;
struct wdb_pipept *head;
{
	register struct wdb_pipept	*psp;

	fprintf(stderr,"\n--- %s:\n", name);
	for( RT_LIST_FOR( psp, wdb_pipept, &head->l ) )
	{
		fprintf(stderr,"id=%g od=%g, coord=(%g,%g,%g), bend radius=%g\n",
			psp->pp_id, psp->pp_od,
			psp->pp_coord[X],
			psp->pp_coord[Y],
			psp->pp_coord[Z],
			psp->pp_bendradius );
	}
}
