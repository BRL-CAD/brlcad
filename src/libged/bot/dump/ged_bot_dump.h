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
    OTYPE_DXF = 1,
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

struct _ged_bot_dump_client_data {
    struct ged *gedp;
    FILE *fp;
    int fd;
    char *file_ext;

    int using_dbot_dump;
    struct bu_list HeadObjMaterials;
    struct bu_vls obj_materials_file;
    FILE *obj_materials_fp;
    int num_obj_materials;
    int curr_obj_red;
    int curr_obj_green;
    int curr_obj_blue;
    fastf_t curr_obj_alpha;

    enum otype output_type;
    int binary;
    int normals;
    fastf_t cfactor;
    char *output_file;	/* output filename */
    char *output_directory;	/* directory name to hold output files */
    unsigned int total_faces;
    int v_offset;
    int curr_line_num;

    int curr_body_id;
    int curr_lump_id;
    int curr_shell_id;
    int curr_face_id;
    int curr_loop_id;
    int curr_edge_id;
};

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
