/*                            N M G . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @addtogroup ged_view
 *
 * NMG specific visualizations that (currently) need libged routines.
 *
 * The logic in this header will almost certainly move elsewhere - anything in
 * here should be considered temporary and not relied on from an API design
 * perspective.
 *
 */
/** @{ */
/** @file ged/view/nmg.h */

#ifndef GED_VIEW_NMG_H
#define GED_VIEW_NMG_H

#include "common.h"
#include "bn.h"
#include "nmg.h"
#include "ged/defines.h"

__BEGIN_DECLS

GED_EXPORT extern void nmg_plot_eu(struct ged *gedp, struct edgeuse *es_eu, struct bn_tol *tol, struct bu_list *vlfree);

__END_DECLS

#endif /* GED_VIEW_SELECT_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
