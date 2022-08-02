/*                          C M D . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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

#ifndef BU_CMD_H
#define BU_CMD_H

#include "common.h"

#include <time.h>

#include "bsocket.h" /* for timeval */

#define BU_CMD_NULL (int (*)(void *, int, const char **))NULL

#include "bu/defines.h"
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

__BEGIN_DECLS

/** @brief Routine(s) for processing subcommands */

/**
 * This function is intended to be used for parsing subcommands.  If
 * the command is found in the array of commands, the associated
 * function is called. Otherwise, an error message is printed along
 * with a list of available commands.  This behavior can be suppressed
 * by registering a bu_log() callback.
 *
 * @code
 * struct bu_hook_list saved_hooks = BU_HOOK_LIST_INIT_ZERO;
 * bu_log_hook_save_all(&saved_hooks);
 * bu_log_hook_delete_all();
 * bu_log_add_hook(NULL, NULL); // disables logging
 * bu_cmd(...);
 * bu_log_hook_restore_all(&saved_hooks);
 * @endcode
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

/**
 * Returns BRLCAD_OK if cmdname defines a command in the cmds table, and BRLCAD_ERROR otherwise */
BU_EXPORT extern int bu_cmd_valid(const struct bu_cmdtab *cmds, const char *cmdname);

/** @brief Routines for maintaining a command history */

/**
 * Prints out the command history.
 *
 * USAGE:
 * history [-delays] [-outfile filename]
 */
DEPRECATED BU_EXPORT extern int bu_cmdhist_history(void *data, int argc, const char **argv);

/**
 * Add a command to the history list.
 *
 * USAGE:
 * procname add cmd
 */
DEPRECATED BU_EXPORT extern int bu_cmdhist_add(void *data, int argc, const char **argv);

/**
 * Return the current command.
 *
 * USAGE:
 * procname curr
 */
DEPRECATED BU_EXPORT extern int bu_cmdhist_curr(void *data, int argc, const char **argv);

/**
 * Set the current command to the next command.
 *
 * USAGE:
 * procname next
 */
DEPRECATED BU_EXPORT extern int bu_cmdhist_next(void *data, int argc, const char **argv);

/**
 * Set the current command to the previous command.
 *
 * USAGE:
 * procname prev
 */
DEPRECATED BU_EXPORT extern int bu_cmdhist_prev(void *data, int argc, const char **argv);

__END_DECLS

#endif  /* BU_CMD_H */

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
