/*                   G E D _ D R A W . H
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
/** @file ged_brep.h
 *
 * Private header for libged draw cmd.
 *
 */

#ifndef LIBGED_DRAW_GED_PRIVATE_H
#define LIBGED_DRAW_GED_PRIVATE_H

#include "common.h"

#include "ged.h"
#include "../ged_private.h"

__BEGIN_DECLS

extern void _ged_drawH_part2(int dashflag, struct bu_list *vhead, const struct db_full_path *pathp, struct db_tree_state *tsp, struct _ged_client_data *dgcdp);

extern int ged_E_core(struct ged *gedp, int argc, const char *argv[]);

__END_DECLS

#endif /* LIBGED_DRAW_GED_PRIVATE_H */

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
