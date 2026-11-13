/*                     A L I A S - P I X . C
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
/** @file util/alias-pix.c
 *
 * Convert ALIAS(tm) PIX format image files to BRL PIX format files.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include "bio.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/exit.h"

struct aliashead {
    short x, y;		/* dimensions of this image in X and Y */
    short xoff, yoff;	/* offsets of pixels */
    short bitplanes;	/* the number of bits per pixel */
};

#define BUFCNT 256
struct pixel {
    unsigned char len, b, g, r;
};

struct alias_pix_args {
    int help;
    int verbose;
};

static const struct bu_arg_option alias_pix_options[] = {
    BU_ARG_FLAG("h", "help", struct alias_pix_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct alias_pix_args, verbose,
	"Print the Alias image header"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema alias_pix_schema = BU_ARG_SCHEMA(
    "alias-pix", "Convert an Alias PIX image to BRL-CAD PIX format",
    alias_pix_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar alias_pix_grammar = {
    &alias_pix_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    int pixcnt, i;
    char *p;
    char *image;
    struct aliashead hdr = {0};
    struct pixel buf[BUFCNT];
    struct alias_pix_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &alias_pix_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&alias_pix_schema,
	&args, &msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&alias_pix_schema, "alias-pix");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "alias-pix help");
	return 0;
    }

    if (isatty(fileno(stdin)))
	bu_exit(1, "alias-pix requires redirected binary input\n");

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);


    /* read header in architecture-independent fashion */
    hdr.x = (getchar() & 0x0ff) << 8;
    hdr.x += (getchar() & 0x0ff);

    hdr.y = (getchar() & 0x0ff) << 8;
    hdr.y += (getchar() & 0x0ff);

    hdr.xoff = (getchar() & 0x0ff) << 8;
    hdr.xoff += (getchar() & 0x0ff);

    hdr.yoff = (getchar() & 0x0ff) << 8;
    hdr.yoff += (getchar() & 0x0ff);

    hdr.bitplanes = (getchar() & 0x0ff) << 8;
    hdr.bitplanes += (getchar() & 0x0ff);

    if (hdr.bitplanes != 24) {
	(void) fprintf(stderr, "Weird image file:\n");
	(void) fprintf(stderr,
		       "X: %d Y: %d xoff: %d yoff: %d bits/pixel: %d\n",
		       hdr.x, hdr.y, hdr.xoff, hdr.yoff, hdr.bitplanes);
	bu_exit(-1, "ERROR: unexpected image file\n");
    }

	    if (args.verbose) {
	(void) fprintf(stderr,
		       "X: %d Y: %d xoff: %d yoff: %d bits/pixel: %d\n",
		       hdr.x, hdr.y, hdr.xoff, hdr.yoff, hdr.bitplanes);
    }


    image=(char *)bu_malloc(hdr.x*hdr.y*3, "image buffer");

    /* read and "unpack" the image */
    p = image;
    while ((pixcnt=fread(buf, sizeof(struct pixel), BUFCNT, stdin)) > 0) {
	for (i=0; i < pixcnt; ++i)
	    do {
		*p++ = buf[i].r;
		*p++ = buf[i].g;
		*p++ = buf[i].b;
	    } while (--buf[i].len > 0);
    }

    /* write out the image scanlines, correcting for different origin */

    for (i=hdr.y-1; i >= 0; --i) {
	if (fwrite(&image[i*hdr.x*3], hdr.x*3, 1, stdout) != 1) {
	    bu_exit(-1, "%s: Error writing image\n", argv[0]);
	}
    }


    bu_free(image, "image buffer");
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
