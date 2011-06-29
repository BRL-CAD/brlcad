/*                          Q R A Y . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/qray.h
 *
 * Private header file for "Query Ray" variables.
 *
 */
/** @} */

#include "ged.h"

#ifndef SEEN_QRAY_H
#define SEEN_QRAY_H

#define DG_QRAY_TEXT(_dgop) ((_dgop)->gd_qray_effects == 't' || (_dgop)->gd_qray_effects == 'b')
#define DG_QRAY_GRAPHICS(_dgop) ((_dgop)->gd_qray_effects == 'g' || (_dgop)->gd_qray_effects == 'b')

#define DG_QRAY_BASENAME "query_ray"
#define DG_QRAY_BOTH ((_dgop)->gd_qray_effects == 'b')
#define DG_QRAY_FORMAT_P "fmt p \"%e %e %e %e\\n\" x_in y_in z_in los"
#define DG_QRAY_FORMAT_O "fmt r \"\\n\" ; fmt p \"\"; fmt o \"%e %e %e %e\\n\" ov_x_in ov_y_in ov_z_in ov_los"
#define DG_QRAY_FORMAT_NULL "fmt r \"\"; fmt h \"\"; fmt p \"\"; fmt m \"\"; fmt o \"\"; fmt f \"\"; fmt g \"\""

#define DG_INV_GED 0.00048828125
#define DG_GED_MAX 2047.0
#define DG_GED_MIN -2048.0

struct qray_fmt_data {
    char type;
    char *fmt;
};


struct qray_dataList {
    struct bu_list l;
    fastf_t x_in;
    fastf_t y_in;
    fastf_t z_in;
    fastf_t los;
};


/* defined in qray.c */
extern void qray_init(struct ged_drawable *gdp);
extern void qray_free(struct ged_drawable *gdp);
extern void qray_data_to_vlist(struct ged *gdp, struct bn_vlblock *vbp, struct qray_dataList *headp, vect_t dir, int do_overlaps);

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
