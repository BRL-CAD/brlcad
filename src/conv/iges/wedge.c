/*                         W E D G E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
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

#include "./iges_struct.h"
#include "./iges_extern.h"

int
wedge(int entityno)
{
    fastf_t xscale=0.0;
    fastf_t yscale=0.0;
    fastf_t zscale=0.0;
    fastf_t txscale=0.0;		/* top xscale */
    fastf_t x_1, y_1, z_1;		/* first vertex components */
    fastf_t x_2, y_2, z_2;		/* xdir vector components */
    fastf_t x_3, y_3, z_3;		/* zdir vector components */
    vect_t xdir;			/* a unit vector */
    vect_t xvec;			/* vector along x-axis */
    vect_t txvec;			/* vector along top x-axis */
    vect_t ydir;			/* a unit vector */
    vect_t yvec;			/* vector along y-axis */
    vect_t zdir;			/* a unit vector */
    vect_t zvec;			/* vector along z-axis */
    point_t pts[9];			/* array of points */
    int sol_num;		/* IGES solid type number */

    /* Default values */
    x_1 = 0.0;
    y_1 = 0.0;
    z_1 = 0.0;
    x_2 = 0.0;
    y_2 = 1.0;
    z_2 = 0.0;
    x_3 = 0.0;
    y_3 = 0.0;
    z_3 = 1.0;


    /* Acquiring Data */
    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[entityno]->direct, dir[entityno]->name);
	return 0;
    }
    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    Readcnv(&xscale, "");
    Readcnv(&yscale, "");
    Readcnv(&zscale, "");
    Readcnv(&txscale, "");
    Readcnv(&x_1, "");
    Readcnv(&y_1, "");
    Readcnv(&z_1, "");
    Readcnv(&x_2, "");
    Readcnv(&y_2, "");
    Readcnv(&z_2, "");
    Readcnv(&x_3, "");
    Readcnv(&y_3, "");
    Readcnv(&z_3, "");

    if (xscale <= 0.0 || yscale <= 0.0 || zscale <= 0.0) {
	bu_log("Illegal parameters for entity D%07d (%s)\n" ,
	       dir[entityno]->direct, dir[entityno]->name);
	return 0;
    }

    /*
     * Making the necessaries. First an id is made for the new entity.
     * Then the vertices for the bottom and top faces are found.  Point
     * is located in the lower left corner of the solid, and the vertices are
     * counted in the counter-clockwise direction, bottom face first.
     * Once all the vertices are identified, the points of these vertices are
     * loaded into an array of points and handed off to mk_arb8().
     * Make and unitize necessary vectors.
     */

    VSET(xdir, x_2, y_2, z_2);			/* Makes x-dir vector */
    VUNITIZE(xdir);
    VSET(zdir, x_3, y_3, z_3);			/* Make z-dir vector */
    VUNITIZE(zdir);
    VCROSS(ydir, zdir, xdir);		/* Make y-dir vector */

    /* Scale all vectors */

    VSCALE(xvec, xdir, xscale);
    VSCALE(txvec, xdir, txscale);
    VSCALE(zvec, zdir, zscale);
    VSCALE(yvec, ydir, yscale);

    /* Make the bottom face. */

    VSET(pts[0], x_1, y_1, z_1);		/* Yields first vertex */
    VADD2(pts[1], pts[0], xvec);		/* Finds second vertex */
    VADD2(pts[2], pts[1], yvec);		/* Finds third vertex */
    VADD2(pts[3], pts[0], yvec);		/* Finds fourth vertex */

    /* Make the top face by extruding the bottom face vertices.
     */

    VADD2(pts[4], pts[0], zvec);		/* Finds fifth vertex */
    VADD2(pts[5], pts[4], txvec);		/* Finds sixth vertex */
    VADD2(pts[6], pts[5], yvec);		/* Finds seventh vertex */
    VADD2(pts[7], pts[4], yvec);		/* Find eighth vertex */


    /* Now the information is handed off to mk_arb8(). */

    mk_arb8(fdout, dir[entityno]->name, &pts[0][X]);

    return 1;

}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
