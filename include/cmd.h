/*                          C M D . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2011 United States Government as represented by
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
/** @addtogroup cmd */
/** @ingroup data */
/** @{ */
/** @file cmdhist.c
 *
 * @brief
 * Routines for maintaining a command history
 *
 */

#ifndef __CMD_H__
#define __CMD_H__

#include "common.h"

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#include <time.h>

/* for timeval via windows.h */
#if defined(_WIN32) && !defined(__CYGWIN__)
#  define NOMINMAX
#  include <windows.h>
#  include <io.h>

#   undef rad1 /* Win32 radio button 1 */
#   undef rad2 /* Win32 radio button 2 */
#   undef small /* defined as part of the Microsoft Interface Definition Language (MIDL) */
#   undef IN
#   undef OUT
#endif

#include "bu.h"


#define CMD_NULL (int (*)(void *, int, const char **))NULL

struct bu_cmdhist {
    struct bu_list l;
    struct bu_vls h_command;
    struct timeval h_start;
    struct timeval h_finish;
    int h_status;
};
#define CMDHIST_NULL (struct bu_cmdhist *)NULL

struct bu_cmdhist_obj {
    struct bu_list l;
    struct bu_vls cho_name;
    struct bu_cmdhist cho_head;
    struct bu_cmdhist *cho_curr;
};
#define CMDHIST_OBJ_NULL (struct bu_cmdhist_obj *)NULL


/**
 * b u _ c m d
 *
 * This function is intended to be used for parsing subcommands.  If
 * the command is found in the array of commands, the associated
 * function is called. Otherwise, an error message is created and
 * added to interp.
 *
 * @param clientData	- data/state associated with the command
 * (Note - the result of the command is also stored here)
 * @param argc		- number of arguments in argv
 * @param argv		- command to execute and its arguments
 * @param cmds		- commands and related function pointers
 * @param cmd_index	- indicates which argv element holds the subcommand
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
BU_EXPORT extern int bu_cmd(void *data, int argc, const char *argv[], struct bu_cmdtab *cmds, int cmd_index);

/**
 * @brief
 * Prints out the command history.
 *
 * USAGE:
 * history [-delays] [-outfile filename]
 */
BU_EXPORT extern int bu_cmdhist_history(void *clientData, int argc, const char **argv);

/**
 * @brief
 * Add a command to the history list.
 *
 * USAGE:
 * procname add cmd
 */
BU_EXPORT extern int bu_cmdhist_add(void *clientData, int argc, const char **argv);

/**
 * Return the current command.
 *
 * USAGE:
 * procname curr
 */
BU_EXPORT extern int bu_cmdhist_curr(void *clientData, int argc, const char **argv);

/**
 * Set the current command to the next command.
 *
 * USAGE:
 * procname next
 */
BU_EXPORT extern int bu_cmdhist_next(void *clientData, int argc, const char **argv);

/**
 * Set the current command to the previous command.
 *
 * USAGE:
 * procname prev
 */
BU_EXPORT extern int bu_cmdhist_prev(void *clientData, int argc, const char **argv);

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
