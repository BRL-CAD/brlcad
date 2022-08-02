/*                            C V . C
 * BRL-CAD
 *
 * Copyright (c) 1991-2022 United States Government as represented by
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
 */
/** @file util/cv.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/cv.h"
#include "raytrace.h"


char usage[] = "\
Usage: cv in_pat out_pat [[infile] outfile]\n\
\n\
Where a pattern (no embedded blanks) is: [h|n] [s|u] [c|s|i|l|d|8|16|32|64]\n\
e.g., hui is host unsigned int, nl is network (signed) long\n\
";

int in_cookie;
int out_cookie;

size_t iitem;
size_t oitem;

size_t inbytes;
size_t outbytes;

FILE *infp;
FILE *outfp;

void *ibuf;
void *obuf;

const char huc[] = "huc";
const char nuc[] = "nuc";

int
main(int argc, char **argv)
{
    const char *in_pat;
    const char *out_pat;
    size_t m;
    size_t n;

    bu_setprogname(argv[0]);

    if (argc < 3 || argc > 5) {
	fputs(usage, stderr);
	return 1;
    }

    in_pat = argv[1];
    if (BU_STR_EQUAL(in_pat, "")) {
	in_pat = huc;
    } else if (strlen(in_pat) > 4 || strlen(in_pat) < 1) {
	fprintf(stderr, "cv: unrecognized input pattern\n");
	return 1;
    }

    out_pat = argv[2];
    if (BU_STR_EQUAL(out_pat, "")) {
	out_pat = nuc;
    } else if (strlen(out_pat) > 4 || strlen(out_pat) < 1) {
	fprintf(stderr, "cv: unrecognized output pattern\n");
	return 1;
    }

    if (argc == 5) {
	if ((outfp = fopen(argv[4], "wb")) == NULL) {
	    perror(argv[4]);
	    return 2;
	}
    } else {
	outfp = stdout;
	setmode(fileno(outfp), O_BINARY);
    }

    if (argc >= 4) {
	if ((infp = fopen(argv[3], "rb")) == NULL) {
	    perror(argv[3]);
	    return 3;
	}
    } else {
	infp = stdin;
	setmode(fileno(infp), O_BINARY);
    }

    if (isatty(fileno(outfp))) {
	fprintf(stderr, "cv: trying to send binary output to terminal\n");
	return 5;
    }

    in_cookie = bu_cv_cookie(in_pat);
    out_cookie = bu_cv_cookie(out_pat);
    iitem = bu_cv_itemlen(in_cookie);
    oitem = bu_cv_itemlen(out_cookie);

    if (iitem == 0) {
	fprintf(stderr, "cv: unrecognized input format [%s]\n", in_pat);
	return 6;
    } else if (oitem == 0) {
	fprintf(stderr, "cv: unrecognized output format [%s]\n", out_pat);
	return 6;
    }

#define NITEMS (64*1024)
    inbytes = NITEMS*iitem;
    outbytes = NITEMS*oitem;

    ibuf = (void *)bu_malloc(inbytes, "cv input buffer");
    obuf = (void *)bu_malloc(outbytes, "cv output buffer");

    while (!feof(infp)) {
	if ((n = fread(ibuf, iitem, NITEMS, infp)) <= 0)
	    break;
	m = bu_cv_w_cookie(obuf, out_cookie, outbytes, ibuf, in_cookie, n);
	if (m != n) {
	    bu_log("cv: bu_cv_w_cookie() ret=%zu, count=%zu\n", m, n);
	    return 4;
	}
	m = fwrite(obuf, oitem, n, outfp);
	if (m != n) {
	    perror("fwrite");
	    bu_log("cv: fwrite() ret=%zu, count=%zu\n", m, n);
	    return 5;
	}
    }
    fclose(infp);
    fclose(outfp);

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
