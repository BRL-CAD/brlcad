/** @file cchannel.c
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file proc-db/cchannel.c
 *
 * Create a section of fully configurable c-channel
 *
 * Default creates a 1" diameter by 12" long section of american standard c-channel
 *
 */
#include "common.h"

#include <math.h>
#include <stdio.h>
#include "bn.h"
#include "bu.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "vmath.h"
#include "wdb.h"

struct channel
{
    double length;
    double diameter;
    double thickness;
    double x;
    double y;
    double z;
    double radius;
    double slope;
    double topR;
    double conversionFactor;
    int holes;
    double holeR;
    const char *filename;
};

static void
usage(char *s)
{
    if (s)
	bu_log("%s\n", s);
    bu_exit(1, "Usage: %s %s\n%s\n",
	    "cchannel", "-l length -d diameter -x X coordinate -y Y coordinate \n-z Z coordinate -H hole radius",
	    "-t thickness -s slope -r corner-radius -R top-radius \n-u conversion factor from millimeters -f filename");
}

static void
parseArgs(int argc, char **argv, const char* options, struct channel *parameters)
{
    int c;
    while ((c=bu_getopt(argc, argv, options)) != -1) {
	switch (c) {
	    case('l'):
		sscanf(bu_optarg, "%lf", &(parameters->length));
		break;
	    case('d'):
		sscanf(bu_optarg, "%lf", &(parameters->diameter));
		break;
	    case('x'):
		sscanf(bu_optarg, "%lf", &(parameters->x));
		break;
	    case('y'):
		sscanf(bu_optarg, "%lf", &(parameters->y));
		break;
	    case('z'):
		sscanf(bu_optarg, "%lf", &(parameters->z));
		break;
	    case('t'):
		sscanf(bu_optarg, "%lf", &(parameters->thickness));
		break;
	    case('s'):
		sscanf(bu_optarg, "%lf", &(parameters->slope));
		break;
	    case('r'):
		sscanf(bu_optarg, "%lf", &(parameters->radius));
		break;
	    case('H'):
		sscanf(bu_optarg, "%lf", &(parameters->holeR));
		parameters->holes = 1;
		break;
	    case('R'):
		sscanf(bu_optarg, "%lf", &(parameters->topR));
		break;
	    case('f'):
		parameters->filename = bu_optarg;
		break;
	    case('u'):
		sscanf(bu_optarg, "%lf", &(parameters->conversionFactor));
		break;
	    case('h'):
	    case('?'):
		usage("");
		break;
	    default:
		usage("error: default option reached");
	}
    }
    if (parameters->slope<.50 || parameters->slope > 1)
	usage("error: slope cannot be less than fifty percent or more than one hundred percent");
    if (parameters->topR < .5*parameters->thickness || EQUAL(parameters->thickness, 0))
	usage("error: top radius cannot be less than half the thickness");
    if (parameters->radius > .5*parameters->diameter)
	usage("error: corner radius cannot be more than one half the diameter");
    if (parameters->thickness > .5*parameters->diameter)
	usage("error: the thickness cannot be more than one half the diameter");
    if (parameters->length <= 0 || parameters->diameter <= 0 || parameters->thickness <= 0 || parameters->radius < 0 || parameters->topR < 0)
	usage("error: size parameters cannot be negative");
    if (parameters->holeR >= parameters->diameter * .5)
	usage("error: hole radius must be smaller than one half the channel diameter");
}

static void
addHoles(struct rt_wdb *db, struct channel parameters)
{
    int i = 0;
    int z2 = parameters.z + .5 * parameters.diameter;
    point_t pts[2];
    int dist = parameters.diameter;
    char name[64];
    struct wmember holeC;
    BU_LIST_INIT(&holeC.l);

    while (i * (dist + .5 * parameters.holeR) < parameters.length) {
	sprintf(name, "hole_%d", i);
	VSET(pts[0], parameters.x + parameters.thickness, parameters.y + .5 * parameters.diameter - .5 * parameters.thickness, z2 + i * (dist + .5 * parameters.holeR));
	VSET(pts[1], -1 * parameters.diameter - parameters.thickness * 2, 0, 0);
	mk_rcc(db, name, pts[0], pts[1], parameters.holeR);
	(void)mk_addmember(name, &holeC.l, NULL, WMOP_UNION);
	i++;
    }
    mk_lcomb(db, "holes.c", &holeC, 0, NULL, NULL, NULL, 0);
}

static void
makeArb(struct rt_wdb *db, double a, double b, double c, double width, double height, double sublength, char* name)
{
    point_t pts[8];
    VSET(pts[0], a, b, c);
    VSET(pts[1], a+width, b, c);
    VSET(pts[2], a+width, b+height, c);
    VSET(pts[3], a, b+height, c);
    VSET(pts[4], a, b, c+sublength);
    VSET(pts[5], a+width, b, c+sublength);
    VSET(pts[6], a+width, b+height, c+sublength);
    VSET(pts[7], a, b+height, c+sublength);
    mk_arb8 (db, name, &pts[0][0]);
}

static double
convert(double a, int all, struct channel *parameters)
{
    if (all == 0) {
	a = a * parameters->conversionFactor;
    }
    else {
	parameters->length *= parameters->conversionFactor;
	parameters->diameter *= parameters->conversionFactor;
	parameters->thickness *= parameters->conversionFactor;
	parameters->x *= parameters->conversionFactor;
	parameters->y *= parameters->conversionFactor;
	parameters->z *= parameters->conversionFactor;
	parameters->radius *= parameters->conversionFactor;
	parameters->topR *= parameters->conversionFactor;
	parameters->holeR *= parameters->conversionFactor;
    }
    return a;
}

int
main (int argc, char **argv)
{
    double center;
    point_t pts[8];
    point_t temp1, temp2;
    const char* options = "l:d:x:y:z:t:s:r:R:H:f:u:h:?:";
    struct wmember sub1;
    struct wmember sub2;
    struct wmember sub2a;
    struct wmember sub2b;
    struct wmember channel;
    struct rt_wdb *db;
    struct channel parameters;
    parameters.length = 12;
    parameters.diameter = 1;
    parameters.thickness = .125;
    parameters.x = 0;
    parameters.y = 0;
    parameters.z = 0;
    parameters.radius = .25;
    parameters.slope = .95;
    parameters.topR = .25;
    parameters.conversionFactor = 25.4;
    parameters.holes = 0;
    parameters.holeR = .25;
    parameters.filename = "channel.g";
    parseArgs(argc, argv, options, &parameters);
    convert(0, 1, &parameters);
    db = wdb_fopen(parameters.filename);
    center = sqrt(pow(parameters.topR,2) - pow(.5*parameters.thickness,2));

    makeArb(db, parameters.x, parameters.y, parameters.z, parameters.thickness, parameters.diameter, parameters.length, "arb_1");
    /* first wall */

    VSET(pts[0], parameters.x+parameters.thickness, parameters.y+parameters.thickness,parameters.z);
    VSET(pts[1], parameters.x+parameters.thickness+parameters.diameter * (1-parameters.slope), parameters.y+parameters.thickness,parameters.z);
    VSET(pts[2], parameters.x+parameters.thickness, parameters.y+parameters.diameter, parameters.z);
    VSET(pts[3], parameters.x+parameters.thickness, parameters.y+parameters.diameter, parameters.z);
    VSET(pts[4], parameters.x+parameters.thickness, parameters.y+parameters.thickness, parameters.z+parameters.length);
    VSET(pts[5], parameters.x+parameters.thickness+parameters.diameter * (1-parameters.slope), parameters.y+parameters.thickness,parameters.z+parameters.length);
    VSET(pts[6], parameters.x+parameters.thickness, parameters.y+parameters.diameter, parameters.z+parameters.length);
    VSET(pts[7], parameters.x+parameters.thickness, parameters.y+parameters.diameter, parameters.z+parameters.length);
    mk_arb8 (db, "slope_1", &pts[0][0]);
    /* Make the wall sloped */

    VSET(temp1, parameters.x + .5 * parameters.thickness, parameters.y + parameters.diameter - center, parameters.z);
    VSET(temp2, 0, 0, parameters.length);
    makeArb (db, parameters.x, parameters.y + parameters.diameter, parameters.z, parameters.thickness, parameters.topR, parameters.length, "top_1");
    mk_rcc (db, "top_2", temp1, temp2, parameters.topR);
    /* Make top of first wall rounded */

    makeArb (db, parameters.x + parameters.thickness, parameters.y + parameters.thickness, parameters.z, parameters.radius, parameters.radius, parameters.length, "corner_1");
    VSET(temp1, parameters.x+parameters.thickness+parameters.radius, parameters.y+parameters.thickness+parameters.radius, parameters.z);
    VSET(temp2, 0, 0, parameters.length);
    mk_rcc (db, "corner_2", temp1, temp2, parameters.radius);
    /* round the corner of the first wall */

    makeArb (db, parameters.x+parameters.thickness, parameters.y, parameters.z, parameters.diameter, parameters.thickness, parameters.length, "arb_2");
    /* make the bottom of the channel */

    makeArb (db, parameters.x + parameters.thickness + parameters.diameter, parameters.y, parameters.z, parameters.thickness, parameters.diameter, parameters.length, "arb_3");
    /* make the second wall of the channel */
    parameters.x = parameters.x+parameters.thickness+parameters.diameter;

    VSET(pts[0], parameters.x, parameters.y+parameters.thickness,parameters.z);
    VSET(pts[1], parameters.x-parameters.diameter * (1-parameters.slope), parameters.y+parameters.thickness,parameters.z);
    VSET(pts[2], parameters.x, parameters.y+parameters.diameter,parameters.z);
    VSET(pts[3], parameters.x, parameters.y+parameters.diameter,parameters.z);
    VSET(pts[4], parameters.x, parameters.y+parameters.thickness, parameters.z+parameters.length);
    VSET(pts[5], parameters.x-parameters.diameter * (1-parameters.slope), parameters.y+parameters.thickness,parameters.z+parameters.length);
    VSET(pts[6], parameters.x, parameters.y+parameters.diameter,parameters.z+parameters.length);
    VSET(pts[7], parameters.x, parameters.y+parameters.diameter,parameters.z+parameters.length);
    mk_arb8 (db, "slope_2", &pts[0][0]);
    /* make the second wall sloped */

    VSET(temp1, parameters.x+.5*parameters.thickness, parameters.y+parameters.diameter-center, parameters.z);
    makeArb (db, parameters.x, parameters.y+parameters.diameter, parameters.z, parameters.thickness, parameters.topR, parameters.length, "top_3");
    mk_rcc (db, "top_4", temp1, temp2, parameters.topR);
    /* Make the top of the second wall rounded */

    makeArb (db, parameters.x, parameters.y + parameters.thickness+parameters.radius, parameters.z, parameters.radius*-1, parameters.radius*-1, parameters.length, "corner_3");
    VSET(temp1, parameters.x-parameters.radius, parameters.y+parameters.thickness+parameters.radius, parameters.z);
    VSET(temp2, 0, 0, parameters.length);
    mk_rcc (db, "corner_4", temp1, temp2, parameters.radius);
    /* round the corner of the first wall */

    if (parameters.holes == 1) {
	addHoles(db, parameters);
    }

    BU_LIST_INIT(&sub1.l);
    (void)mk_addmember("corner_1", &sub1.l, NULL, WMOP_UNION);
    (void)mk_addmember("corner_2", &sub1.l, NULL, WMOP_SUBTRACT);
    (void)mk_addmember("corner_3", &sub1.l, NULL, WMOP_UNION);
    (void)mk_addmember("corner_4", &sub1.l, NULL, WMOP_SUBTRACT);
    mk_lcomb(db,"sub1.c", &sub1, 0, NULL, NULL, NULL, 0);
    /* create comb for rounded corners */

    BU_LIST_INIT(&sub2a.l);
    (void)mk_addmember("top_2", &sub2a.l, NULL, WMOP_UNION);
    (void)mk_addmember("top_4", &sub2a.l, NULL, WMOP_UNION);
    mk_lcomb(db, "sub2a.c", &sub2a, 0, NULL, NULL, NULL, 0);

    BU_LIST_INIT(&sub2b.l);
    (void)mk_addmember("top_1", &sub2b.l, NULL, WMOP_UNION);
    (void)mk_addmember("top_3", &sub2b.l, NULL, WMOP_UNION);
    mk_lcomb(db, "sub2b.c", &sub2b, 0, NULL, NULL, NULL, 0);
    /* create comb for rounding top */

    BU_LIST_INIT(&sub2.l);
    (void)mk_addmember("sub2b.c", &sub2.l, NULL, WMOP_UNION);
    (void)mk_addmember("sub2a.c", &sub2.l, NULL, WMOP_INTERSECT);
    mk_lcomb(db, "sub2.c", &sub2, 0, NULL, NULL, NULL, 0);
    /* create comb for rounding top */

    BU_LIST_INIT(&channel.l);
    (void)mk_addmember("arb_1", &channel.l, NULL, WMOP_UNION);
    (void)mk_addmember("arb_2", &channel.l, NULL, WMOP_UNION);
    (void)mk_addmember("arb_3", &channel.l, NULL, WMOP_UNION);
    (void)mk_addmember("sub1.c", &channel.l, NULL, WMOP_UNION);
    (void)mk_addmember("slope_1", &channel.l, NULL, WMOP_UNION);
    (void)mk_addmember("sub1.c", &channel.l, NULL, WMOP_SUBTRACT);
    (void)mk_addmember("slope_2", &channel.l, NULL, WMOP_UNION);
    (void)mk_addmember("sub1.c", &channel.l, NULL, WMOP_SUBTRACT);
    (void)mk_addmember("sub2.c", &channel.l, NULL, WMOP_UNION);
    (void)mk_addmember("sub1.c", &channel.l, NULL, WMOP_UNION);
    if (parameters.holes)
	(void)mk_addmember("holes.c", &channel.l, NULL, WMOP_SUBTRACT);
    mk_lcomb(db, "channel.r", &channel, 1, NULL, NULL, NULL, 1);
    /* make final region */

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
