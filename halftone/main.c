#include <stdio.h>
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
 *		-t	tonescale selector
 *			0 line
 *			1 cubic
 *		-x	first X parameter
 *		-X	second X parameter
 *		-y	first Y parameter
 *		-Y	second Y parametetr
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
 * $Header$
 * $Log$
 */
extern char	*optarg;
extern int	optind;
main(argc,argv)
int argc;
char **argv;
{
	int c;

	while ((c = getopt(argc, argv, "s:n:w:B:m:SI:t:x:X:y:Y:") != EOF)
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
			method = atoi(optarg);
		break;
		case 'S':
			surpent = 2;
		break;
		case 'I':
			levels = atoi(optarg);
		break;
		case 't':
			tone = atoi(optarg);
		break;
		case 'x':
			PX[0] = atof(optarg);
		break;
		case 'X':
			PX[1] = atof(optarg);
		break;
		case 'y':
			PY[0] = atof(optarg);
		break;
		case 'Y':
			PY[1] = atof(optarg);
		break;
		
