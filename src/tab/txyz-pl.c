/*
 *			X Y Z - P L . C
 *
 *  Program to take (up to) 4-column input, expressed as
 *	time x y z
 *  and produce a plot file of the resulting space curve.
 *  Mostly useful to preview animation results.
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
 *	This software is Copyright (C) 1988-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include "machine.h"
#include "plot3.h"

char	buf[2048];

int	debug = 0;

int
main( argc, argv )
int	argc;
char	*argv[];
{
	double	t, xyz[3];
	int	i;
	int	first = 1;

	if( argc > 1 )  debug = 1;

	for(;;)  {
		t = xyz[0] = xyz[1] = xyz[2] = 0.0;

		buf[0] = '\0';
		fgets( buf, sizeof(buf), stdin );
		if( feof(stdin) )  break;

		if( buf[0] == '#' )  continue;

		i = sscanf( buf, "%lf %lf %lf %lf",
			&t, &xyz[0], &xyz[1], &xyz[2] );
		if(debug)  {
			fprintf(stderr,"buf=%s", buf);
			fprintf(stderr,"%d: %f\t%f\t%f\t%f\n",
				i, t, xyz[0], xyz[1], xyz[2] );
		}
		if( i <= 0 )
			break;
		if( first )  {
			first = 0;
			pdv_3move( stdout, xyz );
		} else {
			pdv_3cont( stdout, xyz );
		}
	}
	return 0;
}
