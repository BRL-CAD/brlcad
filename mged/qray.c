/*
 *			Q R A Y . C
 *
 * Routines to set and get "Query Ray" variables.
 *
 * Source -
 *      SLAD CAD Team
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *      Robert G. Parker
 *
 * Copyright Notice -
 *      This software is Copyright (C) 1998 by the United States Army.
 *      All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./mged_dm.h"
#include "./qray.h"

static void qray_print_fmts();
static void qray_print_vars();
static int qray_get_fmt_index();

struct bu_vls qray_basename;
char qray_effects = 't';
int qray_cmd_echo = 0;
struct qray_fmt *qray_fmts;
static struct qray_color qray_odd_color = { 0, 255, 255 };
static struct qray_color qray_even_color = { 255, 255, 0 };
static struct qray_color qray_void_color = { 255, 0, 255 };
static struct qray_color qray_overlap_color = { 255, 255, 255 };

static struct qray_fmt_data def_qray_fmt_data[] = {
  {'r', "\"Origin (x y z) = (%.2f %.2f %.2f)  (h v d) = (%.2f %.2f %.2f)\\nDirection (x y z) = (%.4f %.4f %.4f)  (az el) = (%.2f %.2f)\\n\" x_orig y_orig z_orig h v d_orig x_dir y_dir z_dir a e"},
  {'h', "\"    Region Name               Entry (x y z)              LOS  Obliq_in\\n\""},
  {'p', "\"%-20s (%9.3f %9.3f %9.3f) %8.2f %8.3f\\n\" reg_name x_in y_in z_in los obliq_in"},
  {'f', "\"\""},
  {'m', "\"You missed the target\\n\""},
  {'o', "\"OVERLAP: '%s' and '%s' xyz_in=(%g %g %g) los=%g\\n\" ov_reg1_name ov_reg2_name ov_x_in ov_y_in ov_z_in ov_los"},
  {(char)NULL, (char *)NULL}
};

static char qray_syntax[] = "\
 qray vars			print a list of all variables (i.e. var = val)\n\
 qray basename [str]		set or get basename for query ray solids\n\
 qray effects [t|g|b]		set or get effects (i.e. text, graphical or both)\n\
 qray echo [0|1]		set or get command echo\n\
 qray oddcolor [r g b]		set or get color of odd partitions\n\
 qray evencolor [r g b]		set or get color of even partitions\n\
 qray voidcolor [r g b]		set or get color of void areas\n\
 qray overlapcolor [r g b]	set or get color of overlap areas\n\
 qray fmt [r|h|p|f|m|o [str]]	set or get format string(s)\n\
 qray [help]			print this help message\n\
";

int
f_qray(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct bu_vls vls;

  if(6 < argc){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help qray");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  /* print help message */
  if(argc == 1){
    Tcl_AppendResult(interp, "Usage:\n", qray_syntax, (char *)NULL);
    return TCL_OK;
  }

  if(strcmp(argv[1], "fmt") == 0){
    int i;

    if(argc == 2){			/* get all format strings */
      qray_print_fmts();
      return TCL_OK;
    }else if(argc == 3){		/* get particular format string */
      if((i = qray_get_fmt_index(*argv[2])) < 0){
	Tcl_AppendResult(interp,
			  "qray: unrecognized format type: '",
			 argv[2], "'\nUsage:\n", qray_syntax, (char *)NULL);
	return TCL_ERROR;
      }

      Tcl_AppendResult(interp, bu_vls_addr(&qray_fmts[i].fmt), (char *)NULL);
      return TCL_OK;
    }else if(argc == 4){		/* set value */
      if((i = qray_get_fmt_index(*argv[2])) < 0){
	Tcl_AppendResult(interp,
			  "qray: unrecognized format type: '",
			 argv[2], "'\nUsage:\n", qray_syntax, (char *)NULL);
	return TCL_ERROR;
      }

      bu_vls_trunc(&qray_fmts[i].fmt, 0);
      bu_vls_printf(&qray_fmts[i].fmt, "%s", argv[3]);
      return TCL_OK;
    }

    Tcl_AppendResult(interp,
		     "The 'qray fmt' command accepts 0, 1, or 2 arguments\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(argv[1], "basename") == 0){
    if(argc == 2){		/* get value */
      Tcl_AppendResult(interp, bu_vls_addr(&qray_basename), (char *)NULL);

      return TCL_OK;
    }else if(argc == 3){		/* set value */
      bu_vls_strcpy(&qray_basename, argv[2]);
      return TCL_OK;
    }

    Tcl_AppendResult(interp,
		     "The 'qray basename' command accepts 0 or 1 argument\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(argv[1], "effects") == 0){
    if(argc == 2){		/* get value */
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%c", qray_effects);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 3){		/* set value */
      if(*argv[2] != 't' && *argv[2] != 'g' && *argv[2] != 'b'){
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "qray effects: bad value - %s", argv[2]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_ERROR;
      }

      qray_effects = *argv[2];

      return TCL_OK;
    }

    Tcl_AppendResult(interp,
		     "The 'qray effects' command accepts 0 or 1 argument\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(argv[1], "echo") == 0){
    if(argc == 2){		/* get value */
      if(qray_cmd_echo)
	Tcl_AppendResult(interp, "1", (char *)NULL);
      else
	Tcl_AppendResult(interp, "0", (char *)NULL);

      return TCL_OK;
    }else if(argc == 3){		/* set value */
      int ival;

      if(sscanf(argv[2], "%d", &ival) < 1){
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "qray echo: bad value - %s", argv[2]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_ERROR;
      }

      if(ival)
	qray_cmd_echo = 1;
      else
	qray_cmd_echo = 0;

      return TCL_OK;
    }

    Tcl_AppendResult(interp,
		     "The 'qray echo' command accepts 0 or 1 argument\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(argv[1], "oddcolor") == 0){
    if(argc == 2){		/* get value */
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d %d %d",
		    qray_odd_color.r, qray_odd_color.g, qray_odd_color.b);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 5){		/* set value */
      int r, g, b;

      if(sscanf(argv[2], "%d", &r) != 1 ||
	 sscanf(argv[3], "%d", &g) != 1 ||
	 sscanf(argv[4], "%d", &b) != 1 ||
	 r < 0 || g < 0 || b < 0 ||
	 255 < r || 255 < g || 255 < b){
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "qray oddcolor %s %s %s - bad value",
		      argv[2], argv[3], argv[4]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
      }

      qray_odd_color.r = r;
      qray_odd_color.g = g;
      qray_odd_color.b = b;

      return TCL_OK;
    }

    Tcl_AppendResult(interp,
		     "The 'qray oddcolor' command accepts 0 or 3 arguments\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(argv[1], "evencolor") == 0){
    if(argc == 2){		/* get value */
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d %d %d",
		    qray_even_color.r, qray_even_color.g, qray_even_color.b);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 5){		/* set value */
      int r, g, b;

      if(sscanf(argv[2], "%d", &r) != 1 ||
	 sscanf(argv[3], "%d", &g) != 1 ||
	 sscanf(argv[4], "%d", &b) != 1 ||
	 r < 0 || g < 0 || b < 0 ||
	 255 < r || 255 < g || 255 < b){
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "qray evencolor %s %s %s - bad value",
		      argv[2], argv[3], argv[4]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
      }

      qray_even_color.r = r;
      qray_even_color.g = g;
      qray_even_color.b = b;

      return TCL_OK;
    }

    Tcl_AppendResult(interp,
		     "The 'qray evencolor' command accepts 0 or 3 arguments\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(argv[1], "voidcolor") == 0){
    if(argc == 2){		/* get value */
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d %d %d",
		    qray_void_color.r, qray_void_color.g, qray_void_color.b);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 5){		/* set value */
      int r, g, b;

      if(sscanf(argv[2], "%d", &r) != 1 ||
	 sscanf(argv[3], "%d", &g) != 1 ||
	 sscanf(argv[4], "%d", &b) != 1 ||
	 r < 0 || g < 0 || b < 0 ||
	 255 < r || 255 < g || 255 < b){
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "qray voidcolor %s %s %s - bad value",
		      argv[2], argv[3], argv[4]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
      }

      qray_void_color.r = r;
      qray_void_color.g = g;
      qray_void_color.b = b;

      return TCL_OK;
    }

    Tcl_AppendResult(interp,
		     "The 'qray voidcolor' command accepts 0 or 3 arguments\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(argv[1], "overlapcolor") == 0){
    if(argc == 2){		/* get value */
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d %d %d",
		    qray_overlap_color.r, qray_overlap_color.g, qray_overlap_color.b);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }else if(argc == 5){		/* set value */
      int r, g, b;

      if(sscanf(argv[2], "%d", &r) != 1 ||
	 sscanf(argv[3], "%d", &g) != 1 ||
	 sscanf(argv[4], "%d", &b) != 1 ||
	 r < 0 || g < 0 || b < 0 ||
	 255 < r || 255 < g || 255 < b){
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "qray overlapcolor %s %s %s - bad value",
		      argv[2], argv[3], argv[4]);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
      }

      qray_overlap_color.r = r;
      qray_overlap_color.g = g;
      qray_overlap_color.b = b;

      return TCL_OK;
    }

    Tcl_AppendResult(interp,
		     "The 'qray overlapcolor' command accepts 0 or 3 arguments\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(argv[1], "vars") == 0){
    qray_print_vars();
    return TCL_OK;
  }

  if(strcmp(argv[1], "help") == 0){
    Tcl_AppendResult(interp, "Usage:\n", qray_syntax, (char *)NULL);
    return TCL_OK;
  }

  Tcl_AppendResult(interp, "qray: unrecognized command: '",
		   argv[1], "'\nUsage:\n", qray_syntax, (char *)NULL);
  return TCL_ERROR;
}

static void
qray_print_fmts()
{
  int i;

  for(i = 0; qray_fmts[i].type != (char)NULL; ++i)
    Tcl_AppendResult(interp, bu_vls_addr(&qray_fmts[i].fmt),
		     "\n", (char *)NULL);
}

static void
qray_print_vars()
{
  struct bu_vls vls;

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "basename = %s\n", bu_vls_addr(&qray_basename));
  bu_vls_printf(&vls, "effects = %c\n", qray_effects);
  bu_vls_printf(&vls, "echo = %d\n", qray_cmd_echo);
  bu_vls_printf(&vls, "oddcolor = %d %d %d\n",
		qray_odd_color.r, qray_odd_color.g, qray_odd_color.b);
  bu_vls_printf(&vls, "evencolor = %d %d %d\n",
		qray_even_color.r, qray_even_color.g, qray_even_color.b);
  bu_vls_printf(&vls, "voidcolor = %d %d %d\n",
		qray_void_color.r, qray_void_color.g, qray_void_color.b);
  bu_vls_printf(&vls, "overlapcolor = %d %d %d\n",
		qray_overlap_color.r, qray_overlap_color.g, qray_overlap_color.b);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  qray_print_fmts();
}

static int
qray_get_fmt_index(c)
char c;
{
  int i;

  for(i = 0; qray_fmts[i].type != (char)NULL; ++i)
    if(c == qray_fmts[i].type)
      return i;

  return -1;
}

void
init_qray()
{
  register int i;
  register int n = 0;
  struct qray_fmt_data *qfdp;

  bu_vls_init(&qray_basename);
  bu_vls_strcpy(&qray_basename, QRAY_BASENAME);

  /* count the number of default format types */
  for(qfdp = def_qray_fmt_data; qfdp->fmt != (char *)NULL; ++qfdp)
    ++n;

  qray_fmts = (struct qray_fmt *)bu_malloc(sizeof(struct qray_fmt) * n + 1, "qray_fmts");

  for(i = 0; i < n; ++i){
    qray_fmts[i].type = def_qray_fmt_data[i].type;
    bu_vls_init(&qray_fmts[i].fmt);
    bu_vls_strcpy(&qray_fmts[i].fmt, def_qray_fmt_data[i].fmt);
  }

  qray_fmts[i].type = (char)NULL;
}

void
qray_data_to_vlist(vbp, headp, dir, do_overlaps)
struct rt_vlblock *vbp;
struct qray_dataList *headp;
vect_t dir;
int do_overlaps;
{
  register int i = 1;			/* start out odd */
  register struct bu_list *vhead;
  register struct qray_dataList *ndlp;
  vect_t in_pt, out_pt;
  vect_t last_out_pt;

  for(BU_LIST_FOR(ndlp, qray_dataList, &headp->l)){
    if(do_overlaps)
      vhead = rt_vlblock_find(vbp,
			      qray_overlap_color.r,
			      qray_overlap_color.g,
			      qray_overlap_color.b);
    else if(i % 2)
      vhead = rt_vlblock_find(vbp,
			      qray_odd_color.r,
			      qray_odd_color.g,
			      qray_odd_color.b);
    else
      vhead = rt_vlblock_find(vbp,
			      qray_even_color.r,
			      qray_even_color.g,
			      qray_even_color.b);

    VSET(in_pt, ndlp->x_in, ndlp->y_in, ndlp->z_in);
    VJOIN1(out_pt, in_pt, ndlp->los, dir);
    VSCALE(in_pt, in_pt, local2base);
    VSCALE(out_pt, out_pt, local2base);
    RT_ADD_VLIST( vhead, in_pt, RT_VLIST_LINE_MOVE );
    RT_ADD_VLIST( vhead, out_pt, RT_VLIST_LINE_DRAW );

    if(!do_overlaps && i > 1 && !VAPPROXEQUAL(last_out_pt,in_pt,SQRT_SMALL_FASTF)){
      vhead = rt_vlblock_find(vbp,
			      qray_void_color.r,
			      qray_void_color.g,
			      qray_void_color.b);
      RT_ADD_VLIST( vhead, last_out_pt, RT_VLIST_LINE_MOVE );
      RT_ADD_VLIST( vhead, in_pt, RT_VLIST_LINE_DRAW );
    }

    VMOVE(last_out_pt, out_pt);
    ++i;
  }
}
