/*                        F B Z O O M . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
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

#include <stdlib.h>
#include "bio.h"

#include "bu.h"
#include "fb.h"
#include "libtermio.h"


#define COMMA ','


int pars_Argv(int argc, char **argv);
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
static char *framebuffer = NULL;
static fb *fbp;

static char usage[] = "\
Usage: fbzoom [-T] [-F framebuffer]\n\
	[-{sS} squarescrsize] [-{wW} scr_width] [-{nN} scr_height]\n";

int
main(int argc, char **argv)
{
    if (! pars_Argv(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }
    if ((fbp = fb_open(framebuffer, scr_width, scr_height)) == NULL)
	bu_exit(1, NULL);

    if (bu_optind+4 == argc) {
	xPan = atoi(argv[bu_optind+0]);
	yPan = atoi(argv[bu_optind+1]);
	xZoom = atoi(argv[bu_optind+2]);
	yZoom = atoi(argv[bu_optind+3]);
	fb_view(fbp, xPan, yPan, xZoom, yZoom);
    }

    fb_getview(fbp, &xPan, &yPan, &xZoom, &yZoom);

    /* Set RAW mode */
    save_Tty(0);
    set_Raw(0);
    clr_Echo(0);

    PanFactor = fb_getwidth(fbp)/16;
    if (PanFactor < 2) PanFactor = 2;

    new_xPan = xPan;
    new_yPan = yPan;
    new_xZoom = xZoom;
    new_yZoom = yZoom;
    do {
	/* Clip values against Min/Max */
	if (new_xPan > MaxXPan) new_xPan = MaxXPan;
	if (new_xPan < MinPan) new_xPan = MinPan;
	if (new_yPan > MaxYPan) new_yPan = MaxYPan;
	if (new_yPan < MinPan) new_yPan = MinPan;
	if (new_xZoom < MinZoom) new_xZoom = MinZoom;
	if (new_yZoom < MinZoom) new_yZoom = MinZoom;

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
printUsage()
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

    if ((ch = getchar()) == EOF)
	return 0;		/* done */
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


int
pars_Argv(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "TF:s:S:w:W:n:N:h?")) != -1) {
	switch (c) {
	    case 'T':
		/* reverse the sense of pan commands */
		toggle_pan = 1;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 's':
	    case 'S':
		scr_height = scr_width = atoi(bu_optarg);
		break;
	    case 'w':
	    case 'W':
		scr_width = atoi(bu_optarg);
		break;
	    case 'n':
	    case 'N':
		scr_height = atoi(bu_optarg);
		break;

	    default:		/* '?' 'h' */
		return 0;
	}
    }

    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout)))
	return 0;

    return 1;
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
