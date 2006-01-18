/*                      M O L E C U L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file molecule.c
 *
 * Create a molecule from G. Adams format
 *
 * Author:	Paul R. Stay
 * 		Ballistic Research Labratory
 * 		Aberdeen Proving Ground, Md.
 * Date:	Mon Dec 29 1986
 */
static const char rcs_ident[] = "$Header$";

#include "common.h"


#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"


struct sphere  {
	struct sphere * next;		/* Next Sphere */
	int	s_id;			/* Sphere id */
	char	s_name[15];		/* Sphere name */
	point_t	s_center;		/* Sphere Center */
	fastf_t	s_rad;			/* Sphere radius */
	int	s_atom_type;		/* Atom Type */
};

struct sphere *s_list = (struct sphere *) 0;
struct sphere *s_head = (struct sphere *) 0;

struct atoms  {
	int a_id;
	char a_name[128];
	unsigned char red, green, blue;
};

struct atoms atom_list[50];

char * matname = "plastic";
char * matparm = "shine=100.0 diffuse=.8 specular=.2";

void	read_data(void), process_sphere(int id, fastf_t *center, double rad, int sph_type);
int	make_bond( int sp1, int sp2 );

struct wmember head;

static const char usage[] = "Usage: molecule db_title < mol-cube.dat > mol.g\n";

struct rt_wdb	*outfp;

int
main(int argc, char **argv)
{

	if( argc != 2 )  {
		fputs( usage, stderr );
		exit(1);
	}

	BU_LIST_INIT( &head.l );
	outfp = wdb_fopen( "molecule.g" );
	mk_id( outfp, argv[1] );
	read_data();

	/* Build the overall combination */
	mk_lfcomb( outfp, "mol.g", &head, 0 );

	wdb_close(outfp);
	return 0;
}

/* File format from stdin
 *
 * For a ATOM DATA_TYPE ATOM_ID ATOM_NAME RED GREEN BLUE
 * For a Sphere DATA_TYPE SPH_ID CENTER ( X, Y, Z) RADIUS ATOM_TYPE
 * For a Bond   DATA_TYPE SPH_ID SPH_ID
 * DATA_TYPE = 0 - Atom1 - Sphere 2 - Bond
 * SPH_ID = integer
 * CENTER = three float values x, y, z
 * RADIUS = Float
 * ATOM_TYPE = integer
 * ATOM_NAME = Character pointer to name value.
 */
void
read_data(void)
{

	int             data_type;
	int             sphere_id;
	point_t		center;
	float           x, y, z;
	float           sphere_radius;
	int             atom_type;
	int             b_1, b_2;
	int red, green, blue;
	int i = 0;


	while (scanf(" %d", &data_type) != 0) {

		switch (data_type) {
		case (0):
			scanf("%d", &i);
			scanf("%s", atom_list[i].a_name);
			scanf("%d", &red);
			scanf("%d", &green);
			scanf("%d", &blue);
			atom_list[i].red  = red;
			atom_list[i].green  = green;
			atom_list[i].blue  = blue;
			break;
		case (1):
		        scanf("%d", &sphere_id);
		        scanf("%f", &x );
		        scanf("%f", &y);
		        scanf("%f", &z);
		        scanf("%f", &sphere_radius);
		        scanf("%d", &atom_type);
			VSET( center, x, y, z );
			process_sphere(sphere_id, center, sphere_radius,
				atom_type);
			break;
		case (2):
			scanf("%d", &b_1);
			scanf("%d", &b_2);
			(void)make_bond( b_1, b_2);
			break;
		case (4):
			return;
		}
	}
}

void
process_sphere(int id, fastf_t *center, double rad, int sph_type)
{
	struct sphere * new = (struct sphere *)
	    malloc( sizeof ( struct sphere) );
	char nm[128], nm1[128];
	unsigned char rgb[3];
	struct wmember	reg_head;

	rgb[0] = atom_list[sph_type].red;
	rgb[1] = atom_list[sph_type].green;
	rgb[2] = atom_list[sph_type].blue;

	sprintf(nm1, "sph.%d", id );
	mk_sph( outfp, nm1, center, rad );

	/* Create a region nm to contain the solid nm1 */
	BU_LIST_INIT( &reg_head.l );
	(void)mk_addmember( nm1, &reg_head.l, NULL, WMOP_UNION );
	sprintf(nm, "SPH.%d", id );
	mk_lcomb( outfp, nm, &reg_head, 1, matname, matparm, rgb, 0 );

	/* Include this region in the larger group */
	(void)mk_addmember( nm, &head.l, NULL, WMOP_UNION );

	new->next = ( struct sphere *)0;
	new->s_id = id;
	strncpy(new->s_name, nm1, sizeof(nm1) );
	VMOVE( new->s_center, center );
	new->s_rad = rad;
	new->s_atom_type = sph_type;

	if ( s_head == (struct sphere *) 0 )
	{
		s_head = s_list = new;
	} else
	{
		s_list->next = new;
		s_list = new;
	}
}

int
make_bond( int sp1, int sp2 )
{
	struct sphere * s1, *s2, *s_ptr;
	point_t base;
	vect_t height;
	char nm[128], nm1[128];
	unsigned char rgb[3];
	struct wmember reg_head;

	s1 = s2 = (struct sphere *) 0;

	for( s_ptr = s_head; s_ptr != (struct sphere *)0; s_ptr = s_ptr->next )
	{
		if ( s_ptr->s_id == sp1 )
			s1 = s_ptr;

		if ( s_ptr->s_id == sp2 )
			s2 = s_ptr;
	}

	if( s1 == (struct sphere *) 0 || s2 == (struct sphere *)0 )
		return -1;		/* error */

	VMOVE( base, s1->s_center );
	VSUB2( height, s2->s_center, s1->s_center );

	sprintf( nm, "bond.%d.%d", sp1, sp2);

	rgb[0] = 191;
	rgb[1] = 142;
	rgb[2] = 57;

#if 1
	/* Use this for mol-cube.dat */
	mk_rcc( outfp, nm, base, height, s1->s_rad * 0.15 );
#else
	/* Use this for chemical molecules */
	mk_rcc( outfp, nm, base, height, s1->s_rad * 0.5 );
#endif

	BU_LIST_INIT( &reg_head.l );
	(void)mk_addmember( nm, &reg_head.l, NULL, WMOP_UNION );
	(void)mk_addmember( s1->s_name, &reg_head.l, NULL, WMOP_SUBTRACT );
	(void)mk_addmember( s2->s_name, &reg_head.l, NULL, WMOP_SUBTRACT );
	sprintf( nm1, "BOND.%d.%d", sp1, sp2);
	mk_lcomb( outfp, nm1, &reg_head, 1, matname, matparm, rgb, 0 );
	(void)mk_addmember( nm1, &head.l, NULL, WMOP_UNION );

	return(0);		/* OK */
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
