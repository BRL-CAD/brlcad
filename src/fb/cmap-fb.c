/*                       C M A P - F B . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
 *
 */
/** @file cmap-fb.c
 *
 * Load a colormap into a framebuffer.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bu.h"
#include "fb.h"


/*
 * Puts the next symbol from cp into the buffer b.
 * Returns a pointer to the current location (one
 * char beyond the symbol or at a NULL).
 */
static
char *
nextsym(char *b, char *cp)
{
    /* skip white */
    while (isspace(*cp))
	cp++;

    while (*cp != '\0' && !isspace(*cp))
	*b++ = *cp++;

    *b = '\0';
    return cp;
}


/*
 * Hex to integer
 * must have NO leading blanks
 * does not check for errors.
 */
static
int
htoi(char *s)
{
    int i;

    i = 0;

    while (*s != '\0') {
	i <<= 4;	/* times 16 */
	if (*s == 'x' || *s == 'X')
	    i = 0;
	else if (*s >= 'a')
	    i += *s - 'a' + 10;
	else if (*s >= 'A')
	    i += *s - 'A' + 10;
	else
	    i += *s - '0';
	s++;
    }
    return i;
}


int
main(int argc, char **argv)
{
    ColorMap cm;
    char usage[] = "Usage: cmap-fb [-h -o] [colormap]\n";

    FBIO *fbp;
    FILE *fp;
    int fbsize = 512;
    int overlay = 0;
    int idx, ret;
    char line[512], buf[512], *str;

    while (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-h")) {
	    fbsize = 1024;
	} else if (BU_STR_EQUAL(argv[1], "-o")) {
	    overlay++;
	} else if (argv[1][0] == '-') {
	    /* unknown flag */
	    bu_exit(1, "%s", usage);
	} else
	    break;	/* must be a filename */
	argc--;
	argv++;
    }

    if (argc > 1) {
	if ((fp = fopen(argv[1], "rb")) == NULL) {
	    fprintf(stderr, "cmap-fb: can't open \"%s\"\n", argv[1]);
	    bu_exit(2, "%s", usage);
	}
    } else
	fp = stdin;

    if ((fbp = fb_open(NULL, fbsize, fbsize)) == FBIO_NULL)
	bu_exit(3, "Unable to open framebuffer\n");

    if (overlay)
	fb_rmap(fbp, &cm);

    while (bu_fgets(line, 511, fp) != NULL) {
	str = line;
	str = nextsym(buf, str);
	if (! isdigit(buf[0])) {
	    /* spare the 0 entry the garbage */
	    continue;
	}
	idx = atoi(buf);
	if (idx < 0 || idx > 255) {
	    continue;
	}
	str = nextsym(buf, str);
	cm.cm_red[idx] = htoi(buf);

	str = nextsym(buf, str);
	cm.cm_green[idx] = htoi(buf);

	str = nextsym(buf, str);
	cm.cm_blue[idx] = htoi(buf);
    }

    ret = fb_wmap(fbp, &cm);
    fb_close(fbp);
    if (ret < 0) {
	bu_exit(1, "cmap-fb: couldn't write colormap\n");
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
