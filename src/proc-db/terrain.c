/* 	T E R R A I N . C
 *
 * Create a random terrain spline model database.
 * Future additions to this program would be to include 
 * random trees and objects to be inserted into the database.
 *
 * Author -
 *	Paul R. Stay
 * 
 * Source
 *     SECAD/VLD Computing Consortium, Bldg 394
 *     The U.S. Army Ballistic Research Laboratory
 *     Aberdeen Proving Ground, Maryland 21005
 *
 * Copyright Notice -
 *     This software is Copyright (C) 1991-2004 by the United States Army.
 *     All rights reserved.
 */


/* Header files which are used for this example */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "machine.h"		/* BRLCAD specific machine data types */
#include "bu.h"
#include "vmath.h"		/* BRLCAD Vector macros */
#include "nurb.h"		/* BRLCAD Spline data structures */
#include "raytrace.h"
#include "wdb.h"

fastf_t grid[10][10][3];

char *Usage = "This program ordinarily generates a database on stdout.\n\
	Your terminal probably wouldn't like it.";

void interpolate_data(void);

struct face_g_snurb *surfs[100];
int nsurf = 0;

struct rt_wdb *outfp;

#ifndef HAVE_DRAND48
/* simulate drand48() --  using 31-bit random() -- assumed to exist */
double drand48() {
  extern long random();
  return (double)random() / 2147483648.0; /* range [0,1) */
}
#endif

int
main(int argc, char **argv)
{

	char * id_name = "terrain database";
	char * nurb_name = "terrain";
	int	i,j;
	fastf_t 	hscale;

	outfp = wdb_fopen("terrain.g");

	hscale = 2.5;


	while ((i=getopt(argc, argv, "dh:")) != EOF) {
		switch (i) {
		case 'd' : rt_g.debug |= DEBUG_MEM | DEBUG_MEM_FULL; break;
		case 'h' :
			hscale = atof( optarg );
			break;
		default	:
			(void)fprintf(stderr,
				"Usage: %s [-d] > database.g\n", *argv);
			return(-1);
		}
	}

	/* Create the database header record.
  	 * this solid will consist of three surfaces
	 * a top surface, bottom surface, and the sides 
	 * (so that it will be closed).
 	 */

	mk_id( outfp, id_name);

	for( i = 0; i < 10; i++)
		for( j = 0; j < 10; j++)
		{
			fastf_t		v;
			fastf_t		drand48(void);

			v = (hscale * drand48()) + 10.0;

			grid[i][j][0] = i;
			grid[i][j][1] = j;
			grid[i][j][2] = v;

		}

	interpolate_data();

	mk_bspline( outfp, nurb_name, surfs);
		
	return 0;
}

/* Interpoate the data using b-splines */
void
interpolate_data(void)
{
	struct face_g_snurb *srf;
	fastf_t * data;
	fastf_t rt_nurb_par_edge();

	data = &grid[0][0][0];

	BU_GETSTRUCT( srf, face_g_snurb );

	rt_nurb_sinterp( srf, 4, data, 10, 10 );
	rt_nurb_kvnorm( &srf->u );
	rt_nurb_kvnorm( &srf->v );

	surfs[nsurf++] = srf;
}
