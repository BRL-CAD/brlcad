/*                  G E D _ B O T _ D U M P . H
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
/** @file ged_bot_dump.h
 *
 * Private header for libged bot dump cmd.
 *
 */

#ifndef LIBGED_BOT_DUMP_GED_PRIVATE_H
#define LIBGED_BOT_DUMP_GED_PRIVATE_H

#include "common.h"

#include "bio.h" // for FILE

#include "bu/list.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "ged.h"

__BEGIN_DECLS

enum otype {
    OTYPE_UNSET = 0,
    OTYPE_DXF,
    OTYPE_OBJ,
    OTYPE_SAT,
    OTYPE_STL
};

struct _ged_obj_material {
    struct bu_list l;
    struct bu_vls name;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    fastf_t a;
};

struct bot_dump_obj {
    struct bu_list HeadObjMaterials;
    struct bu_vls obj_materials_file;
    FILE *obj_materials_fp;
    int num_obj_materials;
    int curr_obj_red;
    int curr_obj_green;
    int curr_obj_blue;
    fastf_t curr_obj_alpha;
    int v_offset;
};

struct bot_dump_sat {
    int curr_body_id;
    int curr_lump_id;
    int curr_shell_id;
    int curr_face_id;
    int curr_loop_id;
    int curr_edge_id;
    int curr_line_num;
};

struct _ged_bot_dump_client_data {
    struct ged *gedp;
    FILE *fp;
    int fd;
    const char *file_ext;

    int using_dbot_dump;
    int view_data;
    int material_info;

    enum otype output_type;
    int binary;
    int normals;
    fastf_t cfactor;
    struct bu_vls output_file;	/* output filename */
    struct bu_vls output_directory;	/* directory name to hold output files */
    unsigned int total_faces;

    // Format specific info
    struct bot_dump_obj obj;
    struct bot_dump_sat sat;
};

#define V3ARGS_SCALE(_a) (_a)[X]*d->cfactor, (_a)[Y]*d->cfactor, (_a)[Z]*d->cfactor

extern int dxf_setup(struct _ged_bot_dump_client_data *d, const char *fname, const char *objname, const char *gname);
extern int dxf_finish(struct _ged_bot_dump_client_data *d);
extern void dxf_write_bot(struct _ged_bot_dump_client_data *d, struct rt_bot_internal *bot, FILE *fp, char *name);

extern int obj_setup(struct _ged_bot_dump_client_data *d, const char *fname, int mtl_only);
extern int obj_finish(struct _ged_bot_dump_client_data *d);
extern void obj_free_materials(struct bot_dump_obj *o);
extern void obj_write_bot(struct _ged_bot_dump_client_data *d, struct rt_bot_internal *bot, FILE *fp, char *name);
extern void obj_write_data(struct _ged_bot_dump_client_data *d, struct ged *gedp, FILE *fp);

extern int sat_setup(struct _ged_bot_dump_client_data *d, const char *fname);
extern int sat_finish(struct _ged_bot_dump_client_data *d);
extern void sat_write_header(FILE *fp);
extern void sat_write_bot(struct _ged_bot_dump_client_data *d, struct rt_bot_internal *bot, FILE *fp, char *name);

extern int stl_setup(struct _ged_bot_dump_client_data *d, const char *fname);
extern int stl_finish(struct _ged_bot_dump_client_data *d);
extern void stl_write_bot(struct _ged_bot_dump_client_data *d, struct rt_bot_internal *bot, FILE *fp, char *name);
extern void stl_write_bot_binary(struct _ged_bot_dump_client_data *d, struct rt_bot_internal *bot, int fd, char *name);

__END_DECLS

#endif /* LIBGED_BOT_DUMP_GED_PRIVATE_H */

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
