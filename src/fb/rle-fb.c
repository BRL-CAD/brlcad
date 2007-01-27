/*                        R L E - F B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file rle-fb.c
 *
 *  Decode a Utah Raster Toolkit RLE image, and display on a
 *  BRL libfb(3) framebuffer.
 *
 *  Authors -
 *	Michael John Muuss
 *	Paul R. Stay
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Id$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "fb.h"
#include "rle.h"

static FILE	*infp;
static char	*infile;

static int	background[3];
static int	override_background;

unsigned char	*rows[4];		/* Character pointers for rle_getrow */

static unsigned char	*scan_buf;		/* single scanline buffer */
static ColorMap	cmap;

static char	*framebuffer = (char *)0;
static int	screen_width = 0;
static int	screen_height = 0;
static int	scr_xoff = 0;
static int	scr_yoff = 0;

static int	crunch;
static int	overlay;
static int	r_debug;

static char	usage[] = "\
Usage: rle-fb [-c -d -h -O] [-F framebuffer]  [-C r/g/b]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height]\n\
	[-X scr_xoff] [-Y scr_yoff] [file.rle]\n\
";

/*
 *			G E T _ A R G S
 */
static int
get_args(int argc, register char **argv)
{
	register int	c;

	while( (c = bu_getopt( argc, argv, "cOdhs:S:w:W:n:N:C:F:X:Y:" )) != EOF )  {
		switch( c )  {
		case 'O':
			overlay = 1;
			break;
		case 'd':
			r_debug = 1;
			break;
		case 'F':
			framebuffer = bu_optarg;
			break;
		case 'c':
			crunch = 1;
			break;
		case 'h':
			/* high-res */
			screen_height = screen_width = 1024;
			break;
		case 'S':
		case 's':
			/* square screen size */
			screen_height = screen_width = atoi(bu_optarg);
			break;
		case 'W':
		case 'w':
			screen_width = atoi(bu_optarg);
			break;
		case 'N':
		case 'n':
			screen_height = atoi(bu_optarg);
			break;
		case 'X':
			scr_xoff = atoi(bu_optarg);
			break;
		case 'Y':
			scr_yoff = atoi(bu_optarg);
			break;
		case 'C':
			{
				register char *cp = bu_optarg;
				register int *conp = background;

				/* premature null => atoi gives zeros */
				for( c=0; c < 3; c++ )  {
					*conp++ = atoi(cp);
					while( *cp && *cp++ != '/' ) ;
				}
				override_background = 1;
			}
			break;
		default:
		case '?':
			return	0;
		}
	}
	if( argv[bu_optind] != NULL )  {
		if( (infp = fopen( (infile=argv[bu_optind]), "r" )) == NULL )  {
			perror(infile);
			return	0;
		}
		bu_optind++;
	} else {
		infile = "-";
	}
	if( argc > ++bu_optind )
		(void) fprintf( stderr, "rle-fb:  excess arguments ignored\n" );

	if( isatty(fileno(infp)) )
		return 0;
	return	1;
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	FBIO	*fbp;
	register int i;
	int	file_width;		/* unclipped width of rectangle */
	int	file_skiplen;		/* # of pixels to skip on l.h.s. */
	int	screen_xbase;		/* screen X of l.h.s. of rectangle */
	int	screen_xlen;		/* clipped len of rectangle */
	int	ncolors;

	infp = stdin;
	if( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	rle_dflt_hdr.rle_file = infp;
	if( rle_get_setup( &rle_dflt_hdr ) < 0 )  {
		fprintf(stderr, "rle-fb: Error reading setup information\n");
		exit(1);
	}

	if (r_debug)  {
		fprintf( stderr,"Image bounds\n\tmin %d %d\n\tmax %d %d\n",
			rle_dflt_hdr.xmin, rle_dflt_hdr.ymin,
			rle_dflt_hdr.xmax, rle_dflt_hdr.ymax );
		fprintf(stderr, "%d color channels\n", rle_dflt_hdr.ncolors);
		fprintf(stderr,"%d color map channels\n", rle_dflt_hdr.ncmap);
		if ( rle_dflt_hdr.alpha )
			fprintf( stderr, "Alpha Channel present in input, ignored.\n");
		for( i=0; i < rle_dflt_hdr.ncolors; i++ )
			fprintf(stderr,"Background channel %d = %d\n",
				i, rle_dflt_hdr.bg_color[i] );
		rle_debug(1);
	}

	if( rle_dflt_hdr.ncmap == 0 )
		crunch = 0;

	/* Only interested in R, G, & B */
	RLE_CLR_BIT(rle_dflt_hdr, RLE_ALPHA);
	for (i = 3; i < rle_dflt_hdr.ncolors; i++)
		RLE_CLR_BIT(rle_dflt_hdr, i);
	ncolors = rle_dflt_hdr.ncolors > 3 ? 3 : rle_dflt_hdr.ncolors;

	/* Optional switch of library to overlay mode */
	if( overlay )  {
		rle_dflt_hdr.background = 1;		/* overlay */
		override_background = 0;
	}

	/* Optional background color override */
	if( override_background )  {
		for( i=0; i<ncolors; i++ )
			rle_dflt_hdr.bg_color[i] = background[i];
	}

	file_width = rle_dflt_hdr.xmax - rle_dflt_hdr.xmin + 1;

	/* If screen sizes not specified, try to display rectangle part > 0 */
	if( screen_width == 0 )  {
		screen_width = rle_dflt_hdr.xmax + 1;
		if( scr_xoff > 0 )
			screen_width += scr_xoff;
	}
	if( screen_height == 0 )  {
		screen_height = rle_dflt_hdr.ymax + 1;
		if( scr_yoff > 0 )
			screen_height += scr_yoff;
	}

	/* Incorporate command-line rectangle repositioning */
	rle_dflt_hdr.xmin += scr_xoff;
	rle_dflt_hdr.xmax += scr_xoff;
	rle_dflt_hdr.ymin += scr_yoff;

	/* Pretend saved image origin is at 0, clip & position in fb_write call */
	screen_xbase = rle_dflt_hdr.xmin;
	rle_dflt_hdr.xmax -= screen_xbase;
	rle_dflt_hdr.xmin = 0;

	if( (fbp = fb_open( framebuffer, screen_width, screen_height )) == FBIO_NULL )
		exit(12);

	/* Honor original screen size desires, if set, unless they shrank */
	if( screen_width > 0 && fb_getwidth(fbp) < screen_width )
		screen_width = fb_getwidth(fbp);
	if( screen_height > 0 && fb_getheight(fbp) < screen_height )
		screen_height = fb_getheight(fbp);

	/* Discard any scanlines which exceed screen height */
	if( rle_dflt_hdr.ymax > screen_height-1 )
		rle_dflt_hdr.ymax = screen_height-1;

	/* Clip left edge */
	screen_xlen = rle_dflt_hdr.xmax + 1;
	file_skiplen = 0;
	if( screen_xbase < 0 )  {
		file_skiplen = -screen_xbase;
		screen_xbase = 0;
		screen_xlen -= file_skiplen;
	}
	/* Clip right edge */
	if( screen_xbase + screen_xlen > screen_width )
		screen_xlen = screen_width - screen_xbase;
	if( screen_xlen <= 0 ||
	    rle_dflt_hdr.ymin > screen_height ||
	    rle_dflt_hdr.ymax < 0 )  {
		fprintf(stderr,
		"rle-fb:  Warning:  RLE image rectangle entirely off screen\n");
		goto done;
	}

	scan_buf = (unsigned char *)malloc( sizeof(RGBpixel) * screen_width );

	for( i=0; i < ncolors; i++ )
		rows[i] = (unsigned char *)malloc((size_t)file_width);
	for( ; i < 3; i++ )
		rows[i] = rows[0];	/* handle monochrome images */

	/*
	 *  Import Utah color map, converting to libfb format.
	 *  Check for old format color maps, where high 8 bits
	 *  were zero, and correct them.
	 *  XXX need to handle < 3 channels of color map, by replication.
	 */
	if( rle_dflt_hdr.ncmap > 0 )  {
		register int maplen = (1 << rle_dflt_hdr.cmaplen);
		register int all = 0;
		for( i=0; i<256; i++ )  {
			cmap.cm_red[i] = rle_dflt_hdr.cmap[i];
			cmap.cm_green[i] = rle_dflt_hdr.cmap[i+maplen];
			cmap.cm_blue[i] = rle_dflt_hdr.cmap[i+2*maplen];
			all |= cmap.cm_red[i] | cmap.cm_green[i] |
				cmap.cm_blue[i];
		}
		if( (all & 0xFF00) == 0 && (all & 0x00FF) != 0 )  {
			/*  This is an old (Edition 2) color map.
			 *  Correct by shifting it left 8 bits.
			 */
			for( i=0; i<256; i++ )  {
				cmap.cm_red[i] <<= 8;
				cmap.cm_green[i] <<= 8;
				cmap.cm_blue[i] <<= 8;
			}
			fprintf(stderr,
				"rle-fb: correcting for old style colormap\n");
		}
	}
	if( rle_dflt_hdr.ncmap > 0 && !crunch )
		(void)fb_wmap( fbp, &cmap );
	else
		(void)fb_wmap( fbp, COLORMAP_NULL );

	/* Handle any lines below zero in y.  Decode and discard. */
	for( i = rle_dflt_hdr.ymin; i < 0; i++ )
		rle_getrow( &rle_dflt_hdr, rows );

	for( ; i <= rle_dflt_hdr.ymax; i++)  {
		register unsigned char	*pp = (unsigned char *)scan_buf;
		register rle_pixel	*rp = &(rows[0][file_skiplen]);
		register rle_pixel	*gp = &(rows[1][file_skiplen]);
		register rle_pixel	*bp = &(rows[2][file_skiplen]);
		register int		j;

		if( overlay )  {
			fb_read( fbp, screen_xbase, i, scan_buf, screen_xlen );
			for( j = 0; j < screen_xlen; j++ )  {
				*rp++ = *pp++;
				*gp++ = *pp++;
				*bp++ = *pp++;
			}
			pp = (unsigned char *)scan_buf;
			rp = &(rows[0][file_skiplen]);
			gp = &(rows[1][file_skiplen]);
			bp = &(rows[2][file_skiplen]);
		}

		rle_getrow(&rle_dflt_hdr, rows );

		/* Grumble, convert from Utah layout */
		if( !crunch )  {
			for( j = 0; j < screen_xlen; j++)  {
				*pp++ = *rp++;
				*pp++ = *gp++;
				*pp++ = *bp++;
			}
		} else {
			for( j = 0; j < screen_xlen; j++)  {
				*pp++ = cmap.cm_red[*rp++]>>8;
				*pp++ = cmap.cm_green[*gp++]>>8;
				*pp++ = cmap.cm_blue[*bp++]>>8;
			}
		}
		if( fb_write( fbp, screen_xbase, i, scan_buf, screen_xlen ) != screen_xlen )  break;
	}
done:
	fb_close( fbp );
	exit(0);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
