/*                          E N V . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2024 United States Government as represented by
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

#ifndef BU_ENV_H
#define BU_ENV_H

#include "common.h"

#include "bu/defines.h"

__BEGIN_DECLS

/** @addtogroup bu_env
 *
 * @brief
 * Platform-independent methods for interacting with the parent operating
 * system environment.
 *
 */
/** @{ */
/** @file bu/env.h */

BU_EXPORT extern int bu_setenv(const char *name, const char *value, int overwrite);


/* Specific types of machine memory information that may be requested */
#define BU_MEM_ALL 0
#define BU_MEM_AVAIL 1
#define BU_MEM_PAGE_SIZE 2

/**
 * Report system memory sizes.
 *
 * Returns -1 on error and the size of the requested memory type on
 * success.  Optionally if sz is non-NULL, the size of the requested
 * memory type will be set to it.
 */
BU_EXPORT extern ssize_t bu_mem(int type, size_t *sz);


/**
 * Select an editor.
 *
 * Returns a string naming an editor to be used.  If an option is needed for
 * invoking the editor, it is supplied via the editor_opt output.
 *
 * When 0 is supplied to the etype parameter, all editors will be considered
 * when using libbu's internal editors list.  To restrict the set searched to
 * console editors, set etype to 1.  For only GUI editors, set etype to 2.
 *
 * The general pattern for trying to locate an editor using inputs (either
 * internal or user supplied is:
 *
 * 1.  If the input is a full, valid path it will be used as-is.
 * 2.  Otherwise, the BRL-CAD's binaries will be checked for a bundled copy.
 * 3.  Otherwise, bu_which will attempt to find the full path.
 *
 * If the EDITOR environment variable is set, the contents of
 * EDITOR take priority over both libbu's internal list and a user supplied
 * list.
 *
 * If EDITOR is unset, the next priority is any user supplied options via the
 * check_for_editors array.  As with the EDITOR variable, an attempt will be
 * made to respect the etype setting.
 *
 * If the optional check_for_editors array is provided, libbu will first
 * attempt to use the contents of that array to find an editor.  The main
 * purpose of check_for_editors is to allow applications to define their own
 * preferred precedence order in case there are specific advantages to using
 * some editors over others.  It is also useful if an app wishes to list some
 * specialized editor not part of the normal listings, although users should
 * note that the etype modal checks will not be useful when such editors are
 * supplied.  If an application wishes to use ONLY a check_for_editors list and
 * not fall back on libbu's internal list if it fails, they should assign the
 * last entry of check_for_editors to be NULL to signal libbu to stop looking
 * for an editor there:
 *
 *  int check_for_cnt = 3; const char *check_for_editors[3] = {"editor1", "editor2", NULL};
 *
 * If none of those inputs result in an editor being returned, libbu will
 * attempt to use its own internal lists to search.  We are deliberately NOT
 * documenting the libbu's internal editor list as public API, nor do we make
 * any guarantees about the presence of any editor that is on the list will
 * take relative to other editors.  What editors are popular in various
 * environments can change over time, and the purpose of this function is to
 * provide *some* editor, rather than locking in any particular precedence.
 * check_for_editors should be used if an app needs more guaranteed stability
 * in lookup behaviors.
 *
 * If etype != 0, bu_editor will attempt to validate any candidate returns
 * against its own internal list of GUI and console editors to avoid returning
 * an editor that is incompatible with the specified environment.  There are no
 * strong guarantees offered for this checking - libbu's knowledge of editors
 * is not comprehensive and specification strings for editors may break the
 * matching being used internally. In a case where libbu CAN'T make a definite
 * categorization (a user supplied custom editor, for example) the default
 * behavior will be to pass through the result as successful.  This is
 * obviously not foolproof, but in various common cases libbu will avoid
 * returning (for example) nano from an EDITOR setting when a GUI editor type
 * was requested.
 *
 * Caller should NOT free either the main string return from bu_editor or the
 * pointer assigned to editor_opt.  Both outputs may both change contents from
 * one call to the next, so caller should duplicate the string outputs if they
 * wish to preserve them beyond the next bu_editor call.
 */
BU_EXPORT const char *bu_editor(const char **editor_opt, int etype, int check_for_cnt, const char **check_for_editors);

/** @} */

__END_DECLS

#endif /* BU_ENV_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
