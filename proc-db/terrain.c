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
 *     This software is Copyright (C) 1991 by the United States Army.
 *     All rights reserved.
 */


/* Header files which are used for this example */

#include <stdio.h>		/* Direct the output to stdout */
#include <stdlib.h>
#include <math.h>
#include "machine.h"		/* BRLCAD specific machine data types */
#include "db.h"			/* BRLCAD data base format */
#include "vmath.h"		/* BRLCAD Vector macros */
#include "nurb.h"		/* BRLCAD Spline data structures */
#include "raytrace.h"
#include "../librt/debug.h"	/* rt_g.debug flag settings */
#include "externs.h"

fastf_t grid[10][10][3];

char *Usage = "This program ordinarily generates a database on stdout.\n\
	Your terminal probably wouldn't like it.";

main(argc, argv)
int argc; char * argv[];
{

	char * id_name = "terrain database";
	char * nurb_name = "terrain";
	int	i,j;
	fastf_t 	hscale;

	if (isatty(fileno(stdout))) {
		(void)fprintf(stderr, "%s: %s\n", *argv, Usage);
		return(-1);
	}

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
			break;
		}
	}

	/* Create the database header record.
  	 * this solid will consist of three surfaces
	 * a top surface, bottom surface, and the sides 
	 * (so that it will be closed).
 	 */

	mk_id( stdout, id_name);
	mk_bsolid( stdout, nurb_name, 1, 1.0);

	for( i = 0; i < 10; i++)
		for( j = 0; j < 10; j++)
		{
			fastf_t		v;
			fastf_t		drand48();

			v = (hscale * drand48()) + 10.0;

			grid[i][j][0] = i;
			grid[i][j][1] = j;
			grid[i][j][2] = v;

		}

	interpolate_data();
		

}
/* Interpoate the data using b-splines */

interpolate_data()
{
	struct snurb srf;
	struct snurb *srf2, *srf3;
	struct knot_vector new_kv;
	fastf_t * data;
	fastf_t rt_nurb_par_edge();
	fastf_t step;
	fastf_t tess;

	data = &grid[0][0][0];

	rt_nurb_sinterp( &srf, 4, data, 10, 10 );
	rt_nurb_kvnorm( &srf.u_knots );
	rt_nurb_kvnorm( &srf.v_knots );

	mk_bsurf(stdout, &srf);

}
