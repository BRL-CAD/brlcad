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

/* Forward declarations - we only need the pointer types, so consumers
 * of this header don't inherit the full ged.h/vls.h definitions. */
struct ged;
struct bu_vls;

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


/**
 * No database is currently open (gedp was NULL).
 */
#define MCPCAD_ERR_NODB     (-1)

/**
 * The command was missing or malformed (NULL cmd, empty argv).
 */
#define MCPCAD_ERR_NOCMD    (-2)

/**
 * A message declared a payload length of MCPCAD_MAXLINE or more; the
 * payload is being skipped.
 */
#define MCPCAD_ERR_OVERFLOW (-3)

/**
 * The command was rejected before execution (parse failure, or a
 * payload containing NUL bytes).
 */
#define MCPCAD_ERR_PARSE    (-4)

/**
 * The byte stream is not speaking the mcpcad protocol (bad frame
 * magic).  Unrecoverable: with no delimiters there is no resync
 * point, so the transport should close the connection.
 */
#define MCPCAD_ERR_PROTO    (-5)

/**
 * Execute a parsed command against an open geometry database.
 *
 * On success and on GED-level failure alike, the command's output (or
 * error message) is written into 'result' if result is non-NULL; any
 * prior contents are replaced.  The parsed command is not consumed -
 * it may be executed again.
 *
 * Returns:
 *  - negative: the command never reached libged.  MCPCAD_ERR_NOCMD if
 *    'c' is NULL or empty (checked first), MCPCAD_ERR_NODB if 'gedp'
 *    is NULL.  A short diagnostic is written to 'result'.
 *  - zero or positive: the verbatim ged_exec() status.  0 (BRLCAD_OK)
 *    is success; nonzero indicates a GED-level error, with the
 *    explanation in 'result'.
 */
MCPCAD_EXPORT extern int mcpcad_cmd_exec(struct ged *gedp,
					 const struct mcpcad_cmd *c,
					 struct bu_vls *result);


/**
 * Wire framing (v0.2): every message, in both directions, is
 *
 *   'M' 'C' | length (4 bytes, big-endian) | payload (length bytes)
 *
 * Same shape as libpkg's magic+type+length header (minus type), so
 * the planned v1 migration to libpkg transport is a header widening
 * rather than a redesign.  Request payloads are command text (no NUL
 * bytes, at most MCPCAD_MAXLINE-1 bytes); reply payloads start with
 * an "OK\n" or "ERR <code>\n" status line.
 */
#define MCPCAD_FRAME_MAGIC0 'M'
#define MCPCAD_FRAME_MAGIC1 'C'
#define MCPCAD_FRAME_HDRLEN 6

/**
 * Accumulates raw transport bytes and yields complete framed
 * messages.  A read() may deliver half a message or three messages -
 * this is the layer that doesn't care.
 *
 * Opaque; allocate with create, release with destroy.
 */
struct mcpcad_framebuf;

MCPCAD_EXPORT extern struct mcpcad_framebuf *mcpcad_framebuf_create(void);
MCPCAD_EXPORT extern void mcpcad_framebuf_destroy(struct mcpcad_framebuf *fb);

/**
 * Feed raw bytes from the transport.  Complete messages and framing
 * faults are queued in arrival order; drain them with
 * mcpcad_framebuf_next().
 */
MCPCAD_EXPORT extern void mcpcad_framebuf_append(struct mcpcad_framebuf *fb,
						 const char *data,
						 size_t len);

/**
 * Pop the next framing event, in arrival order.
 *
 * Returns:
 *  - 1: a complete message; *msgp is the NUL-terminated payload,
 *    newly allocated, release with bu_free()
 *  - 0: nothing further buffered (*msgp set to NULL)
 *  - MCPCAD_ERR_OVERFLOW: a message declared length >= MCPCAD_MAXLINE
 *    and its payload is being skipped (one event per such message)
 *  - MCPCAD_ERR_PARSE: a payload arrived containing NUL bytes and was
 *    dropped (executing a silently-truncated command is never safe)
 *  - MCPCAD_ERR_PROTO: bad frame magic; the stream is unrecoverable
 *    and all further input is ignored
 */
MCPCAD_EXPORT extern int mcpcad_framebuf_next(struct mcpcad_framebuf *fb,
					      char **msgp);


/**
 * Transport write callback: how a session sends reply bytes back to
 * its client.  Implementations exist per transport (TCP, stdio, test
 * capture); the session never knows which it has.
 */
typedef void (*mcpcad_write_t)(const char *data, size_t len, void *ctx);

/**
 * One client connection: a framing buffer, a database handle, and a
 * way to write back.  Opaque.
 *
 * The session does NOT own gedp - the host application (MGED, a test
 * harness) manages database lifetime.
 */
struct mcpcad_session;

MCPCAD_EXPORT extern struct mcpcad_session *mcpcad_session_create(struct ged *gedp,
								  mcpcad_write_t wcb,
								  void *wctx);
MCPCAD_EXPORT extern void mcpcad_session_destroy(struct mcpcad_session *s);

/**
 * Feed raw transport bytes into the session.  Complete messages are
 * parsed and executed in arrival order; exactly one framed reply is
 * written via the session's write callback per message and per
 * framing fault.  Reply payloads are
 *
 *   OK\n<output>             on success
 *   ERR <code>\n<diagnostic> on any failure (code as per above)
 *
 * Returns 0 normally.  Returns MCPCAD_ERR_PROTO when the stream is
 * not speaking this protocol - the caller (transport) should close
 * the connection; further input will be ignored regardless.
 */
MCPCAD_EXPORT extern int mcpcad_session_input(struct mcpcad_session *s,
					      const char *data,
					      size_t len);

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
