/*                  T E S S E L L A T E . H
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
/** @file tessellate.h
 *
 * Private header for facetize subprocess ged_exec module.
 *
 */

#ifndef TESSELLATE_EXEC_H
#define TESSELLATE_EXEC_H

#include "common.h"

#include "vmath.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "bg/spsr.h"
#include "raytrace.h"
#include "../tess_opts.h"

__BEGIN_DECLS

class tess_opts {
    public:
	method_options_t method_opts;
	nmg_opts nmg_options;
	cm_opts cm_options;
	spsr_opts spsr_options;
#ifdef USE_GEOGRAM
	co3ne_opts co3ne_options;
#endif

	int overwrite_obj = 0;
	sample_opts pnt_options; // Values used by sample.cpp
};

extern struct rt_bot_internal *
_tess_facetize_decimate(struct rt_bot_internal *bot, fastf_t feature_size);

extern int
_tess_facetize_write_bot(struct db_i *dbip, struct rt_bot_internal *bot, const char *name, const char *method);

extern struct rt_pnts_internal *
_tess_pnts_sample(const char *oname, struct db_i *dbip, tess_opts *s);

extern int
_brep_csg_tessellate(struct ged *gedp, struct directory *dp, tess_opts *s);

extern int
_nmg_tessellate(struct rt_bot_internal **nbot, struct rt_db_internal *intern, tess_opts *s);

extern int
continuation_mesh(struct rt_bot_internal **obot, struct db_i *dbip, const char *objname, tess_opts *s, point_t seed);

extern int
co3ne_mesh(struct rt_bot_internal **obot, struct db_i *dbip, struct rt_pnts_internal *pnts, tess_opts *s);

extern int
spsr_mesh(struct rt_bot_internal **obot, struct db_i *dbip, struct rt_pnts_internal *pnts, tess_opts *s);

extern bool
bot_is_manifold(struct rt_bot_internal *bot);

__END_DECLS

#endif // TESSELLATE_EXEC_H

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
