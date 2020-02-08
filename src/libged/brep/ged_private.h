/*                   G E D _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2019 United States Government as represented by
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
/** @file ged_private.h
 *
 * Private header for libged brep cmd.
 *
 */

#ifndef LIBGED_BREP_GED_PRIVATE_H
#define LIBGED_BREP_GED_PRIVATE_H

#include "common.h"

#include <time.h>

#include "bu/opt.h"
#include "rt/db4.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "ged.h"

__BEGIN_DECLS

/* defined in draw.c */
extern void _ged_cvt_vlblock_to_solids(struct ged *gedp,
				       struct bn_vlblock *vbp,
				       const char *name,
				       int copy);


extern int _ged_brep_to_csg(struct ged *gedp, const char *obj_name, int verify);

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
