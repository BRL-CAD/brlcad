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

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"
#include "./ged.h"

#ifdef XMGED
extern int update_views;
#endif

extern void	predictor_hook();		/* in ged.c */
extern void	reattach();			/* in attach.c */

struct mged_variables mged_variables = {
/* autosize */		1,
/* rateknobs */		1,
/* sgi_win_size */	0,
/* sgi_win_origin */	{ 0, 0 },
/* faceplate */		1,
#ifdef XMGED
/* w_axis */            0,
/* v_axis */            0,
/* e_axis */            0,
#endif
/* predictor */		0,
/* predictor_advance */	1.0,
/* predictor_length */	2.0,
/* perspective */	-1,
/* nmg_eu_dist */	0.05,
/* eye_sep_dist */	0.0
};

/*
 *  Cause screen to be refreshed when all cmds have been processed.
 */
static void
refresh_hook()
{
	dmaflag = 1;
}
static void
nmg_eu_dist_set()
{
	extern double nmg_eue_dist;

	nmg_eue_dist = mged_variables.nmg_eu_dist;

	rt_log( "New nmg_eue_dist = %g\n", nmg_eue_dist);
}
#define MV_O(_m)	offsetof(struct mged_variables, _m)
struct structparse mged_vparse[] = {
	{"%d",	1, "autosize",		MV_O(autosize),		FUNC_NULL },
	{"%d",	1, "rateknobs",		MV_O(rateknobs),	FUNC_NULL },
	{"%d",	1, "sgi_win_size",	MV_O(sgi_win_size),	FUNC_NULL },
	{"%d",	2, "sgi_win_origin",	MV_O(sgi_win_origin[0]),FUNC_NULL },
	{"%d",	1, "faceplate",		MV_O(faceplate),	refresh_hook },
#ifdef XMGED
	{"%d",  1, "w_axis",            MV_O(w_axis),           refresh_hook },
	{"%d",  1, "v_axis",            MV_O(v_axis),           refresh_hook },
	{"%d",  1, "e_axis",            MV_O(e_axis),           refresh_hook },
#endif
	{"%d",	1, "predictor",		MV_O(predictor),	predictor_hook },
	{"%f",	1, "predictor_advance",	MV_O(predictor_advance),predictor_hook },
	{"%f",	1, "predictor_length",	MV_O(predictor_length),	predictor_hook },
	{"%f",	1, "perspective",	MV_O(perspective),	refresh_hook },
	{"%f",  1, "nmg_eu_dist",	MV_O(nmg_eu_dist),	nmg_eu_dist_set },
	{"%f",  1, "eye_sep_dist",	MV_O(eye_sep_dist),	reattach },
	{"",	0,  (char *)0,		0,			FUNC_NULL }
};

int
f_set(ac,av)
int ac;
char *av[];
{
	struct rt_vls vls;
	int bad = 0;

	rt_vls_init(&vls);

	if (ac <= 1) {
		rt_structprint("mged variables", mged_vparse, (CONST char *)&mged_variables);
		rt_log("%s", rt_vls_addr(&vls) );
	} else if (ac == 2) {
		rt_vls_strcpy(&vls, av[1]);
		rt_structparse(&vls, mged_vparse, (char *)&mged_variables);
	} else {
		rt_log("Usage: set\t\t- prints all options\n\tset opt=val\t- sets an option\n");
		bad = 1;
	}
	rt_vls_free(&vls);

#ifdef XMGED
	update_views = 1;
#endif
	return bad ? CMD_BAD : CMD_OK;
}
