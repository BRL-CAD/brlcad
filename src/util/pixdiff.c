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
 * Highlight differences between two raw PIX or BW images.  Matching
 * PIX pixels provide dim grayscale context; matching BW samples are
 * written at half intensity.  Differing channels are highlighted.
 *
 * Comparison is independent of image resolution.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/str.h"
#include "bu/exit.h"


#define PIX_CHANNELS 3
/* Buffered reads must end on a PIX pixel boundary. */
#define BUFFER_SIZE (8192 / PIX_CHANNELS * PIX_CHANNELS)
#define CONTEXT_DIVISOR 2
#define OFF_BY_ONE_INTENSITY 0xC0
#define OFF_BY_MANY_INTENSITY 0xFF
#define GRAYSCALE_RED_WEIGHT 22937
#define GRAYSCALE_GREEN_WEIGHT 36044
#define GRAYSCALE_BLUE_WEIGHT 6553
#define GRAYSCALE_SHIFT 17

static const char usage[] =
    "Usage: pixdiff [-b] file1 file2 > diff\n"
    "       (default: PIX input/output; -b: BW input/output; -: standard input)\n";


static size_t
read_bytes(FILE *input, unsigned char *buffer, size_t capacity)
{
    size_t count = 0;

    while (count < capacity) {
	size_t n = fread(buffer + count, 1, capacity - count, input);
	if (n == 0) {
	    if (ferror(input))
		bu_exit(1, "pixdiff: input read error\n");
	    break;
	}
	count += n;
    }

    return count;
}


static void
write_byte_diff(int c1, int c2, int matching_intensity, FILE *output,
	size_t *offmany, size_t *off1, size_t *matching)
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
	fputc(matching_intensity, output);
	(*matching)++;
    }
}


static void
write_pix_match(const unsigned char *pixel, FILE *output)
{
    long intensity = ((GRAYSCALE_RED_WEIGHT * pixel[0] +
	GRAYSCALE_GREEN_WEIGHT * pixel[1] +
	GRAYSCALE_BLUE_WEIGHT * pixel[2]) >> GRAYSCALE_SHIFT) / CONTEXT_DIVISOR;
    size_t channel;

    for (channel = 0; channel < PIX_CHANNELS; channel++)
	fputc((int)intensity, output);
}


int
main(int argc, char *argv[])
{
    size_t matching = 0;
    size_t off1 = 0;
    size_t offmany = 0;
    unsigned char buffer1[BUFFER_SIZE];
    unsigned char buffer2[BUFFER_SIZE];
    const char *file1;
    const char *file2;
    int bw_mode = 0;
    int option;

    FILE *f1, *f2;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    bu_optind = 1;
    while ((option = bu_getopt(argc, argv, "bh")) != -1) {
	switch (option) {
	    case 'b':
		bw_mode = 1;
		break;
	    case 'h':
		bu_exit(0, "%s", usage);
	    default:
		bu_exit(1, "%s", usage);
	}
    }

    if (argc - bu_optind != 2 || isatty(fileno(stdout)))
	bu_exit(1, "%s", usage);

    file1 = argv[bu_optind];
    file2 = argv[bu_optind + 1];

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
	size_t count1 = read_bytes(f1, buffer1, sizeof(buffer1));
	size_t count2 = read_bytes(f2, buffer2, sizeof(buffer2));
	size_t i;

	if (count1 != count2)
	    bu_exit(1, "pixdiff: input sizes differ\n");
	if (count1 == 0)
	    break;
	if (!bw_mode && count1 % PIX_CHANNELS != 0)
	    bu_exit(1, "pixdiff: input contains an incomplete PIX pixel\n");

	if (bw_mode) {
	    for (i = 0; i < count1; i++)
		write_byte_diff(buffer1[i], buffer2[i], buffer1[i] / CONTEXT_DIVISOR,
			stdout, &offmany, &off1, &matching);
	} else {
	    for (i = 0; i < count1; i += PIX_CHANNELS) {
	d	if (buffer1[i] == buffer2[i] &&
		    buffer1[i + 1] == buffer2[i + 1] &&
		    buffer1[i + 2] == buffer2[i + 2]) {
		    write_pix_match(buffer1 + i, stdout);
		    matching += PIX_CHANNELS;
		} else {
		    size_t channel;
		    for (channel = 0; channel < PIX_CHANNELS; channel++)
			write_byte_diff(buffer1[i + channel], buffer2[i + channel], 0,
				stdout, &offmany, &off1, &matching);
		}
	    }
	}
    }

    if (fflush(stdout) == EOF)
	bu_exit(1, "pixdiff: output write error\n");

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
