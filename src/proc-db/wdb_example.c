/*                   W D B _ E X A M P L E . C
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
/** @file wdb_example.c
 *  Create a BRL-CAD geometry database from C code.
 *
 *  Note that this is for writing (creating/appending) a database.
 *  There is currently no API for modifying a database.
 *
 *  Note that since the values in the database are stored in millimeters.
 *  This constrains the arguments to the mk_* routines to also be in
 *  millimeters.
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


char *progname ="(noname)";

void usage(void)
{
    fprintf(stderr, "Usage: %s db_file.g\n", progname);
}

int
main(int ac, char *av[])
{
    struct rt_wdb *db_fp;
    point_t p1, p2;
    int is_region;
    unsigned char rgb[3];
    struct wmember wm_hd; /* defined in wdb.h */

    progname = *av;

    if (ac < 2) {
	usage();
	return 1;
    }

    if ((db_fp = wdb_fopen(av[1])) == NULL) {
	perror(av[1]);
	return 1;
    }

    mk_id(db_fp, "My Database"); /* create the database header record */

    /* all units in the database file are stored in millimeters.  This
     * constrains the arguments to the mk_* routines to also be in
     * millimeters
     */


    /* make a sphere centered at 1.0, 2.0, 3.0 with radius .75 */
    VSET(p1, 1.0, 2.0, 3.0);
    mk_sph(db_fp, "ball.s", p1, 0.75);


    /* make an rpp under the sphere (partly overlapping).  Note that
     * this really makes an arb8, but gives us a shortcut for
     * specifying the parameters.
     */
    VSET(p1, 0.0, 0.0, 0.0);
    VSET(p2, 2.0, 4.0, 2.5);
    mk_rpp(db_fp, "box.s", p1, p2);

    /* make a region that is the union of these two objects To
     * accomplish this, we need to create a linked list of the items
     * that make up the combination.  The wm_hd structure serves as
     * the head of the list of items.
     */
    BU_LIST_INIT(&wm_hd.l);

    /* Create a wmember structure for each of the items that we want
     * in the combination.  The return from mk_addmember is a pointer
     * to the wmember structure
     */
    (void)mk_addmember( "box.s", &wm_hd.l, NULL, WMOP_UNION );

    /* If we wanted a transformation matrix for this arc, we could
     * have passed the matrix in to mk_addmember as an argument or we
     * could add the following code:
     *
     memcpy( wm_hd->wm_mat, trans_matrix, sizeof(mat_t));
     *
     * Remember that values in the database are stored in millimeters.
     * so the values in the matrix must take this into account.
     */

    /* add the second member to the database
     *
     * Note that there is nothing which checks to make sure that
     * "ball.s" exists in the database when you create the wmember
     * structure OR when you create the combination.  So mis-typing
     * the name of a sub-element for a region/combination can be a
     * problem.
     */

    (void)mk_addmember( "ball.s", &wm_hd.l, NULL, WMOP_UNION );

    /* Create the combination
     * In this case we are going to make it a region (hence the
     * is_region flag is set, and we provide shader parameter information.
     *
     * When making a combination that is NOT a region, the region flag
     * argument is 0, and the strings for optical shader, and shader
     * parameters should (in general) be null pointers.
     */
    is_region = 1;
    VSET(rgb, 64, 180, 96); /* a nice green */
    mk_lcomb(db_fp,
	     "box_n_ball.r",	/* Name of the db element created */
	     &wm_hd,		/* list of elements & boolean operations */
	     is_region,	/* Flag:  This is a region */
	     "plastic",	/* optical shader */
	     "di=.8 sp=.2", /* shader parameters */
	     rgb,		/* item color */
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
