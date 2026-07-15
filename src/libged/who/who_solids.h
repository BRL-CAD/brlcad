/*                    W H O _ S O L I D S . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file who_solids.h
 *
 * Shared native argument contract for who solids, solid_report, and x.
 */

#ifndef LIBGED_WHO_SOLIDS_H
#define LIBGED_WHO_SOLIDS_H

#include "bu/cmdschema.h"

struct ged_solid_report_args {
    int print_help;
    struct bu_vls view;
    int mode;
};

__BEGIN_DECLS

extern const struct bu_cmd_schema *ged_solid_report_native_schema(void);
extern const struct bu_cmd_schema ged_who_solids_schema;

__END_DECLS

#endif /* LIBGED_WHO_SOLIDS_H */
