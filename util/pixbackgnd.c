/*
 *			P I X B A C K G N D . C
 *
 * Function -
 *	Backgound Maker
 *
 *  Given Hue and Saturation for background, make top light and bottom dark.
 *  Generates a pix(5) stream on stdout.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>	
#include <math.h>

#include "machine.h"
#include "externs.h"		/* For getopt and atof */

double col[3] = {128,128,128};		/* r,g,b */
double hsv[3];				/* h,s,v */

int	file_width = 512;
int	file_height = 512;
int	invert = 0;

double	deltav;
int	title_height = 80;
int	h_start = 240;
int	h_end = 50;

void	rgbhsv(), hsvrgb();

char usage[] = "\
Usage:  pixbackgnd [-h -i] [-s squaresize] [-w width] [-n height]\n\
	[-t title_height] [-a top_inten] [-b bottom_inten]\n\
	hue saturation\n\
or	r g b\n\
	> file.pix";

int
get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "his:w:n:t:a:b:" )) != EOF )  {
		switch( c )  {
		case 'i':
			invert = 1;
			break;
		case 'h':
			/* high-res */
			file_height = file_width = 1024;
			title_height = 90;
			break;
		case 's':
			/* square file size */
			file_height = file_width = atoi(optarg);
			break;
		case 'w':
			file_width = atoi(optarg);
			break;
		case 'n':
			file_height = atoi(optarg);
			break;
		case 't':
			/* Title area size */
			title_height = atoi( optarg );
			break;
		case 'a':
			h_start = atoi(optarg);
			break;
		case 'b':
			h_end = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}
	/* when optind >= argc, we have run out of args */
	if( optind+1 >= argc )
		return(0);		/* only 0 or 1 args */
	if( optind+2 == argc )  {
		/* Paramaters are H S */
		hsv[0] = atof(argv[optind++]);
		hsv[1] = atof(argv[optind]);
		hsv[2] = h_start;

		hsvrgb( hsv, col );
	} else {
		/* parameters are R G B */
		col[0] = atof(argv[optind++]);
		col[1] = atof(argv[optind++]);
		col[2] = atof(argv[optind++]);

		rgbhsv( col, hsv );
		hsv[2] = h_start;	/* Change given RGB to starting inten */
	}
	return(1);			/* OK */
}

int
main(argc, argv )
char **argv;
{
	register int i;
	register int line;
	register unsigned char *horiz_buf;
	unsigned char *vert_buf;
	register unsigned char *vp;

	if ( !get_args( argc, argv ) || isatty(fileno(stdout)) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	horiz_buf = (unsigned char *)malloc( file_width * 3 );
	vert_buf = (unsigned char *)malloc( file_height * 3 );

	/*
	 *  First stage -- prepare the vert_buf with one pixel
	 *  for each scanline, since each scanline has uniform color.
	 *  For ease of thinking about it, this is done top-to-bottom.
	 */
	line = 0;
	vp = vert_buf;
	if( title_height > 0 )  {
		/* Make top area with initial HSV */
		for( ; line<title_height; line++ )  {
			*vp++ = col[0];
			*vp++ = col[1];
			*vp++ = col[2];
		}

		/* A white stripe, 4 lines high */
		for( i=0; i<4; i++,line++ )  {
			*vp++ = 250;
			*vp++ = 250;
			*vp++ = 250;
		}
	}

	/* Do rest with V dropping from start to end values */
	if( hsv[2] > h_end )  {
		/* Go from bright at the top to dim at the bottom */
		deltav = (hsv[2]-h_end) / (double)(file_height-line);

		for( ; line<file_height; line++ )  {
			hsv[2] -= deltav;
			hsvrgb( hsv, col );
			*vp++ = col[0];
			*vp++ = col[1];
			*vp++ = col[2];
		}
	} else {
		/* Go from dim at the top to bright at the bottom */
		deltav = (h_end-hsv[2]) / (double)(file_height-line);

		for( ; line<file_height; line++ )  {
			hsv[2] += deltav;
			hsvrgb( hsv, col );
			*vp++ = col[0];
			*vp++ = col[1];
			*vp++ = col[2];
		}
	}

	/*
	 *  Second stage -- flood each value across the horiz_buf
	 *  and write the scanline out.  Here we proceed bottom-to-top
	 *  for pix(5) format.
	 */
	if( !invert )  {
		for( line = file_height-1; line >= 0; line-- )  {
			register unsigned char *op;

			vp = &vert_buf[line*3];
			op = &horiz_buf[(file_width*3)-1];
			while( op > horiz_buf )  {
				*op-- = vp[2];
				*op-- = vp[1];
				*op-- = *vp;
			}
			write( 1, horiz_buf, file_width*3 );
		}
	} else {
		/* Inverted:  top-to-bottom.  Good with cat-fb */
		for( line=0; line < file_height; line++ )  {
			register unsigned char *op;

			vp = &vert_buf[line*3];
			op = &horiz_buf[(file_width*3)-1];
			while( op > horiz_buf )  {
				*op-- = vp[2];
				*op-- = vp[1];
				*op-- = *vp;
			}
			write( 1, horiz_buf, file_width*3 );
		}
	}
	exit(0);
}

/* rgbhsv
 * 
 * convert red green blue to hue saturation value
 */
void
rgbhsv(rgb, hsv)
register double *rgb;
register double *hsv;
{
        double s, v, r, g, b, x;
	static double h;
        double dif = 0.0;

        r = rgb[0];
        g = rgb[1];
        b = rgb[2];
        v = ((r > g) ? r : g);
        v = ((v > b) ? v : b);
        x = ((r < g) ? r : g);
        x = ((x < b) ? x : b);
	if (v != x)
        {
            dif = (double) (v - x);
            if (r != v)
                if (g == v)
                    if (b != x)
                        h = (double) (42.5 * (3. - (double)(v-b) / dif));
                    else
                        h = (double) (42.5 * (1. + (double)(v-r) / dif));
                else
                    if (r != x)
                        h = (double) (42.5 * (5. - (double)(v-r) / dif));
                    else
                        h = (double) (42.5 * (3. + (double)(v-g) / dif));
            else
                if (g != x)
                    h = (double) (42.5 * (1. - (double)(v-g) / dif));
                else
                    h = (double) (42.5 * (5. + (double)(v-b) / dif));
	}

	if (v != 0)
            s = (double)(255. * dif / (double)v);
        else
            s = 0;

        hsv[0] = h;
        hsv[1] = s;
        hsv[2] = v;
}

/* hsvrgb
 * 
 * convert hue saturation and value to red, green, blue
 */

void
hsvrgb(hsv, rgb)
register double *hsv;
register double *rgb;
{
        double r, g, b, m, n, k, foo;
        double h, s, v;
        double f;

	if(hsv[1] != 0) 
        {
            s = (double)hsv[1] / 255.;
            h = (double)hsv[0] / 42.666;
            f = modf(h, &foo);
            v = (double)hsv[2];
            m = (double) (v * (1. - s) + .5);
            n = (double) (v * (1. - s*f) + .5);
            k = (double) (v * (1. - (s * (1.-f))) + .5);
            switch((int) h)
            {
            case 0:
                r = hsv[2];
                g = k;
                b = m;
                break;
            case 1:
                r = n;
                g = hsv[2];
                b = m;
                break;
            case 2:
                r = m;
                g = hsv[2];
                b = k;
                break;
            case 3:
                r = m;
                g = n;
                b = hsv[2];
                break;
            case 4:
                r = k;
                g = m;
                b = hsv[2];
                break;
            default:
            case 5:
                r = hsv[2];
                g = m;
                b = n;
                break;
            }
        }
        else
            r = g = b = hsv[2];

        rgb[0] = r;
        rgb[1] = g;
        rgb[2] = b;
}
