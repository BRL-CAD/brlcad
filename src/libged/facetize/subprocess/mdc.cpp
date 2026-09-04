/*                         M D C . C P P
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
/** @file mdc.cpp
 *
 * Facetize adapter for libanalyze's ray-driven manifold dual contourer.
 */

#include "common.h"

#include "analyze/contour.h"
#include "bu/log.h"
#include "rt/primitives/bot.h"

#include "./tessellate.h"
#include "../validation.h"


static const char *
mdc_status_message(enum analyze_mdc_status status)
{
    switch (status) {
	case ANALYZE_MDC_OK:
	    return "success";
	case ANALYZE_MDC_INVALID_INPUT:
	    return "invalid settings or input";
	case ANALYZE_MDC_PREP_FAILED:
	    return "raytrace preparation failed";
	case ANALYZE_MDC_NO_SURFACE:
	    return "no closed surface was sampled";
	case ANALYZE_MDC_RAY_LIMIT:
	    return "ray limit reached";
	case ANALYZE_MDC_TIMEOUT:
	    return "time limit reached";
	case ANALYZE_MDC_MEMORY_LIMIT:
	    return "free-memory limit reached";
	case ANALYZE_MDC_AMBIGUOUS:
	    return "surface remained ambiguous at the maximum depth";
	case ANALYZE_MDC_NOT_MANIFOLD:
	    return "generated mesh was not a closed oriented manifold";
    }

    return "unknown error";
}


int
mdc_mesh(struct rt_bot_internal **output, struct db_i *dbip,
	const char *object, tess_opts *settings)
{
    if (!output || !dbip || !object || !settings)
	return BRLCAD_ERROR;
    *output = NULL;

    const mdc_opts &options = settings->mdc_options;
    if (!options.invalid_option.empty()) {
	bu_log("MDC: invalid option for %s: %s\n", object,
		options.invalid_option.c_str());
	return BRLCAD_ERROR;
    }

    struct analyze_mdc_params params = ANALYZE_MDC_PARAMS_DEFAULT;
    params.feature_size = options.feature_size;
    params.max_rays = static_cast<size_t>(options.max_rays);
    params.minimum_free_mem =
	static_cast<size_t>(options.minimum_free_mem);
    params.min_depth = options.min_depth;
    params.max_depth = options.max_depth;
    params.max_time = options.max_time;
    params.verbosity = options.verbosity;

    int *faces = NULL;
    point_t *vertices = NULL;
    size_t face_count = 0;
    size_t vertex_count = 0;
    struct bu_hook_list saved_hooks = BU_HOOK_LIST_INIT_ZERO;
    bool silence_logs = (options.verbosity == 0);
    if (silence_logs)
	facetize_log_hooks_silence(&saved_hooks);
    enum analyze_mdc_status status = analyze_mdc(&faces, &face_count,
	    &vertices, &vertex_count, object, dbip, &params);
    if (silence_logs)
	facetize_log_hooks_restore(&saved_hooks);

    if (status != ANALYZE_MDC_OK) {
	bu_log("MDC: failed to tessellate %s: %s\n", object,
		mdc_status_message(status));
	return BRLCAD_ERROR;
    }

    struct rt_bot_internal *bot;
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_CCW;
    bot->num_faces = face_count;
    bot->num_vertices = vertex_count;
    bot->faces = faces;
    bot->vertices = reinterpret_cast<fastf_t *>(vertices);

    if (!bot_is_manifold(bot)) {
	bu_log("MDC: generated mesh for %s is not accepted by Manifold\n",
		object);
	_tess_facetize_free_bot(bot);
	return BRLCAD_ERROR;
    }

    *output = bot;
    return BRLCAD_OK;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
