/*                          D M _ X V A R S . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2007 United States Government as represented by
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
/** @addtogroup libdm */
/** @{ */
/** @file dm_xvars.h
 *
 */
#ifndef __DM_XVARS__
#define __DM_XVARS__

#include "common.h"

#ifdef HAVE_X11_XLIB_H
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
#endif

#include "tk.h"

#define XVARS_MV_O(_m) offsetof(struct dm_xvars, _m)

/* XXX - this really should not be variable-width and does not allow
 * multiple interfaces to be simultaneously compiled.
 */
struct dm_xvars {
  Display *dpy;
  Window win;
  Tk_Window top;
  Tk_Window xtkwin;
  int depth;
  Colormap cmap;
#ifdef IF_WGL
  PIXELFORMATDESCRIPTOR *vip;
  HFONT fontstruct;
  HDC  hdc;      // device context of device that OpenGL calls are to be drawn on
#else
  XVisualInfo *vip;
  XFontStruct *fontstruct;
#endif
  int devmotionnotify;
  int devbuttonpress;
  int devbuttonrelease;
};

#endif /* __DM_XVARS__ */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

