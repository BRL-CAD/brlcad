/*
 *			P I X F A D E . C
 *
 *  Fade a pixture
 *
 * pixfade will darken a pix by a certen percentage or do an integer
 * max pixel value.  It runs in two modes, truncate which will cut any
 * channel greater than param to param, and scale which will change
 * a channel to param percent of its orignal value (limited by 0-255)
 *
 *  entry:
 *	-i	integer max value
 *	-p	percentage of fade.
 *	file	a pixture file.
 *	<stdin>	a pixture file if file is not given.
 *
 *  Exit:
 *	<stdout>	the faded pixture.
 *
 *  Calls:
 *	none.
 *
 *  Method:
 *	straight-forward.
 *
 *  Author:
 *	Christopher T. Johnson - 88/12/27
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "../rt/mathtab.h"

main(argc,argv)
int argc;
char *argv[];
{
	FILE	*inp;
	int	i;
	int	max;
	double	percent;
	struct color_rec {
		unsigned char red,green,blue;
	} cur_color;

	max = 255;
	percent = 1.0;

	inp = stdin;

/*
 * if argc .... -i max = param
 * if argc .... -p percent = param
 * if argc .... "" inp = fopen(param)
 */
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i],"-i")) {
			if (++i >= argc) {
				usage("max missing");
				exit(1);
			}
			max = atoi(argv[i]);
			if ((max < 0) || (max > 255)) {
				usage("max out of range");
				exit(1);
			}
		} else if (!strcmp(argv[i],"-p")) {
			if (++i >= argc) {
				usage("percent missing");
				exit(1);
			}
			percent = atof(argv[i]) / 100.0;
			if (percent < 0.0) {
				usage("percent is negitive");
				exit(1);
			}
		} else if (!strcmp(argv[i],"-f")) {
			if (++i >= argc) {
				usage("fraction missing");
				exit(1);
			}
			percent = atof(argv[i]);
			if (percent < 0.0) {
				usage("fraction is negitive");
				exit(1);
			}
		} else {
			fclose(stdin);
			inp = fopen(argv[i],"r");
			if (inp == NULL) {
				usage("bad file name?");
				exit(1);
			}
		}
	}

	if( isatty(fileno(stdout)) )  {
		usage("stdout is a tty");
		exit(1);
	}

/* fprintf(stderr,"max = %d, percent = %f\n",max,percent); */


	for(;;)  {
		register double	t;

		if( fread(&cur_color,1,3,inp) != 3 )  break;
		if( feof(inp) )  break;

		t = cur_color.red * percent + rand_half();
		if (t > max)
			cur_color.red   = max;
		else
			cur_color.red = t;

		t = cur_color.green * percent + rand_half();
		if (t > max)
			cur_color.green = max;
		else
			cur_color.green = t;

		t = cur_color.blue * percent + rand_half();
		if (t > max)
			cur_color.blue  = max;
		else
			cur_color.blue = t;

		fwrite(&cur_color,1,3,stdout);
	}
}

usage(s)
char *s;
{
	fprintf(stderr,"pixfade: %s\n",s);
	fprintf(stderr,"\
Usage: pixfade [-i max] [-p percent] [-f fraction] [pix-file]\n");
}
