/*
 *			P I X C O U N T . C
 *
 *	Sort the pixels of an input stream by color value.
 *
 *  Author -
 *	Paul J. Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1998-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"
#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"		/* For getopt(), etc. */
#include "bu.h"
#include "redblack.h"

struct pixel
{
    long		p_magic;
    unsigned char	*p_color;
    int			p_count;
};
#define	PIXEL_NULL	((struct pixel *) 0)
#define	PIXEL_MAGIC	0x7078656c


/*
 *	Global variables
 */
int		pixel_size = 3;		/* Bytes/pixel */
FILE		*outfp = NULL;		/* output file */

static char usage[] = "\
Usage: 'pixcount [-# bytes_per_pixel]\n\
		 [infile.pix [outfile]]'\n";
#define OPT_STRING	"#:?"

static void print_usage (void)
{
    (void) bu_log("%s", usage);
}

/*
 *			     M K _ P I X E L ( )
 *
 */
struct pixel *mk_pixel (unsigned char *color)
{
    int			i;
    struct pixel	*pp;

    pp = (struct pixel *) bu_malloc(sizeof(struct pixel), "pixel");

    pp -> p_magic = PIXEL_MAGIC;
    pp -> p_color = (unsigned char *)
		bu_malloc(pixel_size * sizeof(unsigned char),
			"pixel color");
    for (i = 0; i < pixel_size; ++i)
	pp -> p_color[i] = color[i];
    pp -> p_count = 0;

    return (pp);
}

/*
 *			   F R E E _ P I X E L ( )
 *
 */
void free_pixel (struct pixel *pp)
{
    BU_CKMAG(pp, PIXEL_MAGIC, "pixel");
    bu_free((genptr_t) pp, "pixel");
}

/*
 *			  P R I N T _ P I X E L ( )
 *
 */
void print_pixel (void *p, int depth)
{
    int			i;
    struct pixel	*pp = (struct pixel *) p;

    BU_CKMAG(pp, PIXEL_MAGIC, "pixel");

    for (i = 0; i < pixel_size; ++i)
	fprintf(outfp, "%3d ", pp -> p_color[i]);
    fprintf(outfp, " %d\n", pp -> p_count);
}

/*
 *		C O M P A R E _ P I X E L S ( )
 *
 *	    The comparison callback for the red-black tree
 */
int compare_pixels (void *v1, void *v2)
{
    struct pixel	*p1 = (struct pixel *) v1;
    struct pixel	*p2 = (struct pixel *) v2;
    int			i;

    BU_CKMAG(p1, PIXEL_MAGIC, "pixel");
    BU_CKMAG(p2, PIXEL_MAGIC, "pixel");

    for (i = 0; i < pixel_size; ++i)
    {
	if (p1 -> p_color[i] < p2 -> p_color[i])
	    return (-1);
	else if (p1 -> p_color[i] > p2 -> p_color[i])
	    return (1);
    }
    return (0);
}

/*
 *			 L O O K U P _ P I X E L ( )
 */
struct pixel *lookup_pixel(bu_rb_tree *palette, char *color)
{
    int			rc;	/* Return code from bu_rb_insert() */
    struct pixel	*qpp;	/* The query */
    struct pixel	*pp;	/* Value to return */

#if 0
    bu_log("lookup_pixel( ");
    for (i = 0; i < pixel_size; ++i)
	bu_log("%3d ", color[i]);
    bu_log(")...");
#endif

    /*
     *	Prepare the palette query
     */
    qpp = mk_pixel(color);

    /*
     *	Perform the query by attempting an insertion...
     *	If the query succeeds (i.e., the insertion fails!),
     *	then we have our pixel.
     *	Otherwise, we must create a new pixel.
     */
    switch (rc = bu_rb_insert(palette, (void *) qpp))
    {
	case -1:
#if 0
	    bu_log(" already existed\n");
#endif
	    pp = (struct pixel *) bu_rb_curr1(palette);
	    free_pixel(qpp);
	    break;
	case 0:
#if 0
	    bu_log(" newly added\n");
#endif
	    pp = qpp;
	    break;
	default:
	    bu_log("bu_rb_insert() returns %d:  This should not happen\n", rc);
	    exit (1);
    }

    return (pp);
}

int
main (int argc, char **argv)
{
    bu_rb_tree		*palette;	/* Pixel palette */
    char		*inf_name;	/* name of input stream */
    char		*outf_name;	/*  "   "  output   "   */
    unsigned char	*buf;		/* the current input pixel */
    FILE		*infp = NULL;	/* input stream */
    int			ch;		/* current char in command line */
    struct pixel	*pp;

    extern int	optind;			/* index from getopt(3C) */
    extern char	*optarg;		/* argument from getopt(3C) */

    /*
     *	Process the command line
     */
    while ((ch = getopt(argc, argv, OPT_STRING)) != EOF)
	switch (ch)
	{
	    case '#':
		if (sscanf(optarg, "%d", &pixel_size) != 1)
		{
		    bu_log("Invalid pixel size: '%s'\n", optarg);
		    print_usage();
		    exit (1);
		}
		break;
	    case '?':
	    default:
		print_usage();
		exit (ch != '?');
	}
    switch (argc - optind)
    {
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
	    exit (1);
    }

    /*
     *	Open input and output files, as necessary
     */
    if (infp == NULL)
    {
	inf_name = argv[optind];
	if ((infp = fopen(inf_name, "r")) == NULL)
	{
	    bu_log ("Cannot open input file '%s'\n", inf_name);
	    exit (1);
	}
	if (outfp == NULL)
	{
	    outf_name = argv[++optind];
	    if ((outfp = fopen(outf_name, "w")) == NULL)
	    {
		bu_log ("Cannot open output file '%s'\n", outf_name);
		exit (1);
	    }
	}
    }

    /*
     *	Ensure that infp is kosher,
     */
    if (infp == stdin)
    {
	if (isatty(fileno(stdin)))
	{
	    bu_log("FATAL: pixcount reads only from file or pipe\n");
	    print_usage();
	    exit (1);
	}
    }

    palette = bu_rb_create1("Pixel palette", compare_pixels);
    bu_rb_uniq_on1(palette);

    /*
     *	Read the input stream into the palette
     */
    buf = (unsigned char *)
		bu_malloc(pixel_size * sizeof(unsigned char),
			"pixel buffer");
    while (fread((void *) buf, pixel_size * sizeof(unsigned char), 1, infp)
	    == 1)
    {
	pp = lookup_pixel(palette, buf);
	BU_CKMAG(pp, PIXEL_MAGIC, "pixel");

	++(pp -> p_count);
    }
    bu_free((genptr_t) buf, "pixel buffer");

    bu_rb_walk1(palette, print_pixel, INORDER);

    return 0;
}
