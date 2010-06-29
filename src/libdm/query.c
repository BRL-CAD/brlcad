/*                         Q U E R Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
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
/** @file query.c
 *
 */

#include "common.h"
#include "bio.h"

#include <string.h>

#ifdef HAVE_X11_XLIB_H
#  include <X11/Xlib.h>
#endif

#include "tcl.h"

#include "bu.h"
#include "vmath.h"
#include "dm.h"

int dm_validXType(char *dpy_string, char *name);
char *dm_bestXType(char *dpy_string);

int
dm_validXType(char *dpy_string, char *name)
{
    if (strcmp(name, "wgl")==0) {
#ifdef DM_WGL
	return 1;
#else
	bu_log("Specified display type [%s] is not available in this compilation.", name);
	return 0;
#endif /* DM_WGL */
    }

    if (strcmp(name, "rtgl")==0) {
#ifdef DM_RTGL
	Display *dpy;
	int return_val;
	if ((dpy = XOpenDisplay(dpy_string)) != NULL) {
	    if (XQueryExtension(dpy, "GLX", &return_val, &return_val, &return_val)) {
		XCloseDisplay(dpy);
		return 1;
	    }
	    XCloseDisplay(dpy);
	}
#else
	bu_log("Specified display type [%s] is not available in this compilation.", name);
#endif /* DM_RTGL */
	return 0;
    }

    if (strcmp(name, "ogl")==0) {
#ifdef DM_OGL
	Display *dpy;
	int return_val;
	if ((dpy = XOpenDisplay(dpy_string)) != NULL) {
	    if (XQueryExtension(dpy, "GLX", &return_val, &return_val, &return_val)) {
		XCloseDisplay(dpy);
		return 1;
	    }
	    XCloseDisplay(dpy);
	}
#else
	bu_log("Specified display type [%s] is not available in this compilation.", name);
#endif /* DM_OGL */
	return 0;
    }

    if (strcmp(name, "X")==0) {
#ifdef DM_X
	Display *dpy;
	if ((dpy = XOpenDisplay(dpy_string)) != NULL) {
	    XCloseDisplay(dpy);
	    return 1;
	}
#else
	bu_log("Specified display type [%s] is not available in this compilation.", name);
#endif /* DM_X */
	return 0;
    }

    if (strcmp(name, "tk")==0) {
#ifdef DM_TK
	return 1;
#else
	bu_log("Specified display type [%s] is not available in this compilation.", name);
#endif /* DM_TK */
	return 0;
    }

    return 0;
}

/** dm_bestXType determines what mged will normally
  * use as the default display manager
  */

char *
dm_bestXType(char *dpy_string)
{
#ifdef DM_WGL
    /* should probably make sure wgl works */
    return "wgl";
#endif

#ifdef DM_RTGL
    {
	Display *dpy;
	int return_val;

	if ((dpy = XOpenDisplay(dpy_string)) != NULL) {
	    if (XQueryExtension(dpy, "GLX", &return_val, &return_val, &return_val)) {
		XCloseDisplay(dpy);
		return "rtgl";
	    }
	    XCloseDisplay(dpy);
	}
    }
#endif

#ifdef DM_OGL
    {
	Display *dpy;
	int return_val;

	if ((dpy = XOpenDisplay(dpy_string)) != NULL) {
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
	if ((dpy = XOpenDisplay(dpy_string)) != NULL) {
	    XCloseDisplay(dpy);
	    return "X";
	}
    }
#endif

#ifdef DM_TK
    return "tk";
#endif
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
