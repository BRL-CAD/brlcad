/*                       S G I - P I X . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file sgi-pix.c
 *
 *  Authors -
 *	Multiple Sources (Dykstra, Muuss, etc)
 *	Based on scrsave.c
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"

#if HAS_SGIGL
# ifdef mips
#	include <gl/gl.h>
# else
#	include <gl.h>
# endif
#endif

#define	MIN(a,b)	((a)<(b)?(a):(b))
#define	ABS(a)		((a)<0? -(a):(a))

char rbuf[2048];
char gbuf[2048];
char bbuf[2048];
char obuf[2048*3];
struct cmap {
	unsigned char red;
	unsigned char grn;
	unsigned char blu;
} cmap[4096];

#if !HAS_SGIGL
int
main(int argc, char **argv)
{
	fprintf(stderr, "sgi-pix:  This program only works on SGI machines\n");
	exit(1);
}
#else
static char usage[] = "\
Usage: sgi-pix [x1 x2 y1 y2] [outfile]\n";

int
main(argc,argv)
int argc;
char **argv;
{
	int i, y, gotfirst;
	int x1, x2, y1, y2;
	int xsize, ysize;
	int xorg, yorg;
	FILE *ofp;
	char *fname;
	int mode, planes;

	if(!(argc==1 || argc==2 || argc==5 || argc==6)) {
		fprintf(stderr,usage);
		exit(1);
	}
	if( argc > 4 ) {
		x1 = atoi(argv[1]);
		x2 = atoi(argv[2]);
		y1 = atoi(argv[3]);
		y2 = atoi(argv[4]);
	} else {
		x1 = 0;
		x2 = XMAXSCREEN;
		y1 = 0;
		y2 = YMAXSCREEN;
	}
	if( argc == 2 ) {
		fname = argv[1];
		ofp = fopen(fname,"w");
	} else if( argc == 6 ) {
		fname = argv[5];
		ofp = fopen(fname,"w");
	} else {
		fname = "-";
		ofp = stdout;
	}
	if( ofp == NULL ) {
		fprintf(stderr,"sgi-pix: can't open \"%s\"\n", fname);
		exit(2);
	}
	if( isatty(fileno(ofp)) ) {
		fprintf(stderr,"sgi-pix: refuse to send binary output to terminal\n");
		fprintf(stderr,usage);
		exit(1);
	}

	/* Convert rectangle edges to origin and size */
	xorg = MIN(x1,x2);
	yorg = MIN(y1,y2);
	if(xorg<0)
		xorg = 0;
	if(yorg<0)
		yorg = 0;
	xsize = ABS(x2-x1);
	ysize = ABS(y2-y1);
	if((xorg+xsize)>XMAXSCREEN)
		xsize = XMAXSCREEN-xorg;
	if((yorg+ysize)>YMAXSCREEN)
		ysize = YMAXSCREEN-yorg;
	xsize++;
	ysize++;
	fprintf(stderr,"origin(%d, %d) size (%d, %d)\n",
		xorg, yorg, xsize, ysize);

#ifdef mips
	foreground();
	noport();
	winopen("sgi-pix");
	savescreen(ofp, xorg, yorg, xsize, ysize);
#else
	gbegin();
	foreground();
	noport();
	winopen("sgi-pix");
	cursoff();

	if((mode = getdisplaymode()) == 0) {
		/* RGB mode */
		fprintf(stderr,"RGB mode\n");
		savescreen(ofp, xorg, yorg, xsize, ysize);
	} else {
		if( mode == 1 )
			fprintf(stderr,"CMAP mode (single buffered)\n");
		else  {
			fprintf(stderr,"CMAP mode (double buffered)\n");
			swapbuffers();
		}
		planes = getplanes();
		fprintf(stderr,"%d planes\n", planes);
		for( i = 0; i < 4096; i++ ) {
			short r,g,b;
			getmcolor( i, &r, &g, &b );
			cmap[i].red = r;
			cmap[i].grn = g;
			cmap[i].blu = b;
		}
		cmap_savescreen(ofp,x1,x2,y1,y2);
		if( mode != 1 )  {
			/* Double buffered mode, swap 'em back */
			swapbuffers();
		}
	}
#endif
	return(0);
}
#endif

/*
 *  This block of code is for SGI 3030 machines, and 4Ds running Irix 3.
 */
#if HAS_SGIGL && !defined(__mips)
/*
 *			S A V E S C R E E N
 */
savescreen(ofp,xorg,yorg,xsize,ysize)
FILE	*ofp;
int	xorg,yorg,xsize,ysize;
{
	int y, i;
	int pos, togo, n;

	screenspace();

#if !defined(mips)
	/* 3D only */
	viewport(0,1023,0,767);
	ortho2(-0.5,1023.5,-0.5,767.5);
#endif

	for(y=0; y<ysize; y++) {
#ifdef mips
		/* Note that gl_readscreen() can only do 256 pixels! */
		togo = xsize;
		pos = 0;
		while(togo) {
			n = togo;
			if(n>256)
				n = 256;
			cmov2i(xorg+pos,yorg+y);
			gl_readscreen(n,rbuf+pos,gbuf+pos,bbuf+pos);
			pos += n;
			togo -= n;
		}
#else
		cmov2i(xorg,yorg+y);
		readRGB(n,rbuf,gbuf,bbuf);
#endif
		for( i = 0; i < xsize; i++ ) {
			obuf[3*i] = rbuf[i];
			obuf[3*i+1] = gbuf[i];
			obuf[3*i+2] = bbuf[i];
		}
		if( fwrite(obuf,3,xsize,ofp) != xsize )  {
			perror("fwrite");
			exit(2);
		}
	}
}

cmap_savescreen(ofp,xorg,yorg,xsize,ysize)
FILE	*ofp;
int	xorg,yorg,xsize,ysize;
{
	int y, i;
	int pos, togo, n;
	Colorindex buff[1024];

	screenspace();

	for(y=0; y<ysize; y++) {
		cmov2i(xorg,yorg+y);
		readpixels(xsize,buff);

		for( i = 0; i < xsize; i++ ) {
			obuf[i*3] = cmap[buff[i]].red;
			obuf[i*3+1] = cmap[buff[i]].grn;
			obuf[i*3+2] = cmap[buff[i]].blu;
		}
		if( fwrite(obuf,3,xsize,ofp) != xsize )  {
			perror("fwrite");
			exit(2);
		}
	}
}
#endif

/*
 *  This block of code is for SGI 4Ds running Irix.
 */
#if HAS_SGIGL && defined(__mips)
/*
 *			S A V E S C R E E N
 *
 *  In order for this to work reasonably, the entire screen should be read
 *  at once.
 */
savescreen(ofp,xorg,yorg,xsize,ysize)
FILE	*ofp;
int	xorg,yorg,xsize,ysize;
{
	unsigned long	*lbuf;
	unsigned long	*lp;
	int	npix;
	int	x;
	int	y;
	register unsigned char	*op;
	long	got;

	npix = (xsize+1)*(ysize+1);		/* conservative */
	lbuf = (unsigned long *)malloc( sizeof(long) * npix );
	if( lbuf == (unsigned long *)NULL )  {
		fprintf(stderr, "sgi-pix:  malloc error\n");
		exit(1);
	}

	npix = (xsize)*(ysize);			/* exact */
	got = readdisplay( xorg, yorg, xorg+xsize-1, yorg+ysize-1,
		lbuf, RD_FREEZE );
	if( got != npix )  {
		fprintf(stderr,"sgi-pix: readdisplay() wanted %d, got %d\n",
			npix, got );
	}
	lp = lbuf;
	for( y=0; y<ysize; y++ )  {
		register unsigned long	w;
		op = (unsigned char *)obuf;
		for( x=0; x<xsize; x++ )  {
			w = *lp++;
			*op++ = (w    ) & 0xFF;
			*op++ = (w>> 8) & 0xFF;
			*op++ = (w>>16) & 0xFF;
		}
		if( fwrite(obuf,3,xsize,ofp) != xsize )  {
			perror("fwrite");
			exit(2);
		}
	}
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
