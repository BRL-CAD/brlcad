/*                      M C P C A D . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @addtogroup libmcpcad
 *
 * @brief
 * Headless command interface for external BRL-CAD extensions (e.g.,
 * Model Context Protocol servers).  Provides a hardened parsing
 * pipeline that translates flat, delimited command strings received
 * over IPC into argc/argv form suitable for execution via libged.
 */
#ifndef MCPCAD_H
#define MCPCAD_H

#include "common.h"

#include <stddef.h>

__BEGIN_DECLS

#ifndef MCPCAD_EXPORT
#  if defined(MCPCAD_DLL_EXPORTS) && defined(MCPCAD_DLL_IMPORTS)
#    error "Only MCPCAD_DLL_EXPORTS or MCPCAD_DLL_IMPORTS can be defined, not both."
#  elif defined(MCPCAD_DLL_EXPORTS)
#    define MCPCAD_EXPORT COMPILER_DLLEXPORT
#  elif defined(MCPCAD_DLL_IMPORTS)
#    define MCPCAD_EXPORT COMPILER_DLLIMPORT
#  else
#    define MCPCAD_EXPORT
#  endif
#endif

/** @addtogroup libmcpcad */
/** @{ */
/** @file mcpcad.h */

/**
 * Maximum accepted length of a single command line, including the
 * terminating NUL.  Longer payloads are rejected rather than
 * truncated.
 */
#define MCPCAD_MAXLINE 4096

/**
 * Maximum number of arguments accepted in a single command.  Inputs
 * that hit this cap are rejected rather than silently truncated.
 */
#define MCPCAD_MAXARGS 512

/**
 * A parsed command ready for execution.
 *
 * The argv array is NULL-terminated (argv[argc] == NULL) and its
 * strings point into internal storage owned by the struct - they
 * remain valid until mcpcad_cmd_free() is called and must not be
 * freed individually.
 */
struct mcpcad_cmd {
    int argc;     /**< number of parsed arguments */
    char **argv;  /**< NULL-terminated argument array */
    char *line;   /**< internal storage - argv strings point into this */
};

/**
 * Parse a flat command string into argc/argv form.
 *
 * Tokenization is performed by libbu's bu_argv_from_string(), which
 * honors double-quoted strings and backslash escapes in addition to
 * whitespace splitting.
 *
 * Inputs are validated before parsing.  The following are rejected:
 *
 * - NULL, empty, or whitespace-only input
 * - input of MCPCAD_MAXLINE bytes or more
 * - input containing control characters other than tab (newlines
 *   delimit messages at the transport layer and are never valid
 *   within a single command payload)
 * - input parsing to MCPCAD_MAXARGS or more arguments
 *
 * @return a newly allocated parsed command on success, or NULL if
 * the input was rejected.  The caller is responsible for releasing
 * the result with mcpcad_cmd_free().
 */
MCPCAD_EXPORT extern struct mcpcad_cmd *mcpcad_cmd_parse(const char *line);

/**
 * Release a command allocated by mcpcad_cmd_parse().
 *
 * Passing NULL is a no-op.
 */
MCPCAD_EXPORT extern void mcpcad_cmd_free(struct mcpcad_cmd *c);

/** @} */

__END_DECLS

#endif /* MCPCAD_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
