/*
 *  			S L U R P . C
 *  
 *  Given multiple .pix files with ordinary lines of pixels,
 *  produce a single image with each image side-by-side,
 *  right to left, top to bottom.
 */
#include <stdio.h>

#include "/vld/include/fb.h"

char ibuf[512*3];		/* Input line */

struct pixel obuf[1024*1024];		/* Output screen */

int pix_line;		/* number of pixels/line (512, 1024) */
int scanbytes;		/* bytes per input line */
int w;			/* width of sub-images in pixels */
int im_line;		/* number of images across output scanline */
int image;		/* current sub-image number */

main( argc, argv )
char **argv;
{
	register int i;
	char *basename;
	int framenumber;
	char name[128];

	if( argc < 2 )  {
		printf("Usage: nslurp [-h] basename width [startframe]\n");
		exit(12);
	}

	if( strcmp( argv[1], "-h" ) == 0 )  {
		argc--; argv++;
		pix_line = 1024;
	}  else
		pix_line = 512;

	basename = argv[1];
	w = atoi( argv[2] );
	if( w < 4 || w > 512 ) {
		printf("width of %d out of range\n", w);
		exit(12);
	}
	if( argc == 4 )
		framenumber = atoi(argv[3]);
	else	framenumber = 0;

	if( pix_line > 512 )
		fbsetsize(pix_line);
	if( fbopen( NULL, CREATE ) < 0 )
		exit(12);

	scanbytes = w * 3;
	im_line = pix_line/w;	/* number of images across line */
	for( image=0; ; image++, framenumber++ )  {
		register char *in;
		register struct pixel *out;
		register int j;
		int fd;

		printf("%d ", framenumber);  fflush(stdout);
		if(image >= im_line*im_line )  {
			printf("full\n");
			break;
		}
		sprintf(name,"%s.%d", basename, framenumber);
		if( (fd=open(name,0))<0 )  {
			perror(name);
			goto done;
		}
		/* Read in .pix file.  Bottom to top */
		for( i=0; i<w; i++ )  {
			in = ibuf;
			out = &obuf[(
				/* virtual image start line*/
				((image/im_line)*w*pix_line) +
				/* virtual image l/r offset */
				((image%im_line)*w) +
				/* scanline */
				((w-i-1)*pix_line)
			)];
			if( read( fd, ibuf, scanbytes ) != scanbytes )
				break;
			for( j=0; j<w; j++ )  {
				out->red = *in++;
				out->green = *in++;
				out->blue = *in++;
				(out++)->spare = 0;
			}
		}
		close(fd);
	}
done:
	printf("done\n");
	doit();
}

doit()
{

	fbwrite( 0, 0, obuf, pix_line*pix_line );
}
