/*                         G Q A . H
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
/** @addtogroup libanalyze */
/** @{ */
/** @file analyze/gqa.h */

#ifndef ANALYZE_GQA_H
#define ANALYZE_GQA_H

#include <stddef.h>

#include "common.h"
#include "analyze/defines.h"

__BEGIN_DECLS

struct analyze_densities;
struct bu_vls;
struct db_i;

/**
 * Load the density table selected by @p filename, or the caller's normal
 * default density source when it is NULL.  On success, ownership of both
 * returned allocations passes to analyze_gqa(); @p source must be allocated
 * with bu_malloc() and @p densities must be releasable by
 * analyze_densities_destroy().
 */
typedef int (*analyze_gqa_density_loader)(
    struct analyze_densities **densities,
    char **source,
    const char *filename,
    void *data);

/** Receive an informational progress line during a long analysis. */
typedef void (*analyze_gqa_progress_handler)(const char *message, void *data);

/** Categories used to separate quantitative-analysis plot output. */
enum analyze_gqa_plot_category {
    ANALYZE_GQA_PLOT_VOLUME = 0,
    ANALYZE_GQA_PLOT_GAP,
    ANALYZE_GQA_PLOT_OVERLAP,
    ANALYZE_GQA_PLOT_ADJACENT_AIR,
    ANALYZE_GQA_PLOT_EXPOSED_AIR,
    ANALYZE_GQA_PLOT_CATEGORY_COUNT
};

/** Whether a plot batch extends or replaces the preceding visualization. */
enum analyze_gqa_plot_action {
    ANALYZE_GQA_PLOT_APPEND = 0,
    ANALYZE_GQA_PLOT_REPLACE
};

/** One colored line in model coordinates. */
struct analyze_gqa_plot_line {
    int category;
    unsigned char color[3];
    double start[3];
    double end[3];
};

/**
 * Immutable plot update.  Storage is valid only for the duration of the
 * callback.  An empty replacement clears a previous visualization.
 */
struct analyze_gqa_plot_batch {
    int action;
    const struct analyze_gqa_plot_line *lines;
    size_t line_count;
};

/** Receive a serialized plot update after a completed ray batch. */
typedef void (*analyze_gqa_plot_handler)(
    const struct analyze_gqa_plot_batch *batch,
    void *data);

/**
 * Invocation-local services needed by the quantitative analysis engine.
 *
 * The database and result string remain caller-owned.  A density loader is
 * required only for analyses that request mass-dependent measurements.  The
 * progress and plot handlers are optional.  Plot handlers run serially after
 * completed ray batches and must not call back into the active analysis.
 */
struct analyze_gqa_context {
    struct db_i *dbip;
    struct bu_vls *result;
    analyze_gqa_density_loader load_densities;
    void *density_data;
    analyze_gqa_progress_handler report_progress;
    void *progress_data;
    analyze_gqa_plot_handler report_plot;
    void *plot_data;
};

/**
 * Locate the model filename in a standalone @c gqa @c --analyze command line.
 * This lets a caller open the database before invoking analyze_gqa() without
 * duplicating the engine's option-arity rules.
 *
 * @return the zero-based argument index, or -1 for malformed input.
 */
ANALYZE_EXPORT extern int analyze_gqa_model_argument(
    int argc,
    const char *argv[]);

/**
 * Run the versioned GQA analysis interface.
 *
 * @p argv uses the command form accepted by @c gqa @c --analyze; argv[0] is
 * the command name and argv[1] is @c --analyze.  The selected objects follow
 * all options.  Results and diagnostics replace context->result.
 *
 * The function does not retain any context or geometry pointers after return.
 *
 * @return ANALYZE_OK on success; ANALYZE_ERROR on invalid input or failure.
 */
ANALYZE_EXPORT extern int analyze_gqa(
    const struct analyze_gqa_context *context,
    int argc,
    const char *argv[]);

__END_DECLS

#endif /* ANALYZE_GQA_H */

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
