/*                     P I X U N T I L E . C
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
/** @file util/pixuntile.c
 *
 * Given a single .pix file with multiple images, each side-by-side,
 * right to left, bottom to top, break them up into separate .pix
 * files.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/exit.h"

#include "tile_args.h"


size_t out_width = 64;	/* width of input sub-images in pixels (64) */
size_t out_height = 64;	/* height of input sub-images in scanlines (64) */
size_t in_width = 512;	/* number of output pixels/line (512, 1024) */
size_t in_height = 512;	/* number of output lines (512, 1024) */
char *base_name;		/* basename of input file(s) */
int framenumber = 0;	/* starting frame number (default is 0) */
int islist = 0;

static const struct bu_arg_operand pixuntile_operands[] = {
    BU_ARG_OPERAND("basename-or-file", BU_ARG_VALUE_FILE, 1,
	BU_ARG_COUNT_UNLIMITED, "Output basename or explicit PIX files", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixuntile_schema = BU_ARG_SCHEMA(
    "pixuntile", "Break a tiled PIX image into separate files",
    tile_options, pixuntile_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixuntile_grammar = {
    &pixuntile_schema, NULL, NULL
};


int numx;
int numy;
int pixsize = 3;

int
main(int argc, char **argv)
{
    int i, y;
    char ibuf[1024*3] = {0};
    char name[80] = {0};
    FILE *f[8] = {NULL};
    size_t ret;
    struct tile_args args = {NULL, 0, 512, 512, 64, 64, 0, SIZE_MAX};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    args.self = &args;
    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixuntile_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixuntile_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixuntile_schema, "pixuntile");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixuntile help");
	return 0;
    }
    if (isatty(fileno(stdin)))
	bu_exit(1, "pixuntile requires redirected PIX input\n");

    in_width = args.input_width;
    in_height = args.input_height;
    out_width = args.output_width;
    out_height = args.output_height;
    framenumber = args.first_frame;
    int next_operand = operand_index + 1;

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (next_operand + 1 == argc) {
	base_name = argv[next_operand];
	islist = 0;
    } else {
	islist = 1;
    }

    if (in_width < 1) {
	fprintf(stderr, "pixuntile: width of %lu out of range\n", (long unsigned)in_width);
	bu_exit (12, NULL);
    }

    numx = in_width / out_width;
    numy = in_height / out_height;

    y = 0;
    while (fread(ibuf, pixsize, in_width, stdin) == in_width) {
	if (y == 0) {
	    /* open a new set of output files */
	    for (i = 0; i < numx; i++) {
		if (f[i] != 0)
		    fclose(f[i]);
		fprintf(stderr, "%d ", framenumber);  fflush(stdout);
		if (islist) {
		    /* See if we read all the files */
		    if (next_operand >= argc)
			goto done;
		    bu_strlcpy(name, argv[next_operand++], sizeof(name));
		} else {
		    snprintf(name, sizeof(name), "%s.%d", base_name, framenumber);
		}
		if ((f[i] = fopen(name, "wb")) == NULL) {
		    perror(name);
		    goto done;
		}
		framenumber++;
	    }
	}
	/* split this scanline up into the output files */
	for (i = 0; i < numx; i++) {
	    ret = fwrite(&ibuf[i*out_width*pixsize], pixsize, out_width, f[i]);
	    if (ret < out_width)
		perror("fwrite");
	}
	y = (y + 1) % out_height;
    }
done:
    fprintf(stderr, "\n");

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
