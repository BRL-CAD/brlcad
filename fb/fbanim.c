/*
 *			M O V . C
 *
 * Function -
 *	Dynamicly modify Ikonas Zoom and Window parameters,
 *	to flip betwen sub-images.
 *
 *  Mike Muuss, 8/7/85.
 */
#include <stdio.h>	
#include <sys/ioctl.h>
#include <time.h>		/* for struct timeval */

struct timeval tv;

extern int ikfd;	/* Ikonas FD */
extern int ikhires;

#include <vaxuba/ikio.h>
long invis = INVISIBLEIO;
		
int pix_line;		/* Number of pixels/line */
int zoom;		/* Zoom Factor.			*/
int xPan, yPan;		/* Pan Location.		*/
int xoff, yoff;		/* Ikonas farbling */

char Usage[] = "Usage: mov [-h] [-p#pass] width nframes [fps]\n";

main(argc, argv )
char **argv;
{
	register int w, n;
	register int i;
	register int im_line;
	int fps;			/* frames/sec */
	int passes = 100;		/* limit on number of passes */

	if( argc < 3 )  {
		printf(Usage);
		exit(12);
	}

	pix_line = 512;
	while( argv[1][0] == '-' )  {
		switch( argv[1][1] )  {
		case 'h':
			ikhires = 1;
			pix_line = 1024;
			break;
		case 'p':
			passes = atoi(&argv[1][2]);
			if(passes<1)  passes=1;
			break;
		default:
			printf(Usage);
			exit(12);
		}
		argc--;
		argv++;
	}

	w = atoi(argv[1]);
	if( w < 4 || w > 256 )  {
		printf("w of %d out of range\n");
		exit(12);
	}
	n = atoi(argv[2]);
	if( argc == 4 )
		fps = atoi(argv[3]);
	else
		fps = 8;
	if( fps <= 1 )  {
		tv.tv_sec = fps ? 1 : 4;
		tv.tv_usec = 0;
	} else {
		tv.tv_sec = 0;
		tv.tv_usec = 1000000/fps;
	}

	ikopen();

	zoom = pix_line/w;
	im_line = pix_line/w;	/* number of images across line */
	xPan = yPan = 0;

	ikzoom( zoom-1, zoom-1 );

	while(passes-- > 0)  for( i=0; i<n; i++ )  {
		xPan = (i%im_line)*w;
		yPan = (i/im_line)*w;
		if( fps < 6 )
			printf("%3d: %3d %3d\n", i, xPan, yPan);

		/* Attempt to use "invisible I/O" -- between frames */
		if( ioctl( ikfd, IKSETCONTROL, &invis ) < 0 )
			perror("IKSETCONTROL");

		if( ikhires )
			xPan = xPan;
		else
			xPan = (xPan*4)-2;

		yPan += 4063;
		if( w <= 32 )  {
			ikwindow( xPan, yPan+27);
		} else if( w <= 64 )  {
			ikwindow( xPan, yPan+(ikhires?29:30));
		} else if( w <= 128 )  {
			ikwindow( xPan, yPan+25);
		} else if ( w <= 256 )  {
			ikwindow( xPan, yPan+17);
		} else if ( w <= 512 )  {
			ikwindow( xPan, yPan );
		}
		fflush( stdout );
		(void)select( 0, 0, 0, 0, &tv );
	}
}
