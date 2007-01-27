/*                       P L - H P G L . C
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
/** @file pl-hpgl.c
 *
 * Description -
 *	Convert a unix-plot file to hpgl codes.
 *
 * Author -
 *	Mark Huston Bowden
 *
 *  Source -
 *	Research Institute, E-47
 *	University of Alabama in Huntsville
 *	Huntsville, AL  35899
 *	(205) 895-6467 UAH
 *	(205) 876-1089 Redstone Arsenal
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

#define ASPECT	(9.8/7.1)
#define	geti(x)	{ (x) = getchar(); (x) |= (short)(getchar()<<8); }
#define getb(x)	((x) = getchar())

int
main(int argc, char **argv)
{
	char colors[8][3];
	int	numcolors = 0;
	int	c,i,x,y,x1,x2,y1,y2,r,g,b;

	if (argc != 1) {
		printf("Usage: %s < infile > outfile\n",argv[0]);
		exit(1);
	}

	getb(c);
	do {
		switch (c) {
		case 'p':		/* point */
			geti(x);
			geti(y);
			printf("PU;\n");
			printf("PA %d %d;\n",x,y);
			printf("PD;\n");
			break;
		case 'l':		/* line */
			geti(x1);
			geti(y1);
			geti(x2);
			geti(y2);
			printf("PU;\n");
			printf("PA %d %d;\n",x1,y1);
			printf("PD;\n");
			printf("PA %d %d;\n",x2,y2);
			break;
		case 'f':		/* line style */
			while( getchar() != '\n');
			/* set line style ignored */
			break;
		case 'm':		/* move */
			geti(x);
			geti(y);
			printf("PU;\n");
			printf("PA %d %d;\n",x,y);
			break;
		case 'n':		/* draw */
			geti(x);
			geti(y);
			printf("PD;\n");
			printf("PA %d %d;\n",x,y);
			break;
		case 't':		/* text */
			while( getchar() != '\n' );
			/* draw text ignored */
			break;
		case 's':		/* space */
			geti(x1);
			geti(y1);
			geti(x2);
			geti(y2);
			x1 *= ASPECT;
			x2 *= ASPECT;
			printf("SC %d %d %d %d;\n",x1,x2,y1,y2);
			printf("SP 1;\n");
			printf("PU;\n");
			printf("PA %d %d;\n",x1,y1);
			printf("PD;\n");
			printf("PA %d %d;\n",x1,y2);
			printf("PA %d %d;\n",x2,y2);
			printf("PA %d %d;\n",x2,y1);
			printf("PA %d %d;\n",x1,y1);
			break;
		case 'C':		/* color */
			r = getchar();
			g = getchar();
			b = getchar();
			for (i = 0; i < numcolors; i++)
				if (r == colors[i][0]
				    &&  g == colors[i][1]
				    &&  b == colors[i][2])
					break;
			if (i < numcolors)
				i++;
			else
				if (numcolors < 8) {
					colors[numcolors][0] = r;
					colors[numcolors][1] = g;
					colors[numcolors][2] = b;
					numcolors++;
					i++;
				}
				else
					i = 8;
			printf("SP %d;\n",i);
			break;
		default:
			fprintf(stderr,"unable to process cmd x%x\n", c);
			break;
		}
		getb(c);
	} while (!feof(stdin));
	exit(0);
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
