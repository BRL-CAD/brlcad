/*                       P L D E B U G . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2010 United States Government as represented by
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
/** @file pldebug.c
 *
 * Plot3(5) debugger
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "common.h"


#include "bu.h"


#define TBAD	0	/* no such command */
#define TNONE	1	/* no arguments */
#define TSHORT	2	/* Vax 16-bit short */
#define TIEEE	3	/* IEEE 64-bit floating */
#define TCHAR	4	/* unsigned chars */
#define TSTRING	5	/* linefeed terminated string */

#define COMMA ','


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
long counts['z'-'A'+1];	/* for counting command usage */
FILE *fp;

/* void outchar(), outstring(), outshort(), outfloat(); */

static const char usage[] = "\
Usage: pldebug [-v] [unix_plot]\n";


void
outchar(int n)
{
    int i, c;

    putchar('(');
    for (i = 0; i < n; i++) {
	if (i != 0)
	    putchar(COMMA);
	c = getc(fp);
	printf("%3d", c);
    }
    putchar(')');
}


void
outstring(int UNUSED(n))
{
    int c;

    putchar('"');
    while ((c = getc(fp)) != '\n' && c != EOF)
	putchar(c);
    putchar('"');
}


int
getshort(void)
{
    long v, w;

    v = getc(fp);
    v |= (getc(fp)<<8);	/* order is important! */

    /* worry about sign extension - sigh */
    if (v <= 0x7FFF) return v;
    w = -1;
    w &= ~0x7FFF;
    return w | v;
}


void
outshort(int n)
{
    int i;
    short s;

    putchar('(');
    for (i = 0; i < n; i++) {
	if (i != 0)
	    putchar(COMMA);
	s = getshort();
	printf("%d", s);
    }
    putchar(')');
}


void
outfloat(int n)
{
    int i;
    unsigned char in[8*16];
    double out[16];

    fread(in, 8, n, fp);
    ntohd((unsigned char *)out, in, n);

    putchar('(');
    for (i = 0; i < n; i++) {
	if (i != 0)
	    putchar(COMMA);
	printf("%g", out[i]);
    }
    putchar(')');
}


int
main(int argc, char **argv)
{
    int c;
    struct uplot *up;
    int i;

    while (argc > 1) {
	if (strcmp(argv[1], "-v") == 0) {
	    verbose++;
	} else
	    break;

	argc--;
	argv++;
    }
    if (argc == 2) {
	if ((fp = fopen(argv[1], "r")) == NULL) {
	    perror("pldebug");
	    bu_exit (1, NULL);
	}
    } else {
	fp = stdin;
	if (argc > 1 || isatty(fileno(stdin))) {
	    bu_exit(1, "%s", usage);
	}
    }

    while ((c = getc(fp)) != EOF) {
	/* look it up */
	if (c < 'A' || c > 'z') {
	    up = &uerror;
	} else {
	    up = &letters[ c - 'A' ];
	}

	if (up->targ == TBAD) {
	    fprintf(stderr, "Bad command '%c' (0x%02x)\n", c, c);
	    continue;
	}
	if (verbose)
	    counts[ c - 'A' ]++;

	putchar(c);
	if (up->narg > 0) {
	    switch (up->targ) {
		case TNONE:
		    break;
		case TSHORT:
		    outshort(up->narg);
		    break;
		case TIEEE:
		    outfloat(up->narg);
		    break;
		case TSTRING:
		    outstring(up->narg);
		    break;
		case TCHAR:
		    outchar(up->narg);
		    break;
	    }
	}

	if (verbose)
	    printf(" %s", up->desc);
	putchar('\n');
    }

    if (verbose) {
	/* write command usage summary */
	for (i = 0; i < 'z'-'A'+1; i++) {
	    if (counts[i] != 0) {
		fprintf(stderr, "%s %ld\n", letters[i].desc, counts[i]);
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
