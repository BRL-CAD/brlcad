/*
 *			M O V . C
 *
 * Function -
 *	Dynamicly modify framebuffer Zoom and Window parameters,
 *	to flip betwen sub-images.
 *
 *  Mike Muuss, 8/7/85.
 */
#include <stdio.h>
#include <time.h>		/* for struct timeval */

#include "/vld/include/fb.h"

struct timeval tv;

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
	if( pix_line > 512 )
		fbsetsize(pix_line);

	if( fbopen( NULL, APPEND ) < 0 )  {
		fprintf(stderr,"fbopen failed\n");
		exit(12);
	}

	zoom = pix_line/w;
	im_line = pix_line/w;	/* number of images across line */
	xPan = yPan = 0;

	fbzoom( pix_line==w? 0 : pix_line/w,
		pix_line==w? 0 : pix_line/w );

	while(passes-- > 0)  for( i=0; i<n; i++ )  {
		xPan = (i%im_line)*w+w/2;
		yPan = (i/im_line)*w+w/2;
		if( fps < 6 )
			printf("%3d: %3d %3d\n", i, xPan, yPan);

		fbwindow( xPan, yPan );
		fflush( stdout );
		(void)select( 0, 0, 0, 0, &tv );
	}
}
