/*
 *			F B B A C K G N D . C
 *
 * Function -
 *	Backgound Maker
 *
 *  Given Hue and Saturation for background, make top light and bottom dark.
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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>	
#include "fb.h"

#ifdef SYSV
#define bzero(p,cnt)	memset(p,'\0',cnt);
#endif

int curchan = 0;	/* 0=r, 1=g, 2=b */

double atof();
double col[6] = {128,128,128};		/* r,g,b h,s,v */

int npix = 512;
RGBpixel buf[1024];
RGBpixel pix;

double deltav;
int title_height = 80;
int h_start = 240;
int h_end = 50;

FBIO *fbp;

#define FILL(r,g,b)	{ register RGBpixel *pp; \
	pix[RED]=r; pix[GRN]=g; pix[BLU]=b; \
	for( pp = (RGBpixel *)&buf[npix-1][RED]; pp >= buf; pp-- ) \
		COPYRGB(*pp, pix); }

main(argc, argv )
char **argv;
{
	register int i;
	register int line;

	if( argc < 3 )  {
		fprintf(stderr,"Usage:  fbbackgnd [-h] hue saturation\n");
		return(1);
	}

	if( argv[1][0] == '-' && argv[1][1] == 'h' )  {
		argc--;
		argv++;
		npix = 1024;
		title_height = 90;
	}

	if( (fbp = fb_open( NULL, npix, npix )) == FBIO_NULL )  {
		fprintf(stderr, "fbbackgnd:  fb_open failed\n");
		return	1;
	}

	/* Write out top area with initial HSV */
	col[3] = atof(argv[1]);
	col[4] = atof(argv[2]);
	col[5] = h_start;
	hsvrgb( &col[3], col );
	FILL( col[0], col[1], col[2] );
	for( line=0; line<title_height; line++ )
		fb_write( fbp, 0, npix-1-line, buf, npix );

	/* A white stripe, 4 lines wide */
	FILL( 250, 250, 250 );
	for( i=0; i<4; i++,line++ )
		fb_write( fbp, 0, npix-1-line, buf, npix );

	/* Do rest with V dropping from start to end values */
	col[5] = h_start;
	deltav = (h_start-h_end) / (double)(npix-line);
printf("%d..%d: deltav=%f\n", h_start, h_end, deltav);
	for( ; line<npix; line++ )  {
		col[5] -= deltav;
		hsvrgb( &col[3], col );
		FILL( col[0], col[1], col[2] );
		fb_write( fbp, 0, npix-1-line, buf, npix );
	}
}

/* rgbhsv
 * 
 * convert red green blue to hue saturation value
 */
rgbhsv(rgb, hsv)
register double *rgb;
register double *hsv;
{
        double s, v, r, g, b, x;
	static double h;
        double dif;

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

double modf();

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
            case 5:
                r = hsv[2];
                g = m;
                b = n;
                break;
      	default:
            	printf("hsv2rgb:  gak!\n");
            	break;
            }
        }
        else
            r = g = b = hsv[2];

        rgb[0] = r;
        rgb[1] = g;
        rgb[2] = b;
}
