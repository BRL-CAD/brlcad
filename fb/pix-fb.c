/*
 *  			P I X - F B . C
 *  
 *  Dumb little program to take bottom-up pixel files and
 *  send them to a framebuffer.
 *  
 *  Mike Muuss, BRL.
 *
 *  $Revision$
 */
#include <stdio.h>

#include "/vld/include/fb.h"

#define MAX_LINE	1024		/* Max pixels/line */
static char scanline[MAX_LINE*3];	/* 1 scanline pixel buffer */
static int scanbytes;			/* # of bytes of scanline */

struct pixel outline[MAX_LINE];

int inverse = 0;			/* Draw upside-down */

char usage[] = "Usage: pix-fb [-h] [-i] file.pix [width]\n";

main(argc, argv)
int argc;
char **argv;
{
	static int y;
	static int infd;
	static int nlines;		/* Square:  nlines, npixels/line */
	static int fbsize;

	if( argc < 2 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	fbsize = 512;
	nlines = 512;
	if( strcmp( argv[1], "-h" ) == 0 )  {
		fbsize = 1024;
		nlines = 1024;
		argc--; argv++;
	}
	if( strcmp( argv[1], "-i" ) == 0 )  {
		inverse++;
		argc--; argv++;
	}
	if( strcmp( "-", argv[1] ) == 0 )  {
		infd = 0;	/* stdin */
	} else {
		if( (infd = open( argv[1], 0 )) < 0 )  {
			perror( argv[1] );
			exit(3);
		}
	}
	if( argc >= 3 )
		nlines = atoi(argv[2] );
	if( nlines > 512 )
		fbsetsize(fbsize);

	scanbytes = nlines * 3;

	if( fbopen( NULL, CREATE ) < 0 )
		exit(12);
	fbclear();
	fbzoom( fbsize==nlines? 0 : fbsize/nlines,
		fbsize==nlines? 0 : fbsize/nlines );
	fbwindow( nlines/2, nlines/2 );		/* center of view */

	if( !inverse )  {
		/* Normal way -- bottom to top */
		for( y = nlines-1; y >= 0; y-- )  {
			register char *in;
			register struct pixel *out;
			register int i;

			if( mread( infd, (char *)scanline, scanbytes ) != scanbytes )
				exit(0);

			in = scanline;
			out = outline;
			for( i=0; i<nlines; i++ )  {
				out->red = *in++;
				out->green = *in++;
				out->blue = *in++;
				(out++)->spare = 0;
			}
			fbwrite( 0, y, outline, nlines );
		}
	}  else  {
		/* Inverse -- top to bottom */
		for( y=0; y < nlines; y++ )  {
			register char *in;
			register struct pixel *out;
			register int i;

			if( mread( infd, (char *)scanline, scanbytes ) != scanbytes )
				exit(0);

			in = scanline;
			out = outline;
			for( i=0; i<nlines; i++ )  {
				out->red = *in++;
				out->green = *in++;
				out->blue = *in++;
				(out++)->spare = 0;
			}
			fbwrite( 0, y, outline, nlines );
		}
	}
	exit(0);
}

/*
 *			M R E A D
 *
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This can be necessary because pipes
 * and network connections don't deliver data with the same
 * grouping as it is written with.
 */
static int
mread(fd, bufp, n)
int fd;
register char	*bufp;
unsigned	n;
{
	register unsigned	count = 0;
	register int		nread;

	do {
		nread = read(fd, bufp, n-count);
		if(nread == -1)
			return(nread);
		if(nread == 0)
			return((int)count);
		count += (unsigned)nread;
		bufp += nread;
	 } while(count < n);

	return((int)count);
}
