/*
 *			S E T . C
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"
#include "./ged.h"

struct mged_variables mged_variables = {
/* autosize */		1,
/* sgi_win_size */	0,
/* sgi_win_origin */	{ 0, 0 }
};

#define MV_O(_m)	offsetof(struct mged_variables, _m)
struct structparse mged_vparse[] = {
	{"%d",	1, "autosize",		MV_O(autosize),		FUNC_NULL },
	{"%d",	1, "sgi_win_size",	MV_O(sgi_win_size),	FUNC_NULL },
	{"%d",	2, "sgi_win_origin",	MV_O(sgi_win_origin[0]),FUNC_NULL },
	{"",	0,  (char *)0,		0,			FUNC_NULL }
};

void
f_set(ac,av)
int ac;
char *av[];
{
	struct rt_vls vls;

	rt_vls_init(&vls);

	if (ac <= 1) {
		rt_structprint("mged variables", mged_vparse, (CONST char *)&mged_variables);
		printf("%s", rt_vls_addr(&vls) );
		fflush(stdout);
	} else if (ac == 2) {
		rt_vls_strcpy(&vls, av[1]);
		rt_structparse(&vls, mged_vparse, (char *)&mged_variables);
	} else {
		printf("Usage: set\t\t- prints all options\n\tset opt=val\t- sets an option\n");
		fflush(stdout);
	}
	rt_vls_free(&vls);
}
