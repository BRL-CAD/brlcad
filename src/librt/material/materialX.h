/*                   M A T E R I A L X . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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

#ifndef _MATERIALX_H
#define _MATERIALX_H

#include "common.h"
#include "rt/defines.h"
#include "rt/nongeom.h"
#include "bu/vls.h"

__BEGIN_DECLS

RT_EXPORT extern void rt_material_mtlx_to_osl(const rt_material_internal material_ip, struct bu_vls* vp);

__END_DECLS

#endif		/* _MATERIALX_H */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
