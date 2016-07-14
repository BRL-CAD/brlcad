/*                          C M D . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2016 United States Government as represented by
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

/** @addtogroup bu_cmd
 * @brief
 * Routine(s) for processing subcommands
 */
/** @{ */
/** @file bu/cmd.h */

#ifndef CMD_H
#define CMD_H

#include "common.h"

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#include <time.h>

#include "bsocket.h" /* for timeval */
#include "bio.h"

#define BU_CMD_NULL (int (*)(void *, int, const char **))NULL

#include "bu/defines.h"
#include "bu/list.h"
#include "bu/log.h"
#include "bu/vls.h"

/**
 * Generic keyword-to-command callback interface intended for use with
 * bu_cmd().
 */
struct bu_cmdtab {
    const char *ct_name;
    int (*ct_func)(void *data, int argc, const char *argv[]);
};

struct bu_cmdhist {
    struct bu_list l;
    struct bu_vls h_command;
    struct timeval h_start;
    struct timeval h_finish;
    int h_status;
};
#define BU_CMDHIST_NULL (struct bu_cmdhist *)NULL

struct bu_cmdhist_obj {
    struct bu_list l;
    struct bu_vls cho_name;
    struct bu_cmdhist cho_head;
    struct bu_cmdhist *cho_curr;
};
#define BU_CMDHIST_OBJ_NULL (struct bu_cmdhist_obj *)NULL

__BEGIN_DECLS

/** @brief Routine(s) for processing subcommands */

/**
 * This function is intended to be used for parsing subcommands.  If
 * the command is found in the array of commands, the associated
 * function is called. Otherwise, an error message is created and
 * added to interp.
 *
 * @param cmds		- commands and related function pointers
 * @param argc		- number of arguments in argv
 * @param argv		- command to execute and its arguments
 * @param cmd_index	- indicates which argv element holds the subcommand
 * @param data		- data/state associated with the command
 * @param result	- if non-NULL, return value from the command
 *
 * @return BRLCAD_OK if command was found, otherwise, BRLCAD_ERROR.
 */
BU_EXPORT extern int bu_cmd(const struct bu_cmdtab *cmds, int argc, const char *argv[], int cmd_index, void *data, int *result);

/** @brief Routines for maintaining a command history */

/**
 * Prints out the command history.
 *
 * USAGE:
 * history [-delays] [-outfile filename]
 */
BU_EXPORT extern int bu_cmdhist_history(void *data, int argc, const char **argv);

/**
 * Add a command to the history list.
 *
 * USAGE:
 * procname add cmd
 */
BU_EXPORT extern int bu_cmdhist_add(void *data, int argc, const char **argv);

/**
 * Return the current command.
 *
 * USAGE:
 * procname curr
 */
BU_EXPORT extern int bu_cmdhist_curr(void *data, int argc, const char **argv);

/**
 * Set the current command to the next command.
 *
 * USAGE:
 * procname next
 */
BU_EXPORT extern int bu_cmdhist_next(void *data, int argc, const char **argv);

/**
 * Set the current command to the previous command.
 *
 * USAGE:
 * procname prev
 */
BU_EXPORT extern int bu_cmdhist_prev(void *data, int argc, const char **argv);

__END_DECLS

#endif  /* CMD_H */

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
