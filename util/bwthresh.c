/*
 *				B W T H R E S H . C
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
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#define		USAGE		"Usage: 'bwthresh val ...'\n"

int
main (argc, argv)
int	argc;
char	**argv;
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
