/*          M I M E _ P R I V A T E . H
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

#ifndef BU_MIME_PRIVATE_H
#define BU_MIME_PRIVATE_H
#include "bu/mime_runtime.h"
int mime_inspect_gltf(struct bu_mime_registry *, const struct bu_mime_probe_input *, struct bu_mime_result *);
int mime_inspect_builtin(struct bu_mime_registry *, const char *, const struct bu_mime_probe_input *, struct bu_mime_result *);
#endif
