/*                       F B P O I N T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2022 United States Government as represented by
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

#include <stdlib.h>
#include <string.h>

#include "bio.h"

#include "bu/app.h"
#include "bu/color.h"
#include "bu/str.h"
#include "bu/exit.h"
#include "bu/getopt.h"
#include "vmath.h"
#include "dm.h"
#include "libtermio.h"


struct fb *fbp;

int JumpSpeed;		/* # pixels skipped with fast commands. */

RGBpixel curPix; 		/* Current pixel value */
int curX, curY;		/* current position */
int oldX, oldY;		/* previous position */

int Run = 1;		/* Tells when to stop the main loop */

void SimpleInput(void);

char usage[] = "\
Usage: fbpoint [-F framebuffer] [-s squaresize] [-w width] [-n height] [-x[prefix]] [-y[prefix]] [initialx initialy]\n";

static char *help = "\
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
    int c;
    int width, height;
    int xflag = 0;
    int yflag = 0;

    const char *framebuffer = NULL;
    const char *xprefix = NULL;
    const char *yprefix = NULL;
    const char null_str = '\0';

    bu_setprogname(argv[0]);

    setbuf(stderr, (char *)malloc(BUFSIZ));
    width = height = 512;
    curX = curY = -1;

    while ((c = bu_getopt(argc, argv, "F:s:w:n:S:W:N:x::y::h?")) != -1) {
	if (bu_optopt == '?')
	    c = 'h';
	switch(c) {
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 's':
		width = height = atoi(bu_optarg);
		break;
	    case 'w':
		width = atoi(bu_optarg);
		break;
	    case 'n':
		height = atoi(bu_optarg);
		break;
	    case 'x':
		xflag++;
		if (bu_optarg && *bu_optarg != '\0')
		    xprefix = bu_optarg;
		break;
	    case 'y':
		yflag++;
		if (bu_optarg && *bu_optarg != '\0')
		    yprefix = bu_optarg;
		break;
	    case 'h':
		fprintf(stderr, "%s", usage);
		return 0;
	    default:
		fprintf(stderr, "ERROR: argument missing or bad option specified\n");
		fprintf(stderr, "%s", usage);
		return 1;
	}
    }
    argc -= bu_optind;
    argv += bu_optind;

    /*
     * Check for optional starting coordinate.
     * Test for bad flags while we're at it.
     */
    if (argc > 1 && argv[1][0] != '-') {
	curX = atoi(argv[1]);
	argc--;
	argv++;
    }
    if (argc > 1 && argv[1][0] != '-') {
	curY = atoi(argv[1]);
	argc--;
	argv++;
    }
    if (argc > 1) {
	bu_exit(1, "%s", usage);
    }

    /* fix up pointers for printf */
    if (xprefix == NULL)
	xprefix = &null_str;
    if (yprefix == NULL)
	yprefix = &null_str;

    if ((fbp = fb_open(framebuffer, width, height)) == NULL)
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
    if (xflag != 0 && yflag == 0)
	printf("%s%d\n", xprefix, curX);
    else if (yflag != 0 && xflag == 0)
	printf("%s%d\n", yprefix, curY);
    else
	printf("%s%d %s%d\n", xprefix, curX, yprefix, curY);

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
