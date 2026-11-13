/*                      B W S H R I N K . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file util/bwshrink.c
 *
 * scale down a picture by a uniform factor.
 *
 * Options
 * h help
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"

#include "shrink_args.h"


char Stdin[]   = "(stdin)";
char *filename = Stdin;

void
shrink_image(int w, int h, unsigned char *buffer, int Factor)
{
    unsigned char *finalpixel, *pixelp;	/* output pixel pointer */
    unsigned int p;		/* pixel sum/average */
    int facsq, x, y, px, py;

    facsq = Factor * Factor;
    finalpixel = buffer;

    for (y=0; y < h; y += Factor) {
	for (x=0; x < w; x += Factor) {

	    /* average factor by factor grid of pixels */
	    p = 0;
	    for (py = 0; py < Factor; py++) {

		pixelp = &buffer[(y+py)*w+x];
		/* add pixels from scanline to average */
		for (px = 0; px < Factor; px++) {
		    p += *pixelp++;
		}
	    }

	    /* store resultant pixel back in buffer */
	    *finalpixel++ = p / facsq;
	}
    }
}


/*
 * Undersample image pixels
 */
void
usample_image(int w, int h, unsigned char *buffer, int Factor)
{
    int x, y;
    unsigned char *p;

    p = buffer;

    for (y=0; y < h; y += Factor)
	for (x=0; x < w; x += Factor, p++)
	    p[0] = buffer[x + y * w];
}


int width = 512;
int height = 512;
int factor = 2;

static const struct bu_arg_operand bwshrink_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"BW input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bwshrink_schema = BU_ARG_SCHEMA(
    "bwshrink", "Shrink a BW image by an integer factor",
    shrink_options, bwshrink_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar bwshrink_grammar = {
    &bwshrink_schema, NULL, NULL
};


/*
 * Call parse_args to handle command line arguments first, then
 * process input.
 */
int main(int ac, char **av)
{
    unsigned char *buffer = (unsigned char *)NULL;
    int size;
    int c = 0;
    int t;
    struct shrink_args args = {NULL, 0, 512, 512, 2, SHRINK_METHOD_BOXCAR};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    args.self = &args;

    bu_setprogname(av[0]);

    int introspection = bu_arg_introspect(&msg, &bwshrink_grammar, ac - 1,
	(const char * const *)(av + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&bwshrink_schema, &args,
	&msg, ac - 1, (const char **)(av + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&bwshrink_schema, "bwshrink");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bwshrink help");
	return 0;
    }
    if (ac - operand_index - 1 == 1) {
	char *ifname = bu_file_realpath(av[operand_index + 1], NULL);
	if (freopen(ifname, "rb", stdin) == NULL) {
	    perror(ifname);
	    bu_free(ifname, "bwshrink input path");
	    return 1;
	}
	bu_free(ifname, "bwshrink input path");
	filename = av[operand_index + 1];
    }
    if (isatty(fileno(stdout)))
	bu_exit(1, "bwshrink requires redirected standard output\n");
    if (isatty(fileno(stdin)))
	bu_exit(1, "bwshrink requires an input file or redirected input\n");

    width = args.width;
    height = args.height;
    factor = args.factor;

    /* process stdin */
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    /* get buffer for image */
    size = width * height;
    if (size > 0) {
	buffer = (unsigned char *)bu_malloc(size, "alloc buffer");
    } else {
	bu_log("ERROR: zero dimension image\n");
	return 1;
    }

    /* read in entire image */
    for (t=0; t < size && (c=read(0, (char *)&buffer[t], size-t)) >= 0; t += c) {
	/* do nothing */;
    }

    if (c < 0) {
	perror (filename);
	return -1;
    }

    switch (args.method) {
	case SHRINK_METHOD_BOXCAR:
	    shrink_image(width, height, buffer, factor);
	    break;
	case SHRINK_METHOD_UNDERSAMPLE:
	    usample_image(width, height, buffer, factor);
	    break;
	default:
	    return -1;
    }

    for (t=0; t < size && (c=write(1, (char *)&buffer[t], size-t)) >= 0;
	 t += c);

    bu_free(buffer, "buffer alloc from malloc");

    if (c < 0) {
	perror("stdout");
	return -1;
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
