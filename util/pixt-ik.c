/*
 *  			P I X T - I K . C
 *  
 *  Dumb little program to take bottom-up pixel files and
 *  send them to the Ikonas.
 *  This version expects the .pix files to be comming from a raw
 *  magtape with a block size of 24k, regardless of resolution.
 *  
 *  Mike Muuss, BRL.
 *
 *  $Revision$
 */
#include <stdio.h>

extern int ikfd;
extern int ikhires;

#define BLOCKSIZE	(24*1024)	/* Size of tape record */

#define MAX_LINE	1024		/* Max pixels/line */
static char scanline[BLOCKSIZE];	/* multi-scanline pixel buffer */
static int scanbytes;			/* # of bytes of one scanline */

static char outline[MAX_LINE*4];	/* Ikonas pixels */
int reverse = 0;		/* rotate picture 180 degrees if non-zero */

char usage[] = "Usage: pixt-ik [-h] [-r] file.pix [width]\n";

int
main(int argc, char **argv)
{
	static int y;
	static int infd;
	static int nlines;		/* Square:  nlines, npixels/line */
	static int lines_per_block;
	static int j;

	if( argc < 2 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	nlines = 512;
	while( argv[1][0] == '-' )  {
		if( strcmp( argv[1], "-h" ) == 0 )  {
			nlines = 1024;
			argc--; argv++;
			ikhires = 1;
			continue;
		}
		if( strcmp( argv[1], "-r" ) == 0 )  {
			reverse = 1;
			argc--; argv++;
			continue;
		}
	}
	if( (infd = open( argv[1], 0 )) < 0 )  {
		perror( argv[1] );
		exit(3);
	}
	if( argc >= 3 )
		nlines = atoi(argv[2] );
	if( nlines > 512 )
		ikhires = 1;

	scanbytes = nlines * 3;
	lines_per_block = BLOCKSIZE / scanbytes;

	ikopen();
	ikclear();

	if( !reverse )  {
		/* Normal mode */
		for( y = nlines-1; y >= 0; )  {
			register char *in;
			if( read( infd, (char *)scanline, BLOCKSIZE ) != BLOCKSIZE )
				exit(0);

			in = scanline;
			for( j=0; j<lines_per_block; j++ )  {
				register char *out;
				register int i;

				out = outline;
				for( i=0; i<nlines; i++ )  {
					*out++ = *in++;
					*out++ = *in++;
					*out++ = *in++;
					*out++ = 0;
				}
				clustwrite( outline, y--, nlines );
			}
		}
	} else {
		/* Rotate 180 degrees, for Dunn camera */
		for( y=0; y < nlines; )  {
			register char *in;

			if( read( infd, (char *)scanline, BLOCKSIZE ) != BLOCKSIZE )
				exit(0);

			in = scanline;
			for( j=0; j<lines_per_block; j++ )  {
				register char *out;

				out = outline+(4*nlines)-1;
				while( out > outline )  {
					*out-- = 0;
					*out-- = in[2];
					*out-- = in[1];
					*out-- = *in;
					in += 3;
				}
				clustwrite( outline, y++, nlines );
			}
		}
	}
	if( read( infd, (char *)scanline, BLOCKSIZE ) > 0 )
		printf("EOF missing?\n");
	exit(0);
}
