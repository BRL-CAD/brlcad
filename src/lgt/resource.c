/*                      R E S O U R C E . C
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
/** @file resource.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./extern.h"
#ifdef PARALLEL
static int	lock_tab[12];		/* Lock usage counters */
static char	*all_title[12] = {
	"malloc",
	"worker",
	"stats",
	"results",
	"model refinement",
	"???"
};

/*
 *			L O C K _ P R
 */
lock_pr()
{
	register int i;
	for( i=0; i<3; i++ )  {
		if(lock_tab[i] == 0)  continue;
		(void) fprintf( stderr, "%10d %s\n", lock_tab[i], all_title[i] );
	}
}

/*
 *			R E S _ P R
 */
res_pr()
{
	register struct resource *res;
	register int i;

	res = &resource[0];
	for( i=0; i<npsw; i++, res++ )  {
		(void) fprintf( stderr, "cpu%d seg  len=%10d get=%10d free=%10d\n",
			i,
			res->re_seglen, res->re_segget, res->re_segfree );
		(void) fprintf( stderr, "cpu%d part len=%10d get=%10d free=%10d\n",
			i,
			res->re_partlen, res->re_partget, res->re_partfree );
		(void) fprintf( stderr, "cpu%d bitv len=%10d get=%10d free=%10d\n",
			i,
			res->re_bitvlen, res->re_bitvget, res->re_bitvfree );
	}
}
#endif PARALLEL

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
