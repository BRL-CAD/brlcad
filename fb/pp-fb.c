/*
 *			P P - F B . C
 *
 *	plot color shaded pictures from GIFT on a frame buffer.
 *
 *	Original Version:  Gary Kuehl,  April 1983
 *	Ported to VAX:  Mike Muuss, January 1984
 *
 *	Conversion to generic frame buffer utility using libfb(3).
 *	Gary S. Moss, BRL. 03/14/85
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

extern void	perror(), exit();

#define ABS(i)	((i<0)?-(i):(i))

#ifndef pdp11
#define FBBUFSIZE (16*1024)	/* Size of frame buffer DMA */
#else
#define FBBUFSIZE (4*1024)	/* Size of frame buffer DMA */
#endif
static RGBpixel	pix_buf[FBBUFSIZE]; /* Pixel buffer.			*/
#define FBWPIXEL(pix) \
	{ COPYRGB( *fb_p, pix ); \
	if( ++fb_p >= end_p ) \
		{ \
		fb_write( fbp, 0, fb_y, pix_buf, FBBUFSIZE ); \
		fb_y -= scans_per_buffer; \
		fb_p = pix_buf; \
		} \
	}
char g();
long numb();

static char linebuf[128];		/* For reading text lines into */

static int last_unpacked;		/* Global magic */
static FILE *input;			/* Input file handle */
FBIO *fbp;

#define NCOLORS	((sizeof(ctab))/(sizeof(struct colors)))
static struct colors {
	char *color;
	RGBpixel	c_pixel;
} ctab[] = {
	"black",	0,0,0,
	"white",	255,255,255,
	"red",		255,0,0,
	"green",	0,255,0,
	"blue",		0,0,255,
	"cyan",		0,255,200,
	"magenta",	255,0,255,
	"yellow",	255,200,0,
	"orange",	255,100,0,
	"lime",		200,255,0,
	"olive",	220,190,0,
	"lt blue",	0,255,255,
	"violet",	200,0,255,
	"rose",		255,0,175,
	"gray",		120,120,120,
	"silver",	237,237,237,
	"brown",	200,130,0,
	"pink",		255,200,200,
	"flesh",	255,200,160,
	"rust",		200,100,0
};
static struct items {
	long	low;
	long	high;
	int	color;
} itemtab[50] = {
	0,	99,	18,
	100,	199,	10,
	200,	299,	4,
	300,	399,	6,
	400,	499,	8,
	500,	599,	16,
	600,	699,	15,
	700,	799,	2,
	800,	899,	13,
	900,	999,	12,
	1000,	99999,	3,
};
static int nitems = 11;		/* Number of items in table */

#define REFLECTANCE .003936

int width = 512;

main(argc,argv)
int argc;
char **argv;
	{
	register int i;
	static int horiz_pos,vert_pos;
	static int newsurface;
	static int inten_high;		/* high bits of intentisy */
	static float rm[3];
	static RGBpixel icl;
	static int ibc;			/* background color index */
	static RGBpixel *backgroundp;	/* pointer to background pixel */
	static int maxh,maxv,mnh,mnv;
	static int vert_max;
	static long li,lj;
	register char c;
	register int	scans_per_buffer; /* Increment for 'fb_y'.	*/
	register int	fb_y;		/* Scanline to write to next.	*/
	register RGBpixel *fb_p;	/* Current position in buffer.	*/
	register RGBpixel *end_p;	/* End of buffer.	*/

	end_p = (RGBpixel *)&(pix_buf[FBBUFSIZE][RED]);

	/* check invocation */
	if( ! pars_Argv( argc, argv ) )
		{
		(void) fprintf( stderr, "Usage: pp-fb filename\n" );
		return	1;
		}

	/* print data on first two lines of plot file */
	(void) fgets(linebuf, sizeof(linebuf), input);
line1:
	if( (linebuf[0] == ' ' || (linebuf[0]>='A' && linebuf[0]<='Z')) )
		(void) fprintf( stderr,
				"\007WARNING:  This appears to be a .PC file.  If this does not work, use pc-fb\n\n");
	for( i=0; linebuf[i]!='\n'; )
		(void) putchar(linebuf[i++]);
	(void) putchar('\n');

	for(i=0;i<20;i++){
		c=getc(input);
		(void) putchar(c);
	}
	(void) putchar('\n');

	(void) fscanf( input, "%d", &maxh );
	(void) fscanf( input, "%d", &maxv );
	if( maxh > 512 || maxv > 512 ) /* Automatic high res. mode.	*/
		width = 1024;

	if( (fbp = fb_open( NULL, width, width )) == NULL )  {
		fprintf(stderr,"fb_open failed\n");
		exit(12);
	}
	(void) fb_wmap( fbp, COLORMAP_NULL );

	scans_per_buffer = FBBUFSIZE/fb_getwidth(fbp);

	(void) printf( "Number of Horz cells %4d, ",maxh);
	(void) printf( "Number of Vert cells %4d\n",maxv);

	(void) printf( "Code Color\n");
	for(i=0;i<NCOLORS;i++){
		(void) printf( "%3d - %-8s", i+1, ctab[i].color);
		if((i%5)==4) (void) putchar('\n');
	}
	(void) printf( "Background color? ");
	(void) scanf("%d",&ibc);
	if( ibc <= 0 )  ibc = 1;
	if( ibc > NCOLORS )  ibc = NCOLORS;

	(void) printf( "Code  Item range     Color\n");
	for(i=0;i<nitems;i++)
		(void) printf( "%4d %6ld %6ld  %s \n",
			i+1,
			itemtab[i].low,
			itemtab[i].high,
			ctab[itemtab[i].color].color );
	while(1)  {
		auto int incode, incolor;

		(void) printf( "Code (end<=0)? ");
		(void) scanf("%d",&incode);
		if(incode<=0)
			break;
		if(incode>nitems)
			incode = ++nitems;
		(void) printf( "Lower limit? ");
		(void) scanf("%ld",&itemtab[incode-1].low);
		(void) printf( "Upper limit? ");
		(void) scanf("%ld",&itemtab[incode-1].high);
		(void) printf( "Color code? ");
		(void) scanf("%d", &incolor);
		itemtab[incode-1].color = incolor-1;
	}

	/* compute screen coordinates of min and max */
	/* cause image to be centered on screen */
	mnh=(512-maxh)/2;
	mnv=(512-maxv)/2;
	vert_max=mnv+maxv;

	/* Random initialization */
	newsurface = 0;
	inten_high = 0;
	horiz_pos = 0;
	fb_p = pix_buf;
	fb_y = 511;

	/* paint background on upper part of screen */
	backgroundp= (RGBpixel *)&(ctab[ibc-1].c_pixel[RED]);
	for(vert_pos=512; vert_pos > vert_max; vert_pos--)
		for(i=0;i<512;i++)
			FBWPIXEL(*backgroundp);

	/* paint background on left side of screen */
	for(horiz_pos=0;horiz_pos<mnh;horiz_pos++)
		FBWPIXEL(*backgroundp); 

	while((c=g())!='/')  {
		last_unpacked=c-32;
noread:		
		if(last_unpacked>31){
			/* compute intensity */
			static float ftemp;
			static float spi=0.;		/* Saved "pi" */
			register float pi;

			pi=REFLECTANCE * ( (last_unpacked&0x1F) + inten_high );
			icl[RED] = pi*rm[0];
			icl[GRN] = pi*rm[1];
			icl[BLU] = pi*rm[2];
			ftemp=ABS(spi-pi);
			spi=pi;
			if(newsurface==0||ftemp>.1)  {
				FBWPIXEL( icl );
			}  else {
				/* fill scan between surfaces of same intensity */
				static RGBpixel pixel;

				if(pi<.15) pi+=.15;
				if(pi>=.15) pi-=.15;
				pixel[RED] = pi*rm[0];
				pixel[GRN] = pi*rm[1];
				pixel[BLU] = pi*rm[2];
				FBWPIXEL( pixel );
			}
			newsurface=0;
			horiz_pos++;
		}

		/* high order intensity */
		else if(last_unpacked>15) inten_high=(last_unpacked-16)<<5;

		/* control character */
		else switch(last_unpacked){

		case 0:
			/* miss target */
			lj=numb();
			for(li=0; li<lj; li++,horiz_pos++)  {
				FBWPIXEL(*backgroundp);
			}
			newsurface=0;
			goto noread;

		case 1:
			/* new surface */
			newsurface=1;
			break;

		case 3:
			/* new item */
			lj=numb();
			/* Locate item in itemtab */
			for(i=0;i<(nitems-1);i++)
				if( lj >= itemtab[i].low && lj <= itemtab[i].high )
					break;
			rm[0]=ctab[itemtab[i].color].c_pixel[RED];
			rm[1]=ctab[itemtab[i].color].c_pixel[GRN];
			rm[2]=ctab[itemtab[i].color].c_pixel[BLU];
			goto noread;

		case 10:
			/* repeat intensity */
			lj=numb();
			for(li=0;li<lj;li++,horiz_pos++) FBWPIXEL( icl );
			if(last_unpacked!=10) goto noread;
			break;

		case 14:
			/* end of line -- fill edges with background */
			while((horiz_pos++)<512)
				FBWPIXEL(*backgroundp); 
			vert_pos--;
			for(horiz_pos=0;horiz_pos<mnh;horiz_pos++)
				FBWPIXEL(*backgroundp);
		}
	}

	/* end of view */
	while((horiz_pos++)<512)
		FBWPIXEL(*backgroundp);
	vert_pos--;
	while((vert_pos--)>0)
		for(i=0;i<512;i++)
			FBWPIXEL(*backgroundp);

	/* Gobble up file until we see an alphabetic in col 1 */
	while(1)  {
		if( fgets(linebuf, sizeof(linebuf), input) == NULL )
			{
			return	0;		/* EOF */
			}
		c = linebuf[0];
		if( (c>='a' && c <='z') || (c>='A' && c<='Z') )
			break;
	}
	(void) printf( "\n\n----------------------------------\n");
	if( fb_close( fbp ) == -1 )
		{
		(void) fprintf( stderr, "Can't close framebuffer!\n" );
		return	1;
		}
	goto line1;
}

/*
 *	get number from packed word
 */
long numb()
{
	register long n;
	register int shift;

	n=0;
	shift=0;
	while( (last_unpacked=g()-32) > 31 )  {
		n += ((long)last_unpacked&31) << shift;
		shift += 5;
	}
	return(n);
}


/* get char from plot file - check for 75 columns and discard rest */
char g()
{
	static int nc=0;
	register char c;

	if( feof(input) )  {
		(void) fprintf( stderr, "pp-fb: unexpected EOF on data file\n");
		exit(1);
	}
	if((c=getc(input))!='\n'){
		nc++;
		if( nc > 75 )  {
			while((c=getc(input))!='\n');
			nc=1;
			return(getc(input));
		}
		return(c);
	}
	/* Char was newline */
	if(nc==75){
		nc=1;
		return(getc(input));
	}
	/* Pad with spaces to end of "card" */
	nc++;
	(void) ungetc( c, input );
	return(' ');
}

/*	p a r s _ A r g v ( )
 */
int
pars_Argv( argc, argv )
register char	**argv;
	{
	register int	c;
	extern int	optind;

	while( (c = getopt( argc, argv, "" )) != EOF )
		{
		switch( c )
			{
			case '?' :
				return	0;
			}
		}
	/* get plot file */
	if( argv[optind] == NULL )
		{
		(void) fprintf( stderr, "Need to specify PP file!\n" );
		return	0;
		}
	if( (input = fopen( argv[optind], "r")) == NULL )
		{
		perror( argv[optind] );
		return	0;
		}
	return	1;
	}
