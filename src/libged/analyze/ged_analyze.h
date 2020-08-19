/*                    G E D _ A N A L Y Z E . H
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

#ifndef LIBGED_ANALYZE_GED_PRIVATE_H
#define LIBGED_ANALYZE_GED_PRIVATE_H

#include "common.h"

#include "./ged/defines.h"

__BEGIN_DECLS

GED_EXPORT extern void
print_volume_table(
	struct ged *gedp,
       	const fastf_t tot_vol,
       	const fastf_t tot_area,
       	const fastf_t tot_gallons
	);


__END_DECLS

#endif /* LIBGED_ANALYZE_GED_PRIVATE_H */

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
