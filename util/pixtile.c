/*
 *  			S L U R P . C
 *  
 *  Given multiple .pix files with ordinary lines of pixels,
 *  produce a single image with each image side-by-side,
 *  right to left, top to bottom.
 */
#include <stdio.h>

extern int ikhires;
extern int ikfd;

char ibuf[512*3];		/* Input line */
char obuf[1024*1024*4];		/* Output screen */

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
		ikhires = 1;
		pix_line = 1024;
	}  else
		pix_line = 512;

	basename = argv[1];
	w = atoi( argv[2] );
	if( w < 4 || w > 256 ) {
		printf("width of %d out of range\n", w);
		exit(12);
	}
	if( argc == 4 )
		framenumber = atoi(argv[3]);
	else	framenumber = 0;

	ikopen();
	load_map(1);		/* Standard map: linear */
	ikclear();

	scanbytes = w * 3;
	im_line = pix_line/w;	/* number of images across line */
	for( image=0; ; image++, framenumber++ )  {
		register char *in, *out;
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
			out = &obuf[4*(
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
				*out++ = *in++;
				*out++ = *in++;
				*out++ = *in++;
				*out++ = 0;
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
	register int i;

	(void)lseek(ikfd, 0L, 0);
	for( i=0; i < pix_line; i+=8 )
		write(ikfd, &obuf[i*pix_line*4], 8*pix_line*4 );
}
