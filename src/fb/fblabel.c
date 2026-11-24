/*                       F B L A B E L . C
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
 *
 */
/** @file fblabel.c
 *
 * Draw a Label on a Frame buffer image.
 *
 */

#include "common.h"

#include <limits.h>
#include <string.h>

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vfont.h"
#include "dm.h"


#define FONTBUFSZ 200

static const char *framebuffer = NULL;
static char *font1 = NULL;

struct fb *fbp;

/* Variables controlling the font itself */

int width = 0;	/* Size of current character.		*/
int height = 0;

static int filterbuf[FONTBUFSZ][FONTBUFSZ];

static int scr_width = 0;	/* default input width */
static int scr_height = 0;	/* default input height */
static int clear = 0;

static RGBpixel pixcolor;
static int xpos;
static int ypos;
static const char *textstring;
static int debug = 0;
static int alias_off = 0;

struct fblabel_args {
    struct fblabel_args *self;
    int help;
    int alias_off;
    int clear;
    int debug;
    const char *framebuffer;
    const char *font;
    struct bu_color color;
    int width;
    int height;
};

enum fblabel_dimension {
    FBLABEL_SIZE,
    FBLABEL_WIDTH,
    FBLABEL_HEIGHT
};

static struct fblabel_args *
fblabel_args_from_storage(void *storage)
{
    return storage ? *(struct fblabel_args **)storage : NULL;
}

static int
fblabel_dimension_parse(struct bu_vls *msg, const char *arg, void *storage,
	enum fblabel_dimension dimension)
{
    struct fblabel_args *args = fblabel_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (dimension != FBLABEL_HEIGHT)
	    args->width = value;
	if (dimension != FBLABEL_WIDTH)
	    args->height = value;
    }
    return 0;
}

#define FBLABEL_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return fblabel_dimension_parse(msg, arg, storage, _dimension); \
    }

FBLABEL_DIMENSION_CALLBACK(fblabel_size, FBLABEL_SIZE)
FBLABEL_DIMENSION_CALLBACK(fblabel_width, FBLABEL_WIDTH)
FBLABEL_DIMENSION_CALLBACK(fblabel_height, FBLABEL_HEIGHT)

static int
fblabel_color_channel_parse(struct bu_vls *msg, const char *arg, void *storage,
	int channel)
{
    struct fblabel_args *args = fblabel_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 0, 255))
	return -1;
    if (args) {
	unsigned char rgb[3];
	if (!bu_color_to_rgb_chars(&args->color, rgb))
	    return -1;
	rgb[channel] = (unsigned char)value;
	if (!bu_color_from_rgb_chars(&args->color, rgb))
	    return -1;
    }
    return 0;
}

#define FBLABEL_CHANNEL_CALLBACK(_name, _channel) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return fblabel_color_channel_parse(msg, arg, storage, _channel); \
    }

FBLABEL_CHANNEL_CALLBACK(fblabel_red, RED)
FBLABEL_CHANNEL_CALLBACK(fblabel_green, GRN)
FBLABEL_CHANNEL_CALLBACK(fblabel_blue, BLU)

static int
fblabel_help_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    if (arg && !BU_STR_EQUAL(arg, "all")) {
	if (msg)
	    bu_vls_strcat(msg, "--help accepts only the optional value 'all'\n");
	return -1;
    }
    if (storage)
	*((int *)storage) = arg ? 2 : 1;
    return 0;
}

static const struct bu_arg_option fblabel_options[] = {
    BU_ARG_ATTACHED_OPTIONAL("h", "help", "help", struct fblabel_args,
	help, fblabel_help_parse, "all",
	"Print help and exit; use --help=all for compatibility options"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("a", "no-antialias", struct fblabel_args, alias_off,
	"Render text without antialiasing"),
    BU_ARG_FLAG("c", "clear", struct fblabel_args, clear,
	"Clear the framebuffer before drawing"),
    BU_ARG_FLAG("d", "debug", struct fblabel_args, debug,
	"Print character layout diagnostics"),
    BU_ARG_VALUE("F", "framebuffer", struct fblabel_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer device"),
    BU_ARG_VALUE("f", "font", struct fblabel_args, font,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "name",
	"Select a vfont font"),
    BU_ARG_RGB("C", "color", struct fblabel_args, color, "r/g/b",
	"Set the text color (default 255/255/255)"),
    BU_ARG_CUSTOM("r", "red", struct fblabel_args, self, fblabel_red,
	"value", "Override the red text-color channel"),
    BU_ARG_CUSTOM("g", "green", struct fblabel_args, self, fblabel_green,
	"value", "Override the green text-color channel"),
    BU_ARG_CUSTOM("b", "blue", struct fblabel_args, self, fblabel_blue,
	"value", "Override the blue text-color channel"),
    BU_ARG_CUSTOM("s", "size", struct fblabel_args, self, fblabel_size,
	"pixels", "Set both framebuffer dimensions"),
    BU_ARG_ALIAS("S", NULL, "size", 1),
    BU_ARG_CUSTOM("w", "width", struct fblabel_args, self, fblabel_width,
	"pixels", "Set the framebuffer width"),
    BU_ARG_ALIAS("W", NULL, "width", 1),
    BU_ARG_CUSTOM("n", "height", struct fblabel_args, self, fblabel_height,
	"pixels", "Set the framebuffer height"),
    BU_ARG_ALIAS("N", NULL, "height", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand fblabel_operands[] = {
    BU_ARG_OPERAND("x-position", BU_ARG_VALUE_INTEGER, 1, 1,
	"Text origin x coordinate", NULL),
    BU_ARG_OPERAND("y-position", BU_ARG_VALUE_INTEGER, 1, 1,
	"Text origin y coordinate", NULL),
    BU_ARG_OPERAND("text", BU_ARG_VALUE_STRING, 1, 1,
	"Text to draw", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_option_visibility fblabel_option_visibility[] = {
    BU_ARG_OPTION_VISIBILITY("red", BU_ARG_VISIBILITY_ADVANCED),
    BU_ARG_OPTION_VISIBILITY("green", BU_ARG_VISIBILITY_ADVANCED),
    BU_ARG_OPTION_VISIBILITY("blue", BU_ARG_VISIBILITY_ADVANCED),
    BU_ARG_OPTION_VISIBILITY_NULL
};

static const struct bu_arg_presentation fblabel_presentation =
    BU_ARG_PRESENTATION(NULL, fblabel_option_visibility, NULL);

static const struct bu_arg_schema fblabel_schema = BU_ARG_SCHEMA_PRESENTED(
    "fblabel", "Draw text on a framebuffer", fblabel_options,
    fblabel_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL),
    &fblabel_presentation);

static const struct bu_arg_grammar fblabel_grammar = {
    &fblabel_schema, NULL, NULL
};

static int
fblabel_help(FILE *stream, int extended, int status)
{
    char *help = bu_arg_schema_help_mode(&fblabel_schema, "fblabel",
	extended ? BU_ARG_HELP_EXTENDED : BU_ARG_HELP_DEFAULT);
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "fblabel help");
    return status;
}

/*
 * squash - Filter super-sampled image for one scan line
 */

/* Cone filtering weights.
 * #define CNTR_WT 0.23971778
 * #define MID_WT 0.11985889
 * #define CRNR_WT 0.07021166
 */

/* Gaussian filtering weights. */
#define CNTR_WT 0.3011592441
#define MID_WT 0.1238102667
#define CRNR_WT 0.0508999223

/* Squash takes three super-sampled "bit arrays", and returns an array
   of intensities at half the resolution.  N is the size of the bit
   arrays.  The "bit arrays" are actually int arrays whose values are
   assumed to be only 0 or 1.
*/
void
squash(int *buf0, int *buf1, int *buf2, float *ret_buf, int n)
{
    int j;

    for (j = 1; j < n - 1; j++) {
	if (alias_off) {
	    ret_buf[j] = buf1[j];
	} else {
	    ret_buf[j] =
		(
		    buf2[j - 1] * CRNR_WT +
		    buf2[j] * MID_WT +
		    buf2[j + 1] * CRNR_WT +
		    buf1[j - 1] * MID_WT +
		    buf1[j] * CNTR_WT +
		    buf1[j + 1] * MID_WT +
		    buf0[j - 1] * CRNR_WT +
		    buf0[j] * MID_WT +
		    buf0[j + 1] * CRNR_WT
		    );
	}
    }
}


/*
   Extract a bit field from a bit string.
*/
int
bitx(char *bitstring, int posn)
{
    for (; posn >= 8; posn -= 8, bitstring++);

#if defined(CANT_DO_ZERO_SHIFT)
    if (posn == 0)
	return (int) (*bitstring) & 1;
#endif

    return (int) (*bitstring) & (1 << posn);
}


/*
 Fills in the buffer by reading a row of a bitmap from the character
 font file.  The file pointer is assumed to be in the correct
 position.
*/
void
fill_buf(int wid, int *buf, char *bitrow)
{
    int j;

    /*
     * For each bit in the row, set the array value to 1 if it's
     * on. The bitx routine extracts the bit value.  Can't just use
     * the j-th bit because the bytes are backwards.
     */
    for (j = 0; j < wid; j++)
	if (bitx(bitrow, (j & ~7) + (7 - (j & 7))))
	    buf[j + 2] = 1;
	else
	    buf[j + 2] = 0;

    /*
     * Need two samples worth of background on either end to make the
     * filtering come out right without special casing the filtering.
     */
    buf[0] = buf[1] = buf[wid + 2] = buf[wid + 3] = 0;
}


void
do_char(struct vfont *vfp, struct vfont_dispatch *vdp, int x, int y)
{
    int i, j;
    int base;
    int totwid = width;
    int ln;
    static float resbuf[FONTBUFSZ];
    static RGBpixel fbline[FONTBUFSZ];
    int bytes_wide;	/* # bytes/row in bitmap */

    bytes_wide = (width+7)>>3;

    /* Read in the character bit map, with two blank lines on each end. */
    for (i = 0; i < 2; i++)
	memset((char *)&filterbuf[i][0], 0, (totwid+4)*sizeof(int));

    for (ln=0, i = height + 1; i >= 2; i--, ln++)
	fill_buf (width, &filterbuf[i][0],
		  &vfp->vf_bits[vdp->vd_addr + bytes_wide*ln]);

    for (i = height + 2; i < height + 4; i++)
	memset((char *)&filterbuf[i][0], 0, (totwid+4)*sizeof(int));

    /* Initial base line for filtering depends on odd flag. */
    if (vdp->vd_down % 2)
	base = 1;
    else
	base = 2;

    /* Produce a RGBpixel buffer from a description of the character
     * and the read back data from the frame buffer for anti-aliasing.
     */
    for (i = height + base; i >= base; i--) {
	squash(filterbuf[i - 1],	/* filter info */
	       filterbuf[i],
	       filterbuf[i + 1],
	       resbuf,
	       totwid + 4
	    );
	fb_read(fbp, x, y - vdp->vd_down + i, (unsigned char *)fbline, totwid+3);
	for (j = 0; j < (totwid + 3) - 1; j++) {
	    int tmp;
	    /* EDITOR'S NOTE : do not rearrange this code, the SUN
	     * compiler can't handle more complex expressions.
	     */

	    tmp = fbline[j][RED] & 0377;
	    fbline[j][RED] =
		(int)(pixcolor[RED]*resbuf[j]+(1-resbuf[j])*tmp);
	    fbline[j][RED] &= 0377;
	    tmp = fbline[j][GRN] & 0377;
	    fbline[j][GRN] =
		(int)(pixcolor[GRN]*resbuf[j]+(1-resbuf[j])*tmp);
	    fbline[j][GRN] &= 0377;
	    tmp = fbline[j][BLU] & 0377;
	    fbline[j][BLU] =
		(int)(pixcolor[BLU]*resbuf[j]+(1-resbuf[j])*tmp);
	    fbline[j][BLU] &= 0377;
	}
	if (fb_write(fbp, x, y-vdp->vd_down+i, (unsigned char *)fbline, totwid+3) < totwid+3) {
	    fprintf(stderr, "fblabel: pixel write error\n");
	    bu_exit(1, NULL);
	}
    }
}


void
do_line(struct vfont *vfp, const char *line)
{
    int currx;
    int char_id;
    size_t char_count;
    size_t len = strlen(line);

    if (vfp == VFONT_NULL) return;

    currx = xpos;

    for (char_count = 0; char_count < len; char_count++) {
	struct vfont_dispatch *vdp;

	char_id = (int) line[char_count] & 0377;

	/* Obtain the dimensions for the character */
	vdp = &vfp->vf_dispatch[char_id];
	width = vdp->vd_left + vdp->vd_right;
	height = vdp->vd_up + vdp->vd_down;
	if (debug) fprintf(stderr, "%c w=%2d h=%2d, currx=%d\n", char_id, width, height, currx);

	/*
	 * pace characters are frequently not represented in the font
	 * set, so leave white space here.
	 */
	if (width <= 1) {
	    char_id = 'n';	/* 1-en space */
	    vdp = &vfp->vf_dispatch[char_id];
	    width = vdp->vd_left + vdp->vd_right;
	    if (width <= 1) {
		char_id = 'N';	/* 1-en space */
		vdp = &vfp->vf_dispatch[char_id];
		width = vdp->vd_left + vdp->vd_right;
		if (width <= 1)
		    width = 16;	/* punt */
	    }
	    currx += width;
	    continue;
	}

	if (currx + width > fb_getwidth(fbp) - 1) {
	    fprintf(stderr, "fblabel:  Ran off screen\n");
	    break;
	}

	do_char(vfp, vdp, currx, ypos);
	currx += vdp->vd_width + 2;
    }
}


int
main(int argc, char **argv)
{
    struct fblabel_args args = {.color = BU_COLOR_WHITE};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct vfont *vfp;
    int operand_index;
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &fblabel_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&fblabel_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return fblabel_help(stdout, args.help > 1, 0);

    framebuffer = args.framebuffer;
    font1 = (char *)args.font;
    scr_width = args.width;
    scr_height = args.height;
    clear = args.clear;
    debug = args.debug;
    alias_off = args.alias_off;
    if (!bu_color_to_rgb_chars(&args.color, pixcolor))
	bu_exit(1, "fblabel: invalid text color\n");
    if (!bu_arg_integer_from_str(NULL, &xpos, argv[operand_index + 1], 10) ||
	!bu_arg_integer_from_str(NULL, &ypos, argv[operand_index + 2], 10))
	bu_exit(1, "fblabel: internal coordinate validation failure\n");
    textstring = argv[operand_index + 3];

    if (debug)
	bu_flog(stderr, "fblabel %d %d %s\n", xpos, ypos, textstring);

    if ((fbp = fb_open(framebuffer, scr_width, scr_height)) == NULL) {
	fprintf(stderr, "fblabel:  Unable to open framebuffer %s\n", framebuffer);
	bu_exit(12, NULL);
    }

    if (clear) {
	fb_clear(fbp, PIXEL_NULL);
    }

    if ((vfp = vfont_get(font1)) == VFONT_NULL) {
	fprintf(stderr, "fblabel:  Can't get font \"%s\"\n",
		font1 == NULL ? "(null)" : font1);
	bu_exit(1, NULL);
    }

    do_line(vfp, textstring);

    fb_close(fbp);
    vfont_free(vfp);

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
