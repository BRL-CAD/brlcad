/*                       F B C O L O R . C
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
/** @file fbcolor.c
 *
 * Dynamically show the desired color as the background, and in bargraph
 * form, using the color map.
 *
 */

#include "common.h"

#include <limits.h>
#include <string.h>
#include <math.h>

#define LIBTERMIO_IMPLEMENTATION
#if defined(HAVE_CONIO_H)
#  include <conio.h>
#else
#  include "libtermio.h"
#endif

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "dm.h"

#define COMMA ','
#define COLOR_LEVELS 256


int curchan = 0;	/* 0=r, 1=g, 2=b */

int col[6] = {128, 128, 128};		/* r, g, b h, s, v */

unsigned char buf[3*2048];
ColorMap old_map;
ColorMap cm;

void new_rgb(void), rgbhsv(int *rgb, int *hsv), hsvrgb(int *hsv, int *rgb);
int doKeyPad(void);

struct fbcolor_args {
    struct fbcolor_args *self;
    int help;
    const char *framebuffer;
    int width;
    int height;
};

enum fbcolor_dimension {
    FBCOLOR_SIZE,
    FBCOLOR_WIDTH,
    FBCOLOR_HEIGHT
};

static struct fbcolor_args *
fbcolor_args_from_storage(void *storage)
{
    return storage ? *(struct fbcolor_args **)storage : NULL;
}

static int
fbcolor_dimension_value(struct bu_vls *msg, const char *arg, void *storage,
	enum fbcolor_dimension dimension)
{
    struct fbcolor_args *args = fbcolor_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 0, INT_MAX))
	return -1;
    if (args) {
	if (dimension == FBCOLOR_SIZE)
	    args->width = args->height = value;
	else if (dimension == FBCOLOR_WIDTH)
	    args->width = value;
	else
	    args->height = value;
    }
    return 0;
}

#define FBCOLOR_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return fbcolor_dimension_value(msg, arg, storage, _dimension); \
    }

FBCOLOR_DIMENSION_CALLBACK(fbcolor_size, FBCOLOR_SIZE)
FBCOLOR_DIMENSION_CALLBACK(fbcolor_width, FBCOLOR_WIDTH)
FBCOLOR_DIMENSION_CALLBACK(fbcolor_height, FBCOLOR_HEIGHT)

static const struct bu_arg_option fbcolor_options[] = {
    BU_ARG_FLAG("h", "help", struct fbcolor_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("F", "framebuffer", struct fbcolor_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer instead of the FB_FILE environment default"),
    BU_ARG_CUSTOM("s", "size", struct fbcolor_args, self, fbcolor_size,
	"pixels", "Set both dimensions; zero selects the device default"),
    BU_ARG_ALIAS("S", NULL, "size", 1),
    BU_ARG_CUSTOM("w", "width", struct fbcolor_args, self, fbcolor_width,
	"pixels", "Set the width; zero selects the device default"),
    BU_ARG_ALIAS("W", NULL, "width", 1),
    BU_ARG_CUSTOM("n", "height", struct fbcolor_args, self, fbcolor_height,
	"scanlines", "Set the height; zero selects the device default"),
    BU_ARG_ALIAS("N", NULL, "height", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema fbcolor_schema = BU_ARG_SCHEMA(
    "fbcolor", "Interactively pick a color on a framebuffer",
    fbcolor_options, NULL, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar fbcolor_grammar = {
    &fbcolor_schema, NULL, NULL
};

static int
fbcolor_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&fbcolor_schema, "fbcolor");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "fbcolor help");
    return status;
}

int
main(int argc, char **argv)
{
    struct fbcolor_args args = {NULL, 0, NULL, 0, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct fb *fbp;
    int i;
    args.self = &args;

    bu_setprogname(argv[0]);
    int introspection = bu_arg_introspect(&msg, &fbcolor_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    if (bu_arg_schema_parse_complete(&fbcolor_schema, &args, &msg, argc - 1,
	    (const char **)(argv + 1)) < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return fbcolor_help(stdout, 0);

    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout))) {
	fprintf(stderr, "Opening a window, waiting for user input:\n");
    }

    if ((fbp = fb_open(args.framebuffer, args.width, args.height)) == FB_NULL) {
	bu_log("fbcolor: fb_open(%s) failure\n",
	    args.framebuffer ? args.framebuffer : "default framebuffer");
	return 1;
    }

    fb_rmap(fbp, &old_map);
    fb_clear(fbp, RGBPIXEL_NULL);

    rgbhsv(col, &col[3]);

    /* Note that color 0, 0, 0 is special;  use 1, 1, 1 for black */
    /* Red */
    for (i = 0; i < COLOR_LEVELS; i++) {
	buf[3*i+RED] = i;
	buf[3*i+GRN] = 1;
	buf[3*i+BLU] = 1;
    }
    for (i=0; i<99; i++)
	fb_write(fbp, 0, i, buf, COLOR_LEVELS);

    /* Green */
    memset((char *)buf, 0, sizeof(buf));
    for (i = 0; i < COLOR_LEVELS; i++) {
	buf[3*i+RED] = 1;
	buf[3*i+GRN] = i;
	buf[3*i+BLU] = 1;
    }
    for (i=100; i<199; i++)
	fb_write(fbp, 0, i, buf, COLOR_LEVELS);

    /* Blue */
    memset((char *)buf, 0, sizeof(buf));
    for (i = 0; i < COLOR_LEVELS; i++) {
	buf[3*i+RED] = 1;
	buf[3*i+GRN] = 1;
	buf[3*i+BLU] = i;
    }
    for (i=200; i<299; i++)
	fb_write(fbp, 0, i, buf, COLOR_LEVELS);

    /* Set RAW mode */
#ifndef HAVE_CONIO_H
    save_Tty(0);
    set_Raw(0);
    clr_Echo(0);
#endif

    do {
	/* Build color map for current value */
	memset((char *)&cm, 0, sizeof(cm));
	for (i=0; i<col[RED]; i++)
	    cm.cm_red[i] = 0xFFFF;
	for (; i < COLOR_LEVELS; i++)
	    cm.cm_red[i] = 0;

	for (i=0; i<col[GRN]; i++)
	    cm.cm_green[i] = 0xFFFF;
	for (; i < COLOR_LEVELS; i++)
	    cm.cm_green[i] = 0;

	for (i=0; i<col[BLU]; i++)
	    cm.cm_blue[i] = 0xFFFF;
	for (; i < COLOR_LEVELS; i++)
	    cm.cm_blue[i] = 0;

	/* 0, 0, 0 is color chosen */
	cm.cm_red[0] = col[RED]<<8;
	cm.cm_green[0] = col[GRN]<<8;
	cm.cm_blue[0] = col[BLU]<<8;

	/* 1, 1, 1 is for black */
	cm.cm_red[1] = 0;
	cm.cm_green[1] = 0;
	cm.cm_blue[1] = 0;

	fb_wmap(fbp, &cm);

	(void) fprintf(stdout,
		       "%c rgb=%3d, %3d, %3d hsv=%3d, %3d, %3d   \r",
		       "RGBHSIx"[curchan],
		       col[0], col[1], col[2],
		       col[3], col[4], col[5]);
	(void) fflush(stdout);
    } while (doKeyPad());

    fb_wmap(fbp, &old_map);

#ifndef HAVE_CONIO_H
    reset_Tty(0);
#endif

    (void) fprintf(stdout,  "\n");	/* Move off of the output line.	*/
    return 0;
}


char help[] = "\r\n\
+ .	increase by 1\r\n\
- ,	decrease by 1\r\n\
>	increase by 16\r\n\
<	decrease by 16\r\n\
r	select red\r\n\
g	select green\r\n\
b	select blue\r\n\
h	select hue\r\n\
s	select saturation\r\n\
i v	select intensity value\r\n\
q	quit\r\n\
\\n	Exit\r\n";


int
doKeyPad(void)
{
    int ch;

#if defined(HAVE_CONIO_H)
    if (isatty(fileno(stdin))) {
	ch = getch();
    } else {
	ch = getchar();
    }
#else
    ch = getchar();
#endif
    if (ch == EOF)
	return 0;		/* done */

    switch (ch) {
	default :
	    (void) fprintf(stdout,
			   "\r\n'%c' bad -- Type ? for help\r\n",
			   ch
		);
	    break;
	case '?' :
	    (void) fprintf(stdout, "\r\n%s", help);
	    break;
	case '\r' :
	case '\n' :				/* Done, return to normal */
	case 'q' :
	    return 0;
	case 'Q' :				/* Done, leave "as is" */
	    return 0;

	case 'r':
	    curchan = 0;
	    break;
	case 'g':
	    curchan = 1;
	    break;
	case 'b':
	    curchan = 2;
	    break;
	case 'h':
	    curchan = 3;
	    break;
	case 's':
	    curchan = 4;
	    break;
	case 'v':
	case 'i':
	    curchan = 5;
	    break;
	case '/':
	    if (++curchan >= 6)
		curchan = 0;
	    break;

	    /* unit changes with -+ or , . */
	case '+':
	case '.':
	    col[curchan]++;
	    new_rgb();
	    break;
	case '-':
	case COMMA:
	    col[curchan]--;
	    new_rgb();
	    break;

	    /* big changes with <> */
	case '>':
	    col[curchan]+=16;
	    new_rgb();
	    break;
	case '<':
	    col[curchan]-=16;
	    new_rgb();
	    break;
    }
    return 1;		/* keep going */
}


void
new_rgb(void) {
    /* Wrap values to stay in range 0..255 */
    if (col[curchan] < 0) col[curchan] = 255;
    if (col[curchan] > 255) col[curchan] = 0;
    /* recompute either rgb or hsv from the other */
    if (curchan < 3)
	rgbhsv(col, &col[3]);
    else
	hsvrgb(&col[3], col);
}


/* rgbhsv
 *
 * convert red green blue to hue saturation value
 */
void
rgbhsv(int *rgb, int *hsv)
{
    int s, v;
    int r, g, b;
    int x;
    static int h;
    double dif = 0;

    r = rgb[0];
    g = rgb[1];
    b = rgb[2];
    v = ((r > g) ? r : g);
    v = ((v > b) ? v : b);
    x = ((r < g) ? r : g);
    x = ((x < b) ? x : b);
    if (v != x) {
	dif = (double) (v - x);
	if (r != v) {
	    if (g == v) {
		if (b != x)
		    h = (int) (42.5 * (3.0 - (double)(v-b) / dif));
		else
		    h = (int) (42.5 * (1.0 + (double)(v-r) / dif));
	    } else {
		if (r != x)
		    h = (int) (42.5 * (5.0 - (double)(v-r) / dif));
		else
		    h = (int) (42.5 * (3.0 + (double)(v-g) / dif));
	    }
	} else {
	    if (g != x)
		h = (int) (42.5 * (1.0 - (double)(v-g) / dif));
	    else
		h = (int) (42.5 * (5.0 + (double)(v-b) / dif));
	}
    }

    if (v != 0)
	s = (int)(255. * dif / (double)v);
    else
	s = 0;

    hsv[0] = h;
    hsv[1] = s;
    hsv[2] = v;
}


/* hsvrgb
 *
 * convert hue saturation and value to red, green, blue
 */
void
hsvrgb(int *hsv, int *rgb)
{
    int r, g, b, m, n, k;
    double h, s, v, foo;
    double f;

    if (hsv[1] != 0) {
	s = (double)hsv[1] / 255.0;
	h = (double)hsv[0] / 42.666;
	f = modf(h, &(foo));
	v = (double)hsv[2];
	m = (int) (v * (1.0 - s) + .5);
	n = (int) (v * (1.0 - s*f) + .5);
	k = (int) (v * (1.0 - (s * (1.-f))) + .5);
	switch ((int) h) {
	    case 0:
		r = hsv[2];
		g = k;
		b = m;
		break;
	    case 1:
		r = n;
		g = hsv[2];
		b = m;
		break;
	    case 2:
		r = m;
		g = hsv[2];
		b = k;
		break;
	    case 3:
		r = m;
		g = n;
		b = hsv[2];
		break;
	    case 4:
		r = k;
		g = m;
		b = hsv[2];
		break;
	    default:
	    case 5:
		r = hsv[2];
		g = m;
		b = n;
		break;
	}
    } else
	r = g = b = hsv[2];

    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
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
