/*                      B W T H R E S H . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
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
/** @file bwthresh.c
 *
 *			Threshold data in BW(5) format.
 *
 *	Accepts n arguments, the threshold values.  The grey scale 0..255
 *	is divided evenly into n+1 bins.  Each pixel of input is binned
 *	according to the threshold values, and the appropriate bin color
 *	is output.
 *
 *  Author -
 *	Paul Tanenbaum
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#define		USAGE		"Usage: 'bwthresh val ...'\n"

int
main (int argc, char **argv)
{
    int			Ch;		/* The current input character */
    int			*thresh_val;	/* The threshold value */
    int			nm_threshs;	/* How many thresholds? */
    int			i;
    unsigned char	*bin_color = (unsigned char *)0;/* resultant pixel values */

    if ((nm_threshs = argc - 1) < 1)
    {
	(void) fputs(USAGE, stderr);
	exit (1);
    }
    if (nm_threshs > 255)
    {
	(void) fputs("Too many thresholds!\n", stderr);
	exit (1);
    }
    if (((thresh_val = (int *)
	    malloc((unsigned) (nm_threshs * sizeof(int)))) == NULL) ||
	((bin_color = (unsigned char *)
	    malloc((unsigned) ((nm_threshs + 1) * sizeof(int)))) == NULL))
    {
	(void) fputs("bwthresh: Ran out of memory\n", stderr);
	exit (1);
    }
    for (i = 0; i < nm_threshs; ++i)
    {
	if (sscanf(*++argv, "%d", thresh_val + i) != 1)
	{
	    (void) fprintf(stderr, "Illegal threshold value: '%s'\n", *argv);
	    (void) fputs(USAGE, stderr);
	    exit (1);
	}
	if ((unsigned char) thresh_val[i] != thresh_val[i])
	{
	    (void) fprintf(stderr,
		"Threshold[%d] value %d out of range.  Need 0 <= v <= 255\n",
		i, thresh_val[i]);
	    exit (1);
	}
	if ((i > 0) && (thresh_val[i] <= thresh_val[i - 1]))
	{
	    (void) fputs("Threshold values not strictly increasing\n", stderr);
	    exit (1);
	}
	bin_color[i] = 256 * i / nm_threshs;
    }
    bin_color[i] = 255;

    while ((Ch = getchar()) != EOF)
    {
	for (i = 0; i < nm_threshs; ++i)
	    if (Ch < thresh_val[i])
		break;
	(void) putchar(bin_color[i]);
    }
    return 0;
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
