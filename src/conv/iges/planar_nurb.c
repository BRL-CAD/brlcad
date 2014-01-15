/*                   P L A N A R _ N U R B . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
/** @file iges/planar_nurb.c
 *
 * Checks if nurb surface is planar, returns 1 if so, 0 otherwise
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

int
planar_nurb(int entityno)
{
    int sol_num = 0;		/* IGES solid type number */
    int k1 = 0, k2 = 0;		/* Upper index of sums */
    int m1 = 0, m2 = 0;		/* degree */

    if (dir[entityno]->param <= pstart) {
	bu_log("Illegal parameter pointer for entity D%07d (%s)\n" ,
	       dir[entityno]->direct, dir[entityno]->name);
	return 0;
    }

    Readrec(dir[entityno]->param);
    Readint(&sol_num, "");
    if (sol_num != 128) {
	bu_log("entity at D%07d is not a B-spline surface\n", entityno*2 + 1);
	return 0;
    }
    Readint(&k1, "");
    Readint(&k2, "");
    Readint(&m1, "");
    Readint(&m2, "");

    if (m1 == 1 && m2 == 1)
	return 1;
    else
	return 0;
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
