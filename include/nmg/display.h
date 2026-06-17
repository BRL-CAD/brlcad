/*                      D I S P L A Y . H
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup nmg_display
 */
/** @{ */
/** @file nmg/display.h */

#ifndef NMG_DISPLAY_H
#define NMG_DISPLAY_H

#include "common.h"
#include "nmg/defines.h"

__BEGIN_DECLS

struct bn_tol;
struct bsg_line_layer_builder;
struct edgeuse;
struct model;
struct nmgregion;
struct shell;

/* typed display geometry */
NMG_EXPORT extern void nmg_line_layer_around_eu(struct bsg_line_layer_builder *plot,
						const struct edgeuse *arg_eu,
						long *tab,
						int fancy,
						const struct bn_tol *tol);
NMG_EXPORT extern void nmg_line_layer_s(struct bsg_line_layer_builder *plot,
					const struct shell *s,
					int fancy);
NMG_EXPORT extern void nmg_line_layer_r(struct bsg_line_layer_builder *plot,
					const struct nmgregion *r,
					int fancy);
NMG_EXPORT extern void nmg_line_layer_m(struct bsg_line_layer_builder *plot,
					const struct model *m,
					int fancy);

__END_DECLS

#endif  /* NMG_DISPLAY_H */
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
