/*
 *		P I X P A S T E . C
 *
 * pixpaste will insert an arbitrary pix file into another pixfile.
 * If the image being pasted does not fit within the destination file
 * then the excess is discarded.
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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
                                                                                                                                                                            

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"

static int	org_width = 512;	/* Default file sizes 512x512 */
static int	org_height = 512;
static int	paste_width = 512;
static int	paste_height = 512;
static int	base_x = 0;		/* Default to lower left corner */
static int	base_y = 0;
static int	Verbose = 0;

#if defined(SYSV)
static char	stdiobuf[4*1024*1024];
#endif
static FILE	*orig, *paste;
static char	*orig_name, *paste_name;
static int	paste_autosize = 0;
static int	orig_autosize = 0;
static int	paste_isfile = 0;
static int	orig_isfile = 0;
static int	num_bytes = 3;

static char usage[] = "\
pixpaste: Copyright (C) 1992 Paladin Software\n\
pixpaste: All rights reserved\n\
pixpaste: Usage: pixpaste [-v] [-h] [-H] [-a] [-A] [-# num_bytes]\n\
		 [-s orig_square_size] [-w orig_width] [-n orig_height]\n\
		 [-S paste_square_size] [-W paste_width] [-N paste_height]\n\
		 [-x horizontal] [-y vertical] orig_file paste_file\n\
	A '-' can be used to indicate stdin for orig_file or paste_file\n";

int
get_args(register int argc, register char **argv)
{
	register int c;

	while ( (c = getopt(argc, argv, "vahHs:w:n:S:W:N:x:y:#:" )) != EOF) {
		switch (c) {
		case 'v':
			Verbose = 1;
			break;
		case 'a':
			orig_autosize = 1;
			break;
		case 'A':
			paste_autosize = 1;
			break;
		case 'h':
			org_width = org_height = 1024;
			orig_autosize = 0;
			break;
		case 'H':
			paste_width = paste_height = 1024;
			paste_autosize = 0;
			break;
		case 's':
			org_width = org_height = atoi(optarg);
			orig_autosize = 0;
			break;
		case 'S':
			paste_width = paste_height = atoi(optarg);
			paste_autosize = 0;
			break;
		case 'w':
			org_width = atoi(optarg);
			orig_autosize = 0;
			break;
		case 'W':
			paste_width = atoi(optarg);
			paste_autosize = 0;
			break;
		case 'n':
			org_height = atoi(optarg);
			orig_autosize = 0;
			break;
		case 'N':
			paste_height = atoi(optarg);
			paste_autosize = 0;
			break;
		case 'x':
			base_x = atoi(optarg);
			break;
		case 'y':
			base_y = atoi(optarg);
			break;
		case '#':
			num_bytes = atoi(optarg);
			break;
		default:		/* '?' */
			return(0);
		}
	}
	if (optind >= argc ) {
		return(0);
	} else {
		orig_name = argv[optind];
		if (strcmp(orig_name,"-") == 0) {
			if (isatty(fileno(stdin))) return(0);
			orig = stdin;
		} else {
			if ((orig = fopen(orig_name, "r")) == NULL ) {
				perror(orig_name);
				(void)fprintf(stderr,
				    "pixpaste: cannot open \"%s\" for reading\n",
				    orig_name);
				return(0);
			}
			orig_isfile = 1;
		}
	}
	if (++optind >= argc ) {
		return(0);
	} else {
		paste_name = argv[optind];
		if (strcmp(paste_name, "-") == 0) {
			if (isatty(fileno(stdin))) return(0);
			paste = stdin;
			if (!orig_isfile) {
				(void)fprintf(stderr,
"pixpaste: The original file and paste file cannot both be stdin!.\n");
				return(0);
			}
		} else {
			if ((paste = fopen(paste_name, "r")) == NULL){
				perror(paste_name);
				(void)fprintf(stderr,
				"pixpaste: cannot open \"%s\" for reading",
				paste_name);
				return(0);
			}
			paste_isfile=1;
		}
	}
	return(1);	/* OK */
}

			
int
main(int argc, char **argv)
{
	unsigned char *origbuf, *pastebuf;
	unsigned char *buffer;
	register int i;
	int row, result;

	if (!get_args(argc,argv)) {
		(void)fprintf(stderr,"%s",usage);
		exit(1);
	}
	/* Should we autosize the original? */
	if (orig_isfile && orig_autosize) {
		int w,h;
		if (bn_common_file_size(&w, &h, orig_name, num_bytes)) {
			org_width = w;
			org_height = h;
		} else {
			(void) fprintf(stderr,
"pixpaste: unable to autosize \"%s\"\n",orig_name);
		}
	}

	/* Should we autosize the paste file? */
	if (paste_isfile && paste_autosize) {
		int w,h;
		if (bn_common_file_size(&w, &h, paste_name, num_bytes)) {
			paste_width = w;
			paste_height = h;
		} else {
			(void) fprintf(stderr,
"pixpaste: unable to autosize \"%s\"\n",paste_name);
		}
	}

#if defined( SYSV )
	(void) setvbuf(orig, stdiobuf, _IOFBF, sizeof(stdiobuf) );
#endif
/*
 * Spew some interesting info at the people...
 */
	if (Verbose) {
		(void) fprintf(stderr,"\
pixpaste: Copyright (C) 1992 Paladin Software\npixpaste: All rights reserved\n");
		(void) fprintf(stderr,"pixpaste: Original image %dx%d\n",
		    org_width, org_height);
		(void) fprintf(stderr,"pixpaste: Inserted image %dx%d\n",
		    paste_width, paste_height);
		(void) fprintf(stderr,"pixpaste: Inserted at %dx%d\n",
		    base_x, base_y);
	}

/*
 * Make a buffer will hold a single scan line of assuming a worst
 * case paste of 1 pixel of overlap.
 */
	if ((buffer = (unsigned char *)malloc((org_width+paste_width)*num_bytes)) == (unsigned char *)NULL ) {
		(void) fprintf(stderr, "pixpaste: Out of memory (malloc failed)\n");
		exit(2);
	}
/*
 * Set up the original buffer and the paste buffer.
 */
	if (base_x + paste_width < 0 ||
	    base_x >= org_width ||
	    base_y + paste_height < 0 ||
	    base_y >= org_height ) {
		if (Verbose) {
			(void)fprintf(stderr, "\
pixpaste: No overlap between paste and original image\n\
pixpaste: new image == original image.\n");
		}
		for (i=0; i<org_height; i++) {
			int E=0;
			result = fread(buffer, num_bytes, org_width, orig);
			if (result != org_width) {
				E=1;
				(void)fprintf(stderr,"pixpaste: original file is short.\n");
			}
			result = fwrite(buffer, num_bytes, result, stdout);
			if (!E && result != org_width) {
				perror("pixpaste: fwrite1");
				E=1;
			}
			if (E) break;
		}
		exit(0);
	}

	if (base_x < 0) {
		pastebuf = buffer;
		origbuf  = buffer - num_bytes*base_x;
	} else {
		pastebuf = buffer + num_bytes*base_x;
		origbuf  = buffer;
	}
/*
 * if the base_y scan line is below the bottom of the original
 * then we skip scan lines in the paste file.
 */
	if (base_y < 0) {
		row = base_y;
	} else {
		row = 0;
	}

	while (row < 0) {
		result = fread(pastebuf, num_bytes, paste_width, paste);
		if (result != paste_width) {
			(void)fprintf(stderr,"pixpaste: paste file is short.\n");
			row=0;
			paste_height=0;
		}
		row++;
	}
/*
 * While the current row is less than the base Y scan line move
 * scan lines from the original to stdout.
 */
	while (row < base_y) {
		result=fread(origbuf, num_bytes, org_width, orig);
		if (result != org_width) {
			(void)fprintf(stderr,"pixpaste: original image is short.\n");
			(void)fwrite(origbuf, num_bytes, result, stdout);
			exit(0);
		}
		result = fwrite(origbuf, num_bytes, org_width, stdout);
		if (result != org_width) {
			perror("pixpaste: fwrite2");
			exit(3);
		}
		row++;
	}
/*
 * Read a scan line from the original.  Read a scan line from the
 * paste. Output the composite. until paste_height is reached.
 * If EOF original STOP.  If EOF paste treat as if reached paste_height.
 */
	while (row < org_height && row < base_y+paste_height) {
		result=fread(origbuf, num_bytes, org_width, orig);
		if (result != org_width) {
			register int jj;
			for (jj=result; jj<num_bytes*org_width; jj++) {
				origbuf[jj]=0;
			}
		}
		result=fread(pastebuf, num_bytes, paste_width, paste);
		if (result != paste_width) {
			(void)fprintf(stderr,"pixpaste: paste image is short.\n");
			base_y = paste_height = 0;
		}
		result = fwrite(origbuf, num_bytes, org_width, stdout);
		if (result != org_width) {
			perror("pixpaste: fwrite3");
			exit(3);
		}
		row++;
	}
		
/*
 * Output the rest of the original file.
 */
	while (row < org_height) {
		result=fread(origbuf, num_bytes, org_width, orig);
		if (result != org_width) {
			register int jj;
			for (jj=result; jj<num_bytes*org_width; jj++) {
				origbuf[jj]=0;
			}
		}
		result = fwrite(origbuf, num_bytes, org_width, stdout);
		if (result != org_width) {
			perror("pixpaste: fwrite4");
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
