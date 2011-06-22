/*                    P L G E T F R A M E . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2011 United States Government as represented by
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
/** @file util/plgetframe.c
 *
 * Program to separate Plot3(5) file with flush/clear commands into
 * separate files.
 *
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include "bio.h"

#include "bu.h"


#define TBAD	0	/* no such command */
#define TNONE	1	/* no arguments */
#define TSHORT	2	/* Vax 16-bit short */
#define TIEEE	3	/* IEEE 64-bit floating */
#define TCHAR	4	/* unsigned chars */
#define TSTRING	5	/* linefeed terminated string */

struct uplot {
    int targ;	/* type of args */
    int narg;	/* number or args */
    char *desc;	/* description */
};
struct uplot uerror = { 0, 0, 0 };
struct uplot letters[] = {
    /*A*/	{ 0, 0, 0 },
    /*B*/	{ 0, 0, 0 },
    /*C*/	{ TCHAR, 3, "color" },
    /*D*/	{ 0, 0, 0 },
    /*E*/	{ 0, 0, 0 },
    /*F*/	{ TNONE, 0, "flush" },
    /*G*/	{ 0, 0, 0 },
    /*H*/	{ 0, 0, 0 },
    /*I*/	{ 0, 0, 0 },
    /*J*/	{ 0, 0, 0 },
    /*K*/	{ 0, 0, 0 },
    /*L*/	{ TSHORT, 6, "3line" },
    /*M*/	{ TSHORT, 3, "3move" },
    /*N*/	{ TSHORT, 3, "3cont" },
    /*O*/	{ TIEEE, 3, "d_3move" },
    /*P*/	{ TSHORT, 3, "3point" },
    /*Q*/	{ TIEEE, 3, "d_3cont" },
    /*R*/	{ 0, 0, 0 },
    /*S*/	{ TSHORT, 6, "3space" },
    /*T*/	{ 0, 0, 0 },
    /*U*/	{ 0, 0, 0 },
    /*V*/	{ TIEEE, 6, "d_3line" },
    /*W*/	{ TIEEE, 6, "d_3space" },
    /*X*/	{ TIEEE, 3, "d_3point" },
    /*Y*/	{ 0, 0, 0 },
    /*Z*/	{ 0, 0, 0 },
    /*[*/	{ 0, 0, 0 },
    /*\*/	{ 0, 0, 0 },
    /*]*/	{ 0, 0, 0 },
    /*^*/	{ 0, 0, 0 },
    /*_*/	{ 0, 0, 0 },
    /*`*/	{ 0, 0, 0 },
    /*a*/	{ TSHORT, 6, "arc" },
    /*b*/	{ 0, 0, 0 },
    /*c*/	{ TSHORT, 3, "circle" },
    /*d*/	{ 0, 0, 0 },
    /*e*/	{ TNONE, 0, "erase" },
    /*f*/	{ TSTRING, 1, "linmod" },
    /*g*/	{ 0, 0, 0 },
    /*h*/	{ 0, 0, 0 },
    /*i*/	{ TIEEE, 3, "d_circle" },
    /*j*/	{ 0, 0, 0 },
    /*k*/	{ 0, 0, 0 },
    /*l*/	{ TSHORT, 4, "line" },
    /*m*/	{ TSHORT, 2, "move" },
    /*n*/	{ TSHORT, 2, "cont" },
    /*o*/	{ TIEEE, 2, "d_move" },
    /*p*/	{ TSHORT, 2, "point" },
    /*q*/	{ TIEEE, 2, "d_cont" },
    /*r*/	{ TIEEE, 6, "d_arc" },
    /*s*/	{ TSHORT, 4, "space" },
    /*t*/	{ TSTRING, 1, "label" },
    /*u*/	{ 0, 0, 0 },
    /*v*/	{ TIEEE, 4, "d_line" },
    /*w*/	{ TIEEE, 4, "d_space" },
    /*x*/	{ TIEEE, 2, "d_point" },
    /*y*/	{ 0, 0, 0 },
    /*z*/	{ 0, 0, 0 }
};


int verbose;
char buf[8*32];


int
main(int argc, char **argv)
{
    int c;
    struct uplot *up;
    int desired_frame = 0;
    int current_frame = 0;
    size_t ret;

    while (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-v")) {
	    verbose++;
	} else
	    break;

	argc--;
	argv++;
    }
    if (argc < 2 || isatty(fileno(stdin))) {
	bu_exit(1, "Usage: plgetframe [-v] desired_frame < unix_plot\n");
    }
    desired_frame = atoi(argv[1]);
    current_frame = 0;

    while ((c = getchar()) != EOF) {
	/* look it up */
	if (c < 'A' || c > 'z') {
	    up = &uerror;
	} else {
	    up = &letters[ c - 'A' ];
	}
	if (c == 'e') {
	    current_frame++;
	    if (verbose) {
		bu_log("%d, ", current_frame);
	    }
	    if (current_frame > desired_frame)
		break;
	    continue;
	}

	if (up->targ == TBAD) {
	    bu_log("Bad command '%c' (0x%02x)\n", c, c);
	    continue;
	}

	if (current_frame == desired_frame) {
	    /* Duplicate input as output */
	    putchar(c);
	    if (up->narg <= 0) continue;

	    switch (up->targ) {
		case TNONE:
		    break;
		case TCHAR:
		    ret = fread(buf, 1, up->narg, stdin);
		    if (ret == 0) {
			perror("fread");
			break;
		    }
		    ret = fwrite(buf, 1, up->narg, stdout);
		    if (ret == 0) {
			perror("fwrite");
			break;
		    }
		    break;
		case TSHORT:
		    ret = fread(buf, 2, up->narg, stdin);
		    if (ret == 0) {
			perror("fread");
			break;
		    }
		    ret = fwrite(buf, 2, up->narg, stdout);
		    if (ret == 0) {
			perror("fwrite");
			break;
		    }
		    break;
		case TIEEE:
		    ret = fread(buf, 8, up->narg, stdin);
		    if (ret == 0) {
			perror("fread");
			break;
		    }
		    ret = fwrite(buf, 8, up->narg, stdout);
		    if (ret == 0) {
			perror("fwrite");
			break;
		    }
		    break;
		case TSTRING:
		    while ((c = getchar()) != '\n' && c != EOF)
			putchar(c);
		    putchar('\n');
		    break;
	    }
	} else {
	    /* Silently discard input */
	    if (up->narg <= 0) continue;

	    switch (up->targ) {
		case TNONE:
		    break;
		case TCHAR:
		    ret = fread(buf, 1, up->narg, stdin);
		    if (ret == 0) {
			perror("fread");
			break;
		    }
		    break;
		case TSHORT:
		    ret = fread(buf, 2, up->narg, stdin);
		    if (ret == 0) {
			perror("fread");
			break;
		    }
		    break;
		case TIEEE:
		    ret = fread(buf, 8, up->narg, stdin);
		    if (ret == 0) {
			perror("fread");
			break;
		    }
		    break;
		case TSTRING:
		    while ((c = getchar()) != '\n' && c != EOF)
			; /* NULL */
	    }
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
