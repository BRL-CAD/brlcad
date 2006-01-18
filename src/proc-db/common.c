/*                        C O M M O N . C
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
/** @file common.c
 *
 *  Subroutines common to several procedural database generators.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"

extern struct rt_wdb	*outfp;

struct colors {
	char		*name;
	unsigned char	c_pixel[3];
}colortab[] = {
	{"black",	{20,20,20}},
	{"blue",	{0,0,255}},
	{"brown",	{200,130,0}},
	{"cyan",	{0,255,200}},
	{"flesh",	{255,200,160}},
	{"gray",	{120,120,120}},
	{"green",	{0,255,0}},
	{"lime", 	{200,255,0}},
	{"magenta",	{255,0,255}},
	{"olive",	{220,190,0}},
	{"orange",	{255,100,0}},
	{"pink",	{255,200,200}},
	{"red",		{255,0,0}},
	{"rose",	{255,0,175}},
	{"rust",	{200,100,0}},
	{"silver",	{237,237,237}},
	{"sky",		{0,255,255}},
	{"violet",	{200,0,255}},
	{"white",	{255,255,255}},
	{"yellow",	{255,200,0}},
	{"",		{0,0,0}}
};
int	ncolors = sizeof(colortab)/sizeof(struct colors);
int	curcolor = 0;

void
get_rgb( unsigned char *rgb )
{
	register struct colors *cp;
	if( ++curcolor >= ncolors )  curcolor = 0;
	cp = &colortab[curcolor];
	rgb[0] = cp->c_pixel[0];
	rgb[1] = cp->c_pixel[1];
	rgb[2] = cp->c_pixel[2];
}

void
do_light(char *name, fastf_t *pos, fastf_t *dir_at, int da_flag, double r, unsigned char *rgb, struct wmember *headp)


      	       		/* direction or aim point */
   	        	/* 0 = direction, !0 = aim point */
      	  		/* radius of light */


{
	char	nbuf[64];
	vect_t	center;
	mat_t	rot;
	mat_t	xlate;
	mat_t	both;
	vect_t	from;
	vect_t	dir;

	if( da_flag )  {
		VSUB2( dir, dir_at, pos );
		VUNITIZE( dir );
	} else {
		VMOVE( dir, dir_at );
	}

	sprintf( nbuf, "%s.s", name );
	VSETALL( center, 0 );
	mk_sph( outfp, nbuf, center, r );

	/*
	 * Need to rotate from 0,0,-1 to vect "dir",
	 * then xlate to final position.
	 */
	VSET( from, 0, 0, -1 );
	bn_mat_fromto( rot, from, dir );
	MAT_IDN( xlate );
	MAT_DELTAS( xlate, pos[X], pos[Y], pos[Z] );
	bn_mat_mul( both, xlate, rot );

	mk_region1( outfp, name, nbuf, "light", "shadows=1", rgb );
	(void)mk_addmember( name, &(headp->l), NULL, WMOP_UNION );
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
