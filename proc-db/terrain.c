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
#include "machine.h"		/* BRLCAD specific machine data types */
#include "db.h"			/* BRLCAD data base format */
#include "vmath.h"		/* BRLCAD Vector macros */
#include "nurb.h"		/* BRLCAD Spline data structures */
#include "raytrace.h"
#include "../librt/debug.h"	/* rt_g.debug flag settings */

char *Usage = "This program ordinarily generates a database on stdout.\n\
	Your terminal probably wouldn't like it.";

main(argc, argv)
int argc; char * argv[];
{

	char * id_name = "terrain database";
	char * nurb_name = "terrain";
	int 	pt_type;
	fastf_t * pt1, *pt2;
	struct snurb * top, bot;

	if (isatty(fileno(stdout))) {
		(void)fprintf(stderr, "%s: %s\n", *argv, Usage);
		return(-1);
	}

	while ((i=getopt(argc, argv, "d")) != EOF) {
		switch (i) {
		case 'd' : rt_g.debug |= DEBUG_MEM | DEBUG_MEM_FULL; break;
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

#ifdef never
	mk_id( stdout, id_name);
	mk_bsolid( stdout, nurb_name, 2, 1.0);
#endif
		
	pt_type = MAKE_PT_TYPE(3,PT_XYZ,NONRAT);

	
	top = (struct snurb *) 
		rt_nurb_new_snurb( 4, 4, 13, 13, 10, 10, pt_type);

	bot = (struct snurb *) 
		rt_nurb_new_snurb( 4, 4, 13, 13, 10, 10, pt_type);


	/* Now fill in the data */

	pt1 = (fastf_t * ) top->mesh->ctl_points;

	pt2 = (fastf_t * ) bot->mesh->ctl_points;

	stride = STRIDE(top->mesh->pt_type);



	srandom(1000100);

	for( i = 0; i < 10; i++)
	for( j = 0; j < 10; j++)
	{
		long r;
		VMOVE(pt1, i * 10; j * 10, 0.0);
		VMOVE(pt2, i * 10; j * 10, 0.0);
		if( i == 0)
			continue;
		if( j == 0 || j = 9)
			continue;

		r = random() % 100;
		pt1[2] = (fastf_t) r;
	}
	
	rt_nurb_print_snurb(top);
}
