/*                     O G L C U R S O R . H
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

/** \addtogroup libfb */
/*@{*/
/** @file oglcursor.h
 * OpenGL framebuffer arrow cursor.
 *
 * The origin is in first quadrant coordinates.
 * The bytes are read in starting at the bottom left corner of the 
 * cursor and proceeding to the right and then up. 
 */
/*@}*/

16, 16,		/* size */
0, 0,		/* origin */
{
0xFF, 0xC0,
0xFF, 0x00,
0xFC, 0x00,
0xFE, 0x00,
0xFF, 0x00,
0xEF, 0x80,
0xC7, 0xC0,
0xC3, 0xE0,
0x81, 0xF0,
0x80, 0xF8,
0x00, 0x7C,
0x00, 0x3E,
0x00, 0x1F,
0x00, 0x0F,
0x00, 0x06,
0x00, 0x00
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
