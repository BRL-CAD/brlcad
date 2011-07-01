/*                      P I X C O U N T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
/** @file util/pixcount.c
 *
 * Sort the pixels of an input stream by color value.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "bu.h"


struct pixel {
    long p_magic;
    unsigned char *p_color;
    int p_count;
};
#define PIXEL_NULL ((struct pixel *) 0)
#define PIXEL_MAGIC 0x7078656c


/*
 * Global variables
 */
int pixel_size = 3;		/* Bytes/pixel */
FILE *outfp = NULL;		/* output file */

static const char usage[] = "\
Usage: 'pixcount [-# bytes_per_pixel]\n\
		 [infile.pix [outfile]]'\n";
#define OPT_STRING "#:?"


static void print_usage (void)
{
    bu_exit(1, "%s", usage);
}


/*
 * M K _ P I X E L ()
 *
 */
struct pixel *mk_pixel (unsigned char *color)
{
    int i;
    struct pixel *pp;

    pp = (struct pixel *) bu_malloc(sizeof(struct pixel), "pixel");

    pp->p_magic = PIXEL_MAGIC;
    pp->p_color = (unsigned char *)
	bu_malloc(pixel_size * sizeof(unsigned char),
		  "pixel color");
    for (i = 0; i < pixel_size; ++i)
	pp->p_color[i] = color[i];
    pp->p_count = 0;

    return pp;
}


/*
 * F R E E _ P I X E L ()
 *
 */
void free_pixel (struct pixel *pp)
{
    BU_CKMAG(pp, PIXEL_MAGIC, "pixel");
    bu_free((genptr_t) pp, "pixel");
}


/*
 * P R I N T _ P I X E L ()
 *
 */
void print_pixel (void *p, int UNUSED(depth))
{
    int i;
    struct pixel *pp = (struct pixel *) p;

    BU_CKMAG(pp, PIXEL_MAGIC, "pixel");

    for (i = 0; i < pixel_size; ++i)
	fprintf(outfp, "%3d ", pp->p_color[i]);
    fprintf(outfp, " %d\n", pp->p_count);
}


/*
 * C O M P A R E _ P I X E L S ()
 *
 * The comparison callback for the red-black tree
 */
int compare_pixels (void *v1, void *v2)
{
    struct pixel *p1 = (struct pixel *) v1;
    struct pixel *p2 = (struct pixel *) v2;
    int i;

    BU_CKMAG(p1, PIXEL_MAGIC, "pixel");
    BU_CKMAG(p2, PIXEL_MAGIC, "pixel");

    for (i = 0; i < pixel_size; ++i) {
	if (p1->p_color[i] < p2->p_color[i])
	    return -1;
	else if (p1->p_color[i] > p2->p_color[i])
	    return 1;
    }
    return 0;
}


/*
 * L O O K U P _ P I X E L ()
 */
struct pixel *lookup_pixel(struct bu_rb_tree *palette, unsigned char *color)
{
    int rc = 0;	/* Return code from bu_rb_insert() */
    struct pixel *qpp = NULL;	/* The query */
    struct pixel *pp = NULL;	/* Value to return */

    /*
     * Prepare the palette query
     */
    qpp = mk_pixel(color);

    /*
     * Perform the query by attempting an insertion...
     * If the query succeeds (i.e., the insertion fails!),
     * then we have our pixel.
     * Otherwise, we must create a new pixel.
     */
    switch (rc = bu_rb_insert(palette, (void *) qpp)) {
	case -1:
	    pp = (struct pixel *) bu_rb_curr1(palette);
	    free_pixel(qpp);
	    break;
	case 0:
	    pp = qpp;
	    break;
	default:
	    bu_exit(1, "bu_rb_insert() returns %d:  This should not happen\n", rc);
    }

    return pp;
}


int
main (int argc, char **argv)
{
    struct bu_rb_tree *palette;	/* Pixel palette */
    char *inf_name;	/* name of input stream */
    char *outf_name;	/* " " output " */
    unsigned char *buf;		/* the current input pixel */
    FILE *infp = NULL;	/* input stream */
    int ch;		/* current char in command line */
    struct pixel *pp;

    /*
     * Process the command line
     */
    while ((ch = bu_getopt(argc, argv, OPT_STRING)) != -1)
	switch (ch) {
	    case '#':
		if (sscanf(bu_optarg, "%d", &pixel_size) != 1) {
		    bu_log("Invalid pixel size: '%s'\n", bu_optarg);
		    print_usage();
		}
		break;
	    case '?':
	    default:
		print_usage();
	}
    switch (argc - bu_optind) {
	case 0:
	    inf_name = "stdin";
	    infp = stdin;
	    /* Break intentionally missing */
	case 1:
	    outf_name = "stdout";
	    outfp = stdout;
	    /* Break intentionally missing */
	case 2:
	    break;
	default:
	    print_usage();
    }

    /*
     * Open input and output files, as necessary
     */
    if (infp == NULL) {
	inf_name = argv[bu_optind];
	if ((infp = fopen(inf_name, "r")) == NULL)
	    bu_exit(1, "Cannot open input file '%s'\n", inf_name);
	if (outfp == NULL) {
	    outf_name = argv[++bu_optind];
	    if ((outfp = fopen(outf_name, "w")) == NULL)
		bu_exit(1, "Cannot open output file '%s'\n", outf_name);
	}
    }

    /*
     * Ensure that infp is kosher,
     */
    if (infp == stdin) {
	if (isatty(fileno(stdin))) {
	    bu_log("FATAL: pixcount reads only from file or pipe\n");
	    print_usage();
	}
    }

    palette = bu_rb_create1("Pixel palette", compare_pixels);
    bu_rb_uniq_on1(palette);

    /*
     * Read the input stream into the palette
     */
    buf = (unsigned char *)
	bu_malloc(pixel_size * sizeof(unsigned char),
		  "pixel buffer");
    while (fread((void *) buf, pixel_size * sizeof(unsigned char), 1, infp) == 1) {
	pp = lookup_pixel(palette, buf);
	BU_CKMAG(pp, PIXEL_MAGIC, "pixel");

	++(pp->p_count);
    }
    bu_free((genptr_t) buf, "pixel buffer");

    bu_rb_walk1(palette, print_pixel, INORDER);

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
