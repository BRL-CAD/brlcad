/*
 *  			P I X - I N T E R P . C
 *  
 *  Read a .pix file of a given resolution, and produce one with
 *  twice as many pixels by interpolating between the pixels.
 *
 *  This program is prepared for the .pix files to be comming from
 *  and/or going to a raw
 *  magtape with a block size of 24k, regardless of image resolution.
 *  
 *  Mike Muuss, BRL.
 *
 *  $Revision$
 */
#include <stdio.h>

#define BLOCKSIZE	(24*1024)	/* Size of tape record */

static char tapebuf[BLOCKSIZE];		/* multi-scanline pixel buffer */

int inbytes;				/* # bytes of one input scanline */
int outbytes;				/* # bytes of one output scanline */
int outsize;				/* size of output buffer */
char *outbuf;				/* ptr to output image buffer */
extern char *malloc();

char usage[] = "Usage: pix-interp file.pix [in_width]\n";

main(argc, argv)
int argc;
char **argv;
{
	register int iny, outy;
	static int infd;
	static int nlines;		/* Square:  nlines, npixels/line */
	static int lines_per_block;
	static int j;

	if( argc < 2 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	nlines = 512;
	if( strcmp( argv[1], "-h" ) == 0 )  {
		nlines = 1024;
		argc--; argv++;
	}
	if( (infd = open( argv[1], 0 )) < 0 )  {
		perror( argv[1] );
		exit(3);
	}
	if( argc >= 3 )
		nlines = atoi(argv[2] );

	inbytes = nlines * 3;		/* bytes/ input line */
	outbytes = nlines * 2 * 3;	/* bytes/ output line */
	lines_per_block = BLOCKSIZE / inbytes;
	outsize = nlines * nlines * 4 * 3;
	outbuf = malloc( outsize );

	outy = -2;
	for( iny = 0; iny < nlines;  )  {
		register char *in;
		if( read( infd, (char *)tapebuf, BLOCKSIZE ) != BLOCKSIZE ) {
			perror("pix-interp read");
			break;
		}
		in = tapebuf;

		for( j=0; j<lines_per_block; j++ )  {
			outy += 2;
			/* outy is line we will write on */
			widen_line( in, outy );
			if( outy == 0 )
				widen_line( in, ++outy );
			else
				interp_lines( outy-1, outy, outy-2 );
			in += inbytes;
			iny++;
		}
	}
	{
		register char *cp = outbuf;
		while( outsize > 0 )  {
			if( write( 1, cp, BLOCKSIZE ) != BLOCKSIZE )  {
				perror("pix-interp write");
				exit(1);
			}
			cp += BLOCKSIZE;
			outsize -= BLOCKSIZE;
		}
	}

	if( read( infd, (char *)tapebuf, BLOCKSIZE ) > 0 )
		printf("EOF missing?\n");
	exit(0);
}

widen_line( cp, y )
register unsigned char *cp;
int y;
{
	register unsigned char *op;
	register int i;

	op = (unsigned char *)outbuf + (y * outbytes);
	/* Replicate first pixel */
	*op++ = cp[0];
	*op++ = cp[1];
	*op++ = cp[2];
	/* Prep process by copying first pixel */
	*op++ = *cp++;
	*op++ = *cp++;
	*op++ = *cp++;
	for( i=0; i<inbytes; i+=3)  {
		/* Average previous pixel with current pixel */
		*op++ = (cp[-3+0] + cp[0])>>1;
		*op++ = (cp[-3+1] + cp[1])>>1;
		*op++ = (cp[-3+2] + cp[2])>>1;
		/* Copy pixel */
		*op++ = *cp++;
		*op++ = *cp++;
		*op++ = *cp++;
	}
}
interp_lines( out, i1, i2 )
int out, i1, i2;
{
	register unsigned char *a, *b;	/* inputs */
	register unsigned char *op;
	register int i;

	a = (unsigned char *)outbuf + (i1 * outbytes);
	b = (unsigned char *)outbuf + (i2 * outbytes);
	op = (unsigned char *)outbuf + (out * outbytes);

	for( i=0; i<outbytes; i+=3 )  {
		*op++ = (*a++ + *b++)>>1;
		*op++ = (*a++ + *b++)>>1;
		*op++ = (*a++ + *b++)>>1;
	}
}
