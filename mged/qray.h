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
 *      This software is Copyright (C) 1998 by the United States Army.
 *      All rights reserved.
 *
 */

#define QRAY_BASENAME "query_ray"
#define QRAY_TEXT	(qray_effects == 't' ||\
			 qray_effects == 'b')
#define QRAY_GRAPHICS	(qray_effects == 'g' ||\
			 qray_effects == 'b')
#define QRAY_BOTH	(qray_effects == 'b')
#define QRAY_FORMAT_P "fmt p \"%e %e %e %e\\n\" x_in y_in z_in los"
#define QRAY_FORMAT_O "fmt p \"\"; fmt o \"%e %e %e %e\\n\" ov_x_in ov_y_in ov_z_in ov_los"
#define QRAY_FORMAT_NULL "fmt r \"\"; fmt h \"\"; fmt p \"\"; fmt m \"\"; fmt o \"\"; fmt f \"\""

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
extern char qray_effects;
extern int qray_cmd_echo;
extern struct qray_fmt *qray_fmts;

extern void qray_data_to_vlist();
#endif
