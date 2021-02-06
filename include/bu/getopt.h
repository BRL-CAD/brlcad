/*                         G E T O P T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#ifndef BU_GETOPT_H
#define BU_GETOPT_H

#include "common.h"
#include "bu/defines.h"

__BEGIN_DECLS

/** @addtogroup bu_getopt
 * @brief
 * Special portable re-entrant version of getopt.
 *
 * Everything is prefixed with bu_, to distinguish it from the various
 * getopt routines found in libc.
 *
 * Important note -
 * If bu_getopt() is going to be used more than once, it is necessary
 * to reinitialize bu_optind=1 before beginning on the next argument
 * list.
 */
/** @{ */
/** @file bu/getopt.h */

/**
 * for bu_getopt().  set to zero to suppress errors.
 */
BU_EXPORT extern int bu_opterr;

/**
 * for bu_getopt().  current index into parent argv vector.
 */
BU_EXPORT extern int bu_optind;

/**
 * for bu_getopt().  current option being checked for validity.
 */
BU_EXPORT extern int bu_optopt;

/**
 * for bu_getopt().  current argument associated with current option.
 */
BU_EXPORT extern char *bu_optarg;

/**
 * Get option letter from argument vector.
 *
 * @param nargc number of arguments
 * @param nargv array of argument strings
 * @param ostr option string
 *
 * The 'ostr' option string may contain individual characters (e.g.,
 * "abc"), characters followed by a colon to indicate a required
 * argument (e.g., "a:bc", 'a' requires an argument, 'b' and 'c' do
 * not), and characters followed by two colons to indicate an optional
 * argument (e.g., "ab::c", 'b' may be followed by an argument, but
 * none require an argument).
 *
 * Returns -1 when the argument list is exhausted, otherwise it
 * returns the next known option character.  If bu_getopt() encounters
 * a character not found in ostr or if it detects a required missing
 * option argument, it returns `?' (question mark).  If ostr has a
 * leading `:' then a missing option argument causes `:' to be
 * returned instead of `?'.  In either case, the variable bu_optopt is
 * set to the character that caused the error.
 */
BU_EXPORT extern int bu_getopt(int nargc, char * const nargv[], const char *ostr);

/** @} */

__END_DECLS

#endif  /* BU_GETOPT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
