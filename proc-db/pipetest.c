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

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "wdb.h"

main(argc, argv)
char	**argv;
{
	point_t	vert;
	vect_t	h;

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
}
