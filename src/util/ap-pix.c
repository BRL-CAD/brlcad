/*                        A P - P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file util/ap-pix.c
 *
 * Applicon color ink jet printer to .pix file converter.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/file.h"
#include "bu/str.h"
#include "bu/exit.h"


static void
usage(const char *argv0) {
    bu_exit(1, "Usage: %s [-v] file.ap > file.pix (3456 x ?)\n", argv0);
}


int
main(int argc, char **argv)
{
    static const int separation_records = 50;

    /* Dots are least most significant bit first in increasing index */
    struct app_record {
	unsigned char ml[432];
	unsigned char yl[432];
	unsigned char cl[432];
    } magline, yelline, cyaline;

    struct {
	unsigned char red, green, blue;
    } out;

    FILE *magfp, *yelfp, *cyafp;

    int i, bit;
    int line;
    const char *argv0 = argv[0];
    int verbose = 0;

    bu_setprogname(argv[0]);
    setmode(fileno(stdout), O_BINARY);

    if (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-h") ||  BU_STR_EQUAL(argv[1], "-?"))
	    usage(argv0);
	if (BU_STR_EQUAL(argv[1], "-v")) {
	    verbose++;
	    argc--;
	    argv++;
	}
    }

    if (argc != 2)
	usage(argv0);

    magfp = fopen(argv[1], "rb");
    if (!magfp)
	bu_exit(2, "%s: can't open \"%s\"\n", argv0, argv[1]);
    yelfp = fopen(argv[1], "rb");
    if (!yelfp)
	bu_exit(2, "%s: can't open \"%s\"\n", argv0, argv[1]);
    cyafp = fopen(argv[1], "rb");
    if (!cyafp)
	bu_exit(2, "%s: can't open \"%s\"\n", argv0, argv[1]);
    if (bu_fseek(yelfp, separation_records * sizeof(yelline), SEEK_SET) != 0 ||
	bu_fseek(cyafp, 2 * separation_records * sizeof(cyaline), SEEK_SET) != 0)
	bu_exit(1, "%s: seek failure\n", argv0);

    line = 0;
    size_t cyan_bytes;
    while ((cyan_bytes = fread(&cyaline, 1, sizeof(cyaline), cyafp)) == sizeof(cyaline)) {
	if (fread(&magline, sizeof(magline), 1, magfp) != 1 ||
	    fread(&yelline, sizeof(yelline), 1, yelfp) != 1)
	    bu_exit(1, "%s: incomplete color separation data\n", argv0);

	line++;

	for (i = 0; i < 432; i++) {
	    for (bit = 7; bit >= 0; bit--) {
		out.red = ((cyaline.cl[i]>>bit)&1) ? 0 : 255;
		out.green = ((magline.ml[i]>>bit)&1) ? 0 : 255;
		out.blue = ((yelline.yl[i]>>bit)&1) ? 0 : 255;
		if (fwrite(&out, sizeof(out), 1, stdout) != 1)
		    bu_exit(1, "%s: write failure\n", argv0);
	    }
	}
	if (verbose)
	    fprintf(stderr, "wrote line %d\n", line);
    }
    if (cyan_bytes != 0)
	bu_exit(1, "%s: incomplete cyan separation record\n", argv0);
    if (ferror(cyafp))
	bu_exit(1, "%s: read failure\n", argv0);
    int close_failed = 0;
    if (fclose(magfp) == EOF)
	close_failed = 1;
    if (fclose(yelfp) == EOF)
	close_failed = 1;
    if (fclose(cyafp) == EOF)
	close_failed = 1;
    if (close_failed)
	bu_exit(1, "%s: input close failure\n", argv0);
    if (fflush(stdout) == EOF)
	bu_exit(1, "%s: write failure\n", argv0);
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
