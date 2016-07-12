/*                       F B P O I N T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2016 United States Government as represented by
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

#include "bu/color.h"
#include "bu/str.h"
#include "bu/log.h"
#include "vmath.h"
#include "fb.h"
#include "libtermio.h"

fb *fbp;

int JumpSpeed;		/* # pixels skiped with fast commands. */

RGBpixel curPix; 		/* Current pixel value */
int curX, curY;		/* current position */
int oldX, oldY;		/* previous position */

int Run = 1;		/* Tells when to stop the main loop */

int xflag, yflag;
char *xprefix = NULL;
char *yprefix = NULL;
char null_str = '\0';

void SimpleInput(void);

char usage[] = "\
Usage: fbpoint [-H] [-x[prefix]] [-y[prefix]] [initx inity]\n";

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
    int width, height;

    setbuf(stderr, (char *)malloc(BUFSIZ));
    width = height = 512;
    curX = curY = -1;

    while (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-H")) {
	    width = height = 1024;
	} else if (bu_strncmp(argv[1], "-x", 2) == 0) {
	    if (xflag++ != 0)
		break;
	    xprefix = &argv[1][2];
	} else if (bu_strncmp(argv[1], "-y", 2) == 0) {
	    if (yflag++ != 0)
		break;
	    yprefix = &argv[1][2];
	} else
	    break;
	argc--;
	argv++;
    }
    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout)))
	bu_exit(1, "%s", usage);
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

    if ((fbp = fb_open(NULL, width, height)) == NULL)
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
