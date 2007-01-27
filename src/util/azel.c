/*                          A Z E L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file azel.c
 *
 *	     This program reads data for points in Euclidean 3-space
 *	and prints out the same data, having transformed the coordinates
 *	of the points according to an azimuth-elevation rotation.
 *
 *	     Each point in the input must be of the form:
 *
 *			a  b  c  field_1  field_2  ...  field_n.
 *
 *	The default behavior of the program is to interpret (a, b, c) as
 *	the point (x, y, z) in unrotated coordinates and to output
 *
 *			d  h  v  field_1  field_2  ...  field_n.
 *
 *	where (d, h, v) are the "viewer's coordinates."
 *
 *	     The -p option causes the program to output the projection
 *	of the point onto a plane normal to the line of sight:
 *
 *			 h  v  field_1  field_2  ...  field_n.
 *
 *	     The -i option causes the program to invert the rotation.
 *	(a, b, c) is interpreted as the point (d, h, v) in the viewer's
 *	coordinates, and the point is output in derotated form:
 *
 *			x  y  z  field_1  field_2  ...  field_n.
 *  Author -
 *	Paul J. Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"


#define		DEG2RAD		0.01745329	/* Convert degrees to radians */
#define		OPT_STRING	"a:c:e:ipr?"	/* For bu_getopt(3) */
#define		fpeek(f)	ungetc(fgetc(f), f)

void	PrintUsage(void);
void	GetCoord(FILE *Whence, double *Coord, char Label, int LineNm, char *FileName);

int
main (int argc, char **argv)
{
    char            *inFname = "stdin"; /* Name of input source */
    char            *outFname = "stdout";  /* Name of output destination */
    char            *Label;             /* Names of input coordinates */
    char            Tail[4096] = {0};   /* Rest of input line beyond coords */
    extern char     *bu_optarg;         /* argument from bu_getopt(3C) */
    FILE            *inPtr = stdin;     /* Pointer to input */
    FILE            *outPtr = stdout;   /* Pointer to output */
    double          Azim = 0.0;         /* Azimuth angle (in degrees) */
    double          Elev = 0.0;         /* Elevation angle (in degrees) */
    double          CelSiz = 1.0;       /* Size of cells (dimensionless) */
    double          Cazim;              /* Cosine of the azimuth angle */
    double          Celev;              /* Cosine of the elevation angle */
    double          Sazim;              /* Sine of the azimuth angle */
    double          Selev;              /* Sine of the elevation angle */
    double          U1;                 /* Input coords of current point */
    double          V1;                 /*   "      "    "    "      "   */
    double          W1;                 /*   "      "    "    "      "   */
    double          U2;			/* Output coords of current point */
    double          V2;                 /*   "      "    "    "      "   */
    double          W2;                 /*   "      "    "    "      "   */
    double          UU;                 /* Weight of U1 in computing U2 */
    double          UV;                 /*    "    " U1  "     "     V2 */
    double          UW;                 /*    "    " U1  "     "     W2 */
    double          VU;                 /* Weight of V1 in computing U2 */
    double          VV;                 /*    "    " V1  "     "     V2 */
    double          VW;                 /*    "    " V1  "     "     W2 */
    double          WU;                 /* Weight of W1 in computing U2 */
    double          WV;                 /*    "    " W1  "     "     V2 */
    double          WW;                 /*    "    " W1  "     "     W2 */
    int             Invert = 0;		/* Undo az-el rotation? */
    int             PlanarProj = 0;	/* Project points onto plane? */
    int             Round = 0;		/* Round the output coords? */
    int             LineNm = 0;         /* How far through input? */
    int             Ch;                 /* Input character */
    int             i;                  /* Dummy variable for loop indexing */
    extern int      bu_optind;             /* index from bu_getopt(3C) */

    /* Handle command-line options */
    while ((Ch = bu_getopt(argc, argv, OPT_STRING)) != EOF)
	switch (Ch)
	{
	    case 'a':
		if (sscanf(bu_optarg, "%lf", &Azim) != 1)
		{
		    (void) fprintf(stderr,
			"Bad azimuth specification: '%s'\n", bu_optarg);
		    PrintUsage();
		    exit(1);
		}
		break;
	    case 'c':
		if (sscanf(bu_optarg, "%lf", &CelSiz) != 1)
		{
		    (void) fprintf(stderr,
			"Bad cell-size specification: '%s'\n", bu_optarg);
		    PrintUsage();
		    exit(1);
		}
		break;
	    case 'e':
		if (sscanf(bu_optarg, "%lf", &Elev) != 1)
		{
		    (void) fprintf(stderr,
			"Bad elevation specification: '%s'\n", bu_optarg);
		    PrintUsage();
		    exit(1);
		}
		break;
	    case 'i':
		Invert = 1;
		break;
	    case 'p':
		PlanarProj = 1;
		break;
	    case 'r':
		Round = 1;
		break;
	    default:
		fprintf(stderr, "Bad option '-%c'\n", Ch);
	    case '?':
		PrintUsage();
		exit (Ch != '?');
	}

    if (PlanarProj && Invert)
    {
	fputs("Incompatible options: -i and -p\n", stderr);
	PrintUsage();
	exit (1);
    }

    /* Determine source and destination */
    if (argc - bu_optind > 0)
    {
	inFname = argv[bu_optind];
	if ((inPtr = fopen(inFname, "r")) == NULL)
	{
	    fprintf(stderr, "azel:  Cannot open file '%s'\n", inFname);
	    exit(1);
	}
	if (argc - bu_optind > 1)
	{
	    outFname = argv[bu_optind + 1];
	    if ((outPtr = fopen(outFname, "w")) == NULL)
	    {
		fprintf(stderr, "azel:  Cannot create file '%s'\n", outFname);
		exit(1);
	    }
	}
	if (argc - bu_optind > 2)
	{
	    PrintUsage();
	    exit (1);
	}
    }

    /* Compute transformation coefficients */
    Cazim = cos(Azim * DEG2RAD);
    Celev = cos(Elev * DEG2RAD);
    Sazim = sin(Azim * DEG2RAD);
    Selev = sin(Elev * DEG2RAD);
    if (Invert)
    {
	UU = Celev * Cazim;
	VU = -Sazim;
	WU = -(Selev * Cazim);
	UV = Celev * Sazim;
	VV = Cazim;
	WV = -(Selev * Sazim);
	UW = Selev;
	VW = 0.0;
	WW = Celev;
    }
    else
    {
	UU = Celev * Cazim;
	VU = Celev * Sazim;
	WU = Selev;
	UV = -Sazim;
	VV = Cazim;
	WV = 0.0;
	UW = -(Selev * Cazim);
	VW = -(Selev * Sazim);
	WW = Celev;
    }

/* * * * * Filter Data * * * * */
    Label = Invert ? "DHV" : "XYZ";

    while ((Ch = fpeek(inPtr)) != EOF)
    {
/* Read U1, V1, and W1 of next point in input frame of reference */
	GetCoord(inPtr, &U1, *Label, LineNm + 1, inFname);
	GetCoord(inPtr, &V1, *(Label + 1), LineNm + 1, inFname);
	GetCoord(inPtr, &W1, *(Label + 2), LineNm + 1, inFname);

/* Compute U2, V2, and W2 for this point */
	U2 = (U1 * UU + V1 * VU + W1 * WU) / CelSiz;
	V2 = (U1 * UV + V1 * VV + W1 * WV) / CelSiz;
	W2 = (U1 * UW + V1 * VW + W1 * WW) / CelSiz;

	if (Round)
	{
	    U2 = floor(U2 + .5);
	    V2 = floor(V2 + .5);
	    W2 = floor(W2 + .5);
	}

/* Read in the rest of the line for subsequent dumping out */
	for (i = 0; (Ch = fgetc(inPtr)) != '\n'; i++)
	    if (Ch == EOF)
	    {
		Tail[i] = '\n';
		break;
	    }
	    else
		Tail[i] = Ch;
	Tail[i] = '\0';

/* Write out the filtered version of this line */
	if (! PlanarProj)
	    fprintf(outPtr, "%g\t", U2);
	fprintf(outPtr, "%g\t%g\t%s\n", V2, W2, Tail);
	LineNm++;
    }
    exit (0);
}
/* ======================================================================== */
void
PrintUsage (void)
{
    fputs("Usage:  'azel [-a azim] [-e elev] [-c celsiz] [-{ip}r] [infile [outfile]]'\n", stderr);
}
/* ======================================================================== */
void
GetCoord (FILE *Whence, double *Coord, char Label, int LineNm, char *FileName)

			/* File from which to read */
			/* Where to store coordinate */
			/* Name of coordinate */
			/* How far in input? */
			/* What input stream? */

{
    int     Ch;

/* Skip leading white space */
    while (((Ch = fgetc(Whence)) == ' ') || (Ch == '\t'))
	;

    if (ungetc(Ch, Whence) == EOF)
    {
	fprintf(stderr, "azel:  Premature end-of-file, file %s\n",
		FileName);
	exit(1);
    }
    if (Ch == '\n')
    {
	fprintf(stderr, "azel:  Premature end-of-line on line %d, file %s\n",
		LineNm, FileName);
	exit(1);
    }

    if (fscanf(Whence, "%lf", Coord) != 1)
    {
	fprintf(stderr, "azel:  Bad %c-coordinate at line %d, file %s\n",
		Label, LineNm, FileName);
	exit(1);
    }
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
