/*                         Z A P . H
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */
/** @file libged/zap/zap.h
 *
 * Private native command schema shared by the C compatibility core and the
 * C++ new-form implementation.
 */

#ifndef LIBGED_ZAP_H
#define LIBGED_ZAP_H

#include "bu/cmdschema.h"


struct zap_args {
    int print_help;
    const char *view;
    int shared_only;
    int clear_view_objs;
    int clear_solid_objs;
    int clear_all_views;
};


__BEGIN_DECLS

extern const struct bu_cmd_schema ged_zap_cmd_schema;
extern const struct bu_cmd_schema ged_Z_cmd_schema;

__END_DECLS

#endif /* LIBGED_ZAP_H */
