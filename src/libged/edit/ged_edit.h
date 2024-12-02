/*                   G E D _ E D I T . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file ged_edit.h
 *
 * Private header for libged edit cmd and related commands.
 *
 */

#ifndef LIBGED_EDIT_GED_PRIVATE_H
#define LIBGED_EDIT_GED_PRIVATE_H

#include "common.h"

#ifdef __cplusplus
#include <set>
#include <string>
#include <vector>
#endif
#include <time.h>

#include "ged.h"
#include "../ged_private.h"

__BEGIN_DECLS

extern int ged_protate_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_pscale_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_ptranslate_core(struct ged *gedp, int argc, const char *argv[]);

__END_DECLS

#endif /* LIBGED_EDIT_GED_PRIVATE_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
