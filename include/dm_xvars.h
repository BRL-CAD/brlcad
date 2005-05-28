/*                          D M _ X V A R S . H
 * BRL-CAD
 *
 * Copyright (C) 1993-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
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
/** @file dm_xvars.h
 *
 */
#ifndef SEEN_DM_XVARS
#define SEEN_DM_XVARS

#define XVARS_MV_O(_m) offsetof(struct dm_xvars, _m)

#ifdef _WIN32
struct dm_xvars {
  HDC  hdc;      // device context of device that OpenGL calls are to be drawn on
  Display *dpy;
  Window win;
  Tk_Window top;
  Tk_Window xtkwin;
  int depth;
  Colormap cmap;
  PIXELFORMATDESCRIPTOR *vip;
  HFONT fontstruct;
  int devmotionnotify;
  int devbuttonpress;
  int devbuttonrelease;
};
#else
struct dm_xvars {
  Display *dpy;
  Window win;
  Tk_Window top;
  Tk_Window xtkwin;
  int depth;
  Colormap cmap;
  XVisualInfo *vip;
  XFontStruct *fontstruct;
  int devmotionnotify;
  int devbuttonpress;
  int devbuttonrelease;
};
#endif
#endif /* SEEN_DM_XVARS */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
