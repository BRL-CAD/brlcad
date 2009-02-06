/*                          C M D . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2009 United States Government as represented by
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
/** @addtogroup libbu */
/** @{ */
/** @file cmd.h
 *
 * @brief
 *   Command history data structures
 *
 */
#ifndef __CMD_H__
#define __CMD_H__

#include "common.h"

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#include <time.h>

#include "bu.h"

#define MAXARGS 9000
#define CMD_NULL (int (*)(ClientData, Tcl_Interp *, int, const char **))NULL
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

BU_EXPORT BU_EXTERN(int bu_cmd, (ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[], struct bu_cmdtab *cmds, int cmd_index));
BU_EXPORT BU_EXTERN(void bu_register_cmds, (Tcl_Interp *interp, struct bu_cmdtab *cmds));

BU_EXPORT BU_EXTERN(int bu_cmdhist_history, (ClientData clientData, Tcl_Interp *interp, int argc, const char **argv));
BU_EXPORT BU_EXTERN(int bu_cmdhist_add, (ClientData clientData, Tcl_Interp *interp, int argc, const char **argv));
BU_EXPORT BU_EXTERN(int bu_cmdhist_curr, (ClientData clientData, Tcl_Interp *interp, int argc, const char **argv));
BU_EXPORT BU_EXTERN(int bu_cmdhist_next, (ClientData clientData, Tcl_Interp *interp, int argc, const char **argv));
BU_EXPORT BU_EXTERN(int bu_cmdhist_prev, (ClientData clientData, Tcl_Interp *interp, int argc, const char **argv));

BU_EXPORT BU_EXTERN(int cho_open_tcl, (ClientData clientData, Tcl_Interp *interp, int argc, char **argv));

#endif  /* __CMD_H__ */
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
