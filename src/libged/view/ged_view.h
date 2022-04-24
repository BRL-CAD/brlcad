/*                    G E D _ V I E W . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file ged_view.h
 *
 * Private header for libged view cmd.
 *
 */

#ifndef GED_VIEW_SUBCMDS_H
#define GED_VIEW_SUBCMDS_H

#include "common.h"

#include "ged.h"

__BEGIN_DECLS

struct _ged_view_info {
    struct ged *gedp;
    int verbosity;
    const struct bu_cmdtab *cmds;
    struct bu_opt_desc *gopts;
    const char *vobj;
    struct bv_scene_obj *s;
};
extern int _view_cmd_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps);
extern int _view_cmd_lines(void *bs, int argc, const char **argv);
extern int _view_cmd_axes(void *bs, int argc, const char **argv);
extern int _view_cmd_labels(void *bs, int argc, const char **argv);
extern int _view_cmd_lod(void *bs, int argc, const char **argv);
extern int _view_cmd_polygons(void *bs, int argc, const char **argv);
extern int _view_cmd_objs(void *bs, int argc, const char **argv);
extern int _view_cmd_gobjs(void *bs, int argc, const char **argv);

extern int ged_aet_core(struct ged *gedp, int argc, const char **argv);
extern int ged_center_core(struct ged *gedp, int argc, const char **argv);
extern int ged_eye_core(struct ged *gedp, int argc, const char **argv);
extern int ged_faceplate_core(struct ged *gedp, int argc, const char **argv);
extern int ged_quat_core(struct ged *gedp, int argc, const char **argv);
extern int ged_size_core(struct ged *gedp, int argc, const char **argv);
extern int ged_view_snap(struct ged *gedp, int argc, const char *argv[]);
extern int ged_ypr_core(struct ged *gedp, int argc, const char **argv);

__END_DECLS

#endif /* GED_VIEW_SUBCMDS_H */

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
