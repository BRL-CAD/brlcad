/*			P I X - A L I A S
 *
 * 	Convert BRL PIX format image files to ALIAS(tm) PIX fomat files.
 *
 *	Author
 *	Lee A. Butler	butler@stsci.edu
 *
 *	Options
 *	x	set x dimension
 *	y	set y dimension
 *	s	set size of image (square)
 *	h	help
 *
 *
 *	Format of a BRL PIX file:
 *	RGB RGB RGB RGB .... RGB
 *	--------------------------------------------------
 *	Format of an ALIAS(tm) PIX file:
 *	16bit-x-dimension
 *	16bit-y-dimension
 *	16bit-x-offset (usually 0)
 *	16bit-y-offset (usually y-1)
 *	16bit-depth-count (a 16 bit int containing the number of bits per pixel)
 *	run-length encoded pixel data.  Each entry of the form:
 *	1byte run length, B, G, R
 *	
 *	Run length of 0 seems to be meaningless.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"			/* For getopt */

/* declarations to support use of getopt() system call */
char *options = "hs:w:n:";
char optflags[sizeof(options)];
char *progname = "(noname)";

int x=512;
int y=512;

struct aliashead {
	short	x, y;		/* dimensions of this image in X and Y */
	short	xoff, yoff;	/* offsets of pixels */
	short	bitplanes;	/* the number of bits per pixel */
};

/*
 *	D O I T --- Main function of program
 */
void doit(void)
{
	struct aliashead ah;
	char *image;
	unsigned bufsize, idx, cpix, cnt;
	int n;

	bufsize=(x*y*3);

	/* allocate a buffer for the image */
	if ((image = malloc(bufsize)) == (char *)NULL) {
		(void) fprintf(stderr, "Error: Insufficient memory for image buffer\n");
		exit(-2);
	}
	/* read in the image (reverse the order of the scanlines) */
	for (n=y-1 ; n >= 0 ; --n)
		if (fread(&image[n*x*3], x*3, 1, stdin) != 1) {
			(void) fprintf(stderr, "Error reading image at scanline %u\n", n);
			exit(-2);
		}

	/* create & write the alias pix file header */
	ah.x = x;
	ah.y = y;
	ah.xoff = 0;
	ah.yoff = y - 1;
	ah.bitplanes = 24;

	/* the weird output style is to circumvent differences in
	 * machine architectures
	 */
	
	(void) putchar( (x & 0x0ff00) >> 8);
	(void) putchar( (x & 0x0ff));
	(void) putchar( (y & 0x0ff00) >> 8);
	(void) putchar( (y & 0x0ff));
	(void) putchar(0);
	(void) putchar(0);
	(void) putchar( (ah.yoff & 0x0ff00) >> 8);
	(void) putchar( (ah.yoff & 0x0ff));
	(void) putchar(0);
	(void) putchar(24);

	for (idx=0 ; idx < bufsize ; ) {
		cpix = idx; cnt=0;
		while (cnt < 0x0ff && idx < bufsize-2 &&
			image[idx] == image[cpix] &&
			image[idx+1] == image[cpix+1] &&
			image[idx+2] == image[cpix+2] ) {

			idx += 3; ++cnt;
		}
		/* Alias files are count, B, G, R */
		(void) putchar((char) cnt);
		(void) putchar(image[cpix+2]);
		(void) putchar(image[cpix+1]);
		(void) putchar(image[cpix]);
	}
}

void usage(void)
{
	(void)fprintf(stderr,"Usage: %s [ -s squaresize ] [-w file_width ] [-n file_height ]\n", progname);
	(void)fprintf(stderr,"\t< BRLpixfile > ALIASpixfile\n");
	exit(1);
}


int
main(int ac, char **av)
{
	int  c, optlen;

	progname = *av;
	if (isatty(fileno(stdin))) usage();
	
	/* Get # of options & turn all the option flags off
	 */
	optlen = strlen(options);

	for (c=0 ; c < optlen ; optflags[c++] = '\0');
	
	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line
	 */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case 'w' : x = atoi(optarg); break;
		case 'n' : y = atoi(optarg); break;
		case 's' : x = atoi(optarg); y = atoi(optarg); break;
		default	: usage(); break;
		}

	if (optind >= ac) doit();
	else usage();

	return 0;
}
