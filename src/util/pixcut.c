/*
 *		P I X C U T . C
 *
 * Extract a piece of a pix file.  If the parameters of the file to be
 * extracted do not fit within the original pix file then the extra area is
 * filled with a background solid color.
 *
 *  Author -
 *	Christopher T. Johnson
 *	September 12, 1992
 *
 *  Source -
 *	Paladin Software
 *	P.O. Box 187
 *	Aberdeen, MD	21001-0187
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1992 by Paladin Software
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"


static long int	org_width = 512L;	/* Default file sizes 512x512 */
static long int	org_height = 512L;
static long int	new_width = 512L;
static long int	new_height = 512L;
static long int	base_x = 0;		/* Default to lower left corner */
static long int	base_y = 0;
static int	Verbose = 0;
static long int	num_bytes = 3L;

#define SIZEBACK	256
static unsigned char background[SIZEBACK];	/* Holds the fill background color */

#if defined(SYSV)
static char	stdiobuf[4*1024*1024];
#endif

static FILE	*input;
static char	*in_name;
static int	autosize = 0;
static int	isfile = 0;

static char usage[] = "\
pixcut: Copyright (C) 1992 Paladin Software\n\
pixcut: All rights reserved\n\
pixcut: Usage:	[-v] [-h] [-H] [-a] [-# num_bytes] [-C red/green/blue]\n\
		[-s in_square_size] [-w in_width] [-n in_height]\n\
		[-S out_square_size] [-W out_width] [-N out_height]\n\
		[-x horizontal] [-y vertical] [file_in]\n";

void
parse_color(unsigned char *bak, char *s)
{
	int red,green,blue;
	int result;

	result = sscanf(s, "%d/%d/%d", &red, &green, &blue);
	if (result != 3) return;
	bak[0] = red;
	bak[1] = green;
	bak[2] = blue;
}

int
get_args(register int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt(argc, argv, "vahHC:s:w:n:S:W:N:x:y:#:" )) != EOF) {
		switch (c) {
		case 'v':
			Verbose = 1;
			break;
		case 'a':
			autosize = 1;
			break;
		case 'h':
			org_width = org_height = 1024L;
			autosize = 0;
			break;
		case 'H':
			new_width = new_height = 1024L;
			break;
		case 's':
			org_width = org_height = atol(bu_optarg);
			autosize = 0;
			break;
		case 'S':
			new_width = new_height = atol(bu_optarg);
			break;
		case 'w':
			org_width = atol(bu_optarg);
			autosize = 0;
			break;
		case 'W':
			new_width = atol(bu_optarg);
			break;
		case 'n':
			org_height = atol(bu_optarg);
			autosize = 0;
			break;
		case 'N':
			new_height = atol(bu_optarg);
			break;
		case 'x':
			base_x = atol(bu_optarg);
			break;
		case 'y':
			base_y = atol(bu_optarg);
			break;
		case 'C':
			parse_color(background, bu_optarg);
			break;
		case '#':
			num_bytes = atol(bu_optarg);
			break;
		default:		/* '?' */
			return(0);
		}
	}
	if (bu_optind >= argc ) {
		if ( isatty(fileno(stdin))) return(0);
		in_name = "-";
		input = stdin;
	} else {
		in_name = argv[bu_optind];
		if (strcmp(in_name,"-") == 0) {
			if (isatty(fileno(stdin))) return(0);
			input = stdin;
		} else {
			if ((input = fopen(in_name, "r")) == NULL ) {
				perror(in_name);
				(void)fprintf(stderr,
				    "pixcut: cannot open \"%s\" for reading\n",
				    in_name);
				return(0);
			}
			isfile = 1;
		}
	}
	if (argc > ++bu_optind) {
		(void)fprintf(stderr, "pixcut: excess argument(s) ignored\n");
	}
	return(1);	/* OK */
}


int
main(int argc, char **argv)
{
	unsigned char *inbuf, *outbuf;
	unsigned char *buffer;
	register long int i;
	register unsigned char *cp;
	int finish, row, result;

	for (i=0;i<SIZEBACK;i++) background[i] = 0;
	background[2] = 1;

	if (!get_args(argc,argv)) {
		(void)fprintf(stderr,"%s",usage);
		exit(1);
	}
	/* Should we autosize the input? */
	if (isfile && autosize) {
		unsigned long int w,h;
		if (bn_common_file_size(&w, &h, in_name, num_bytes)) {
			org_width = (long)w;
			org_height = (long)h;
		} else {
			(void) fprintf(stderr, "pixcut: unable to autosize\n");
		}
	}

/*
 * On the assumption that there will be lots more input to paw through
 * than there will be output to write, give STDIO a big input buffer
 * to allow decent sized transfers from the filesystem.
 */
#if defined( SYSV )
	(void) setvbuf( input, stdiobuf, _IOFBF, sizeof(stdiobuf) );
#endif

/*
 * Make a buffer will hold a single scan line of assuming a worst
 * case cut of 1 pixel of the edge.
 */
	buffer = (unsigned char *)bu_malloc((org_width+new_width)*num_bytes, "buffer");

/*
 * Spew at the user if they asked.
 */
	if (Verbose) {
		(void)fprintf(stderr,"pixcut: Copyright (C) 1992 Paladin Software\n");
		(void)fprintf(stderr,"pixcut: All rights reserved.\npixcut:\n");
		(void)fprintf(stderr,"pixcut: original image %ldx%ld\n",
		    org_width, org_height);
		(void)fprintf(stderr,"pixcut: new image %ldx%ld\n",
		    new_width, new_height);
		(void)fprintf(stderr,"pixcut: offset %ldx%ld\n", base_x, base_y);
		(void)fprintf(stderr,"pixcut: background color %d/%d/%d\n",
		    background[0], background[1], background[2]);

		if (base_x < 0 || base_y < 0 ||
		    base_x+new_width >org_width ||
		    base_y+new_height > org_height) {
		    	int comma=0;
		    	char *last = 0;
			(void) fprintf(stderr,
"pixcut: adding background strip on the");

		    	if (base_x < 0) {
		    		last = "left";
		    	}
		    	if (base_y < 0) {
		    		if (last) {
		    			(void) fprintf(stderr," %s",last);
		    			comma=1;
		    		}
		    		last = "bottom";
		    	}
		    	if (base_x+new_width >org_width ){
		    		if (last) {
		    			if (comma) {
		    				(void)fprintf(stderr,", %s",last);
		    			} else {
		    				(void)fprintf(stderr," %s",last);
		    			}
		    			comma=1;
		    		}
		    	}
		    	if (base_y+new_height > org_height) {
		    		if (last) {
		    			if (comma) {
		    				(void)fprintf(stderr,", %s",last);
		    			} else {
		    				(void)fprintf(stderr," %s",last);
		    			}
		    			comma = 1;
		    		}
		    		last = "top";
		    	}
		    	if (comma) {
		    		(void)fprintf(stderr," and %s.\n",last);
		    	} else {
		    		(void)fprintf(stderr," %s.\n",last);
		    	}
		}
	}
/*
 * If the new image does not intersect the original, then set the base_x
 * so that it does not overlap the original but at the same time minmizes
 * the memory hit.
 */
	if (base_x + new_width < 0 || base_x > org_width) {
		base_x = org_width;
	}
/*
 * Assign the inbuf and outbuf pointers so that reads and writes take place
 * from a consistent location.
 */
	if (base_x < 0) {
		outbuf = buffer;
		inbuf = buffer - base_x*num_bytes;	/* base_x < 0 so - not + */
	} else {
		outbuf = buffer + base_x*num_bytes;
		inbuf = buffer;
	}
/*
 * Now fill the output buffer with the background color if needed.
 */
	if (base_x < 0 || base_y < 0 || base_x+new_width > org_width) {
		for (i=0, cp = outbuf; i<new_width; i++,cp+=num_bytes) {
			register long int jj;
			for (jj=0; jj<num_bytes && jj<SIZEBACK; jj++) {
				cp[jj]=background[jj];
			}
		}
	}
	finish = base_y + new_height;
	if (base_y < 0) {
		row = base_y;
	} else {
		row = 0;
	}
/*
 * Now sync the input file to the output file.
 */
	while (row < 0 && row < finish) {
		result = fwrite(outbuf, num_bytes, new_width, stdout);
		if (result != new_width) {
			perror("pixcut: fwrite");
			exit(3);
		}
		row++;
	}

	while(row < base_y) {
		result = fread(inbuf, num_bytes, org_width, input);
		row++;
	}
/*
 * At this point "row" is an index into the original file.
 */
	while (row < finish && row < org_height) {
		result = fread(inbuf, num_bytes, org_width, input);
		if (result != org_width) {
			for (cp=inbuf+result*num_bytes; result < org_width; cp+=num_bytes,++result) {
				register long int jj;
				for (jj=0; jj<num_bytes && jj<SIZEBACK; jj++) {
					cp[jj] = background[jj];
				}
			}
			org_height = row-1;
		}
		result = fwrite(outbuf, num_bytes, new_width, stdout);
		if (result != new_width) {
			perror("pixcut: fwrite");
			exit(3);
		}
		row++;
	}
/*
 * Refill the output buffer if we are going to be outputing background
 * lines.
 */
	if (row >= org_height) {
		for (cp=outbuf,i=0;i<new_width;cp+=num_bytes,i++) {
			register long int jj;
			for (jj=0; jj<num_bytes && jj<SIZEBACK;jj++) {
				cp[jj] = background[jj];
			}
		}
	}
/*
 * We've taken all we can from the input file, now it's time to
 * output the remaining background lines (if any).
 */
	while (row < finish) {
		result = fwrite(outbuf,num_bytes, new_width, stdout);
		if (result != new_width) {
			perror("pixcut: fwrite");
			exit(3);
		}
		row++;
	}
	return(0);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
