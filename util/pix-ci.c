/*
 *			P I X - C I
 *
 *  SGI Iris-specific program to
 *  convert .pix file into color image for SGI utilities, especially
 *  'dither' and 'movie' in /usr/people/gifts/mextools/imgtools.
 */
#include <stdio.h>
#include <math.h>
#include <gl.h>
#include <device.h>
#include <image.h>

unsigned char rgb[1024*3];

unsigned short rs[2048];
unsigned short gs[2048];
unsigned short bs[2048];

unsigned short rb[2048];
unsigned short gb[2048];
unsigned short bb[2048];

int
main(argc,argv)
int argc;
char **argv;
{
	IMAGE *image;
	int y, xsize, ysize;
	short val;

	xsize = ysize = 128;
	if( argc<2 ) {
		printf("usage: pix-ci outfile [size]\n");
		exit(1);
	}
	if( argc == 3 )
		xsize = ysize = atoi( argv[2] );

	if( (image=iopen(argv[1],"w",VERBATIM(1), 3, xsize, ysize, 3)) == NULL ) {
		printf("pix-ci: can't open output file %s\n",argv[1]);
		exit(1);
	}
	isetname(image,"from .pix");

	for(y=0; y<ysize; y++) {
		register short x;
		register unsigned char *cp = rgb;
		read( 0, rgb, xsize*3);
		for( x=0; x<xsize; x++ ) {
			rs[x] = *cp++;
			gs[x] = *cp++;
			bs[x] = *cp++;
		}
		putrow(image,rs,y,0);
		putrow(image,gs,y,1);
		putrow(image,bs,y,2);
	}
	iclose(image);
}
