/*
 *  			P I X - I K . C
 *  
 *  Dumb little program to take bottom-up pixel files and
 *  send them to the Ikonas.
 *  
 *  Easier than hacking around with RT.
 *  
 *  Mike Muuss, BRL, 05/05/84.
 *
 *  $Revision$
 */
extern int ikfd;
extern int ikhires;

#define MAX_LINE	1024	/* Max pixels/line */
static long scanline[MAX_LINE];	/* 1 scanline pixel buffer */
static int scanbytes;		/* # of bytes of scanline to be written */

char usage[] = "Usage: pix-ik [-h] file.pix\n";

main(argc, argv)
int argc;
char **argv;
{
	register int y;
	register int infd;
	static int nlines;

	if( argc < 2 || argc > 3 )  {
		printf("%s", usage);
		exit(1);
	}

	nlines = 512;
	if( argc == 3 )  {
		if( strcmp( argv[1], "-h" ) != 0 )  {
			printf("%s", usage);
			exit(2);
		}
		ikhires = 1;
		nlines = 1024;
		argc--; argv++;
	}
	if( (infd = open( argv[1], 0 )) < 0 )  {
		perror( argv[1] );
		exit(3);
	}

	ikopen();
	load_map(1);		/* Standard map: linear */
	ikclear();

	scanbytes = nlines * sizeof(long);

	y = nlines-1;
	while( read( infd, (char *)scanline, scanbytes ) == scanbytes )  {
		clustwrite( scanline, y, nlines );
		/* Done twice to avoid hardware problems right now... */
		clustwrite( scanline, y, nlines );
		if( --y < 0 )
			break;
	}
}
