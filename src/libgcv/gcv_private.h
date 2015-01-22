/*                   G C V _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file gcv_private.h
 *
 * Private header for libgcv.
 *
 */


#include "common.h"

#include "plugin.h"


__BEGIN_DECLS


void gcv_plugin_register(const struct gcv_plugin_info *plugin_info);


enum gcv_conversion_type {GCV_CONVERSION_READ, GCV_CONVERSION_WRITE};

const struct gcv_converter *gcv_converter_find(const char *path, enum gcv_conversion_type type);


__END_DECLS


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
