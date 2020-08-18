/*                        G E D _ C G . H
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file ged_cg.h
 *
 * Brief description
 *
 */

#ifndef LIBGED_BREP_GED_PRIVATE_H
#define LIBGED_BREP_GED_PRIVATE_H

#include "common.h"

#include "./rt/db_internal.h"
#include "./rt/op.h"
#include "./ged/defines.h"

__BEGIN_DECLS

GED_EXPORT extern int
pnts_eval_op(
	struct ged *gedp,
	db_op_t op,
	struct rt_db_internal *tpnts_intern,
	const char *vol_obj,
	const char *output_pnts_obj
	);

__END_DECLS

#endif /* LIBGED_BREP_GED_PRIVATE_H */

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
