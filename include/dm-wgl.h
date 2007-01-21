/*                          D M - W G L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2007 United States Government as represented by
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
/** @addtogroup libdm */
/** @{ */
/** @file dm-wgl.h
 *
 */
#ifndef SEEN_DM_WGL
#define SEEN_DM_WGL

#include "dm_color.h"
#define CMAP_BASE 40

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

#define Wgl_MV_O(_m) offsetof(struct modifiable_wgl_vars, _m)

struct modifiable_wgl_vars {
  int cueing_on;
  int zclipping_on;
  int zbuffer_on;
  int lighting_on;
  int transparency_on;
  int fastfog;
  double fogdensity;
  int zbuf;
  int rgb;
  int doublebuffer;
  int depth;
  int debug;
  double bound;
  int boundFlag;
};

struct wgl_vars {
  HGLRC glxc;
  GLdouble faceplate_mat[16];
  int face_flag;
  int *perspective_mode;
  int fontOffset;
  int ovec;		/* Old color map entry number */
  char is_direct;
  GLclampf r, g, b;
  struct modifiable_wgl_vars mvars;
};

extern void wgl_fogHint();

#endif /* SEEN_DM_WGL */
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

