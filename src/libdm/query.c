/*                         Q U E R Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file query.c
 *
 */

#include "common.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#endif

#ifdef HAVE_X11_XLIB_H
#  include <X11/Xlib.h>
#endif

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "dm.h"

int dm_validXType(char *dpy_string, char *name);
char *dm_bestXType(char *dpy_string);

int
dm_validXType(char *dpy_string, char *name)
{
#ifdef DM_WGL
    if (!strcmp(name, "wgl")) {
	return 1;
    }
#endif /* DM_WGL */

#ifdef DM_OGL
    if (!strcmp(name, "ogl")) {
	Display *dpy;
	int return_val;
	if ((dpy = XOpenDisplay(dpy_string)) == NULL) {
	    bu_log("dm_validXType: failed to open display - %s\n", dpy_string?dpy_string:"(DISPLAY unspecified)");
	    return 0;
	}
	if (XQueryExtension(dpy, "GLX", &return_val, &return_val, &return_val)) {
	    XCloseDisplay(dpy);
	    return 1;
	}
	XCloseDisplay(dpy);
	return 0;
    }

#endif /* DM_OGL */

#ifdef DM_X
    if (!strcmp(name, "X")) {
	Display *dpy;
	if ((dpy = XOpenDisplay(dpy_string)) == NULL) {
	    bu_log("dm_validXType: failed to open display - %s\n", dpy_string?dpy_string:"(DISPLAY unspecified)");
	    return 0;
	}
	XCloseDisplay(dpy);
	return 1;
    }
#endif /* DM_X */

    return 0;
}

char *
dm_bestXType(char *dpy_string)
{
#ifdef DM_WGL
    /* should probably make sure wgl works */
    return "wgl";
#endif

#ifdef DM_OGL
    {
	Display *dpy;
	int return_val;

	if ((dpy = XOpenDisplay(dpy_string)) == NULL) {
	    bu_log("dm_bestXType: failed to open display - %s\n", dpy_string?dpy_string:"(DISPLAY unspecified)");
	} else {
	    if (XQueryExtension(dpy, "GLX", &return_val, &return_val, &return_val)) {
		XCloseDisplay(dpy);
		return "ogl";
	    }
	    XCloseDisplay(dpy);
	}
    }
#endif

#ifdef DM_X
    {
	Display *dpy;
	if ((dpy = XOpenDisplay(dpy_string)) == NULL) {
	    bu_log("dm_bestXType: failed to open display - %s\n", dpy_string?dpy_string:"(DISPLAY unspecified)");
	} else {
	    XCloseDisplay(dpy);
	    return "X";
	}
    }
#endif

    return NULL;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
