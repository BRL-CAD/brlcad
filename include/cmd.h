/*                          C M D . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file cmd.h
 *
 */
#ifndef SEEN_CMD_H
#define SEEN_CMD_H

#ifdef USE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>
#include "bu.h"

#define MAXARGS 9000
#define CMD_NULL (int (*)())NULL
#define CMDHIST_NULL (struct bu_cmdhist *)NULL
#define CMDHIST_OBJ_NULL (struct bu_cmdhist_obj *)NULL

struct bu_cmdhist {
  struct bu_list l;
  struct bu_vls h_command;
  struct timeval h_start;
  struct timeval h_finish;
  int h_status;
};

struct bu_cmdhist_obj {
  struct bu_list l;
  struct bu_vls cho_name;
  struct bu_cmdhist cho_head;
  struct bu_cmdhist *cho_curr;
};

BU_EXPORT BU_EXTERN(int bu_cmd,
		    ());
BU_EXPORT BU_EXTERN(void bu_register_cmds,
		    ());

/* bu_cmdhist routines are defined in libbu/cmdhist.c */
BU_EXPORT BU_EXTERN(int bu_cmdhist_history,
		    ());
BU_EXPORT BU_EXTERN(int bu_cmdhist_add,
		    ());
BU_EXPORT BU_EXTERN(int bu_cmdhist_curr,
		    ());
BU_EXPORT BU_EXTERN(int bu_cmdhist_next,
		    ());
BU_EXPORT BU_EXTERN(int bu_cmdhist_prev,
		    ());

BU_EXPORT BU_EXTERN(int cho_open_tcl,
		    ());

#endif /* SEEN_CMD_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
