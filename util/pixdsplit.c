/*
 *			P I X D S P L I T . C
 *
 *	Disentangle the chars from the doubles in a pixd(5) stream
 *
 *  Author -
 *	Paul J. Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

#define	made_it()	bu_log("%s:%d\n", __FILE__, __LINE__);

void print_usage (void)
{
#define OPT_STRING	"c:d:#:?"

    bu_log("Usage: 'pixdsplit %s'\n",
	"[-c file.pix] [-d file.d] [-# n.m] [file.pixd]");
}

main (argc, argv)

int	argc;
char	*argv[];

{
    unsigned char	*inbuf;		/* Buffer */
    unsigned char	*cbuf;		/*    "   */
    unsigned char	*dbuf;		/*    "   */
    unsigned char	*inbp;		/* Buffer pointer */
    unsigned char	*cbp;		/*    "      "    */
    unsigned char	*dbp;		/*    "      "    */
    char		*inf_name;	/* File name */
    char		*cf_name;	/*   "    "  */
    char		*df_name;	/*   "    "  */
    int			p_per_b;	/* pixels/buffer */
    int			inb_size;	/* Buffer Size (in bytes) */
    int			cb_size;	/*   "      "    "   "    */
    int			db_size;	/*   "      "    "   "    */
    int			ch;
    int			i;
    int			c_per_p;	/* chars/pixel */
    int			cwidth;		/* chars/pixel (in bytes) */
    int			d_per_p;	/* doubles/pixel (in doubles) */
    int			dwidth;		/* doubles/pixel (in bytes) */
    int			pwidth;		/* bytes/pixel, total */
    int			num;
    int			infd;		/* File descriptor */
    int			cfd;		/*   "       "     */
    int			dfd;		/*   "       "     */

    extern int	optind;			/* index from getopt(3C) */
    extern char	*optarg;		/* argument from getopt(3C) */

    c_per_p = 3; cf_name = "-";
    d_per_p = 1; df_name = "";
    while ((ch = getopt(argc, argv, OPT_STRING)) != EOF)
	switch (ch)
	{
	    case 'd':
		df_name = (char *) bu_malloc(strlen(optarg) + 1, "df_name");
		(void) strcpy(df_name, optarg);
		break;
	    case 'c':
		cf_name = (char *) bu_malloc(strlen(optarg) + 1, "cf_name");
		(void) strcpy(cf_name, optarg);
		break;
	    case '#':
		if ((sscanf(optarg, "%d.%d", &c_per_p, &d_per_p) != 2)
		 && (sscanf(optarg, ".%d", &d_per_p) != 1)
		 && (sscanf(optarg, "%d", &c_per_p) != 1))
		{
		    bu_log("Invalid pixel-size specification: '%s'\n",
			optarg);
		    print_usage();
		    exit (1);
		}
		break;
	    case '?':
	    default:
		print_usage();
		exit (ch != '?');
	}

	if (c_per_p <= 0)
	{
	    bu_log("Illegal number of color bytes per pixel: %d\n", c_per_p);
	    exit (1);
	}
	if (d_per_p <= 0)
	{
	    bu_log("Illegal number of doubles per pixel: %d\n", d_per_p);
	    exit (1);
	}

    /*
     *	Establish the input stream
     */
    switch (argc - optind)
    {
	case 0:
	    inf_name = "stdin";
	    infd = 0;
	    break;
	case 1:
	    inf_name = argv[optind++];
	    if ((infd = open(inf_name, O_RDONLY)) == -1)
	    {
		rt_log ("Cannot open file '%s'\n", inf_name);
		exit (1);
	    }
	    break;
	default:
	    print_usage();
	    exit (1);
    }
    /*
     *	Establish the output stream for chars
     */
    if (*cf_name == '\0')
    {
	cf_name = 0;
    }
    else if ((*cf_name == '-') && (*(cf_name + 1) == '\0'))
    {
	cf_name = "stdout";
	cfd = 1;
    }
    else if ((cfd = open(cf_name, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1)
    {
	rt_log ("Cannot open file '%s'\n", cf_name);
	exit (1);
    }
    /*
     *	Establish the output stream for doubles
     */
    if (*df_name == '\0')
    {
	df_name = 0;
    }
    else if ((*df_name == '-') && (*(df_name + 1) == '\0'))
    {
	df_name = "stdout";
	dfd = 1;
    }
    else if ((dfd = open(df_name, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1)
    {
	rt_log ("Cannot open file '%s'\n", df_name);
	exit (1);
    }

    bu_log("OK, Reading from '%s', %d pix->'%s', %d d->'%s'\n",
	inf_name, c_per_p, cf_name, d_per_p, df_name);
    bu_log("We %s RGB, we %s d\n",
	cf_name ? "want" : "don't want",
	  df_name ? "want" : "don't want");

    cwidth = c_per_p * 1;
    dwidth = d_per_p * 8;
    pwidth = cwidth + dwidth;
    p_per_b = ((1 << 16) / pwidth);	/* A nearly 64-kbyte buffer */
    inb_size = p_per_b * pwidth;
    cb_size = p_per_b * cwidth;
    db_size = p_per_b * dwidth;
    bu_log("Hmm, Buffers will contain %d pixels...\n", p_per_b);
    bu_log("  that's %d --> %d %d\n", inb_size, cb_size, db_size);

    inbuf = (char *) bu_malloc(inb_size, "char buffer");
    cbuf = (char *) bu_malloc(cb_size, "char buffer");
    dbuf = (char *) bu_malloc(db_size, "double buffer");

    while ((num = read(infd, inbuf, inb_size)) > 0)
    {
	inbp = inbuf;
	cbp = cbuf;
	dbp = dbuf;
	for (i = 0; i < num / pwidth; ++i)
	{
#	    if BSD
		bcopy(    inbp,      cbp, cwidth);
		bcopy(inbp + cwidth, dbp, dwidth);
#	    else
		memcpy(cbp,     inbp,      cwidth);
		memcpy(dbp, inbp + cwidth, cwidth);
#	    endif
	    inbp += pwidth;
	    cbp += cwidth;
	    dbp += dwidth;
	}
	if (cf_name)
	    write(cfd, cbuf, i * cwidth);
	if (df_name)
	    write(dfd, dbuf, i * dwidth);
	if (num % pwidth != 0)
	    bu_log("pixdsplit: WARNING: incomplete final pixel ignored\n");
    }
    if (num < 0)
    {
	bu_log("pixdsplit: %s\n", strerror(errno));
	exit (1);
    }
}
