/*                       F B P O I N T . C
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
/** @file fbpoint.c
 *
 * Tool to identify X, Y coordinates on the screen.
 *
 */

#include "common.h"

#include <limits.h>
#include <string.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "dm.h"
#define LIBTERMIO_IMPLEMENTATION
#include "libtermio.h"


struct fb *fbp;

int JumpSpeed;		/* # pixels skipped with fast commands. */

RGBpixel curPix; 		/* Current pixel value */
int curX, curY;		/* current position */
int oldX, oldY;		/* previous position */

int Run = 1;		/* Tells when to stop the main loop */

void SimpleInput(void);

struct fbpoint_axis_output {
    int requested;
    const char *prefix;
};

struct fbpoint_args {
    struct fbpoint_args *self;
    int help;
    const char *framebuffer;
    int width;
    int height;
    struct fbpoint_axis_output x_output;
    struct fbpoint_axis_output y_output;
};

enum fbpoint_dimension {
    FBPOINT_SIZE,
    FBPOINT_WIDTH,
    FBPOINT_HEIGHT
};

static struct fbpoint_args *
fbpoint_args_from_storage(void *storage)
{
    return storage ? *(struct fbpoint_args **)storage : NULL;
}

static int
fbpoint_dimension_value(struct bu_vls *msg, const char *arg, void *storage,
	enum fbpoint_dimension dimension)
{
    struct fbpoint_args *args = fbpoint_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (dimension == FBPOINT_SIZE)
	    args->width = args->height = value;
	else if (dimension == FBPOINT_WIDTH)
	    args->width = value;
	else
	    args->height = value;
    }
    return 0;
}

#define FBPOINT_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return fbpoint_dimension_value(msg, arg, storage, _dimension); \
    }

FBPOINT_DIMENSION_CALLBACK(fbpoint_size, FBPOINT_SIZE)
FBPOINT_DIMENSION_CALLBACK(fbpoint_width, FBPOINT_WIDTH)
FBPOINT_DIMENSION_CALLBACK(fbpoint_height, FBPOINT_HEIGHT)

static int
fbpoint_axis_output(struct bu_vls *UNUSED(msg), const char *arg, void *storage)
{
    struct fbpoint_axis_output *output =
	(struct fbpoint_axis_output *)storage;

    if (output) {
	output->requested++;
	if (arg && arg[0])
	    output->prefix = arg;
    }
    return 0;
}

static const struct bu_arg_option fbpoint_options[] = {
    BU_ARG_FLAG("h", "help", struct fbpoint_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("F", "framebuffer", struct fbpoint_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer instead of the FB_FILE environment default"),
    BU_ARG_CUSTOM("s", "size", struct fbpoint_args, self, fbpoint_size,
	"pixels", "Set both framebuffer dimensions (default 512)"),
    BU_ARG_ALIAS("S", NULL, "size", 1),
    BU_ARG_CUSTOM("w", "width", struct fbpoint_args, self, fbpoint_width,
	"pixels", "Set the framebuffer width (default 512)"),
    BU_ARG_ALIAS("W", NULL, "width", 1),
    BU_ARG_CUSTOM("n", "height", struct fbpoint_args, self, fbpoint_height,
	"scanlines", "Set the framebuffer height (default 512)"),
    BU_ARG_ALIAS("N", NULL, "height", 1),
    BU_ARG_ATTACHED_OPTIONAL("x", "x-coordinate", "x-coordinate",
	struct fbpoint_args, x_output, fbpoint_axis_output, "prefix",
	"Write only the x coordinate, optionally preceded by an attached prefix"),
    BU_ARG_ATTACHED_OPTIONAL("y", "y-coordinate", "y-coordinate",
	struct fbpoint_args, y_output, fbpoint_axis_output, "prefix",
	"Write only the y coordinate, optionally preceded by an attached prefix"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand fbpoint_operands[] = {
    BU_ARG_OPERAND("initial-x", BU_ARG_VALUE_INTEGER, 0, 1,
	"Initial x coordinate; the framebuffer center is the default", NULL),
    BU_ARG_OPERAND("initial-y", BU_ARG_VALUE_INTEGER, 0, 1,
	"Initial y coordinate; the framebuffer center is the default", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema fbpoint_schema = BU_ARG_SCHEMA(
    "fbpoint", "Point at and identify framebuffer coordinates",
    fbpoint_options, fbpoint_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar fbpoint_grammar = {
    &fbpoint_schema, NULL, NULL
};

static int
fbpoint_help(FILE *stream, int status)
{
    char *schema_help = bu_arg_schema_help(&fbpoint_schema, "fbpoint");
    bu_flog(stream, "%s", schema_help ? schema_help : "");
    bu_free(schema_help, "fbpoint help");
    return status;
}

static const char *help = "\
Char:   Command:                                                \r\n\
h B	Left (1)\r\n\
j N	Down (1)\r\n\
k P	Up (1)\r\n\
l F	Right (1)\r\n\
H ^B	Left (many)\r\n\
J ^N	Down (many)\r\n\
K ^P	Up (many)\r\n\
L ^F	Right (many)\r\n\
q Q cr	QUIT\r\n\
";

#define ctl(x)	(x&037)

void
SimpleInput(void)	/* ==== get keyboard input.	*/
{
    char ch;
    static char c;

    if (read(0, &c, 1) <= 0) {
	Run = 0;
	return;
    }
    ch = c & ~0x80;		/* strip off parity bit */
    switch (ch) {
	default:
	    fprintf(stderr,
		    "Unknown command(%c:0%o). Type '?' for help!           \r\n",
		    ch, ch);
	    break;

	case '?':
	    fprintf(stderr, "%s", help);
	    return;

	case 'q':
	case 'Q':
	case '\r':
	    Run = 0;
	    return;

	case 'B':
	case 'h':
	    --curX;		/* Go left.	*/
	    return;
	case 'N':
	case 'j':
	    --curY;		/* Go down.	*/
	    return;
	case 'P':
	case 'k':
	    ++curY;		/* Go up.	*/
	    return;
	case 'F':
	case 'l':
	    ++curX;		/* Go right.	*/
	    return;
	case ctl('b'):
	case 'H':
	    curX -= JumpSpeed;	/* Go LEFT.	*/
	    return;
	case ctl('n'):
	case 'J':
	    curY -= JumpSpeed;	/* Go DOWN.	*/
	    return;
	case ctl('p'):
	case 'K':
	    curY += JumpSpeed;	/* Go UP.	*/
	    return;
	case ctl('f'):
	case 'L':
	    curX += JumpSpeed;	/* Go RIGHT.	*/
	    return;
    }
}


int
main(int argc, char **argv)
{
    struct fbpoint_args args = {NULL, 0, NULL, 512, 512,
	{0, NULL}, {0, NULL}};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int operand_index;
    int operand_count;
    args.self = &args;

    bu_setprogname(argv[0]);

    setbuf(stderr, (char *)bu_malloc(BUFSIZ, "fbpoint stderr buffer"));
    curX = curY = -1;
    int introspection = bu_arg_introspect(&msg, &fbpoint_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&fbpoint_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return fbpoint_help(stdout, 0);
    operand_count = argc - 1 - operand_index;
    if (operand_count > 0 &&
	!bu_arg_integer_from_str(NULL, &curX, argv[operand_index + 1], 10))
	bu_exit(1, "fbpoint: internal initial-x validation mismatch\n");
    if (operand_count > 1 &&
	!bu_arg_integer_from_str(NULL, &curY, argv[operand_index + 2], 10))
	bu_exit(1, "fbpoint: internal initial-y validation mismatch\n");

    if ((fbp = fb_open(args.framebuffer, args.width, args.height)) == NULL)
	bu_exit(12, "Unable to open framebuffer\n");

    JumpSpeed = fb_getwidth(fbp)/16;
    if (JumpSpeed < 2) JumpSpeed = 2;
    /* check for default starting positions */
    if (curX < 0)
	curX = fb_getwidth(fbp)/2;
    if (curY < 0)
	curY = fb_getheight(fbp)/2;
    oldX = oldY = -1;

    /* Set RAW mode */
    save_Tty(0);
    set_Raw(0);
    clr_Echo(0);

    while (Run) {
	CLAMP(curX, 0, fb_getwidth(fbp)-1);
	CLAMP(curY, 0, fb_getheight(fbp)-1);

	if (oldX != curX || oldY != curY) {
	    /* get pixel value, move cursor */
	    fb_read(fbp, curX, curY, curPix, 1);
	    fb_cursor(fbp, 1, curX, curY);
	    oldX = curX;
	    oldY = curY;
	}
	fprintf(stderr, "xy=(%4d, %4d)  [%3d, %3d, %3d]      \r",
		curX, curY, curPix[RED], curPix[GRN], curPix[BLU]);
	fflush(stderr);

	SimpleInput();			/* read and do keyboard */
    }

    fb_cursor(fbp, 0, curX, curY);	/* turn off */

    fprintf(stderr, "\n");
    fflush(stderr);

    reset_Tty(0);

    /* write final location on stdout */
    if (args.x_output.requested && !args.y_output.requested)
	printf("%s%d\n", args.x_output.prefix ? args.x_output.prefix : "", curX);
    else if (args.y_output.requested && !args.x_output.requested)
	printf("%s%d\n", args.y_output.prefix ? args.y_output.prefix : "", curY);
    else
	printf("%s%d %s%d\n",
	    args.x_output.prefix ? args.x_output.prefix : "", curX,
	    args.y_output.prefix ? args.y_output.prefix : "", curY);

    fb_close(fbp);
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
