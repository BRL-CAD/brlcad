/*                     P I X D S P L I T . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2010 United States Government as represented by
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
/** @file pixdsplit.c
 *
 * Disentangle the chars from the doubles in a pixd(5) stream
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <errno.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"


#define OPT_STRING	"c:d:#:?"
#define	made_it()	bu_log("%s:%d\n", __FILE__, __LINE__);

void print_usage (void)
{
    bu_exit(1, "Usage: 'pixdsplit %s'\n",
	    "[-c file.pix] [-d file.d] [-# n.m] [file.pixd]");
}


int
main (int argc, char *argv[])
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
    int			cfd = -1;		/*   "       "     */
    int			dfd = -1;		/*   "       "     */

    extern int	bu_optind;			/* index from bu_getopt(3C) */
    extern char	*bu_optarg;		/* argument from bu_getopt(3C) */

    c_per_p = 3; cf_name = "-";
    d_per_p = 1; df_name = "";
    while ((ch = bu_getopt(argc, argv, OPT_STRING)) != EOF)
	switch (ch)
	{
	    case 'd':
		df_name = (char *) bu_malloc(strlen(bu_optarg)+1, "df_name");
		bu_strlcpy(df_name, bu_optarg, strlen(bu_optarg)+1);
		break;
	    case 'c':
		cf_name = (char *) bu_malloc(strlen(bu_optarg)+1, "cf_name");
		bu_strlcpy(cf_name, bu_optarg, strlen(bu_optarg)+1);
		break;
	    case '#':
		if ((sscanf(bu_optarg, "%d.%d", &c_per_p, &d_per_p) != 2)
		    && (sscanf(bu_optarg, ".%d", &d_per_p) != 1)
		    && (sscanf(bu_optarg, "%d", &c_per_p) != 1))
		{
		    bu_log("Invalid pixel-size specification: '%s'\n",
			   bu_optarg);
		    print_usage();
		}
		break;
	    case '?':
	    default:
		print_usage();
	}

    if (c_per_p <= 0)
    {
	bu_log("Illegal number of color bytes per pixel: %d\n", c_per_p);
	return 1;
    }
    if (d_per_p <= 0)
    {
	bu_log("Illegal number of doubles per pixel: %d\n", d_per_p);
	return 1;
    }

    /*
     *	Establish the input stream
     */
    switch (argc - bu_optind)
    {
	case 0:
	    inf_name = "stdin";
	    infd = 0;
	    break;
	case 1:
	    inf_name = argv[bu_optind++];
	    if ((infd = open(inf_name, O_RDONLY)) == -1)
	    {
		bu_log("Cannot open file '%s'\n", inf_name);
		return 1;
	    }
	    break;
	default:
	    print_usage();
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
	bu_log("Cannot open file '%s'\n", cf_name);
	return 1;
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
	bu_log("Cannot open file '%s'\n", df_name);
	return 1;
    }

    cwidth = c_per_p * 1;
    dwidth = d_per_p * 8;
    pwidth = cwidth + dwidth;
    p_per_b = ((1 << 16) / pwidth);	/* A nearly 64-kbyte buffer */
    inb_size = p_per_b * pwidth;
    cb_size = p_per_b * cwidth;
    db_size = p_per_b * dwidth;

    inbuf = (unsigned char *) bu_malloc(inb_size, "char buffer");
    cbuf = (unsigned char *) bu_malloc(cb_size, "char buffer");
    dbuf = (unsigned char *) bu_malloc(db_size, "double buffer");

    while ((num = read(infd, inbuf, inb_size)) > 0)
    {
	inbp = inbuf;
	cbp = cbuf;
	dbp = dbuf;
	for (i = 0; i < num / pwidth; ++i)
	{
	    memcpy(cbp,     inbp,      cwidth);
	    memcpy(dbp, inbp + cwidth, cwidth);
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
	perror("pixdsplit");
	return 1;
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
