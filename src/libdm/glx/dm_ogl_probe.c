/*                    D M _ G L X _ P R O B E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

/* glx.h on Mac OS X (and perhaps elsewhere) defines a slew of
 * parameter names that shadow system symbols.  protect the system
 * symbols by redefining the parameters prior to header inclusion.
 */
#define j1 J1
#define y1 Y1
#define read rd
#define index idx
#define access acs
#define remainder rem
#ifdef HAVE_GL_GLX_H
#  include <GL/glx.h>
#endif
#undef remainder
#undef access
#undef index
#undef read
#undef y1
#undef j1

static XVisualInfo *
probe_visual(Display *dpy)
{
    XVisualInfo *vip = NULL;
    XVisualInfo *vibase = NULL;
    XVisualInfo *best = NULL;
    XVisualInfo vitemp;
    int num = 0;
    int screen = DefaultScreen(dpy);
    int use = 0;
    int rgba = 0;
    int fail = 0;

    memset((void *)&vitemp, 0, sizeof(XVisualInfo));
    vibase = XGetVisualInfo(dpy, 0, &vitemp, &num);
    if (!vibase) {
	return NULL;
    }

    for (vip = vibase; vip < vibase + num; vip++) {
	if (vip->screen != screen) {
	    continue;
	}

	fail = glXGetConfig(dpy, vip, GLX_USE_GL, &use);
	if (fail || !use) {
	    continue;
	}

	fail = glXGetConfig(dpy, vip, GLX_RGBA, &rgba);
	if (fail || !rgba) {
	    continue;
	}

	if (!best || vip->depth > best->depth) {
	    best = vip;
	}
    }

    if (!best) {
	XFree(vibase);
	return NULL;
    }

    vip = (XVisualInfo *)malloc(sizeof(XVisualInfo));
    if (!vip) {
	XFree(vibase);
	return NULL;
    }
    *vip = *best;
    XFree(vibase);

    return vip;
}

int
main(int argc, char **argv)
{
    const char *dpy_arg = NULL;
    Display *dpy = NULL;
    XVisualInfo *vip = NULL;
    GLXContext ctx = NULL;
    Colormap cmap = 0;
    Window win = 0;
    XSetWindowAttributes swa;
    int glx_event = 0;
    int glx_error = 0;
    int glx_opcode = 0;

    if (argc > 1 && argv[1] && argv[1][0] != '\0') {
	dpy_arg = argv[1];
    }

    dpy = XOpenDisplay(dpy_arg);
    if (!dpy) {
	return 2;
    }

    if (!XQueryExtension(dpy, "GLX", &glx_opcode, &glx_event, &glx_error)) {
	XCloseDisplay(dpy);
	return 3;
    }

    vip = probe_visual(dpy);
    if (!vip) {
	XCloseDisplay(dpy);
	return 4;
    }

    ctx = glXCreateContext(dpy, vip, NULL, GL_TRUE);
    if (!ctx) {
	XFree(vip);
	XCloseDisplay(dpy);
	return 5;
    }

    cmap = XCreateColormap(dpy, RootWindow(dpy, vip->screen), vip->visual, AllocNone);

    memset((void *)&swa, 0, sizeof(XSetWindowAttributes));
    swa.background_pixel = BlackPixel(dpy, vip->screen);
    swa.border_pixel = BlackPixel(dpy, vip->screen);
    swa.colormap = cmap;

    win = XCreateWindow(dpy,
			RootWindow(dpy, vip->screen),
			0, 0, 1, 1, 0,
			vip->depth,
			InputOutput,
			vip->visual,
			CWBackPixel | CWBorderPixel | CWColormap,
			&swa);
    if (!win) {
	glXDestroyContext(dpy, ctx);
	XFreeColormap(dpy, cmap);
	XFree(vip);
	XCloseDisplay(dpy);
	return 6;
    }

    XSync(dpy, 0);

    if (glXMakeCurrent(dpy, win, ctx) == False) {
	XDestroyWindow(dpy, win);
	glXDestroyContext(dpy, ctx);
	XFreeColormap(dpy, cmap);
	XFree(vip);
	XCloseDisplay(dpy);
	return 7;
    }

    glXMakeCurrent(dpy, None, NULL);
    XDestroyWindow(dpy, win);
    glXDestroyContext(dpy, ctx);
    XFreeColormap(dpy, cmap);
    XFree(vip);
    XCloseDisplay(dpy);

    return 0;
}
