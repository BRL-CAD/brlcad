/*                         C O L O R . C
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
/** @file color.c
 *
 */

#include "common.h"



#include <stdio.h>
#include <X11/Xlib.h>
#include "machine.h"
#include "bu.h"
#include "dm_color.h"

unsigned long dm_get_pixel(unsigned char r, unsigned char g, unsigned char b, long unsigned int *pixels, int cd);
void dm_copy_cmap(Display *dpy, Colormap dest, Colormap src, int low, int hi, int store);
void dm_allocate_color_cube(Display *dpy, Colormap cmap, long unsigned int *pixels, int cd, int cmap_base, int store);

/* Return the allocated pixel value that most closely represents
the color requested. */
unsigned long
dm_get_pixel(unsigned char r, unsigned char g, unsigned char b, long unsigned int *pixels, int cd)
                        /* values assumed to be [0,255] */

        /* cube dimension */
{
  fastf_t f;
  int rf, gf, bf;
  int index;

  if(r == 0 && g == 0 && b == 0)
    return pixels[0];

  f = cd >> 8;
  rf = r * f;
  gf = g * f;
  bf = b * f;

  index = rf * cd * cd + gf * cd + bf;

  if(index == 0){
    if(r != 0)
      index = cd * cd;

    if(g != 0)
      index += cd;

    if(b != 0)
      index += 1;
  }

  return pixels[index];
}

/*
 * Alloc/Store (hi - low) colors from src colormap into dest.
 */
void
dm_copy_cmap(Display *dpy, Colormap dest, Colormap src, int low, int hi, int store)
{
  int i;
  int ncolors;
  XColor *colors;

  ncolors = hi - low;
  colors = (XColor *)bu_malloc(sizeof(XColor) * ncolors, "dm_load_cmap: colors");

  for(i = low; i < hi; ++i)
    colors[i].pixel = i;
  XQueryColors(dpy, src, colors, ncolors);

  if(store){
    XStoreColors(dpy, dest, colors, ncolors);
  }else{
    for(i = 0; i < ncolors; ++i){
      XAllocColor(dpy, dest, &colors[i]);
    }
  }

  bu_free((genptr_t)colors, "dm_load_cmap: colors");
}

void
dm_allocate_color_cube(Display *dpy, Colormap cmap, long unsigned int *pixels, int cd, int cmap_base, int store)



          /* cube dimension */


{
  XColor color;
  Colormap default_cmap;
  int i;
  int r, g, b;
  int incr;  /* increment */

  /*
   * Copy default colors below cmap_base to private colormap to help
   * reduce flashing. Assuming cmap is private and empty, we can be
   * fairly certain to get the colors we want in the right order even
   * though cmap may be shared.
   */
  default_cmap = DefaultColormap(dpy, DefaultScreen(dpy));
  dm_copy_cmap(dpy, cmap, default_cmap, 0, cmap_base, store);

  incr = 65535 / (cd - 1);

  /* store color cube at cmap_base and above */
  for(i = r = 0; r < 65536; r = r + incr)
    for(g = 0; g < 65536; g = g + incr)
      for(b = 0; b < 65536; b = b + incr, ++i){
	color.red = (unsigned short)r;
	color.green = (unsigned short)g;
	color.blue = (unsigned short)b;

	if(store){
	  color.flags = DoRed|DoGreen|DoBlue;
	  pixels[i] = color.pixel = i + cmap_base;
	  XStoreColor(dpy, cmap, &color);
	}else{
	  XAllocColor(dpy, cmap, &color);
	  pixels[i] = color.pixel;
	}
      }
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
