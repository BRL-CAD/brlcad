/*                         Q U E R Y . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
dm_validXType(char	*dpy_string,
	      char	*name)
{
	int val = 0;

#ifdef HAVE_XOPENDISPLAY
	/* Here we assume the X server supports OpenGL */
	Display *dpy;
	int return_val;

	if ((dpy = XOpenDisplay(dpy_string)) == NULL) {
		bu_log("dm_validXType: failed to open display - %s\n", dpy_string);
		return val;
	}

#  ifdef DM_OGL
	if (!strcmp(name, "ogl") &&
	    XQueryExtension(dpy, "GLX", &return_val, &return_val, &return_val))
		val = 1;
	else
#  endif
#  ifdef DM_X
		if (!strcmp(name, "X"))
			val = 1;
#  endif

	XCloseDisplay(dpy);

#else /* HAVE_XOPENDISPLAY */

#  ifdef DM_OGL
	if (!strcmp(name, "ogl"))
		return 1;
#  endif
#  ifdef DM_X
	if (!strcmp(name, "X"))
		return 1;
#  endif

#endif /* HAVE_XOPENDISPLAY */

  return val;
}

char *
dm_bestXType(char *dpy_string)
{
	char *name = (char *)NULL;

#ifdef HAVE_XOPENDISPLAY
	/* Here we assume the X server supports OpenGL */
	Display *dpy;
	int return_val;

	if ((dpy = XOpenDisplay(dpy_string)) == NULL) {
		bu_log("dm_bestXType: failed to open display - %s\n", dpy_string);
		return name;
	}

#  ifdef DM_OGL
	if (XQueryExtension(dpy, "GLX", &return_val, &return_val, &return_val)) {
		name = "ogl";
	}
#    ifdef DM_X
	else {
	    name = "X";
	}
#    endif /* DM_X */
#  endif  /* DM_OGL */

#ifdef DM_X
		name = "X";
#endif

	XCloseDisplay(dpy);

#else /* HAVE_XOPENDISPLAY */

#  ifdef DM_OGL
	return "ogl";
#  endif
#  ifdef DM_X
	return "X";
#  endif

#endif /* HAVE_XOPENDISPLAY */

	return name;
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
