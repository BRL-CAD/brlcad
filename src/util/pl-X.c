/*                          P L - X . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2011 United States Government as represented by
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
/** @file util/pl-X.c
 *
 * Display plot3(5) on an X Window System display (X11R2)
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_X11_XLIB_H
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#endif
#include "bio.h"

#include "dm.h" /* for dm_applicationfocus() */
#include "bu.h"


#define TBAD	0	/* no such command */
#define TNONE	1	/* no arguments */
#define TSHORT	2	/* Vax 16-bit short */
#define TIEEE	3	/* IEEE 64-bit floating */
#define TCHAR	4	/* unsigned chars */
#define TSTRING	5	/* linefeed terminated string */

#define FONT "fixed"

XWMHints xwmh = {
    (InputHint|StateHint),		/* flags */
    False,				/* input */
    NormalState,			/* initial_state */
    0,				/* icon pixmap */
    0,				/* icon window */
    0, 0,				/* icon location */
    0,				/* icon mask */
    0				/* Window group */
};


struct uplot {
    int targ;	/* type of args */
    int narg;	/* number or args */
    char *desc;	/* description */
};
struct uplot uerror = { 0, 0, 0 };
struct uplot letters[] = {
    /*A*/	{ 0, 0, 0 },
    /*B*/	{ 0, 0, 0 },
    /*C*/	{ TCHAR, 3, "color" },
    /*D*/	{ 0, 0, 0 },
    /*E*/	{ 0, 0, 0 },
    /*F*/	{ TNONE, 0, "flush" },
    /*G*/	{ 0, 0, 0 },
    /*H*/	{ 0, 0, 0 },
    /*I*/	{ 0, 0, 0 },
    /*J*/	{ 0, 0, 0 },
    /*K*/	{ 0, 0, 0 },
    /*L*/	{ TSHORT, 6, "3line" },
    /*M*/	{ TSHORT, 3, "3move" },
    /*N*/	{ TSHORT, 3, "3cont" },
    /*O*/	{ TIEEE, 3, "d_3move" },
    /*P*/	{ TSHORT, 3, "3point" },
    /*Q*/	{ TIEEE, 3, "d_3cont" },
    /*R*/	{ 0, 0, 0 },
    /*S*/	{ TSHORT, 6, "3space" },
    /*T*/	{ 0, 0, 0 },
    /*U*/	{ 0, 0, 0 },
    /*V*/	{ TIEEE, 6, "d_3line" },
    /*W*/	{ TIEEE, 6, "d_3space" },
    /*X*/	{ TIEEE, 3, "d_3point" },
    /*Y*/	{ 0, 0, 0 },
    /*Z*/	{ 0, 0, 0 },
    /*[*/	{ 0, 0, 0 },
    /*\*/	{ 0, 0, 0 },
    /*]*/	{ 0, 0, 0 },
    /*^*/	{ 0, 0, 0 },
    /*_*/	{ 0, 0, 0 },
    /*`*/	{ 0, 0, 0 },
    /*a*/	{ TSHORT, 6, "arc" },
    /*b*/	{ 0, 0, 0 },
    /*c*/	{ TSHORT, 3, "circle" },
    /*d*/	{ 0, 0, 0 },
    /*e*/	{ TNONE, 0, "erase" },
    /*f*/	{ TSTRING, 1, "linmod" },
    /*g*/	{ 0, 0, 0 },
    /*h*/	{ 0, 0, 0 },
    /*i*/	{ TIEEE, 3, "d_circle" },
    /*j*/	{ 0, 0, 0 },
    /*k*/	{ 0, 0, 0 },
    /*l*/	{ TSHORT, 4, "line" },
    /*m*/	{ TSHORT, 2, "move" },
    /*n*/	{ TSHORT, 2, "cont" },
    /*o*/	{ TIEEE, 2, "d_move" },
    /*p*/	{ TSHORT, 2, "point" },
    /*q*/	{ TIEEE, 2, "d_cont" },
    /*r*/	{ TIEEE, 6, "d_arc" },
    /*s*/	{ TSHORT, 4, "space" },
    /*t*/	{ TSTRING, 1, "label" },
    /*u*/	{ 0, 0, 0 },
    /*v*/	{ TIEEE, 4, "d_line" },
    /*w*/	{ TIEEE, 4, "d_space" },
    /*x*/	{ TIEEE, 2, "d_point" },
    /*y*/	{ 0, 0, 0 },
    /*z*/	{ 0, 0, 0 }
};


int verbose;
double cx, cy, cz;		/* current x, y, z, point */
double arg[6];			/* parsed plot command arguments */
double sp[6];			/* space command */
char strarg[512];		/* string buffer */
int width, height;


Display *dpy;
Window win;
GC gc;
XFontStruct *fontstruct;


void
getstring(void)
{
    int c;
    char *cp;

    cp = strarg;
    while ((c = getchar()) != '\n' && c != '\r' && c != EOF)
	*cp++ = c;
    *cp = 0;
}


long
getshort(void)
{
    long v, w;

    v = getchar();
    v |= (getchar()<<8);	/* order is important! */

    /* worry about sign extension - sigh */
    if (v <= 0x7FFF) return v;
    w = -1;
    w &= ~0x7FFF;
    return w | v;
}


double
getieee(void)
{
    unsigned char in[8];
    double d;
    size_t ret;

    ret = fread(in, 8, 1, stdin);
    ntohd((unsigned char *)&d, in, 1);
    return d;
}


void
getargs(struct uplot *up)
{
    int i;

    for (i = 0; i < up->narg; i++) {
	switch (up->targ) {
	    case TSHORT:
		arg[i] = getshort();
		break;
	    case TIEEE:
		arg[i] = getieee();
		break;
	    case TSTRING:
		getstring();
		break;
	    case TCHAR:
		arg[i] = getchar();
		break;
	    case TNONE:
	    default:
		arg[i] = 0;	/* ? */
		break;
	}
    }
}


void
draw(double x_1, double y_1, double UNUSED(z_1), double x_2, double y_2, double z_2)
    /* from point */
    /* to point */
{
    int sx_1, sy_1, sx_2, sy_2;

    sx_1 = (x_1 - sp[0]) / (sp[3] - sp[0]) * width;
    sy_1 = height - (y_1 - sp[1]) / (sp[4] - sp[1]) * height;
    sx_2 = (x_2 - sp[0]) / (sp[3] - sp[0]) * width;
    sy_2 = height - (y_2 - sp[1]) / (sp[4] - sp[1]) * height;

    if (sx_1 == sx_2 && sy_1 == sy_2)
	XDrawPoint(dpy, win, gc, sx_1, sy_1);
    else
	XDrawLine(dpy, win, gc, sx_1, sy_1, sx_2, sy_2);

    cx = x_2;
    cy = y_2;
    cz = z_2;
}


void
label(double x, double y, char *str)
{
    int sx, sy;

    sx = (x - sp[0]) / (sp[3] - sp[0]) * width;
    sy = height - (y - sp[1]) / (sp[4] - sp[1]) * height;

    XDrawString(dpy, win, gc, sx, sy, str, strlen(str));
}


void
xsetup(int argc, char **argv)
{
    char hostname[81];
    char display[81];
    char *envp;
    unsigned long bd, bg, fg, bw;
    XSizeHints xsh;
    XEvent event;
    XGCValues gcv;

    width = height = 512;

    if ((envp = getenv("DISPLAY")) == NULL) {
	/* Env not set, use local host */
	gethostname(hostname, 80);
	snprintf(display, 81, "%s:0", hostname);
	envp = display;
    }

    /* Open the display - XXX see what NULL does now */
    if ((dpy = XOpenDisplay(envp)) == NULL) {
	bu_exit(2, "pl-X: Can't open X display\n");
    }

    /* Load the font to use */
    if ((fontstruct = XLoadQueryFont(dpy, FONT)) == NULL) {
	bu_exit(4, "pl-X: Can't open font\n");
    }

    /* Select border, background, foreground colors,
     * and border width.
     */
    bd = WhitePixel(dpy, DefaultScreen(dpy));
    bg = BlackPixel(dpy, DefaultScreen(dpy));
    fg = WhitePixel(dpy, DefaultScreen(dpy));
    bw = 1;

    /* Fill in XSizeHints struct to inform window
     * manager about initial size and location.
     */
    xsh.flags = (PSize);
    xsh.height = height + 10;
    xsh.width = width + 10;
    xsh.x = xsh.y = 0;

    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
			      xsh.x, xsh.y, xsh.width, xsh.height,
			      bw, bd, bg);
    if (win == 0) {
	bu_exit(3, "pl-X: Can't create window\n");
    }

    /* Set standard properties for Window Managers */
    XSetStandardProperties(dpy, win, "Unix Plot", "Unix Plot", None, argv, argc, &xsh);
    XSetWMHints(dpy, win, &xwmh);

    /* Create a Graphics Context for drawing */
    gcv.font = fontstruct->fid;
    gcv.foreground = fg;
    gcv.background = bg;
    gc = XCreateGC(dpy, win, (GCFont|GCForeground|GCBackground), &gcv);

    XSelectInput(dpy, win, ExposureMask | ButtonPressMask | KeyPressMask);
    XMapWindow(dpy, win); /* show the window */

    while (1) {
	XNextEvent(dpy, &event);
	if (event.type == Expose && event.xexpose.count == 0) {
	    XWindowAttributes xwa;

	    /* remove other exposure events */
	    while (XCheckTypedEvent(dpy, Expose, &event))
		;

	    if (XGetWindowAttributes(dpy, win, &xwa) == 0)
		break;

	    width = xwa.width;
	    height = xwa.height;
	    break;
	}
    }
    XSetInputFocus(dpy, win, RevertToNone, CurrentTime);

    dm_applicationfocus();
}


int
main(int argc, char **argv)
{
    int c;
    struct uplot *up;
    int erase = 0;
    int waiting = 1;

    while (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-v")) {
	    verbose++;
	} else
	    break;

	argc--;
	argv++;
    }
    if (isatty(fileno(stdin))) {
	bu_exit(1, "Usage: pl-X [-v] < unix_plot\n");
    }
    xsetup(argc, argv);

    while ((c = getchar()) != EOF) {
	/* look it up */

	if (c == '\n' || c == '\r') {
	    /* ignore blank lines */
	    continue;
	} else if (c < 'A' || c > 'z') {
	    up = &uerror;
	} else {
	    up = &letters[ c - 'A' ];
	}

	if (up->targ == TBAD) {
	    bu_log("Bad command '%c' (0x%02x)\n", c, c);
	    continue;
	}

	/* was the previous command an erase? */
	if (erase) {
	    XClearWindow(dpy, win);
	    erase = 0;
	}

	if (up->narg > 0)
	    getargs(up);

	switch (c) {
	    case 's':
	    case 'w':
		sp[0] = arg[0];
		sp[1] = arg[1];
		sp[2] = 0;
		sp[3] = arg[2];
		sp[4] = arg[3];
		sp[5] = 0;
		break;
	    case 'S':
	    case 'W':
		sp[0] = arg[0];
		sp[1] = arg[1];
		sp[2] = arg[2];
		sp[3] = arg[3];
		sp[4] = arg[4];
		sp[5] = arg[5];
		break;
	    case 'm':
	    case 'o':
		cx = arg[0];
		cy = arg[1];
		cz = 0;
		break;
	    case 'M':
	    case 'O':
		cx = arg[0];
		cy = arg[1];
		cz = arg[2];
		break;
	    case 'n':
	    case 'q':
		draw(cx, cy, cz, arg[0], arg[1], 0.0);
		break;
	    case 'N':
	    case 'Q':
		draw(cx, cy, cz, arg[0], arg[1], arg[2]);
		break;
	    case 'l':
	    case 'v':
		draw(arg[0], arg[1], 0.0, arg[2], arg[3], 0.0);
		break;
	    case 'L':
	    case 'V':
		draw(arg[0], arg[1], arg[2], arg[3], arg[4], arg[5]);
		break;
	    case 'p':
	    case 'x':
		draw(arg[0], arg[1], 0.0, arg[0], arg[1], 0.0);
		break;
	    case 'P':
	    case 'X':
		draw(arg[0], arg[1], arg[2], arg[0], arg[1], arg[2]);
		break;
	    case 't':
		label(cx, cy, strarg);
		break;
	    case 'F':
		XFlush(dpy);
		break;
	    case 'e':
		/* erase might be the last command in the file
		 * which makes pl-X pointless so don't erase
		 * unless this isn't the last command (check
		 * during the next loop iteration)
		 */
		erase = 1;
		break;
	}

	if (verbose) {
	    printf("%s\n", up->desc);
	}
    }
    XFlush(dpy);

    printf("Press any key to quit...\n");
    do {
	XEvent event;
	XNextEvent(dpy, &event);
	switch (event.type) {
	    case ButtonPress:
	    case ButtonRelease:
		/* ignore */
		break;
	    case KeyPress:
	    case KeyRelease:
		waiting = 0;
		break;
	    case Expose:

	    default:
		printf("unhandled event: %d\n", event.type);
	}
    } while (waiting);

    /* clean up */
    XFreeGC(dpy, gc);
    XCloseDisplay(dpy);

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
