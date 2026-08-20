/*                       P I X D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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
/** @file util/pixdiff.c
 *
 * Compute the byte-wise difference between two raw image files.  To
 * establish context, matching bytes are written at half intensity;
 * differing bytes are highlighted.
 *
 * This routine does not interpret pixel structure, and thus works for
 * both .pix and .bw data independent of image resolution.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/str.h"
#include "bu/exit.h"


#define BUFFER_SIZE 8192
#define CONTEXT_DIVISOR 2
#define OFF_BY_ONE_INTENSITY 0xC0
#define OFF_BY_MANY_INTENSITY 0xFF


static void
write_byte_diff(int c1, int c2, FILE *output, size_t *offmany, size_t *off1, size_t *matching)
{
    if (c1 != c2) {
	int difference = abs(c1 - c2);
	if (difference > 1) {
	    fputc(OFF_BY_MANY_INTENSITY, output);
	    (*offmany)++;
	} else {
	    fputc(OFF_BY_ONE_INTENSITY, output);
	    (*off1)++;
	}
    } else {
	fputc(c1 / CONTEXT_DIVISOR, output);
	(*matching)++;
    }
}


int
main(int argc, char *argv[])
{
    size_t matching = 0;
    size_t off1 = 0;
    size_t offmany = 0;

    FILE *f1, *f2;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (argc != 3 || isatty(fileno(stdout))) {
	bu_exit(1, "Usage: pixdiff file1 file2 >diff\n");
    }

    const char *file1 = argv[1];
    const char *file2 = argv[2];

    if (BU_STR_EQUAL(file1, "-") && BU_STR_EQUAL(file2, "-")) {
	bu_exit(1, "pixdiff: standard input cannot supply both images\n");
    }

    if (BU_STR_EQUAL(file1, "-"))
	f1 = stdin;
    else if ((f1 = fopen(file1, "rb")) == NULL) {
	perror(file1);
	return 1;
    }
    if (BU_STR_EQUAL(file2, "-"))
	f2 = stdin;
    else if ((f2 = fopen(file2, "rb")) == NULL) {
	perror(file2);
	if (f1 != stdin)
	    fclose(f1);
	return 1;
    }

    while (1) {
	unsigned char buffer1[BUFFER_SIZE];
	unsigned char buffer2[BUFFER_SIZE];
	size_t count1 = fread(buffer1, 1, sizeof(buffer1), f1);
	size_t count2 = fread(buffer2, 1, sizeof(buffer2), f2);
	size_t i;

	if (ferror(f1) || ferror(f2))
	    bu_exit(1, "pixdiff: input read error\n");
	if (count1 != count2)
	    bu_exit(1, "pixdiff: input sizes differ\n");
	if (count1 == 0)
	    break;

	for (i = 0; i < count1; i++)
	    write_byte_diff(buffer1[i], buffer2[i], stdout, &offmany, &off1, &matching);
    }

    if (f1 != stdin)
	fclose(f1);
    if (f2 != stdin)
	fclose(f2);

    fprintf(stderr,
	    "pixdiff bytes: %7zu matching, %7zu off by 1, %7zu off by many\n",
	    matching, off1, offmany);

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
