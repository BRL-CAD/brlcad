#ifndef SEEN_QRAY_H
#define SEEN_QRAY_H

/*
 *
 *			Q R A Y . H
 *
 * Header file for "Query Ray" variables.
 *
 * Source -
 *	SLAD CAD Team
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005
 *
 * Copyright Notice -
 *      This software is Copyright (C) 1998-2004 by the United States Army.
 *      All rights reserved.
 *
 */

#define DG_QRAY_BASENAME "query_ray"
#define DG_QRAY_TEXT(_dgop) ((_dgop)->dgo_qray_effects == 't' || (_dgop)->dgo_qray_effects == 'b')
#define DG_QRAY_GRAPHICS(_dgop) ((_dgop)->dgo_qray_effects == 'g' || (_dgop)->dgo_qray_effects == 'b')
#define DG_QRAY_BOTH ((_dgop)->dgo_qray_effects == 'b')
#define DG_QRAY_FORMAT_P "fmt p \"%e %e %e %e\\n\" x_in y_in z_in los"
#define DG_QRAY_FORMAT_O "fmt r \"\\n\" ; fmt p \"\"; fmt o \"%e %e %e %e\\n\" ov_x_in ov_y_in ov_z_in ov_los"
#define DG_QRAY_FORMAT_NULL "fmt r \"\"; fmt h \"\"; fmt p \"\"; fmt m \"\"; fmt o \"\"; fmt f \"\""

struct dg_qray_fmt_data {
  char type;
  char *fmt;
};

struct dg_qray_dataList {
  struct bu_list l;
  fastf_t x_in;
  fastf_t y_in;
  fastf_t z_in;
  fastf_t los;
};

extern void qray_data_to_vlist();
#endif
