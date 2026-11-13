/*                     P I X S H R I N K . C
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
/** @file util/pixshrink.c
 *
 * scale down a picture by a uniform factor.
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


#define UCHAR unsigned char

char Stdin[]   = "(stdin)";
char *filename = Stdin;

void shrink_image(int scanlen, int Width, int Height, unsigned char *buffer, int Factor), usample_image(int scanlen, int Width, int Height, unsigned char *buffer, int Factor);

/*
 * read image into memory
 */
UCHAR *read_image(int scanlen, int Width, int Height, unsigned char *buffer)
{
    int total_bytes, in_bytes;
    int count = 0;

    if (!buffer &&
	(buffer=(UCHAR *)calloc(scanlen, Height)) == (UCHAR *)NULL) {
	fprintf(stderr, "%s: cannot allocate input buffer\n",
		bu_getprogname());
	bu_exit (-1, NULL);
    }

    total_bytes = Width * Height * 3;
    in_bytes = 0;
    while (in_bytes < total_bytes &&
	   (count=read(0, (char *)&buffer[in_bytes], (unsigned)(total_bytes-in_bytes))) >= 0)
	in_bytes += count;

    if (count < 0) {
	perror(filename);
	bu_exit (-1, NULL);
    }

    return buffer;
}


void write_image(int Width, int Height, unsigned char *buffer)
{
    int count = 0;
    int out_bytes, total_bytes;

    total_bytes = Width * Height * 3;
    out_bytes = 0;

    while (out_bytes < total_bytes &&
	   (count=write(1, (char *)&buffer[out_bytes], total_bytes-out_bytes)) >= 0)
	out_bytes += count;

    if (count < 0) {
	perror("stdout");
	bu_exit (-1, NULL);
    }
}


void
shrink_image(int scanlen, int Width, int Height, unsigned char *buffer, int Factor)
{
    UCHAR *pixelp, *out_buf;
    unsigned int p[3];
    int facsq, x, y, px, py;

    facsq = Factor * Factor;

    out_buf = buffer;

    for (y=0; y < Height; y += Factor)
	for (x=0; x < Width; x += Factor) {

	    /* average factor by factor pixels */

	    for (py = 0; py < 3; ++py)
		p[py] = 0;

	    for (py = 0; py < Factor; py++) {

		/* get first pixel in scanline */
		pixelp = &buffer[(y+py)*scanlen+x*3];

		/* add pixels from scanline to average */
		for (px = 0; px < Factor; px++) {
		    p[0] += *pixelp++;
		    p[1] += *pixelp++;
		    p[2] += *pixelp++;
		}
	    }

	    /* store resultant pixel back in buffer */
	    for (py = 0; py < 3; ++py)
		*out_buf++ = p[py] / facsq;

	}
}


/*
 * Undersample image pixels
 */
void
usample_image(int scanlen, int Width, int Height, unsigned char *buffer, int Factor)
{
    int t, x, y;

    UCHAR *p;

    p = buffer;

    for (y=0; y < Height; y += Factor)
	for (x=0; x < Width; x += Factor, p += 3) {
	    t = x*3 + y*scanlen;
	    p[0] = buffer[t];
	    p[1] = buffer[t+1];
	    p[2] = buffer[t+2];
	}

}


int width = 512;
int height = 512;
int scanlen;
int factor = 2;

static const struct bu_arg_operand pixshrink_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PIX input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixshrink_schema = BU_ARG_SCHEMA(
    "pixshrink", "Shrink a PIX image by an integer factor",
    shrink_options, pixshrink_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixshrink_grammar = {
    &pixshrink_schema, NULL, NULL
};


/*
 * Call parse_args to handle command line arguments first, then
 * process input.
 */
int main(int ac, char **av)
{
    UCHAR *buffer = (UCHAR *)NULL;
    struct shrink_args args = {NULL, 0, 512, 512, 2, SHRINK_METHOD_BOXCAR};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    args.self = &args;

    bu_setprogname(av[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    int introspection = bu_arg_introspect(&msg, &pixshrink_grammar, ac - 1,
	(const char * const *)(av + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixshrink_schema, &args,
	&msg, ac - 1, (const char **)(av + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixshrink_schema, "pixshrink");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixshrink help");
	return 0;
    }
    if (ac - operand_index - 1 == 1) {
	char *ifname = bu_file_realpath(av[operand_index + 1], NULL);
	if (freopen(ifname, "rb", stdin) == NULL) {
	    perror(ifname);
	    bu_free(ifname, "pixshrink input path");
	    return 1;
	}
	bu_free(ifname, "pixshrink input path");
	filename = av[operand_index + 1];
    }
    if (isatty(fileno(stdout)))
	bu_exit(1, "pixshrink requires redirected standard output\n");
    if (isatty(fileno(stdin)))
	bu_exit(1, "pixshrink requires an input file or redirected input\n");

    width = args.width;
    height = args.height;
    factor = args.factor;

    /* process stdin */
    scanlen = width * 3;
    buffer = read_image(scanlen, width, height, buffer);

    switch (args.method) {
	case SHRINK_METHOD_BOXCAR : shrink_image(scanlen, width, height, buffer, factor); break;
	case SHRINK_METHOD_UNDERSAMPLE : usample_image(scanlen, width, height, buffer, factor); break;
	default: bu_free(buffer, "buffer alloc from malloc"); return -1;
    }

    write_image(width/factor, height/factor, buffer);
    bu_free(buffer, "pixshrink: buffer");
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
