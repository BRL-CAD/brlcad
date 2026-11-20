/* O B J _ M I M E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 * See the file named COPYING for more information.
 */
#ifndef GCV_OBJ_MIME_H
#define GCV_OBJ_MIME_H
#include "common.h"
#include "bu/mime.h"
struct gcv_context;
__BEGIN_DECLS
extern enum bu_mime_probe_status obj_probe(const struct bu_mime_probe_input *, const char *);
extern int obj_input_profile(struct gcv_context *, const char *, const struct bu_mime_traits *);
__END_DECLS
#endif
