#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
#include <ctype.h>
int Debug=0;
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
 * Revision 1.1  90/04/09  16:13:04  cjohnson
 * Initial revision
 * 
 */
int width;	/* width of pixture */
int height;	/* height of pixture */
double Beta;	/* Beta for sharpening */
#define	M_FLOYD	0
#define	M_CLASSIC 1
#define	M_THRESH 2
#define	M_FOLLY 3
int Method=M_FLOYD;	/* Method of halftoning */
int Surpent=0;		/* use serpentine scan lines */
int Levels=2;		/* Number of levels */
main(argc,argv)
int argc;
char **argv;
{
	extern char	*optarg;
	extern int	optind;
	int c;
	int i,j;
	int *Xlist, *Ylist;
	unsigned int pixel;

	while ((c = getopt(argc, argv, "s:n:w:B:m:SI:T:")) != EOF) {
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
		case '?':
			fprintf(stderr,"Read the usage message.\n");
			exit(1);
		break;
		}
	}

	
	while((pixel = (unsigned int) getchar()) != (unsigned) EOF) {
		if (tone_simple(pixel,0,0,0,0,0)) {
			putchar('\255');
		} else {
			putchar('\0');
		}
	}
}
