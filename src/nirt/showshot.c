/*                      S H O W S H O T . C
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
/** @file nirt/showshot.c
 *
 * Filter output from NIRT(1) to generate an MGED(1) object
 * representing a shotline through some geometry.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "db.h"


#define BUF_LEN 128
#define OPT_STRING "n:r:?"
#define RAY_COLOR "255 255 0"
#define print_usage() bu_exit(1, "Usage: 'show-shot [-n name] [-r radius]'\n")

int
main (int argc, char **argv)
{
    char buf[BUF_LEN];		/* Contents of current input line */
    char *bp;			/* Pointer into buf */
    char *nlp;			/* Location of newline in buf */
    char rname[BUF_LEN];	/* Name of current region */
    char rayname[BUF_LEN];	/* Name of ray */
    fastf_t ray_radius = 1.0;	/* Thickness of the RCC */
    int i;			/* Index into rname */
    int line_nm = 0;		/* Number of current line of input */
    int opt;			/* Command-line option returned by bu_getopt */
    int pid;			/* Process ID for unique group name */
    point_t entryp = VINIT_ZERO;	/* Ray's entry into current region */
    point_t exitp = VINIT_ZERO;		/* Ray's exit from current region */
    point_t first_entryp = VINIT_ZERO;	/* Ray's entry into the entire geometry */

    pid = bu_process_id();

    *rayname = '\0';
    /* Handle command-line options */
    while ((opt = bu_getopt(argc, argv, OPT_STRING)) != -1)
	switch (opt) {
	    case 'n':
		bu_strlcpy(rayname, bu_optarg, BUF_LEN);
		break;
	    case 'r':
		if (sscanf(bu_optarg, "%lf", &ray_radius) != 1) {
		    bu_exit(1, "Illegal radius: '%s'\n", bu_optarg);
		}
		break;
	    case '?':
		print_usage();
	}

    /* Ensure proper command-line syntax */
    if (bu_optind != argc) {
	print_usage();
    }

    /* Construct the names of the objects to add to the database */
    if (*rayname == '\0')	/* Was one given on command line? */
	sprintf(rayname, "ray.%d", pid);

    if (strlen(rayname) > NAMESIZE) {
	fprintf(stderr,
		"Name '%s.s' for ray solid may not exceed %d characters\n",
		rayname, NAMESIZE);
	bu_exit(1, "Use the '-n name' option to specify a different name\n");
    }

    printf("in %s.s sph 0 0 0 1\n", rayname);
    printf("r %s.r u %s.s\n", rayname, rayname);
    printf("mater %s.r\n\n\n%s\n\n", rayname, RAY_COLOR);
    printf("g %s", rayname);

    /* Read the input */
    while (bu_fgets(buf, BUF_LEN, stdin) != NULL) {
	++line_nm;
	bp = buf;
	if ((nlp = strchr(bp, '\n')) != 0)
	    *nlp = '\0';

	/* Skip initial white space */
	while (isspace(*bp))
	    ++bp;

	/* Get region name */
	for (i = 0; ! isspace(*bp) && (*bp != '\0'); ++i, ++bp)
	    rname[i] = *bp;
	rname[i] = '\0';

	/* Read entry and exit coordinates for this partition */
	if (sscanf(bp, "%lf%lf%lf%lf%lf%lf",
		   &entryp[X], &entryp[Y], &entryp[Z],
		   &exitp[X], &exitp[Y], &exitp[Z]) != 6) {
	    bu_exit(1, "Illegal data on line %d: '%s'\n", line_nm, bp);
	}

	printf(" %s", rname);

	if (line_nm == 1) {
	    VMOVE(first_entryp, entryp);
	}
    }
    if (! feof(stdin)) {
	bu_exit(1, "Error from bu_fgets().  This shouldn't happen");
    }

    printf("\nkill %s.s\nin %s.s rcc\n\t%f %f %f\n\t%f %f %f\n\t%f\n",
	   rayname, rayname,
	   first_entryp[X], first_entryp[Y], first_entryp[Z],
	   exitp[X] - first_entryp[X],
	   exitp[Y] - first_entryp[Y],
	   exitp[Z] - first_entryp[Z],
	   ray_radius);
    printf("g %s %s.r\n", rayname, rayname);
    fprintf(stderr, "Group is '%s'\n", rayname);

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
