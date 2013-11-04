/*                      M O L E C U L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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
/** @file proc-db/molecule.c
 *
 * Create a molecule from G. Adams format
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"


struct sphere  {
    struct sphere * next;		/* Next Sphere */
    int s_id;			/* Sphere id */
    char s_name[15];		/* Sphere name */
    point_t s_center;		/* Sphere Center */
    fastf_t s_rad;			/* Sphere radius */
    int s_atom_type;		/* Atom Type */
};


struct sphere *s_list = (struct sphere *) 0;
struct sphere *s_head = (struct sphere *) 0;

struct atoms  {
    int a_id;
    char a_name[128];
    unsigned char red, green, blue;
};

#define MAX_ATOMS 50
struct atoms atom_list[MAX_ATOMS];

char * matname = "plastic";
char * matparm = "shine=100.0 diffuse=.8 specular=.2";

void read_data(void), process_sphere(int id, fastf_t *center, double rad, int sph_type);
int make_bond(int sp1, int sp2);

struct wmember head;

static const char usage[] = "Usage: molecule db_title < mol-cube.dat > mol.g\n";

struct rt_wdb *outfp;

int
main(int argc, char **argv)
{

    if (argc != 2 || (argc == 2 && (BU_STR_EQUAL(argv[1],"-h") || BU_STR_EQUAL(argv[1],"-?")))) {
	fputs(usage, stderr);
	return 1;
    }

    BU_LIST_INIT(&head.l);
    outfp = wdb_fopen("molecule.g");
    mk_id(outfp, argv[1]);
    read_data();

    /* Build the overall combination */
    mk_lfcomb(outfp, "mol.g", &head, 0);

    wdb_close(outfp);
    return 0;
}


/* File format from stdin
 * 1st value on the line is the data_type (integer).
 *
 * For ATOM DATA_TYPE (i.e., data_type = 0)
 *     0   ATOM_INDEX ATOM_NAME RED GREEN BLUE
 *    (int int       string    int int   int)
 * For Sphere DATA_TYPE (i.e., data_type = 1)
 *     1   SPH_ID CENTER_X CENTER_Y CENTER_Z RADIUS ATOM_TYPE
 *    (int int    float    float    float    float  int)
 * For Bond DATA_TYPE (i.e., data_type = 2)
 *     2   SPH_ID SPH_ID
 *    (int int    int)
 */
void
read_data(void)
{

    int data_type;
    int sphere_id;
    point_t center;
    float x, y, z;
    float sphere_radius;
    int atom_type;
    int b_1, b_2;
    int red, green, blue;
    int i = 0;
    int ret;

    while (scanf("%d", &data_type) != EOF) {

	switch (data_type) {
	    case (0):
		ret = scanf("%d", &i);
		if (ret == 0)
		    perror("scanf");
		if (i < 0 || i >= MAX_ATOMS) {
		    fprintf(stderr, "Atom index value %d is out of bounds [0, %d]\n", i, MAX_ATOMS - 1);
		    return;
		}
		ret = scanf("%128s", atom_list[i].a_name);
		if (ret == 0)
		    perror("scanf");
		ret = scanf("%d", &red);
		if (ret == 0)
		    perror("scanf");
		ret = scanf("%d", &green);
		if (ret == 0)
		    perror("scanf");
		ret = scanf("%d", &blue);
		if (ret == 0)
		    perror("scanf");
		atom_list[i].red  = red;
		atom_list[i].green  = green;
		atom_list[i].blue  = blue;
		break;
	    case (1):
		ret = scanf("%d", &sphere_id);
		if (ret == 0)
		    perror("scanf");
		ret = scanf("%f", &x);
		if (ret == 0)
		    perror("scanf");
		ret = scanf("%f", &y);
		if (ret == 0)
		    perror("scanf");
		ret = scanf("%f", &z);
		if (ret == 0)
		    perror("scanf");
		ret = scanf("%f", &sphere_radius);
		if (ret == 0)
		    perror("scanf");
		ret = scanf("%d", &atom_type);
		if (ret == 0)
		    perror("scanf");
		VSET(center, x, y, z);
		process_sphere(sphere_id, center, sphere_radius,
			       atom_type);
		break;
	    case (2):
		ret = scanf("%d", &b_1);
		if (ret == 0)
		    perror("scanf");
		ret = scanf("%d", &b_2);
		if (ret == 0)
		    perror("scanf");
		(void)make_bond(b_1, b_2);
		break;
	    default:
		return;
	}
    }
}


void
process_sphere(int id, fastf_t *center, double rad, int sph_type)
{
    struct sphere *newsph;
    char nm[128], nm1[128];
    unsigned char rgb[3];
    struct wmember reg_head;

    BU_ALLOC(newsph, struct sphere);

    rgb[0] = atom_list[sph_type].red;
    rgb[1] = atom_list[sph_type].green;
    rgb[2] = atom_list[sph_type].blue;

    sprintf(nm1, "sph.%d", id);
    mk_sph(outfp, nm1, center, rad);

    /* Create a region nm to contain the solid nm1 */
    BU_LIST_INIT(&reg_head.l);
    (void)mk_addmember(nm1, &reg_head.l, NULL, WMOP_UNION);
    sprintf(nm, "SPH.%d", id);
    mk_lcomb(outfp, nm, &reg_head, 1, matname, matparm, rgb, 0);

    /* Include this region in the larger group */
    (void)mk_addmember(nm, &head.l, NULL, WMOP_UNION);

    newsph->next = (struct sphere *)0;
    newsph->s_id = id;
    bu_strlcpy(newsph->s_name, nm1, sizeof(newsph->s_name));
    newsph->s_name[14] = '\0';
    VMOVE(newsph->s_center, center);
    newsph->s_rad = rad;
    newsph->s_atom_type = sph_type;

    if (s_head == (struct sphere *) 0) {
	s_head = s_list = newsph;
    } else {
	s_list->next = newsph;
	s_list = newsph;
    }
}


int
make_bond(int sp1, int sp2)
{
    struct sphere * s1, *s2, *s_ptr;
    point_t base;
    vect_t height;
    char nm[128], nm1[128];
    unsigned char rgb[3];
    struct wmember reg_head;

    s1 = s2 = (struct sphere *) 0;

    for (s_ptr = s_head; s_ptr != (struct sphere *)0; s_ptr = s_ptr->next) {
	if (s_ptr->s_id == sp1)
	    s1 = s_ptr;

	if (s_ptr->s_id == sp2)
	    s2 = s_ptr;
    }

    if (s1 == (struct sphere *) 0 || s2 == (struct sphere *)0)
	return -1;		/* error */

    VMOVE(base, s1->s_center);
    VSUB2(height, s2->s_center, s1->s_center);

    sprintf(nm, "bond.%d.%d", sp1, sp2);

    rgb[0] = 191;
    rgb[1] = 142;
    rgb[2] = 57;

    /* TODO: make the scaling factor configurable or autosize based on
     * overall complexity.
     */
    mk_rcc(outfp, nm, base, height, s1->s_rad * 0.25);

    BU_LIST_INIT(&reg_head.l);
    (void)mk_addmember(nm, &reg_head.l, NULL, WMOP_UNION);
    (void)mk_addmember(s1->s_name, &reg_head.l, NULL, WMOP_SUBTRACT);
    (void)mk_addmember(s2->s_name, &reg_head.l, NULL, WMOP_SUBTRACT);
    sprintf(nm1, "BOND.%d.%d", sp1, sp2);
    mk_lcomb(outfp, nm1, &reg_head, 1, matname, matparm, rgb, 0);
    (void)mk_addmember(nm1, &head.l, NULL, WMOP_UNION);

    return 0;		/* OK */
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
