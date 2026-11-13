/*                      R E C T _ U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#ifndef BRLCAD_SRC_UTIL_RECT_UTIL_H
#define BRLCAD_SRC_UTIL_RECT_UTIL_H

#include "bu/mime.h"

int rect_main(int argc, char **argv, const char *name, const char *description,
	enum bu_mime_image_t image_type);

#endif /* BRLCAD_SRC_UTIL_RECT_UTIL_H */
