/*                     A N I M _ S O R T . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file anim_sort.c
 *
 * Combine multiple animation scripts on standard input into a single
 * script on standard output. The output can be in natural order or in
 * a scrambled order for incrementally increasing time resolution (-i
 * option).
 *
 */

#include "common.h"

#include "bio.h"
#include <string.h>
#include <stdlib.h>

#include "bu.h"


#define OPT_STR "cih?"

#define MAXLEN 50 /*maximum length of lines to be read */
#define MAXLINES 30		/* maximum length of lines to be stored*/

int suppressed;		/* flag: suppress printing of 'clean;' commands */
int incremental;	/* flag: order for incremental time resolution */

static void
usage(void)
{
    fprintf(stderr,"Usage: anim_sort [-ic] < mixed.script > ordered.script\n");
}


int get_args(int argc, char **argv)
{

    int c;
    suppressed = 0;

    while ((c=bu_getopt(argc, argv, OPT_STR)) != -1) {
	switch (c) {
	    case 'c':
		suppressed = 1;
		break;
	    case 'i':
		incremental = 1;
		break;
	    default:
		return 0;
	}
    }
    return 1;
}


int
main(int argc, char *argv[])
{
    int length, frame_number, number, success, maxnum;
    int first_frame, spread, reserve;
    off_t last_pos;
    char line[MAXLEN];
    char pbuffer[MAXLEN*MAXLINES];

    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout))) {
	usage();
	return 0;
    }

    if (!get_args(argc, argv)) {
	usage();
	return 0;
    }

    /* copy any lines preceding the first "start" command */
    last_pos = bu_ftell(stdin);
    while (bu_fgets(line, MAXLEN, stdin)!=NULL) {
	if (bu_strncmp(line, "start", 5)) {
	    printf("%s", line);
	    last_pos = bu_ftell(stdin);
	} else
	    break;
    }

    /* read the frame number of the first "start" command */
    sscanf(strpbrk(line, "0123456789"), "%d", &frame_number);

    /* find the highest frame number in the file */
    maxnum = 0;
    while (bu_fgets(line, MAXLEN, stdin)!=NULL) {
	if (!bu_strncmp(line, "start", 5)) {
	    sscanf(strpbrk(line, "0123456789"), "%d", &number);
	    maxnum = (maxnum>number)?maxnum:number;
	}
    }

    length = maxnum - frame_number + 1;
    /* spread should initially be the smallest power of two larger than
     * or equal to length */
    spread = 2;
    while (spread < length)
	spread = spread<<1;

    first_frame = frame_number;
    success = 1;
    while (length--) {
	number = -1;
	success = 0; /* tells whether or not any frames have been found which have the current frame number*/
	if (incremental) {
	    bu_fseek(stdin, 0, 0);
	} else {
	    bu_fseek(stdin, last_pos, 0);
	}

	reserve = MAXLEN*MAXLINES;
	pbuffer[0] = '\0'; /* delete old pbuffer */

	/* inner loop: search through the entire file for frames */
	/* which have the current frame number */
	while (!feof(stdin)) {

	    /*read to next "start" command*/
	    while (bu_fgets(line, MAXLEN, stdin)!=NULL) {
		if (!bu_strncmp(line, "start", 5)) {
		    sscanf(strpbrk(line, "0123456789"), "%d", &number);
		    break;
		}
	    }
	    if (number==frame_number) {
		if (!success) {
		    /*first successful match*/
		    printf("%s", line);
		    if (!suppressed) printf("clean;\n");
		    success = 1;
		    last_pos = bu_ftell(stdin);
		}
		/* print contents until next "end" */
		while (bu_fgets(line, MAXLEN, stdin)!=NULL) {
		    if (!bu_strncmp(line, "end;", 4))
			break;
		    else if (bu_strncmp(line, "clean", 5))
			printf("%s", line);
		}
		/* save contents until next "start" */
		while (bu_fgets(line, MAXLEN, stdin)!=NULL) {
		    if (!bu_strncmp(line, "start", 5))
			break;
		    else {
			reserve -= strlen(line);
			reserve -= 1;
			if (reserve > 0) {
			    bu_strlcat(pbuffer, line, reserve + strlen(line) + 1);
			} else {
			    printf("ERROR: ran out of buffer space (%d characters)\n", MAXLEN*MAXLINES);
			}
		    }
		}
	    }
	}
	if (success)
	    printf("end;\n");
	/* print saved-up post-raytracing commands, if any */
	printf("%s", pbuffer);

	/* get next frame number */
	if (incremental) {
	    frame_number = frame_number + 2*spread;
	    while (frame_number > maxnum) {
		spread = spread>>1;
		frame_number = first_frame + spread;
	    }
	} else {
	    frame_number += 1;
	}
    }
    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
