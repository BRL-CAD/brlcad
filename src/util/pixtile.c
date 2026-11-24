/*                       P I X T I L E . C
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
/** @file util/pixtile.c
 *
 * Given multiple .pix files with ordinary lines of pixels, produce a
 * single image with each image side-by-side, right to left, bottom to
 * top on STDOUT.
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
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/exit.h"
#include "bu/log.h"

#include "tile_args.h"


int file_width = 64;	/* width of input sub-images in pixels (64) */
int file_height = 64;	/* height of input sub-images in scanlines (64) */
int scr_width = 512;	/* number of output pixels/line (512, 1024) */
int scr_height = 512;	/* number of output lines (512, 1024) */
char *base_name;		/* basename of input file(s) */
int framenumber = 0;	/* starting frame number (default is 0) */

static const struct bu_arg_operand pixtile_operands[] = {
    BU_ARG_OPERAND("basename-or-file", BU_ARG_VALUE_FILE, 1,
	BU_ARG_COUNT_UNLIMITED, "Input basename or explicit PIX files", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixtile_schema = BU_ARG_SCHEMA(
    "pixtile", "Paste small PIX images into one large image",
    tile_options, pixtile_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixtile_grammar = {
    &pixtile_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    int i;
    char *obuf;
    int im_line;		/* number of images across output scanline */
    int im_high;		/* number of images (swaths) high */
    int scanbytes;		/* bytes per input line */
    int swathbytes;		/* bytes per swath of images */
    int image;		/* current sub-image number */
    int rel = 0;		/* Relative image # within swath */
    int maximage;		/* Maximum # of images that will fit */
    int islist = 0;		/* set if a list, zero if basename */
    int is_stream = 0;	/* set if input is stream on stdin */
    char name[256] = {0};
    ssize_t ret;
    struct tile_args args = {NULL, 0, 64, 64, 512, 512, 0, INT_MAX};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    args.self = &args;
    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixtile_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixtile_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixtile_schema, "pixtile");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixtile help");
	return 0;
    }
    if (isatty(fileno(stdout)))
	bu_exit(1, "pixtile cannot send PIX output to a terminal\n");

    file_width = (int)args.input_width;
    file_height = (int)args.input_height;
    scr_width = (int)args.output_width;
    scr_height = (int)args.output_height;
    framenumber = args.first_frame;
    int next_operand = operand_index + 1;

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (next_operand + 1 == argc) {
	base_name = argv[next_operand];
	islist = 0;
	if (base_name[0] == '-' && base_name[1] == '\0')
	    is_stream = 1;
    } else {
	islist = 1;
    }

    if (file_width < 1) {
	fprintf(stderr, "pixtile: width of %d out of range\n", file_width);
	bu_exit(12, NULL);
    }

    scanbytes = file_width * 3;

    /* number of images across line */
    im_line = scr_width/file_width;

    /* number of images high */
    im_high = scr_height/file_height;

    /* One swath of images */
    swathbytes = scr_width * file_height * 3;

    maximage = im_line * im_high;

    if ((obuf = (char *)malloc(swathbytes)) == (char *)0) {
	fprintf(stderr, "pixtile:  malloc %d failure\n", swathbytes);
	bu_exit(10, NULL);
    }

    image = 0;
    while (image < maximage) {
	char *ifname;

	memset(obuf, 0, swathbytes);
	/*
	 * Collect together one swath
	 */
	for (rel = 0; rel<im_line; rel++, image++, framenumber++) {
	    int fd;

	    if (image >= maximage) {
		fprintf(stderr, "\npixtile: frame full\n");
		/* All swaths already written out */
		bu_exit(0, NULL);
	    }
	    fprintf(stderr, "%d ", framenumber);  fflush(stdout);
	    if (is_stream) {
		fd = 0;		/* stdin */
	    } else {
		if (islist) {
		    /* See if we read all the files */
		    if (next_operand >= argc)
			goto done;
		    bu_strlcpy(name, argv[next_operand++], sizeof(name));
		} else {
		    snprintf(name, sizeof(name), "%s.%d", base_name, framenumber);
		}

		ifname = bu_file_realpath(name, NULL);
		fd = open(ifname, O_RDONLY|O_BINARY);
		if (fd < 0) {
		    perror(ifname);
		    bu_free(ifname, "ifname alloc from bu_file_realpath");
		    goto done;
		}
		bu_free(ifname, "ifname alloc from bu_file_realpath");
	    }
	    /* Read in .pix file.  Bottom to top */
	    for (i=0; i<file_height; i++) {
		int j;

		/* virtual image l/r offset */
		j = (rel*file_width);

		/* select proper scanline within image */
		j += i*scr_width;

		if (bu_mread(fd, &obuf[j*3], scanbytes) != scanbytes) {
		    perror("READ ERROR");
		    break;
		}
	    }
	    if (fd > 0)
		close(fd);
	}
	ret = write(fileno(stdout), obuf, swathbytes);
	if (ret < 0)
	    perror("write");

	rel = 0;	/* in case we fall through */
    }
done:
    /* Flush partial frame? */
    if (rel != 0) {
	ret = write(fileno(stdout), obuf, swathbytes);
	if (ret < 0)
	    perror("write");
    }

    bu_free(obuf, "obuf alloc from malloc");

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
