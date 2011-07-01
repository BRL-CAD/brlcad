/*                          R O O M . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file proc-db/room.c
 *
 * Program to generate procedural rooms.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"


#define HEIGHT 4000		/* 4 meter high walls */

#define EAST 1
#define NORTH 2
#define WEST 4
#define SOUTH 8

mat_t identity;
double degtorad = 0.0174532925199433;
double sin60;

struct mtab {
    char mt_name[64];
    char mt_param[96];
} mtab[] = {
    {"plastic",	""},
    {"glass",	""},
    {"plastic",	""},
    {"mirror",	""},
    {"plastic",	""},
    {"testmap",	""},
    {"plastic",	""}
};
int nmtab = sizeof(mtab)/sizeof(struct mtab);

#define PICK_MAT ((rand() % nmtab))

void make_room(char *rname, fastf_t *imin, fastf_t *imax, fastf_t *thickness, struct wmember *headp);
void make_walls(char *rname, fastf_t *imin, fastf_t *imax, fastf_t *thickness, int bits, struct wmember *headp);
void make_pillar(char *prefix, int ix, int iy, fastf_t *center, fastf_t *lwh, struct wmember *headp);
void make_carpet(char *rname, fastf_t *min, fastf_t *max, char *file, struct wmember *headp);
extern void get_rgb(unsigned char *rgb);

struct rt_wdb *outfp;

int
main(int argc, char **argv)
{
    vect_t norm;
    unsigned char rgb[3];
    int ix;
    double x;
    double size;
    int quant;
    struct wmember head;
    vect_t bmin, bmax, bthick;
    vect_t r1min, r1max, r1thick;
    vect_t lwh;		/* length, width, height */
    vect_t pbase;

    if (argc > 0)
	bu_log("Usage: %s\n", argv[0]);

    BU_LIST_INIT(&head.l);

    MAT_IDN(identity);
    sin60 = sin(60.0 * M_PI / 180.0);

    outfp = wdb_fopen("room.g");
    mk_id(outfp, "Procedural Rooms");

    /* Create the building */
    VSET(bmin, 0, 0, 0);
    VSET(bmax, 80000, 60000, HEIGHT);
    VSET(bthick, 100, 100, 100);
    make_room("bldg", bmin, bmax, bthick, &head);

    /* Create the first room */
    VSET(r1thick, 100, 100, 0);
    VMOVE(r1min, bmin);
    VSET(r1max, 40000, 10000, HEIGHT);
    VADD2(r1max, r1min, r1max);
    make_walls("rm1", r1min, r1max, r1thick, NORTH|EAST, &head);
    make_carpet("rm1carpet", r1min, r1max, "carpet.pix", &head);

    /* Create the golden earth */
    VSET(norm, 0, 0, 1);
    mk_half(outfp, "plane", norm, -bthick[Z]-10.0);
    rgb[0] = 240;	/* gold/brown */
    rgb[1] = 180;
    rgb[2] = 64;
    mk_region1(outfp, "plane.r", "plane", NULL, NULL, rgb);
    (void)mk_addmember("plane.r", &head.l, NULL, WMOP_UNION);

    /* Create the display pillars */
    size = 4000;	/* separation between centers */
    quant = 5;	/* pairs */
    VSET(lwh, 400, 400, 1000);
    for (ix=quant-1; ix>=0; ix--) {
	x = 10000 + ix*size;
	VSET(pbase, x, 10000*.25, r1min[Z]);
	make_pillar("Pil", ix, 0, pbase, lwh, &head);
	VSET(pbase, x, 10000*.75, r1min[Z]);
	make_pillar("Pil", ix, 1, pbase, lwh, &head);
    }

#ifdef never
    /* Create some light */
    white[0] = white[1] = white[2] = 255;
    base = size*(quant/2+1);
    VSET(aim, 0, 0, 0);
    VSET(pos, base, base, minheight+maxheight*bn_rand0to1(randp));
    do_light("l1", pos, aim, 1, 100.0, white, &head);
    VSET(pos, -base, base, minheight+maxheight*bn_rand0to1(randp));
    do_light("l2", pos, aim, 1, 100.0, white, &head);
    VSET(pos, -base, -base, minheight+maxheight*bn_rand0to1(randp));
    do_light("l3", pos, aim, 1, 100.0, white, &head);
    VSET(pos, base, -base, minheight+maxheight*bn_rand0to1(randp));
    do_light("l4", pos, aim, 1, 100.0, white, &head);
#endif

    /* Build the overall combination */
    mk_lfcomb(outfp, "room", &head, 0);

    return 0;
}

void
make_room(char *rname, fastf_t *imin, fastf_t *imax, fastf_t *thickness, struct wmember *headp)

    /* Interior RPP min point */


{
    struct wmember head;
    char name[32];
    vect_t omin;
    vect_t omax;

    BU_LIST_INIT(&head.l);

    VSUB2(omin, imin, thickness);
    VADD2(omax, imax, thickness);

    snprintf(name, 32, "o%s", rname);
    mk_rpp(outfp, name, omin, omax);
    (void)mk_addmember(name, &head.l, NULL, WMOP_UNION);

    snprintf(name, 32, "i%s", rname);
    mk_rpp(outfp, name, imin, imax);
    mk_addmember(name, &head.l, NULL, WMOP_SUBTRACT);

    mk_lfcomb(outfp, rname, &head, 1);
    (void)mk_addmember(rname, &(headp->l), NULL, WMOP_UNION);
}

void
make_walls(char *rname, fastf_t *imin, fastf_t *imax, fastf_t *thickness, int bits, struct wmember *headp)

    /* Interior RPP min point */


{
    struct wmember head;
    char name[32];
    vect_t omin, omax;	/* outer dimensions */
    vect_t wmin, wmax;
    int mask;

    BU_LIST_INIT(&head.l);

    /* thickness[Z] = 0; */

    /*
     * Set exterior dimensions to interior dimensions.
     * Then, thicken them as necessary due to presence of
     * exterior walls.
     * It may be useful to return the exterior min, max.
     */
    VMOVE(omin, imin);
    VMOVE(omax, imax);
    if (bits & EAST)
	omax[X] += thickness[X];
    if (bits & WEST)
	omin[X] -= thickness[X];
    if (bits & NORTH)
	omax[Y] += thickness[Y];
    if (bits & SOUTH)
	omin[Y] -= thickness[Y];

    for (mask=8; mask > 0; mask >>= 1) {
	if ((bits & mask) == 0)  continue;

	VMOVE(wmin, omin);
	VMOVE(wmax, omax);

	switch (mask) {
	    case SOUTH:
		/* South (-Y) wall */
		snprintf(name, 32, "S%s", rname);
		wmax[Y] = imin[Y];
		break;
	    case WEST:
		/* West (-X) wall */
		snprintf(name, 32, "W%s", rname);
		wmax[X] = imin[X];
		break;
	    case NORTH:
		/* North (+Y) wall */
		snprintf(name, 32, "N%s", rname);
		wmin[Y] = imax[Y];
		break;
	    case EAST:
		/* East (+X) wall */
		snprintf(name, 32, "E%s", rname);
		wmin[X] = imax[X];
		break;
	}
	mk_rpp(outfp, name, wmin, wmax);
	(void)mk_addmember(name, &head.l, NULL, WMOP_UNION);
    }

    mk_lfcomb(outfp, rname, &head, 1);
    (void)mk_addmember(rname, &(headp->l), NULL, WMOP_UNION);
}

void
make_pillar(char *prefix, int ix, int iy, fastf_t *center, fastf_t *lwh, struct wmember *headp)


    /* center of base */


{
    vect_t min, max;
    unsigned char rgb[4];		/* needs all 4 */
    char pilname[32], rname[32], sname[32], oname[32];
    int i;
    struct wmember head;
    struct wmember *wp;

    BU_LIST_INIT(&head.l);

    snprintf(pilname, 32, "%s%d, %d", prefix, ix, iy);
    snprintf(rname, 32, "%s.r", pilname);
    snprintf(sname, 32, "%s.s", pilname);
    snprintf(oname, 32, "Obj%d, %d", ix, iy);

    VMOVE(min, center);
    min[X] -= lwh[X];
    min[Y] -= lwh[Y];
    VADD2(max, center, lwh);
    mk_rpp(outfp, sname, min, max);

    /* Needs to be in a region, with color!  */
    get_rgb(rgb);
    i = PICK_MAT;
    mk_region1(outfp, rname, sname,
	       mtab[i].mt_name, mtab[i].mt_param, rgb);

    (void)mk_addmember(rname, &head.l, NULL, WMOP_UNION);
    wp = mk_addmember(oname, &head.l, NULL, WMOP_UNION);
    MAT_DELTAS(wp->wm_mat, center[X], center[Y], center[Z]+lwh[Z]);
    mk_lfcomb(outfp, pilname, &head, 0);

    (void)mk_addmember(pilname, &(headp->l), NULL, WMOP_UNION);
}

void
make_carpet(char *rname, fastf_t *min, fastf_t *max, char *file, struct wmember *headp)
{
    char sname[32];
    char args[128];
    vect_t cmin, cmax;

    VMOVE(cmin, min);
    VMOVE(cmax, max);
    cmax[Z] = cmin[Z] + 10;		/* not very plush carpet */
    min[Z] = cmax[Z];		/* raise the caller's floor */

    snprintf(sname, 32, "%s.s", rname);
    snprintf(args, 128, "texture file=%s;plastic", file);
    mk_rpp(outfp, sname, cmin, cmax);
    mk_region1(outfp, rname, sname,
	       "stack", args,
	       (unsigned char *)0);

    (void)mk_addmember(rname, &(headp->l), NULL, WMOP_UNION);
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
