/*
 *			X Y Z - P L . C
 *
 *  Program to take input with up to 3 white-space separated columns,
 *  expressed as
 *	x y z
 *  and produce a 3-D UNIX-plot file of the resulting space curve.
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
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include <stdio.h>
#include "conf.h"
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "plot3.h"


char	buf[2048];

int	debug = 0;

int
main(int argc, char *argv)
{
	double	xyz[3];
	int	i;
	int	first = 1;

	for(;;)  {
		xyz[0] = xyz[1] = xyz[2] = 0.0;

		buf[0] = '\0';
		fgets( buf, sizeof(buf), stdin );
		if( feof(stdin) )  break;
		i = sscanf( buf, "%lf %lf %lf",
			&xyz[0], &xyz[1], &xyz[2] );
		if(debug)  {
			fprintf(stderr,"buf=%s", buf);
			fprintf(stderr,"%d: %f\t%f\t%f\n",
				i, xyz[0], xyz[1], xyz[2] );
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
