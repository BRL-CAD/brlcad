/*                         Q R A Y . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/qray.h
 *
 * Header file for "Query Ray" variables.
 *
 */

#ifndef SEEN_QRAY_H
#define SEEN_QRAY_H

#include "common.h"

#include "bu.h"

#define QRAY_BASENAME "query_ray"
#define QRAY_TEXT (qray_effects == 't' || qray_effects == 'b')
#define QRAY_GRAPHICS (qray_effects == 'g' || qray_effects == 'b')
#define QRAY_BOTH (qray_effects == 'b')

#ifndef _WIN32
#  define QRAY_FORMAT_P "fmt p \"%e %e %e %e\\n\" x_in y_in z_in los"
#  define QRAY_FORMAT_O "fmt r \"\\n\" ; fmt p \"\"; fmt o \"%e %e %e %e\\n\" ov_x_in ov_y_in ov_z_in ov_los"
#  define QRAY_FORMAT_NULL "fmt r \"\"; fmt h \"\"; fmt p \"\"; fmt m \"\"; fmt o \"\"; fmt f \"\""
#else
#  define QRAY_FORMAT_P "fmt p \\\"%e %e %e %e\\\\n\\\" x_in y_in z_in los"
#  define QRAY_FORMAT_O "fmt r \\\"\\\\n\\\" ; fmt p \\\"\\\"; fmt o \\\"%e %e %e %e\\\\n\\\" ov_x_in ov_y_in ov_z_in ov_los"
#  define QRAY_FORMAT_NULL "fmt r \\\"\\\"; fmt h \\\"\\\"; fmt p \\\"\\\"; fmt m \\\"\\\"; fmt o \\\"\\\"; fmt f \\\"\\\"; fmt g \\\"\\\""
#endif

struct qray_color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};


struct qray_fmt {
    char type;
    struct bu_vls fmt;
};


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


extern struct bu_vls qray_basename;
extern struct bu_vls qray_script;
extern char qray_effects;
extern int qray_cmd_echo;
extern struct qray_fmt *qray_fmts;

extern void qray_data_to_vlist(struct bn_vlblock *vbp, struct qray_dataList *headp, fastf_t *dir, int do_overlaps);
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
