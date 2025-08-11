/*                       C O M M A N D S . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @addtogroup libged
 *
 * Geometry EDiting Library Commands
 *
 */
/** @{ */
/** @file ged/commands.h */
/** @} */

#ifndef GED_COMMANDS_H
#define GED_COMMANDS_H

#include "common.h"
#include "ged/defines.h"


__BEGIN_DECLS

/** @addtogroup ged_plugins */
/** @{ */
/** Execute plugin based command */
#if !defined(GED_PLUGIN) && !defined(GED_EXEC_NORAW)
GED_EXPORT extern int ged_exec(struct ged *gedp, int argc, const char *argv[]);
#endif
/** @} */

/* LIBGED maintains this list - callers should regard it as read only.  This
 * list will change (size and pointers to individual command strings if
 * commands are added or removed - caller is responsible for performing a new
 * call to get an updated list and size if commands are altered.  */
GED_EXPORT size_t ged_cmd_list(const char * const **cmd_list);

/* Report whether a string identifies a valid LIBGED command.
 *
 * Returns 1 if cmd is a valid GED command, else returns 0.
 */
GED_EXPORT int ged_cmd_exists(const char *cmd);

/* Determine whether cmd1 and cmd2 both refer to the same function pointer
 * (i.e., they are aliases for the same command.)
 *
 * Returns 1 if both cmd1 and cmd2 invoke the same LIBGED function, else
 * returns 0
 */
GED_EXPORT int ged_cmd_same(const char *cmd1, const char *cmd2);

/* Report whether a string identifies a valid LIBGED command.  If func is
 * non-NULL, check that cmd and func both refer to the same function pointer
 * (i.e., they are aliases for the same command.)
 *
 * If func is NULL, a 0 return indicates an valid GED command and non-zero
 * indicates an invalid command.
 *
 * If func is non-null:
 * 0 indicates both cmd and func strings invoke the same LIBGED function
 * 1 indicates that either or both of cmd and func were invalid GED commands
 * 2 indicates that both were valid commands, but they did not match.
 *
 * DEPRECATED - use ged_cmd_same and ged_cmd_exists instead.
 */
DEPRECATED GED_EXPORT int ged_cmd_valid(const char *cmd, const char *func);

/* Given a candidate cmd name, find the closest match to it among defined
 * GED commands.  Returns the bu_editdist distance between cmd and *ncmd
 * (0 if they match exactly - i.e. cmd does define a command.)
 *
 * Useful for suggesting corrections to commands which are not found.
 */
GED_EXPORT extern int
ged_cmd_lookup(const char **ncmd, const char *cmd);


/* Given a partial command string, analyze it and return possible command
 * completions of the seed string.  Typically this functionality is used to
 * implement "tab completion" or "tab expansion" features in applications.  The
 * possible completions are returned in the completions array, and the returned
 * value is the count of possible completions found.
 *
 * If completions array returned is non-NULL, caller is responsible for freeing
 * it using bu_argv_free.
 */
GED_EXPORT extern int
ged_cmd_completions(const char ***completions, const char *seed);

/* Given a object name or path string, analyze it and return possible
 * object-based completions.  Typically this functionality is used to implement
 * "tab completion" or "tab expansion" features in applications.  The possible
 * completions are returned in the completions array, and the returned value is
 * the count of possible completions found.
 *
 * If completions array returned is non-NULL, caller is responsible for freeing
 * it using bu_argv_free.
 */
GED_EXPORT extern int
ged_geom_completions(const char ***completions, struct bu_vls *cprefix, struct db_i *dbip, const char *seed);


/**
 * Use bu_editor to set up an editor for use with GED
 * commands that require launching a text editor.
 *
 * Will first try to respect environment variables (including looking for
 * terminal options to launch text editors normally used only in graphical
 * mode) and then fall back to lookups.
 *
 * Applications may supply their own argv array of editors to check using
 * app_editors_cnt and app_editors in the ged struct - see bu_editor
 * documentation for more details.
 */
GED_EXPORT extern int ged_set_editor(struct ged *gedp, int non_gui);

/**
 * Clear editor data set by ged_set_editor.  User specified app_editors data
 * is left unchanged.
 */
GED_EXPORT extern void ged_clear_editor(struct ged *gedp);


/* defined in track.c */
GED_EXPORT extern int ged_track2(struct bu_vls *log_str, struct rt_wdb *wdbp, const char *argv[]);


/* defined in wdb_importFg4Section.c */
GED_EXPORT int wdb_importFg4Section_cmd(void *data, int argc, const char *argv[]);


/* defined in inside.c */
GED_EXPORT extern int ged_inside_internal(struct ged *gedp,
					  struct rt_db_internal *ip,
					  int argc,
					  const char *argv[],
					  int arg,
					  char *o_name);


GED_EXPORT void draw_scene(struct bv_scene_obj *s, struct bview *v);


/** @} */


__END_DECLS


#endif /* GED_COMMANDS_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
