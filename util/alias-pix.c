/*			A L I A S - P I X
 *
 *	Convert ALIAS(tm) PIX format image files to BRL PIX fomat files.
 *
 *	Author:
 *	Lee A. Butler
 *
 */
#include "conf.h"

#include <stdio.h>
char *progname = "(noname)";
void usage()
{
	(void) fprintf(stderr, "Usage: %s [ -v ] < ALIASpixfile >BRLpixfile\n", progname);
	exit(1);
}

struct aliashead {
	short	x, y;		/* dimensions of this image in X and Y */
	short	xoff, yoff;	/* offsets of pixels */
	short	bitplanes;	/* the number of bits per pixel */
} hdr;

#define BUFCNT 256
struct pixel {
	unsigned char len, b, g, r;
} buf[BUFCNT];

int verbose = 0;

char *image;

main(ac,av)
int ac;
char *av[];
{
	int pixcnt, i;
	char *p, *malloc();

	progname = *av;

	if (ac > 2 || isatty(fileno(stdin))) usage();

	if (ac > 1) {
		p = av[1];
		if (*p == '-' && *(p+1) == 'v') verbose = 1;
		else usage();
	}


	/* read header in architecture-independent fashion */
	hdr.x = (getchar() & 0x0ff) << 8;
	hdr.x += (getchar() & 0x0ff);

	hdr.y = (getchar() & 0x0ff) << 8;
	hdr.y += (getchar() & 0x0ff);

	hdr.xoff = (getchar() & 0x0ff) << 8;
	hdr.xoff += (getchar() & 0x0ff);

	hdr.yoff = (getchar() & 0x0ff) << 8;
	hdr.yoff += (getchar() & 0x0ff);

	hdr.bitplanes = (getchar() & 0x0ff) << 8;
	hdr.bitplanes += (getchar() & 0x0ff);
	
	if (hdr.bitplanes != 24) {
		(void) fprintf(stderr, "Weird image file:\n");
		(void) fprintf(stderr,
			"X: %d Y: %d xoff: %d yoff: %d bits/pixel: %d\n", 
			hdr.x, hdr.y, hdr.xoff, hdr.yoff, hdr.bitplanes);
		exit(-1);
	}

	if (verbose) {
		(void) fprintf(stderr,
			"X: %d Y: %d xoff: %d yoff: %d bits/pixel: %d\n", 
			hdr.x, hdr.y, hdr.xoff, hdr.yoff, hdr.bitplanes);
	}


	if ( (image=malloc(hdr.x*hdr.y*3)) == (char *)NULL) {
		(void) fprintf(stderr, "%s: insufficient memory for %d * %d image\n",
			progname, hdr.x, hdr.y);
		exit(-1);
	}

	/* read and "unpack" the image */
	p = image;
	while ((pixcnt=fread(buf, sizeof(struct pixel), BUFCNT, stdin)) > 0) {
		for (i=0 ; i < pixcnt ; ++i)
			do {
				*p++ = buf[i].r;
				*p++ = buf[i].g;
				*p++ = buf[i].b;
			} while (--buf[i].len > 0);
	}

	/* write out the image scanlines, correcting for different origin */
	
	for(i=hdr.y-1 ; i >= 0 ; --i)
		if (fwrite(&image[i*hdr.x*3], hdr.x*3, 1, stdout) != 1) {
			(void) fprintf(stderr, "%s: Error writing image\n", *av);
			exit(-1);
		}


	free(image);
	return(0);
}
