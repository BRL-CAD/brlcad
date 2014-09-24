/*                  C H A N _ P E R M U T E . C
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
/** @file chan_permute.c
 *
 * This program mixes, matches, and merges files organized in channels
 * or columns.
 *
 * It's nothing cut and paste can't do, but sometimes it's nice to be
 * able to do it in one step, especially when working with animation
 * tables. Usage:
 *

 chan_permute -i infile1 id id id ... [-i infile2 ...] -o outfile1 id id ... [-o outfile2 ...]

 *
 * where infiles are files to be read from, outfiles are files to be
 * written to, and each id is a small positive integer identifying a
 * channel. All of the input id's should be distinct integers, or the
 * results are not guaranteed.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bio.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"


#define MAXLEN 40

typedef char Word[MAXLEN];

struct unit {
    FILE *file;
    int channels;
    short *list;
    unsigned i_o;	/*i=1 o=0*/
};


char ihead[] = "-i";
char ohead[] = "-o";

static void
printusage (void)
{
    fprintf(stderr,
	    "Usage: chan_permute -i infile1 id id id ... [-i infile2 ...] -o outfile1 id id ... [-o outfile2 ...]\n");
    bu_exit(-1, NULL);
}

int
main(int argc, char *argv[])
{
    int i, j, maxlength, num_done;
    int icount, ocount;
    struct unit *x, *y;
    Word *arrayd;

    if ( BU_STR_EQUAL(argv[1], "-h") || BU_STR_EQUAL(argv[1], "-?") )
	printusage();
    if (argc == 1)
	printusage();

    i=j=icount = ocount = maxlength = 0;
    for (i=1;i<argc;i++) {
	if (!bu_strncmp(argv[i], ihead, 2)) {
	    j=0;
	    icount++;
	} else if (!bu_strncmp(argv[i], ohead, 2)) {
	    j=0;
	    ocount++;
	} else
	    maxlength = (++j>maxlength) ? j : maxlength;
    }

    y = (struct unit *) bu_calloc(icount+ocount, sizeof(struct unit), "struct unit");
    x = y - 1;
    for (i=1;i<argc;i++) {
	if (!bu_strncmp(argv[i], "-", 1)) {
	    j=0;
	    x++;
	    x->list = (short *) bu_calloc(maxlength, sizeof(short), "short array");
	    if (argv[i][1] == 'i') {
		i++;
		(x)->i_o = 1;
		if (BU_STR_EQUAL(argv[i], "stdin"))
		    x->file = stdin;
		else if (!(x->file = fopen(argv[i], "rb")))
		    fprintf(stderr, "chan_permute: can't open %s for reading\n", argv[i]);
	    } else if (argv[i][1] == 'o') {
		i++;
		(x)->i_o = 0;
		if (BU_STR_EQUAL(argv[i], "stdout"))
		    x->file = stdout;
		else if (!(x->file = fopen(argv[i], "wb")))
		    fprintf(stderr, "chan_permute: can't open %s for writing\n", argv[i]);
	    } else {
		fprintf(stderr, "Illegal option %c\n", argv[i][1]);
		bu_exit(-1, NULL);
	    }
	} else {
	    sscanf(argv[i], "%hd", x->list+(j++));
	    x->channels++;
	}
    }
    arrayd = (Word *) bu_calloc(argc, sizeof(Word), "Word"); /*may use more memory than absolutely necessary*/
    num_done = 0;
    while (num_done < icount) {
	/* go until all in files are done */
	num_done = 0;
	for (x=y;x<y+ocount+icount;x++) {
	    /* do one line */
	    if (num_done >= icount)
		;/*chill - all in files done */
	    else if (x->i_o == 1) {
		if (feof(x->file)) {
		    num_done += 1;
		} else {
		    for (j=0;j<x->channels;j++) {
			int ret = fscanf(x->file, "%40s ", arrayd[x->list[j]]);
			if (ret != 1)
			    perror("fscanf");
		    }
		}
	    } else if (x->i_o == 0) {
		for (j=0;j<x->channels;j++)
		    fprintf(x->file, "%s\t", arrayd[x->list[j]]);
		fprintf(x->file, "\n");
	    }
	}
    }

    /* release memory */
    bu_free(arrayd, "Word");
    for (x=y;x<y+ocount+icount;x++) {
	bu_free(x->list, "short array");
    }
    bu_free(y, "struct unit");

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
