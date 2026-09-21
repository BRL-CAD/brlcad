/*                   G Q A _ A N A L Y Z E . C P P
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

#include "common.h"

#include <utility>
#include <vector>

#include "analyze/gqa.h"
#include "bv/vlist.h"
#include "bu/log.h"
#include "ged.h"
#include "rt/vlist.h"
#include "../ged_private.h"


static int
load_densities(struct analyze_densities **densities, char **source,
    const char *filename, void *data)
{
    struct ged *gedp = static_cast<struct ged *>(data);
    return _ged_read_densities(densities, source, gedp, filename, 0) ==
        BRLCAD_OK ? ANALYZE_OK : ANALYZE_ERROR;
}


static void
report_progress(const char *message, void *UNUSED(data))
{
    bu_log("%s\n", message);
}


struct PlotViewState {
    struct ged *gedp = NULL;
    std::vector<analyze_gqa_plot_line> lines;
};

static const char GQA_PLOT_VIEW_NAME[] = "gqa::analysis";


struct PlotViewUpdate {
    PlotViewState *state;
    int action;
    std::vector<analyze_gqa_plot_line> lines;
};


static void
update_plot_view(void *data)
{
    PlotViewUpdate *update = static_cast<PlotViewUpdate *>(data);
    PlotViewState &state = *update->state;
    if (update->action == ANALYZE_GQA_PLOT_REPLACE)
	state.lines = std::move(update->lines);
    else
	state.lines.insert(state.lines.end(), update->lines.begin(),
	    update->lines.end());

    struct bv_vlblock *vbp = rt_vlblock_init();
    for (const auto &line : state.lines) {
	struct bu_list *vhead = bv_vlblock_find(vbp,
	    line.color[0], line.color[1], line.color[2]);
	BV_ADD_VLIST(vbp->free_vlist_hd, vhead, line.start,
	    BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(vbp->free_vlist_hd, vhead, line.end,
	    BV_VLIST_LINE_DRAW);
    }
    bv_vlblock_obj(vbp, state.gedp->ged_gvp, GQA_PLOT_VIEW_NAME);
    bv_vlblock_free(vbp);
    ged_refresh_cb(state.gedp);
}


static void
report_plot(const struct analyze_gqa_plot_batch *batch, void *data)
{
    PlotViewState *state = static_cast<PlotViewState *>(data);
    PlotViewUpdate update = {state, batch->action, {}};
    if (batch->lines && batch->line_count)
	update.lines.assign(batch->lines, batch->lines + batch->line_count);

    struct ged *gedp = state->gedp;
    gedp->ged_run_on_main_thread(gedp->ged_main_thread_clientdata,
	update_plot_view, &update);
}


extern "C" int
ged_gqa_analyze(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    PlotViewState plot_state;
    plot_state.gedp = gedp;
    struct analyze_gqa_context context = {
        gedp->dbip, gedp->ged_result_str, load_densities, gedp,
	report_progress, NULL,
	gedp->ged_gvp && gedp->ged_run_on_main_thread ? report_plot : NULL,
	&plot_state};
    return analyze_gqa(&context, argc, argv) == ANALYZE_OK ?
        BRLCAD_OK : BRLCAD_ERROR;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
