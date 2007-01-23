/*                         G L O B E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file globe.c
 * Creates a set of concentric "shells" that, when put together, comprise a
 * unified solid spherical object.  Kinda like an onion, not a cake, an onion.
 *
 */
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "machine.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

char *progname ="globe";

void usage(void)
{
	fprintf(stderr, "Usage: %s db_file.g [stepSize [finalSize [initialSize]]]\n", progname);
	exit(-1);
}

int
main(int ac, char *av[])
{
	struct rt_wdb *db_fp;
	point_t p1;
	int is_region;
	unsigned char rgb[3];
	struct wmember wm_hd; /* defined in wdb.h */
	struct wmember bigList;
	double initialSize=900.0;
	double finalSize=1000.0;
	double stepSize=10.0;
	double currentSize=0.0;
	int counter=0;
	char name[256]="";
	char solidName[256]="";
	char prevSolid[256]="";
	char shaderparams[256]="";

	progname = *av;

	if (ac < 2) usage();

	if (ac > 2) stepSize=(double)atof(av[2]);
	if (ac > 3) finalSize=(double)atof(av[3]);
	if (ac > 4) initialSize=(double)atof(av[4]);

	if ((db_fp = wdb_fopen(av[1])) == NULL) {
		perror(av[1]);
		exit(-1);
	}

	mk_id(db_fp, "Globe Database"); /* create the database header record */

	/* make a region that is the union of these two objects
	 * To accomplish this, we need to create a linked list of the
	 * items that make up the combination.  The wm_hd structure serves
	 * as the head of the list of items.
	 */
	BU_LIST_INIT(&wm_hd.l);
	BU_LIST_INIT(&bigList.l);

	/*
	 * make the CORE of the globe with a given color
	 ***************/
	VSET(p1, 0.0, 0.0, 0.0);
	VSET(rgb, 130, 253, 194); /* some green */
	is_region = 1;
	/* make a sphere centered at 1.0, 2.0, 3.0 with radius .75 */
	mk_sph(db_fp, "land.s", p1, initialSize);
	mk_addmember("land.s", &wm_hd.l, NULL, WMOP_UNION);
	mk_lcomb(db_fp, "land.c", &wm_hd, 0, "", "", rgb, 0);
	mk_addmember("land.s", &wm_hd.l, NULL, WMOP_UNION);
	mk_lcomb(db_fp, "land.r", &wm_hd, is_region, "plastic", "di=.8 sp=.2", rgb, 0);

	/*
	 * make the AIR of the globe with a given color
	 ***************/
	VSET(rgb, 130, 194, 253); /* a light blue */
	sprintf(prevSolid, "land.s");
	for (counter=0, currentSize=initialSize+stepSize; currentSize < finalSize; counter += 1, currentSize+=stepSize) {
	  BU_LIST_INIT(&wm_hd.l);

	  sprintf(solidName, "air.%d.s", counter);
	  mk_sph(db_fp, solidName , p1, currentSize);
	  mk_addmember(solidName, &wm_hd.l, NULL, WMOP_UNION);
	  mk_addmember(prevSolid, &wm_hd.l, NULL, WMOP_SUBTRACT);

		/* make the spatial combination */
	  sprintf(name, "air.%d.c", counter);
		mk_lcomb(db_fp, name, &wm_hd, 0, NULL, NULL, NULL, 0);

	  mk_addmember(name, &wm_hd.l, NULL, WMOP_UNION);

		/*	  sprintf(shaderparams, "{alpha %f}", (float)1.0 - (((float)finalSize/(float)currentSize)-(float)1.0));  */

		/* make the spatial region */
	  sprintf(name, "air.%d.r", counter);
		sprintf(shaderparams, "{tr %f}", (float)currentSize/(float)finalSize);
	  mk_lcomb(db_fp,
						 name,	/* Name of the db element created */
						 &wm_hd,		/* list of elements & boolean operations */
						 is_region,	/* Flag:  This is a region */
						 "plastic",	/* optical shader */
						 shaderparams, /* shader parameters */
						 rgb,		/* item color */
						 0);		/* inherit (override) flag */

		/* add the region to a master region list */
	  mk_addmember(name, &bigList.l, NULL, WMOP_UNION);

		/* keep track of the last combination we made for the next iteration */
	  sprintf(prevSolid, "%s", solidName);
	}

	/* make one final air region that comprises all the air regions */
	mk_lcomb(db_fp, "air.c", &bigList, 0, NULL, NULL, NULL, 0);

	/* Create the master globe region
	 *
	 * In this case we are going to make it a region (hence the
	 * is_region flag is set, and we provide shader parameter information.
	 *
	 * When making a combination that is NOT a region, the region flag
	 * argument is 0, and the strings for optical shader, and shader
	 * parameters should (in general) be null pointers.
	 */

	/* add the land to the main globe object that gets created at the end */
	BU_LIST_INIT(&wm_hd.l);
	mk_addmember("land.r", &wm_hd.l, NULL, WMOP_UNION);
	mk_addmember("air.c", &wm_hd.l, NULL, WMOP_UNION);

	mk_lcomb(db_fp,
					 "globe.r",	/* Name of the db element created */
					 &wm_hd,		/* list of elements & boolean operations */
					 is_region,	/* Flag:  This is a region */
					 NULL,	/* optical shader */
					 NULL, /* shader parameters */
					 NULL,		/* item color */
					 0);		/* inherit (override) flag */

	wdb_close(db_fp);
	return 0;
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
