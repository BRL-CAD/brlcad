#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
#include <ctype.h>
/*	halftone	given a bw file, generate a ht file.
 *
 * Usage:
 *	halftone
 *		-s	square size
 *		-n	number of lines
 *		-w	width
 *		-B	Beta for sharpining
 *		-m	method
 *			0 thresh hold method.
 *			1 ordered dither
 *			2 Floyd-Steinburg
 *		-S	Surpent flag.
 *		-I	number of intensities.
 *		-T	tonescale points
 *
 * Exit:
 *	writes a ht(bw) file.
 *		an ht file is a bw file with a limited set of values
 *		ranging from 0 to -I(n) as integers.
 *
 * Uses:
 *	None.
 *
 * Calls:
 *	None.
 *
 * Method:
 *	
 *
 * Author:
 *	Christopher T. Johnson	- 90/03/21
 *
 * $Log$
 * Revision 1.2  90/04/09  17:12:26  cjohnson
 * Finished parameter cracking and processing threshold halftoning.
 * 
 * Revision 1.1  90/04/09  16:13:04  cjohnson
 * Initial revision
 * 
 */
int width=512;	/* width of pixture */
int height=512;	/* height of pixture */
double Beta;	/* Beta for sharpening */
#define	M_FLOYD	0
#define	M_CLASSIC 1
#define	M_THRESH 2
#define	M_FOLLY 3
int Method=M_FLOYD;	/* Method of halftoning */
int Surpent=0;		/* use serpentine scan lines */
int Levels=2;		/* Number of levels */
int Debug=0;


void
setup(argc,argv)
int argc;
char **argv;
{
	extern char	*optarg;
	extern int	optind;
	int c;
	int i,j;
	int *Xlist, *Ylist;

	while ((c = getopt(argc, argv, "D:s:n:w:B:m:SI:T:")) != EOF) {
		switch(c) {
		case 's':
			width = height = atoi(optarg);
		break;
		case 'n':
			height = atoi(optarg);
		break;
		case 'w':
			width = atoi(optarg);
		break;
		case 'B':
			Beta = atof(optarg);
		break;
		case 'm':
			Method = atoi(optarg);
		break;
		case 'S':
			Surpent = 1;
		break;
		case 'I':
			Levels = atoi(optarg);
		break;
		case 't':
			for(i=optind; i < argc && (isdigit(*argv[i]) || 
			    (*argv[i] == '-' && isdigit(*(argv[i]+1)))) ; i++);
			if ((c=i-optind) % 2) {
				fprintf(stderr,"Missing Y coordent for tone map.\n");
				exit(1);
			}
			Xlist = (int *) malloc((c+2)*sizeof(int));
			Ylist = (int *) malloc((c+2)*sizeof(int));

			for (j=0;optind <= i; optind++) {
				Xlist[j] = atoi(argv[optind++]);
				Ylist[j] = atoi(argv[optind++]);
				j++;
			}
			Xlist[j] = 1024;
			Ylist[j] = 1024;
			(void) cubic_init(c,Xlist,Ylist);
			free(Xlist);
			free(Ylist);
		break;
		case 'D':
			Debug = atoi(optarg);
		break;
		case '?':
			fprintf(stderr,"Read the usage message.\n");
			exit(1);
		break;
		}
	}
}
main(argc,argv)
int argc;
char **argv;
{
	int pixel,x,y;
	unsigned char *Line;
	int NewFlag = 1;

	setup(argc,argv);
	
	Line = (unsigned char *) malloc(width);

	for (y=0; y<height; y++) {
		int NextX;
		NewFlag =1;
(void)		fread(Line,1,width,stdin);
		for (x=0; x<width; x++) {
			NextX = (x == width-1) ? 0 : x+1;
			pixel = Line[x];
			switch (Method) {
			case M_FLOYD:
			case M_FOLLY:
			case M_THRESH:
				putchar(255*tone_simple(pixel, x, y, NextX,
				    y+1, NewFlag));
			break;
			case M_CLASSIC:
				putchar(255*tone_classic(pixel, x, y, NextX,
				    y+1, NewFlag));
			break;
			}
			NewFlag=0;
		}
	}
}
