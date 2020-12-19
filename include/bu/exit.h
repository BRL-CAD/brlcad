/*                        E X I T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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

#ifndef BU_EXIT_H
#define BU_EXIT_H

#include "common.h"

#include <stdio.h> /* for FILE */

#include "bu/defines.h"
#include "bu/hook.h"


__BEGIN_DECLS

/** @{ */
/** @file bu/exit.h */

/**
 * this routine provides a trace of the call stack to the caller,
 * generally called either directly, via a signal handler, or through
 * bu_bomb() with the appropriate bu_debug flags set.
 *
 * the routine waits indefinitely (in a spin loop) until a signal
 * (SIGINT) is received, at which point execution continues, or until
 * some other signal is received that terminates the application.
 *
 * the stack backtrace will be written to the provided 'fp' file
 * pointer.  it's the caller's responsibility to open and close
 * that pointer if necessary.  If 'fp' is NULL, stdout will be used.
 *
 * returns truthfully if a backtrace was attempted.
 */
BU_EXPORT extern int bu_backtrace(FILE *fp);

/**
 * A version of bu_backtrace where the caller provides their own
 * full path to their executable for passing to GDB, rather than
 * having libbu attempt to determine that path.
 *
 * Passing NULL to argv0 makes the behavior identical to
 * that of bu_backtrace.
 */
BU_EXPORT extern int bu_backtrace_app(FILE *fp, const char *argv0);

/**
 * Adds a hook to the list of bu_bomb hooks.  The top (newest) one of these
 * will be called with its associated client data and a string to be
 * processed.  Typically, these hook functions will display the output
 * (possibly in an X window) or record it.
 *
 * NOTE: The hook functions are all non-PARALLEL.
 */
BU_EXPORT extern void bu_bomb_add_hook(bu_hook_t func, void *clientdata);

/* TODO - missing functions, if we're not going to expose the bu_bomb hook
 * list.. is this the API we want? */
BU_EXPORT extern void bu_bomb_save_all_hooks(struct bu_hook_list *save_hlp);
BU_EXPORT extern void bu_bomb_delete_all_hooks();
BU_EXPORT extern void bu_bomb_restore_hooks(struct bu_hook_list *save_hlp);

/**
 * Abort the running process.
 *
 * The bu_bomb routine is called on a fatal error, generally where no
 * recovery is possible.  Error handlers may, however, be registered
 * with BU_SETJUMP().  This routine intentionally limits calls to
 * other functions and intentionally uses no stack variables.  Just in
 * case the application is out of memory, bu_bomb deallocates a small
 * buffer of memory.
 *
 * Before termination, it optionally performs the following operations
 * in the order listed:
 *
 * 1. Outputs str to standard error
 *
 * 2. Calls any callback functions set in the global bu_bomb_hook_list
 *    variable with str passed as an argument.
 *
 * 3. Jumps to any user specified error handler registered with the
 *    BU_SETJUMP() facility.
 *
 * 4. Outputs str to the terminal device in case standard error is
 *    redirected.
 *
 * 5. Aborts abnormally (via abort) if BU_DEBUG_COREDUMP is defined.
 *
 * 6. Exits with exit(12).
 *
 * Only produce a core-dump when that debugging bit is set.  Note that
 * this function is meant to be a last resort semi-graceful abort.
 *
 * This routine should never return unless there is a BU_SETJUMP()
 * handler registered.
 */
BU_EXPORT extern void bu_bomb(const char *str) _BU_ATTR_ANALYZE_NORETURN _BU_ATTR_NORETURN;


/**
 * Semi-graceful termination of the application that doesn't cause a
 * stack trace, exiting with the specified status after printing the
 * given message.  It's okay for this routine to use the stack,
 * contrary to bu_bomb's behavior since it should be called for
 * expected termination situations.
 *
 * This routine should generally not be called within a library.  Use
 * bu_bomb or (better) cascade the error back up to the application.
 *
 * This routine should never return.
 */
BU_EXPORT extern void bu_exit(int status, const char *fmt, ...) _BU_ATTR_ANALYZE_NORETURN _BU_ATTR_NORETURN _BU_ATTR_PRINTF23;


/**
 * @brief
 * Generate a crash report file, including a call stack backtrace and
 * other system details.
 */

/**
 * this routine writes out details of the currently running process to
 * the specified file, including an informational header about the
 * execution environment, stack trace details, kernel and hardware
 * information, and current version information.
 *
 * returns truthfully if the crash report was written.
 *
 * due to various reasons, this routine is NOT thread-safe.
 */
BU_EXPORT extern int bu_crashreport(const char *filename);

/**
 * A version of bu_crashreport where the caller provides their own
 * full path to their executable for passing to GDB, rather than
 * having libbu attempt to determine that path.
 *
 * Passing NULL to argv0 makes the behavior identical to
 * that of bu_crashreport.
 */
BU_EXPORT extern int bu_crashreport_app(const char *filename, const char *argv0);


/** @} */

__END_DECLS

#endif  /* BU_EXIT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
