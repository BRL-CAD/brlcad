/*                     B R I C K W A L L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file proc-db/brickwall.c
 *
 * builds a brick wall.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"

/* declarations to support use of bu_getopt() */
char *options = "w:n:d:W:N:sB:t:Du:mc:C:h?";

char *progname = "(noname)";
double brick_width=8.0;
double brick_height=2.25;
double brick_depth=3.325;
double wall_width=0.0;
double wall_height=0.0;
char *brick_name="brick";
int standalone=0;
int make_mortar=0;	/* put mortar between bricks */
double tol=0.125;	/* minimum mortar thickness in in */
double tol2;		/* minimum brick dimension allowed */
int debug=0;
double units_conv=25.4;	/* by default, units are converted internally from in to mm */
char color[32] = "160 40 40";
char mortar_color[32] = "190 190 190";


static void
usage(char *s)
{
    if (s) (void)fputs(s, stderr);

    (void) fprintf(stderr,
		   "Usage: %s %s\n%s\n%s\n%s\n",
		   progname,
		   "  [ -u units ] [ -s(tandalone) ] [-t tolerance ]",
		   "  [-m(ortar) ] [ -c R/G/B (brick) ] [ -C R/G/B (mortar)]",
		   "  -w brick_width -n brick_height -d brick_depth -B brick_name",
		   "  -W wall_width -N wall_height\n  > mged_commands \n");

    bu_exit(1, NULL);
}


int
parse_args(int ac, char **av)
{
    int c;
    double d;
    int red, grn, blu;

    if (! (progname=strrchr(*av, '/')))
	progname = *av;
    else
	++progname;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, options)) != -1)
	switch (c) {
	    case 'c':
		if ((c=sscanf(bu_optarg, "%d/%d/%d",
			      &red, &grn, &blu)) == 3)
		    (void)sprintf(color, "%d %d %d", red&0x0ff,
				  grn&0x0ff, blu&0x0ff);
		break;
	    case 'C':
		if ((c=sscanf(bu_optarg, "%d/%d/%d",
			      &red, &grn, &blu)) == 3)
		    (void)sprintf(mortar_color, "%d %d %d",
				  red&0x0ff, grn&0x0ff, blu&0x0ff);

		break;
	    case 'm':
		make_mortar = !make_mortar;
		break;
	    case 'u':
		units_conv = bu_units_conversion(bu_optarg);
		break;
	    case 'D':
		debug = !debug;
		break;
	    case 't':
		d=atof(bu_optarg);
		if (!ZERO(d))
		    tol = d;
		break;
	    case 'w':
		d=atof(bu_optarg);
		if (!ZERO(d))
		    brick_width = d;
		break;
	    case 'n':
		d=atof(bu_optarg);
		if (!ZERO(d))
		    brick_height = d;
		break;
	    case 'd':
		d=atof(bu_optarg);
		if (!ZERO(d))
		    brick_depth = d;
		break;
	    case 'W':
		d=atof(bu_optarg);
		if (!ZERO(d))
		    wall_width = d;
		break;
	    case 'N':
		d=atof(bu_optarg);
		if (!ZERO(d))
		    wall_height = d;
		break;
	    case 'B':
		brick_name = bu_optarg;
		break;
	    case 's':
		standalone = !standalone;
		break;
	    default:
		usage("\n");
		break;
	}

    brick_width *= units_conv;
    brick_height *= units_conv;
    brick_depth *= units_conv;

    wall_width *= units_conv;
    wall_height *= units_conv;

    tol *= units_conv;
    tol2 = tol * 2;

    if (brick_width <= tol2)
	usage("brick width too small\n");

    if (brick_height <= tol2)
	usage("brick height too small\n");

    if (brick_depth <= tol2)
	usage("brick depth too small\n");

    if (wall_width < brick_width)
	usage("wall width < brick width\n");

    if (wall_height < brick_height)
	usage("wall height < brick height\n");

    if (brick_name == (char *)NULL || *brick_name == '\0')
	usage("bad or no brick name\n");

    return bu_optind;
}


void gen_mortar(int horiz_bricks, int vert_bricks, double horiz_spacing, double vert_spacing)
{
    int row;
    int i;
    double xstart;
    double zstart;

    if (vert_spacing > tol)
	if (horiz_spacing > tol)
	    fprintf(stderr, "generating mortar\n");
	else
	    fprintf(stderr, "generating vertical mortar\n");
    else
	if (horiz_spacing > tol)
	    fprintf(stderr, "generating horizontal mortar\n");
	else {
	    fprintf(stderr, "bricks too close for mortar\n");
	    return;
	}

    for (row=0; row < vert_bricks; ++row) {

	if (vert_spacing > tol) {
	    if (row % 2) xstart = brick_depth;
	    else xstart = 0.0;

	    zstart = brick_height + (vert_spacing+brick_height) * row;

	    /* generate a slab of mortar underneath the brick row */
	    fprintf(stdout,
		    "in s.%s.rm.%d rpp %g %g  %g %g  %g %g\n",
		    brick_name, row,
		    xstart, xstart + (wall_width - brick_depth),
		    0.0, brick_depth,
		    zstart, zstart+vert_spacing);

	    fprintf(stdout,
		    "r r.m.%s u s.%s.rm.%d\n",
		    brick_name, brick_name, row);
	}
	if (horiz_spacing > tol) {
	    /* generate mortar between bricks */

	    for (i=0; i < horiz_bricks; ++i) {
		if (row %2)
		    xstart = brick_depth + (brick_width+horiz_spacing)*i;
		else
		    xstart = brick_width + (brick_width+horiz_spacing)*i;

		zstart = (brick_height+vert_spacing) * row;

		fprintf(stdout,
			"in s.%s.bm.%d.%d rpp %g %g  %g %g  %g %g\n",
			brick_name, row, i,
			xstart, xstart + horiz_spacing,
			0.0, brick_depth,
			zstart, zstart+brick_height);

		fprintf(stdout,
			"r r.m.%s u s.%s.bm.%d.%d\n",
			brick_name, brick_name, row, i);

	    }
	}
    }

    fprintf(stdout, "mater r.m.%s\nplastic\n%s\n%s\n\n",
	    brick_name, "sh=40 di=0.9 sp=0.1", mortar_color);

    fprintf(stdout, "g g.%s g.%s.wall r.m.%s\n",
	    brick_name, brick_name, brick_name);

}


/*
 * generate the brick solids, regions thereof, groups for rows
 * and a group for the wall as a whole.
 */
void gen_bricks(int horiz_bricks, int vert_bricks, double horiz_spacing, double vert_spacing)
{
    int row;
    int brick;
    double offset;
    double xstart;
    double zstart;

    /* print the commands to make the wall */

    for (row=0; row < vert_bricks; ++row) {

	if (row % 2) offset = brick_depth + horiz_spacing;
	else offset = 0.0;


	for (brick=0; brick < horiz_bricks; ++ brick) {
	    xstart = brick * brick_width +
		brick * horiz_spacing + offset;
	    zstart = row * brick_height + row * vert_spacing;

	    fprintf(stdout,
		    "in s.%s.%d.%d rpp %g %g  %g %g  %g %g\n",
		    brick_name, row, brick,
		    xstart, xstart + brick_width,
		    0.0, brick_depth,
		    zstart, zstart + brick_height);

	    fprintf(stdout,
		    "r r.%s.%d.%d u s.%s.%d.%d\n",
		    brick_name, row, brick,
		    brick_name, row, brick);

	    fprintf(stdout, "g g.%s.r.%d r.%s.%d.%d\n",
		    brick_name, row, brick_name, row, brick);
	}

	fprintf(stdout, "g g.%s.wall g.%s.r.%d\n",
		brick_name, brick_name, row);
    }

    fprintf(stdout, "mater g.%s.wall\nplastic\n%s\n%s\n\n",
	    brick_name, "sh=40 di=0.9 sp=0.1", color);
}
/*
 * Call parse_args to handle command line arguments first, then
 * process input.
 */
int main(int ac, char **av)
{
    int horiz_bricks;
    int vert_bricks;
    double horiz_spacing;
    double vert_spacing;

    if (ac == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout)))
	usage("\n");

    /* parse command flags, and make sure there are arguments
     * left over for processing.
     */
    if (parse_args(ac, av) < ac)
	usage("Excess command line arguments\n");

    /* build the wall

       if (debug)
       fprintf(stderr,
       "bw %g bh %g bd %g ww %g wh %g bn\"%s\"\n",
       brick_width, brick_height, brick_depth,
       wall_width, wall_height, brick_name);
    */

    horiz_bricks = (int)(wall_width / brick_width);

    /* leave room for sideways brick at one end */
    while (horiz_bricks * brick_width + brick_depth > wall_width &&
	   horiz_bricks > 0)
	--horiz_bricks;

    if (horiz_bricks <= 0) {
	fprintf(stderr, "wall not wide enough for brick\n");
	return -1;
    }

    if (standalone) {
	horiz_spacing =
	    (wall_width - horiz_bricks * brick_width) /
	    (horiz_bricks - 1);
    } else {
	horiz_spacing =
	    (wall_width - (horiz_bricks * brick_width + brick_depth))/
	    horiz_bricks;
    }


    vert_bricks = (int)(wall_height / brick_height);

    if (vert_bricks > 1) {
	vert_spacing =
	    (wall_height - vert_bricks * brick_height) /
	    (vert_bricks - 1);
    } else {
	vert_spacing = 0;
    }


    fprintf(stderr, "wall %d bricks wide,  %d bricks high\n",
	    horiz_bricks, vert_bricks);
    fprintf(stderr, "horizontal distance between adjacent bricks %g\n",
	    horiz_spacing / units_conv);
    fprintf(stderr, "distance between layers of bricks %g\n",
	    vert_spacing / units_conv);


    (void)putc((int)'\n', stdout);
    gen_bricks(horiz_bricks, vert_bricks, horiz_spacing, vert_spacing);

    if (make_mortar && (vert_spacing > tol || horiz_spacing > tol))
	gen_mortar(horiz_bricks, vert_bricks, horiz_spacing, vert_spacing);

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
