/*                           S Y N . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file syn.c
 *
 * Multi Sine Synthesis
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"

#define TABSIZE 512
double sintab[TABSIZE];


void
makesintab(void)
{
    int i;
    double theta;

    for (i = 0; i < TABSIZE; i ++) {
	theta = i / (double)TABSIZE * 2 * M_PI;
	sintab[i] = sin(theta);
    }
}


int
main(int argc, char *argv[])
{
    int i;
    double d;
    double period, stepsize, findex;
    int setsize;
    size_t ret;

    static const char usage[] = "\
Usage: syn samples_per_set [ratio] > doubles\n";

    if (isatty(fileno(stdout)) || argc < 2) {
	bu_exit(1, "%s", usage);
    }

    makesintab();
    fprintf(stderr, "init done\n");

    setsize = atoi(argv[1]);

    findex = 0;
    stepsize = 0;
    while (fread(&period, sizeof(period), 1, stdin) == 1) {
	if (period > 0)
	    stepsize = TABSIZE / period;
	for (i = setsize; i > 0; i--) {
	    d = sintab[(int)findex];
	    d *= 0.4;
	    ret = fwrite(&d, sizeof(d), 1, stdout);
	    if (ret != 1)
		perror("fwrite");
	    findex += stepsize;
	    if (findex > TABSIZE)
		findex -= TABSIZE;
	}
    }
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
