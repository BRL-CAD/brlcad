/*                    P L O T 3 - H P G L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
/** @file util/plot3-hpgl.c
 *
 * Description -
 * Convert a unix-plot file to hpgl codes.
 *
 * Author -
 * Mark Huston Bowden
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/log.h"
#include "bu/str.h"
#include "bu/cv.h"


#define ASPECT (9.8/7.1)
#define geti(x) { (x) = getchar(); (x) |= (short)(getchar()<<8); }
#define getb(x)	((x) = getchar())

void printusage (const char *name)
{
	bu_exit(1, "Usage: %s < infile > outfile\n", name);
}

int
main(int argc, char **argv)
{
    char colors[8][3];
    int numcolors = 0;
    int c, i, x, y, x1, x2, y1, y2, r, g, b;

    if (argc != 1) {
	printusage(argv[0]);
    }

    if (isatty(fileno(stdin)) || isatty(fileno(stdout))) {
	printusage(argv[0]);
    }

    getb(c);
    do {
	switch (c) {
	    case 'p':		/* point */
		geti(x);
		geti(y);
		printf("PU;\n");
		printf("PA %d %d;\n", x, y);
		printf("PD;\n");
		break;
	    case 'l':		/* line */
		geti(x1);
		geti(y1);
		geti(x2);
		geti(y2);
		printf("PU;\n");
		printf("PA %d %d;\n", x1, y1);
		printf("PD;\n");
		printf("PA %d %d;\n", x2, y2);
		break;
	    case 'f':		/* line style */
		while (getchar() != '\n');
		/* set line style ignored */
		break;
	    case 'm':		/* move */
		geti(x);
		geti(y);
		printf("PU;\n");
		printf("PA %d %d;\n", x, y);
		break;
	    case 'n':		/* draw */
		geti(x);
		geti(y);
		printf("PD;\n");
		printf("PA %d %d;\n", x, y);
		break;
	    case 't':		/* text */
		while (getchar() != '\n');
		/* draw text ignored */
		break;
	    case 's':		/* space */
		geti(x1);
		geti(y1);
		geti(x2);
		geti(y2);
		x1 *= ASPECT;
		x2 *= ASPECT;
		printf("SC %d %d %d %d;\n", x1, x2, y1, y2);
		printf("SP 1;\n");
		printf("PU;\n");
		printf("PA %d %d;\n", x1, y1);
		printf("PD;\n");
		printf("PA %d %d;\n", x1, y2);
		printf("PA %d %d;\n", x2, y2);
		printf("PA %d %d;\n", x2, y1);
		printf("PA %d %d;\n", x1, y1);
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
		    } else
			i = 8;
		printf("SP %d;\n", i);
		break;
	    default:
		bu_log("unable to process cmd x%x\n", c);
		break;
	}
	getb(c);
    } while (!feof(stdin));

    return 0;
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
