/*
 *  Bustup.c -- take concatenated .pix files, and write them one
 *  per file.
 *  
 *  Mike Muuss, BRL.
 *
 *  $Revision$
 */
#include <stdio.h>

static int scanbytes;			/* # of bytes of scanline */

unsigned char *in1;

static int nlines;		/* Number of input lines */
static int pix_line;		/* Number of pixels/line */

char usage[] = 
"Usage: bustup basename.pix width [image_offset] [first_number] <input.pix\n";

int infd;

main(argc, argv)
int argc;
char **argv;
{
	static int x,y;
	int image_offset;
	int first_number;
	int framenumber;
	char *basename;
	char name[128];

	if( argc < 3 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	basename = argv[1];
	nlines = atoi(argv[2] );

	pix_line = nlines;	/* Square pictures */
	scanbytes = nlines * pix_line * 3;
	in1 = (unsigned char  *) malloc( scanbytes );

	if( argc == 4 )  {
		image_offset = atoi(argv[3]);
		lseek(0, image_offset*scanbytes, 0);
	}
	if( argc == 5 )
		framenumber = atoi(argv[4]);
	else
		framenumber = 0;

	for( ; ; framenumber++ )  {
		int fd;

		if( read( 0, in1, scanbytes ) != scanbytes )
			exit(0);
		sprintf(name, "%s.%d", basename, framenumber);
		if( (fd=creat(name,0444))<0 )  {
			perror(name);
			continue;
		}
		if( write( fd, in1, scanbytes ) != scanbytes ) {
			perror("write");
		}
		(void)close(fd);
		printf("wrote %s\n", name);
	}
}
