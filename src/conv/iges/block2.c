/*                        B L O C K 2 . C
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
block(int entityno)
{

    fastf_t xscale=0.0;
    fastf_t yscale=0.0;
    fastf_t zscale=0.0;
    fastf_t x_1, y_1, z_1;		/* First vertex components */
    fastf_t x_2, y_2, z_2;		/* xdir vector components */
    fastf_t x_3, y_3, z_3;		/* zdir vector components */
    point_t v;			/* the first vertex */
    vect_t xdir;			/* a unit vector */
    vect_t xvec;			/* vector along x-axis */
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
    x_2 = 1.0;
    y_2 = 0.0;
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
    Readcnv(&x_1, "");
    Readcnv(&y_1, "");
    Readcnv(&z_1, "");
    Readflt(&x_2, "");
    Readflt(&y_2, "");
    Readflt(&z_2, "");
    Readflt(&x_3, "");
    Readflt(&y_3, "");
    Readflt(&z_3, "");

    if (xscale <= 0.0 || yscale <= 0.0 || zscale <= 0.0) {
	bu_log("Illegal parameters for entity D%07d (%s)\n" ,
	       dir[entityno]->direct, dir[entityno]->name);
	return 0;
    }

    /*
     * Making the necessaries. First an id is made for the new entity.
     * Then the vertices for the bottom and top faces are found.  Point
     * is located in the lower left corner of the solid, and the vertices are
     * counted in the counter-clockwise direction, around the bottom face.
     * Next these vertices are extruded to form the top face.  The points
     * thus made are loaded into an array of points and handed off to mk_arb8().
     * Make and unitize necessary vectors.
     */

    VSET(xdir, x_2, y_2, z_2);			/* Makes x-dir vector */
    VUNITIZE(xdir);
    VSET(zdir, x_3, y_3, z_3);			/* Make z-dir vector */
    VUNITIZE(zdir);
    VCROSS(ydir, zdir, xdir);		/* Make y-dir vector */

    /* Scale all vectors */

    VSCALE(xvec, xdir, xscale);
    VSCALE(zvec, zdir, zscale);
    VSCALE(yvec, ydir, yscale);

    /* Make the bottom face. */

    VSET(v, x_1, y_1, z_1);			/* Yields first vertex */
    VMOVE(pts[0], v);			/* put first vertex into array */
    VADD2(pts[1], v, xvec);			/* Finds second vertex */
    VADD3(pts[2], v, xvec, yvec);		/* Finds third vertex */
    VADD2(pts[3], v, yvec);			/* Finds fourth vertex */

    /* Now extrude the bottom face to make the top.
     */

    VADD2(pts[4], v, zvec);			/* Finds fifth vertex */
    VADD2(pts[5], pts[1], zvec);		/* Finds sixth vertex */
    VADD2(pts[6], pts[2], zvec);		/* Finds seventh vertex */
    VADD2(pts[7], pts[3], zvec);		/* Find eighth vertex */


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
