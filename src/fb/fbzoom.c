/*                        F B Z O O M . C
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
/** @file fbzoom.c
 *
 * Dynamically modify Ikonas Zoom and Window parameters, using VI and/or
 * EMACS-like keystrokes on a regular terminal.
 *
 */

#include "common.h"

#include <limits.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "vmath.h"

#include "dm.h"
#define LIBTERMIO_IMPLEMENTATION
#include "libtermio.h"


#define COMMA ','


int doKeyPad(void);

/* Zoom rate and limits */
#define MinZoom (1)

/* Pan limits */
#define MaxXPan (fb_getwidth(fbp)-1)
#define MaxYPan (fb_getheight(fbp)-1)
#define MinPan (0)

static int PanFactor;			/* Speed with which to pan.	*/
static int xPan, yPan;			/* Pan Location.		*/
static int xZoom, yZoom;		/* Zoom Factor.			*/
static int new_xPan, new_yPan;
static int new_xZoom, new_yZoom;

static int scr_width = 512;		/* screen size */
static int scr_height = 512;
static int toggle_pan = 0;		/* Reverse sense of pan commands? */
static const char *framebuffer = NULL;
static struct fb *fbp;

struct fbzoom_dimensions {
    int width;
    int height;
};

struct fbzoom_args {
    int help;
    int reverse_pan;
    const char *framebuffer;
    struct fbzoom_dimensions dimensions;
};

static int
fbzoom_dimension(struct bu_vls *msg, const char *arg, void *storage,
	int set_width, int set_height)
{
    struct fbzoom_dimensions *dimensions =
	(struct fbzoom_dimensions *)storage;
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (dimensions) {
	if (set_width)
	    dimensions->width = value;
	if (set_height)
	    dimensions->height = value;
    }
    return 0;
}

#define FBZOOM_DIMENSION_CALLBACK(_name, _width, _height) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return fbzoom_dimension(msg, arg, storage, _width, _height); \
    }

FBZOOM_DIMENSION_CALLBACK(fbzoom_size, 1, 1)
FBZOOM_DIMENSION_CALLBACK(fbzoom_width, 1, 0)
FBZOOM_DIMENSION_CALLBACK(fbzoom_height, 0, 1)

static const struct bu_arg_option fbzoom_options[] = {
    BU_ARG_FLAG("h", "help", struct fbzoom_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("T", "reverse-pan", struct fbzoom_args, reverse_pan,
	"Make panning keys move the image instead of the window"),
    BU_ARG_VALUE("F", "framebuffer", struct fbzoom_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer instead of the FB_FILE environment default"),
    BU_ARG_CUSTOM("s", "size", struct fbzoom_args, dimensions, fbzoom_size,
	"pixels", "Set both framebuffer dimensions (default 512)"),
    BU_ARG_ALIAS("S", NULL, "size", 1),
    BU_ARG_CUSTOM("w", "width", struct fbzoom_args, dimensions,
	fbzoom_width, "pixels", "Set the framebuffer width (default 512)"),
    BU_ARG_ALIAS("W", NULL, "width", 1),
    BU_ARG_CUSTOM("n", "height", struct fbzoom_args, dimensions,
	fbzoom_height, "scanlines", "Set the framebuffer height (default 512)"),
    BU_ARG_ALIAS("N", NULL, "height", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand fbzoom_view_roles[] = {
    BU_ARG_OPERAND("x-pan", BU_ARG_VALUE_INTEGER, 1, 1,
	"Initial horizontal center coordinate", NULL),
    BU_ARG_OPERAND("y-pan", BU_ARG_VALUE_INTEGER, 1, 1,
	"Initial vertical center coordinate", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("x-zoom", 1, 1, 1, 1, 0, 0,
	"Initial positive horizontal zoom factor", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("y-zoom", 1, 1, 1, 1, 0, 0,
	"Initial positive vertical zoom factor", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_operand_group fbzoom_operand_groups[] = {
    BU_ARG_OPERAND_GROUP("initial-view", fbzoom_view_roles, 0, 1,
	"Optional initial center and zoom values"),
    BU_ARG_OPERAND_GROUP_NULL
};

static const struct bu_arg_schema fbzoom_schema = BU_ARG_SCHEMA(
    "fbzoom", "Interactively zoom and pan a framebuffer", fbzoom_options,
    NULL, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(fbzoom_operand_groups, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar fbzoom_grammar = {
    &fbzoom_schema, NULL, NULL
};

static int
fbzoom_help(FILE *stream, int status)
{
    char *schema_help = bu_arg_schema_help(&fbzoom_schema, "fbzoom");
    bu_flog(stream, "%s", schema_help ? schema_help : "");
    bu_free(schema_help, "fbzoom help");
    return status;
}

int
main(int argc, char **argv)
{
    struct fbzoom_args args = {0, 0, NULL, {512, 512}};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int operand_index;
    int remaining;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &fbzoom_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&fbzoom_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return fbzoom_help(stdout, 0);
    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout)))
	return fbzoom_help(stderr, 1);

    remaining = argc - 1 - operand_index;
    if (remaining == 4) {
	if (!bu_arg_integer_from_str(NULL, &xPan, argv[operand_index + 1], 10) ||
	    !bu_arg_integer_from_str(NULL, &yPan, argv[operand_index + 2], 10) ||
	    !bu_arg_integer_from_str(NULL, &xZoom, argv[operand_index + 3], 10) ||
	    !bu_arg_integer_from_str(NULL, &yZoom, argv[operand_index + 4], 10))
	    bu_exit(1, "fbzoom: internal initial-view validation mismatch\n");
    }

    toggle_pan = args.reverse_pan;
    framebuffer = args.framebuffer;
    scr_width = args.dimensions.width;
    scr_height = args.dimensions.height;

    if ((fbp = fb_open(framebuffer, scr_width, scr_height)) == NULL)
	bu_exit(1, NULL);

    if (remaining == 4) {
	fb_view(fbp, xPan, yPan, xZoom, yZoom);
    }

    fb_getview(fbp, &xPan, &yPan, &xZoom, &yZoom);

    /* Set RAW mode */
    save_Tty(0);
    set_Raw(0);
    clr_Echo(0);

    PanFactor = fb_getwidth(fbp)/16;
    V_MAX(PanFactor, 2);

    new_xPan = xPan;
    new_yPan = yPan;
    new_xZoom = xZoom;
    new_yZoom = yZoom;
    do {
	/* Clip values against Min/Max */
	CLAMP(new_xPan, MinPan, MaxXPan);
	CLAMP(new_yPan, MinPan, MaxYPan);
	V_MAX(new_xZoom, MinZoom);
	V_MAX(new_yZoom, MinZoom);

	if (new_xPan != xPan || new_yPan != yPan
	    || new_xZoom != xZoom || new_yZoom != yZoom) {
	    /* values have changed, write them */
	    if (fb_view(fbp, new_xPan, new_yPan,
			new_xZoom, new_yZoom) >= 0) {
		/* good values, save them */
		xPan = new_xPan;
		yPan = new_yPan;
		xZoom = new_xZoom;
		yZoom = new_yZoom;
	    } else {
		/* bad values, reset to old ones */
		new_xPan = xPan;
		new_yPan = yPan;
		new_xZoom = xZoom;
		new_yZoom = yZoom;
	    }
	}
	(void) fprintf(stdout,
		       "Center Pixel: %4d %4d   Zoom: %2d %2d   %s\r",
		       xPan, yPan, xZoom, yZoom,
		       toggle_pan ? "Pan: image "
		       : "Pan: window");
	(void) fflush(stdout);
    }  while (doKeyPad());

    reset_Tty(0);
    (void) fb_view(fbp, xPan, yPan, xZoom, yZoom);
    (void) fb_close(fbp);
    (void) fprintf(stdout,  "\n");	/* Move off of the output line.	*/
    return 0;
}


#define ctl(x)	(x&037)

static void
printUsage(void)
{
    bu_log("\n\nBoth VI and EMACS motions work.\r\n");
    bu_log("b ^V	zoom Bigger (*2) s	zoom Smaller (*0.5)\n");
    bu_log("+ =	zoom Bigger (+1)		-	zoom Smaller (-1)\n");
    bu_log("(zoom Y Bigger (*2)) zoom Y Smaller (*0.5)\n");
    bu_log("0	zoom Y Bigger (+1)		9	zoom Y Smaller (-1)\n");
    bu_log("<	zoom X Bigger (*2)		>	zoom X Smaller (*0.5)\n");
    bu_log(",	zoom X Bigger (+1)		.	zoom X Smaller (-1)\n");
    bu_log("h B 	pan Left (1) l F	pan Right (1)\n");
    bu_log("H ^B	pan Left (many) L ^F	pan Right (many)\n");
    bu_log("k P	pan Up (1) j N	pan Down (1)\n");
    bu_log("K ^P	pan Up (many) J ^N	pan Down (many)\n");
    bu_log("T	toggle sense of pan commands\n");
    bu_log("c	goto Center\n");
    bu_log("z	zoom 1 1\n");
    bu_log("r	Reset to normal\n");
    bu_log("q	Exit\n");
    bu_log("RETURN	Exit\n");
}


int
doKeyPad(void)
{
    int ch;

#if defined(HAVE_CONIO_H)
    if ((ch = getch()) == EOF)
	return 0;		/* done */
#else
    if ((ch = getchar()) == EOF)
	return 0;		/* done */
#endif
    ch &= ~0x80;			/* strip off parity bit */
    switch (ch) {
	case '?' :
	    printUsage();
	    break;
	case 'T' :
	    toggle_pan = 1 - toggle_pan;
	    break;
	case '\r' :				/* Done, leave "as is" */
	case '\n' :
	case 'q' :
	case 'Q' :
	    return 0;
	case 'c' :				/* Reset Pan (Center) */
	case 'C' :
	    new_xPan = fb_getwidth(fbp)/2;
	    new_yPan = fb_getheight(fbp)/2;
	    break;
	case 'z' :				/* Reset Zoom */
	case 'Z' :
	    new_xZoom = 1;
	    new_yZoom = 1;
	    break;
	case 'r' :				/* Reset Pan and Zoom */
	case 'R' :
	    new_xZoom = 1;
	    new_yZoom = 1;
	    new_xPan = fb_getwidth(fbp)/2;
	    new_yPan = fb_getheight(fbp)/2;
	    break;

	case ctl('v') :
	case 'b' :				/* zoom BIG binary */
	    new_xZoom *= 2;
	    new_yZoom *= 2;
	    break;
	case '=' :
	case '+' :				/* zoom BIG incr */
	    new_xZoom++;
	    new_yZoom++;
	    break;
	case 's' :				/* zoom small binary */
	    new_xZoom /= 2;
	    new_yZoom /= 2;
	    break;
	case '-' :				/* zoom small incr */
	    --new_xZoom;
	    --new_yZoom;
	    break;

	case '>' :				/* X Zoom */
	    new_xZoom *= 2;
	    break;
	case '.' :
	    ++new_xZoom;
	    break;
	case '<' :
	    new_xZoom /= 2;
	    break;
	case COMMA :
	    --new_xZoom;
	    break;

	case ')' :				/* Y Zoom */
	    new_yZoom *= 2;
	    break;
	case '0' :
	    ++new_yZoom;
	    break;
	case '(' :
	    new_yZoom /= 2;
	    break;
	case '9' :
	    --new_yZoom;
	    break;

	case 'h' :				/* pan LEFT.	*/
	case 'B' :
	    new_xPan -= 1 - 2 * toggle_pan;
	    break;
	case 'H' :
	case ctl('b') :
	    new_xPan -= PanFactor * (1 - 2 * toggle_pan);
	    break;
	case 'j' :				/* pan DOWN.	*/
	case 'N' :
	    new_yPan -= 1 - 2 * toggle_pan;
	    break;
	case 'J' :
	case ctl('n') :
	    new_yPan -= PanFactor * (1 - 2 * toggle_pan);
	    break;
	case 'k' :				/* pan UP.	*/
	case 'P' :
	    new_yPan += 1 - 2 * toggle_pan;
	    break;
	case 'K' :
	case ctl('p') :
	    new_yPan += PanFactor * (1 - 2 * toggle_pan);
	    break;
	case 'l' :				/* pan RIGHT.	*/
	case 'F' :
	    new_xPan += 1 - 2 * toggle_pan;
	    break;
	case 'L' :
	case ctl('f') :
	    new_xPan += PanFactor * (1 - 2 * toggle_pan);
	    break;
	default :
	    (void) fprintf(stdout, "\r\n'%c' bad -- Type ? for help\r\n", ch);
	    printUsage();
	    break;
    }
    return 1;		/* keep going */
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
