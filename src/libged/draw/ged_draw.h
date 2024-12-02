/*                   G E D _ D R A W . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file ged_brep.h
 *
 * Private header for libged draw cmd.
 *
 */

#ifndef LIBGED_DRAW_GED_PRIVATE_H
#define LIBGED_DRAW_GED_PRIVATE_H

#include "common.h"

#include "ged.h"
#include "../ged_private.h"

__BEGIN_DECLS

struct _ged_client_data {
    uint32_t magic;  /* add this so a pointer to the struct and a pointer to any of its active elements will differ */
    struct ged *gedp;
    struct rt_wdb *wdbp;
    struct display_list *gdlp;
    int fastpath_count;			/* statistics */
    struct bv_vlblock *draw_edge_uses_vbp;
    struct bview *v;

    /* bigE related members */
    struct application *ap;
    struct bu_ptbl leaf_list;
    struct rt_i *rtip;
    time_t start_time;
    time_t etime;
    long nvectors;
    int do_polysolids;
    int num_halfs;
    int autoview;
    int nmg_fast_wireframe_draw;

    // Debugging plotting specific options.  These don't actually belong with
    // the drawing routines at all - they are analogous to the brep debugging
    // plotting routines, and belong with nmg/bot/etc. plot subcommands.
    int draw_nmg_only;
    int nmg_triangulate;
    int draw_normals;
    int draw_no_surfaces;
    int shade_per_vertex_normals;
    int draw_edge_uses;
    int do_not_draw_nmg_solids_during_debugging;

    struct bv_obj_settings vs;
};

extern int _ged_cm_vsize(const int argc, const char **argv);
extern int _ged_cm_eyept(const int argc, const char **argv);
extern int _ged_cm_lookat_pt(const int argc, const char **argv);
extern int _ged_cm_vrot(const int argc, const char **argv);
extern int _ged_cm_orientation(const int argc, const char **argv);
extern int _ged_cm_set(const int argc, const char **argv);
extern int _ged_cm_end(const int argc, const char **argv);
extern int _ged_cm_null(const int argc, const char **argv);

extern void _ged_drawH_part2(int dashflag, struct bu_list *vhead, const struct db_full_path *pathp, struct db_tree_state *tsp, struct _ged_client_data *dgcdp);

extern int _ged_drawtrees(struct ged *gedp,
			  int argc,
			  const char *argv[],
			  int kind,
			  struct _ged_client_data *_dgcdp);

extern int ged_E_core(struct ged *gedp, int argc, const char *argv[]);

__END_DECLS

#endif /* LIBGED_DRAW_GED_PRIVATE_H */

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
