/*                       M A C - P I X . C
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
/** @file util/mac-pix.c
 *
 * Read MacPaint document and output pix(5) or bw(5) raster image
 *
 * The Macintosh bitmap file has a 512 byte header which is
 * unimportant for this application, followed by a run-length-encoded
 * image.
 *
 * The image is stored as a sequence of bits, proceeding from the
 * upper right corner down to the lower right corner, and then
 * advancing left one column.  This unusual format
 * somewhat complicates matters here.
 *
 * Within each byte, bits are extracted MSB to LSB;  these bits go down
 * the page.
 *
 * Because two files are being processed, the "file_*" things refer
 * to the input file, and the "scr_*" things refer to the output file.
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
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"


#define MAC_HEIGHT 576	/* input height (y), in bits */
#define MAC_WIDTH 720	/* input width (x), in bits */

struct macpaint_hdr {
    long version;		/* 4 bytes */
    char patterns[38*8];		/* 304 bytes */
    char spare[204];
} hdr;

unsigned char pix[MAC_HEIGHT*MAC_WIDTH];

unsigned char color[3] = { 0xFF, 0xFF, 0xFF };
unsigned char black[3*2048];

int file_height = MAC_HEIGHT;	/* generally constant */
int file_width = MAC_WIDTH;
int file_xoff = 0;
int file_yoff = 0;
int scr_width = 1024;	/* If this and scr_height are later found to be zero,
			 * they assume the values of file_width and file_height .
			 */
int scr_height = 1024;
int scr_xoff = 0;
int scr_yoff = 0;

int bwflag = 0;
static const char *file_name;
FILE *infp;

struct mac_pix_args {
    struct mac_pix_args *self;
    int help;
    int bw;
};

enum mac_pix_dimension {
    MAC_PIX_INPUT_SIZE,
    MAC_PIX_INPUT_WIDTH,
    MAC_PIX_INPUT_HEIGHT,
    MAC_PIX_OUTPUT_SIZE,
    MAC_PIX_OUTPUT_WIDTH,
    MAC_PIX_OUTPUT_HEIGHT
};

enum mac_pix_offset {
    MAC_PIX_INPUT_X_OFFSET,
    MAC_PIX_INPUT_Y_OFFSET,
    MAC_PIX_OUTPUT_X_OFFSET,
    MAC_PIX_OUTPUT_Y_OFFSET
};

static struct mac_pix_args *
mac_pix_args_from_storage(void *storage)
{
    return storage ? *(struct mac_pix_args **)storage : NULL;
}

static int
parse_color_arg(const char *arg, unsigned char *out_color)
{
    int i;
    char *end = NULL;
    long int value;
    const char *component = arg;

    for (i = 0; i < 3; i++) {
	errno = 0;
	value = strtol(component, &end, 10);
	if (errno != 0 || end == component || value < 0 || value > 255)
	    return 0;
	out_color[i] = (unsigned char)value;

	if (i == 2) {
	    if (*end != '\0')
		return 0;
	} else {
	    if (*end != '/')
		return 0;
	    component = end + 1;
	}
    }

    return 1;
}

static int
mac_pix_center(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    if (mac_pix_args_from_storage(storage)) {
	scr_xoff = (scr_width - file_width) / 2;
	scr_yoff = (scr_height - file_height) / 2;
    }
    return 0;
}

static int
mac_pix_native_size(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    if (mac_pix_args_from_storage(storage))
	scr_height = scr_width = 0;
    return 0;
}

static int
mac_pix_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum mac_pix_dimension dimension)
{
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (!mac_pix_args_from_storage(storage))
	return 0;

    switch (dimension) {
	case MAC_PIX_INPUT_SIZE:
	    file_width = value > MAC_WIDTH ? MAC_WIDTH : value;
	    file_height = value > MAC_HEIGHT ? MAC_HEIGHT : value;
	    break;
	case MAC_PIX_INPUT_WIDTH:
	    file_width = value > MAC_WIDTH ? MAC_WIDTH : value;
	    break;
	case MAC_PIX_INPUT_HEIGHT:
	    file_height = value > MAC_HEIGHT ? MAC_HEIGHT : value;
	    break;
	case MAC_PIX_OUTPUT_SIZE:
	    scr_width = scr_height = value;
	    break;
	case MAC_PIX_OUTPUT_WIDTH:
	    scr_width = value;
	    break;
	case MAC_PIX_OUTPUT_HEIGHT:
	    scr_height = value;
	    break;
    }
    return 0;
}

#define MAC_PIX_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return mac_pix_dimension(msg, arg, storage, _dimension); \
    }

MAC_PIX_DIMENSION_CALLBACK(mac_pix_input_size, MAC_PIX_INPUT_SIZE)
MAC_PIX_DIMENSION_CALLBACK(mac_pix_input_width, MAC_PIX_INPUT_WIDTH)
MAC_PIX_DIMENSION_CALLBACK(mac_pix_input_height, MAC_PIX_INPUT_HEIGHT)
MAC_PIX_DIMENSION_CALLBACK(mac_pix_output_size, MAC_PIX_OUTPUT_SIZE)
MAC_PIX_DIMENSION_CALLBACK(mac_pix_output_width, MAC_PIX_OUTPUT_WIDTH)
MAC_PIX_DIMENSION_CALLBACK(mac_pix_output_height, MAC_PIX_OUTPUT_HEIGHT)

static int
mac_pix_offset(struct bu_vls *msg, const char *arg, void *storage,
	enum mac_pix_offset offset)
{
    int value;

    if (!bu_arg_integer_from_str(msg, &value, arg, 10))
	return -1;
    if (!mac_pix_args_from_storage(storage))
	return 0;

    switch (offset) {
	case MAC_PIX_INPUT_X_OFFSET:
	    file_xoff = (value < 0 || value >= MAC_WIDTH) ? 0 : value;
	    break;
	case MAC_PIX_INPUT_Y_OFFSET:
	    file_yoff = (value < 0 || value >= MAC_HEIGHT) ? 0 : value;
	    break;
	case MAC_PIX_OUTPUT_X_OFFSET:
	    if ((value > 0 && scr_xoff > INT_MAX - value) ||
		(value < 0 && scr_xoff < INT_MIN - value)) {
		if (msg)
		    bu_vls_printf(msg, "output horizontal offset is out of range");
		return -1;
	    }
	    scr_xoff += value;
	    break;
	case MAC_PIX_OUTPUT_Y_OFFSET:
	    if ((value > 0 && scr_yoff > INT_MAX - value) ||
		(value < 0 && scr_yoff < INT_MIN - value)) {
		if (msg)
		    bu_vls_printf(msg, "output vertical offset is out of range");
		return -1;
	    }
	    scr_yoff += value;
	    break;
    }
    return 0;
}

#define MAC_PIX_OFFSET_CALLBACK(_name, _offset) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return mac_pix_offset(msg, arg, storage, _offset); \
    }

MAC_PIX_OFFSET_CALLBACK(mac_pix_input_x_offset, MAC_PIX_INPUT_X_OFFSET)
MAC_PIX_OFFSET_CALLBACK(mac_pix_input_y_offset, MAC_PIX_INPUT_Y_OFFSET)
MAC_PIX_OFFSET_CALLBACK(mac_pix_output_x_offset, MAC_PIX_OUTPUT_X_OFFSET)
MAC_PIX_OFFSET_CALLBACK(mac_pix_output_y_offset, MAC_PIX_OUTPUT_Y_OFFSET)

static int
mac_pix_color(struct bu_vls *msg, const char *arg, void *storage)
{
    unsigned char parsed_color[3];

    if (!parse_color_arg(arg, parsed_color)) {
	if (msg)
	    bu_vls_printf(msg, "invalid RGB color '%s' (expected r/g/b)", arg);
	return -1;
    }
    if (mac_pix_args_from_storage(storage))
	memcpy(color, parsed_color, sizeof(color));
    return 0;
}

static const struct bu_arg_option mac_pix_options[] = {
    BU_ARG_FLAG("h", "help", struct mac_pix_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("c", "center", "center", struct mac_pix_args, self,
	mac_pix_center, "Center the input image in the output image"),
    BU_ARG_CUSTOM_FLAG("l", "native-size", "native-size",
	struct mac_pix_args, self, mac_pix_native_size,
	"Set the output dimensions from the input dimensions"),
    BU_ARG_FLAG("b", "bw", struct mac_pix_args, bw,
	"Write a one-byte BW image instead of a three-byte PIX image"),
    BU_ARG_CUSTOM("s", "input-size", struct mac_pix_args, self,
	mac_pix_input_size, "pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "input-width", struct mac_pix_args, self,
	mac_pix_input_width, "pixels", "Set the input width"),
    BU_ARG_CUSTOM("n", "input-height", struct mac_pix_args, self,
	mac_pix_input_height, "scanlines", "Set the input height"),
    BU_ARG_CUSTOM("x", "input-x-offset", struct mac_pix_args, self,
	mac_pix_input_x_offset, "pixels", "Set the input horizontal offset"),
    BU_ARG_CUSTOM("y", "input-y-offset", struct mac_pix_args, self,
	mac_pix_input_y_offset, "scanlines", "Set the input vertical offset"),
    BU_ARG_CUSTOM("X", "output-x-offset", struct mac_pix_args, self,
	mac_pix_output_x_offset, "pixels", "Add to the output horizontal offset"),
    BU_ARG_CUSTOM("Y", "output-y-offset", struct mac_pix_args, self,
	mac_pix_output_y_offset, "scanlines", "Add to the output vertical offset"),
    BU_ARG_CUSTOM("S", "output-size", struct mac_pix_args, self,
	mac_pix_output_size, "pixels", "Set both output dimensions"),
    BU_ARG_CUSTOM("W", "output-width", struct mac_pix_args, self,
	mac_pix_output_width, "pixels", "Set the output width"),
    BU_ARG_CUSTOM("N", "output-height", struct mac_pix_args, self,
	mac_pix_output_height, "scanlines", "Set the output height"),
    BU_ARG_CUSTOM("C", "color", struct mac_pix_args, self,
	mac_pix_color, "r/g/b", "Set the color of active pixels"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand mac_pix_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"MacPaint input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema mac_pix_schema = BU_ARG_SCHEMA(
    "mac-pix", "Convert a MacPaint bitmap to a PIX or BW image",
    mac_pix_options, mac_pix_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar mac_pix_grammar = {
    &mac_pix_schema, NULL, NULL
};

static int
get_args(int argc, char **argv, struct mac_pix_args *args, struct bu_vls *msg)
{
    int operand_index = bu_arg_schema_parse_complete(&mac_pix_schema, args,
	msg, argc - 1, (const char **)(argv + 1));

    if (operand_index < 0 || args->help)
	return operand_index >= 0;

    bwflag = args->bw;
    if (argc - operand_index - 1 == 0) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = "[stdin]";
	infp = stdin;
    } else {
	file_name = argv[operand_index + 1];
	if ((infp = fopen(file_name, "rb")) == NULL) {
	    bu_vls_printf(msg, "cannot open '%s' for reading: %s",
		file_name, strerror(errno));
	    return 0;
	}
    }
    return 1;
}


int
getbits(FILE *fp)
{
    static int count, rep, chr;
    int c;

    if (rep) {
	rep--;
	return chr;
    }
    if (count) {
	count--;
	return getc(fp);
    }
    c = getc(fp);
    if (c & 0x80) {
	/* repeated character count */
	rep = 0x100 - c;	/* byte length 2's comp + 1 */
	/* allow for this call */
	chr = getc(fp);		/* character to repeat */
	return chr;
    } else {
	count = c;		/* already counted this char */
	return getc(fp);
    }
}


int
main(int argc, char **argv)
{
    struct mac_pix_args args = {NULL, 0, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int c;
    int x, y;
    int x1, x2, x3;		/* x zone widths */
    int y1, y2, y3;		/* y zone widths */
    int first_x;		/* x: first pixel to be output in pix[] */
    int first_y;		/* y: first pixel to be output in pix[] */
    size_t ret;
    int introspection;

    args.self = &args;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    introspection = bu_arg_introspect(&msg, &mac_pix_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    bu_vls_trunc(&msg, 0);

    if (!get_args(argc, argv, &args, &msg)) {
	char *help = bu_arg_schema_help(&mac_pix_schema, "mac-pix");
	if (bu_vls_strlen(&msg))
	    bu_flog(stderr, "mac-pix: %s\n", bu_vls_cstr(&msg));
	bu_flog(stderr, "%s", help ? help : "");
	bu_free(help, "mac-pix help");
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&mac_pix_schema, "mac-pix");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "mac-pix help");
	return 0;
    }
    if (isatty(fileno(stdout))) {
	char *help = bu_arg_schema_help(&mac_pix_schema, "mac-pix");
	bu_flog(stderr, "mac-pix: standard output must be redirected\n");
	bu_flog(stderr, "%s", help ? help : "");
	bu_free(help, "mac-pix help");
	return 1;
    }

    /* If screen size was not set, track the file size */
    if (scr_width == 0)
	scr_width = file_width;
    if (scr_height == 0)
	scr_height = file_height;

    ret = fread((char *)&hdr, sizeof(hdr), 1, infp);
    if (ret == 0) {
	perror("fread");
	bu_exit (1, NULL);
    }

    /* x and y are in terms of 1st quadrant coordinates */
    /* Very first bit input is in upper right of screen */
    x = file_width-1;
    y = file_height-1;

    while ((c = getbits(infp)) != EOF) {
	int mask;
	unsigned char *cp;

	cp = &pix[(file_width*y)+x];

	for (mask = 0x80; mask; mask >>= 1) {
	    if (c & mask)
		*cp = 0xFF;
	    else
		*cp = 0;
	    cp -= file_width;
	}

	y -= 8;
	if (y < 0) {
	    y = file_height-1;
	    if (--x < 0)
		break;
	}
    }
    fclose(infp);

    /*
     * There are potentially 9 regions in the output file, only one
     * will contain actual image.  x1 is the width of the left border
     * for each scanline, x2 is the width of the image area,
     * with the first pixel being first_x, and x3 being the width
     * of the right border.
     * Computations in y are similar.
     */
    first_y = file_yoff;		/* always >= 0 */
    y1 = scr_yoff;
    if (y1 < 0) {
	y1 = 0;
	first_y += -scr_yoff;
    }
    y2 = file_height - first_y;
    if (y1 + y2 > scr_height)
	y2 = scr_height - y1;
    if (first_y >= file_height)
	y2 = 0;
    y3 = scr_height - y1 - y2;

    first_x = file_xoff;
    x1 = scr_xoff;
    if (x1 < 0) {
	x1 = 0;
	first_x += -scr_xoff;
    }
    x2 = file_width - first_x;
    if (x1 + x2 > scr_width)
	x2 = scr_width - x1;
    if (first_x >= file_width)
	x2 = 0;
    x3 = scr_width - x1 - x2;

    if (bwflag) {
	for (y = 0; y < y1; y++) {
	    ret = fwrite(black, scr_width, 1, stdout);
	    if (ret == 0) {
		perror("fwrite");
		break;
	    }
	}
	for (y = 0; y < y2; y++) {
	    ret = fwrite(black, x1, 1, stdout);
	    if (ret == 0) {
		perror("fwrite");
		break;
	    }
	    ret = fwrite(&pix[(file_width*(y+first_y))+first_x], x2, 1, stdout);
	    if (ret == 0) {
		perror("fwrite");
		break;
	    }

	    ret = fwrite(black, x3, 1, stdout);
	    if (ret == 0) {
		perror("fwrite");
		break;
	    }
	}
	for (y = 0; y < y3; y++) {
	    ret = fwrite(black, scr_width, 1, stdout);
	    if (ret == 0) {
		perror("fwrite");
		break;
	    }
	}
    } else {
	for (y = 0; y < y1; y++) {
	    ret = fwrite(black, scr_width, 3, stdout);
	    if (ret == 0) {
		perror("fwrite");
		break;
	    }
	}
	for (y = 0; y < y2; y++) {
	    unsigned char *cp;

	    ret = fwrite(black, x1, 3, stdout);
	    if (ret == 0) {
		perror("fwrite");
		break;
	    }
	    cp = &pix[(file_width*(y+first_y))+first_x];
	    for (x = 0; x < x2; x++) {
		if (*cp++)
		    ret = fwrite(color, 3, 1, stdout);
		else
		    ret = fwrite(black, 3, 1, stdout);

		if (ret == 0) {
		    perror("fwrite");
		    break;
		}
	    }
	    ret = fwrite(black, x3, 3, stdout);
	    if (ret == 0) {
		perror("fwrite");
		break;
	    }
	}
	for (y = 0; y < y3; y++) {
	    ret = fwrite(black, scr_width, 3, stdout);
	    if (ret == 0) {
		perror("fwrite");
		break;
	    }
	}
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
