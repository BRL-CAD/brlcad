/*                         L O G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

/**  @defgroup io Input/Output */
/**   @defgroup log Logging */

/** @file log.h
 *
 */
#ifndef BU_LOG_H
#define BU_LOG_H

#include "common.h"

#include <stdio.h> /* For FILE */

#include "bu/defines.h"
#include "bu/magic.h"
#include "bu/parse.h"
#include "bu/list.h"

/** @addtogroup log */
/** @{ */
/** @file libbu/backtrace.c
 *
 * Extract a backtrace of the current call stack.
 *
 */

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

/** log indentation hook */
typedef int (*bu_hook_t)(genptr_t, genptr_t);

struct bu_hook_list {
    struct bu_list l; /**< linked list */
    bu_hook_t hookfunc; /**< function to call */
    genptr_t clientdata; /**< data for caller */
};
typedef struct bu_hook_list bu_hook_list_t;
#define BU_HOOK_LIST_NULL ((struct bu_hook_list *) 0)

/**
 * assert the integrity of a non-head node bu_hook_list struct.
 */
#define BU_CK_HOOK_LIST(_hl) BU_CKMAG(_hl, BU_HOOK_LIST_MAGIC, "bu_hook_list")

/**
 * initialize a bu_hook_list struct without allocating any memory.
 * this macro is not suitable for initialization of a list head node.
 */
#define BU_HOOK_LIST_INIT(_hl) { \
	BU_LIST_INIT_MAGIC(&(_hl)->l, BU_HOOK_LIST_MAGIC); \
	(_hl)->hookfunc = (_hl)->clientdata = NULL; \
    }

/**
 * macro suitable for declaration statement initialization of a
 * bu_hook_list struct.  does not allocate memory.  not suitable for
 * initialization of a list head node.
 */
#define BU_HOOK_LIST_INIT_ZERO { {BU_HOOK_LIST_MAGIC, BU_LIST_NULL, BU_LIST_NULL}, NULL, NULL }

/**
 * returns truthfully whether a non-head node bu_hook_list has been
 * initialized via BU_HOOK_LIST_INIT() or BU_HOOK_LIST_INIT_ZERO.
 */
#define BU_HOOK_LIST_IS_INITIALIZED(_p) (((struct bu_hook_list *)(_p) != BU_HOOK_LIST_NULL) && LIKELY((_p)->l.magic == BU_HOOK_LIST_MAGIC))

/**
 * Adds a hook to the list of bu_bomb hooks.  The top (newest) one of these
 * will be called with its associated client data and a string to be
 * processed.  Typically, these hook functions will display the output
 * (possibly in an X window) or record it.
 *
 * NOTE: The hook functions are all non-PARALLEL.
 */
BU_EXPORT extern void bu_bomb_add_hook(bu_hook_t func, genptr_t clientdata);

/**
 * Abort the running process.
 *
 * The bu_bomb routine is called on a fatal error, generally where no
 * recovery is possible.  Error handlers may, however, be registered
 * with BU_SETJUMP.  This routine intentionally limits calls to other
 * functions and intentionally uses no stack variables.  Just in case
 * the application is out of memory, bu_bomb deallocates a small
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
 *    bu_setjmp_valid/bu_jmpbuf setjmp facility.
 *
 * 4. Outputs str to the terminal device in case standard error is
 *    redirected.
 *
 * 5. Aborts abnormally (via abort()) if BU_DEBUG_COREDUMP is defined.
 *
 * 6. Exits with exit(12).
 *
 * Only produce a core-dump when that debugging bit is set.  Note that
 * this function is meant to be a last resort semi-graceful abort.
 *
 * This routine should never return unless there is a bu_setjmp
 * handler registered.
 */
BU_EXPORT extern void bu_bomb(const char *str) _BU_ATTR_NORETURN;

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
BU_EXPORT extern void bu_exit(int status, const char *fmt, ...) _BU_ATTR_NORETURN _BU_ATTR_PRINTF23;

/** @file libbu/crashreport.c
 *
 * Generate a crash report file, including a call stack backtrace and
 * other system details.
 *
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

/** @file libbu/fgets.c
 *
 * fgets replacement function that also handles CR as an EOL marker
 *
 */

/**
 * Reads in at most one less than size characters from stream and
 * stores them into the buffer pointed to by s. Reading stops after an
 * EOF, CR, LF, or a CR/LF combination. If a LF or CR is read, it is
 * stored into the buffer. If a CR/LF is read, just a CR is stored
 * into the buffer. A '\\0' is stored after the last character in the
 * buffer. Returns s on success, and NULL on error or when end of file
 * occurs while no characters have been read.
 */
BU_EXPORT extern char *bu_fgets(char *s, int size, FILE *stream);

/** @file libbu/linebuf.c
 *
 * A portable way of doing setlinebuf().
 *
 */

BU_EXPORT extern void bu_setlinebuf(FILE *fp);

/** @file libbu/hook.c
 *
 * @brief
 * BRL-CAD support library's hook utility.
 *
 */
BU_EXPORT extern void bu_hook_list_init(struct bu_hook_list *hlp);
BU_EXPORT extern void bu_hook_add(struct bu_hook_list *hlp,
				  bu_hook_t func,
				  genptr_t clientdata);
BU_EXPORT extern void bu_hook_delete(struct bu_hook_list *hlp,
				     bu_hook_t func,
				     genptr_t clientdata);
BU_EXPORT extern void bu_hook_call(struct bu_hook_list *hlp,
				   genptr_t buf);
BU_EXPORT extern void bu_hook_save_all(struct bu_hook_list *hlp,
				       struct bu_hook_list *save_hlp);
BU_EXPORT extern void bu_hook_delete_all(struct bu_hook_list *hlp);
BU_EXPORT extern void bu_hook_restore_all(struct bu_hook_list *hlp,
					  struct bu_hook_list *restore_hlp);

/** @file libbu/log.c
 *
 * @brief
 * parallel safe version of fprintf for logging
 *
 * BRL-CAD support library, error logging routine.  Note that the user
 * may provide his own logging routine, by replacing these functions.
 * That is why this is in file of its own.  For example, LGT and
 * RTSRV take advantage of this.
 *
 * Here is an example of how to set up a custom logging callback.
 * While bu_log presently writes to STDERR by default, this behavior
 * should not be relied upon and may be changed to STDOUT in the
 * future without notice.
 *
 @code
 --- BEGIN EXAMPLE ---

 int log_output_to_file(genptr_t data, genptr_t str)
 {
   FILE *fp = (FILE *)data;
   fprintf(fp, "LOG: %s", str);
   return 0;
 }

 int main(int ac, char *av[])
 {
   FILE *fp = fopen("whatever.log", "w+");
   bu_log_add_hook(log_output_to_file, (genptr_t)fp);
   bu_log("Logging to file.\n");
   bu_log_delete_hook(log_output_to_file, (genptr_t)fp);
   bu_log("Logging to stderr.\n");
   fclose(fp);
   return 0;
 }

 --- END EXAMPLE ---
 @endcode
 *
 */


/**
 * Change global indentation level by indicated number of characters.
 * Call with a large negative number to cancel all indentation.
 */
BU_EXPORT extern void bu_log_indent_delta(int delta);

/**
 * For multi-line vls generators, honor logindent level like bu_log() does,
 * and prefix the proper number of spaces.
 * Should be called at the front of each new line.
 */
BU_EXPORT extern void bu_log_indent_vls(struct bu_vls *v);

/**
 * Adds a hook to the list of bu_log hooks.  The top (newest) one of these
 * will be called with its associated client data and a string to be
 * processed.  Typically, these hook functions will display the output
 * (possibly in an X window) or record it.
 *
 * NOTE: The hook functions are all non-PARALLEL.
 */
BU_EXPORT extern void bu_log_add_hook(bu_hook_t func, genptr_t clientdata);

/**
 * Removes the hook matching the function and clientdata parameters from
 * the hook list.  Note that it is not necessarily the active (top) hook.
 */
BU_EXPORT extern void bu_log_delete_hook(bu_hook_t func, genptr_t clientdata);

BU_EXPORT extern void bu_log_hook_save_all(struct bu_hook_list *save_hlp);
BU_EXPORT extern void bu_log_hook_delete_all(void);
BU_EXPORT extern void bu_log_hook_restore_all(struct bu_hook_list *restore_hlp);

/**
 * Log a single character with no flushing.
 */
BU_EXPORT extern void bu_putchar(int c);

/**
 * The routine is primarily called to log library events.
 *
 * The function is essentially a semaphore-protected version of
 * fprintf(stderr) with optional logging hooks and automatic
 * indentation options.  The main difference is that this function
 * does not keep track of characters printed, so nothing is returned.
 *
 * This function recognizes a %V format specifier to print a bu_vls
 * struct pointer.  See bu_vsscanf() for details.
 */
BU_EXPORT extern void bu_log(const char *, ...) _BU_ATTR_PRINTF12;

/**
 * Just like bu_log() except that you can send output to a specified
 * file pointer.
 */
BU_EXPORT extern void bu_flog(FILE *, const char *, ...) _BU_ATTR_PRINTF23;

/**
 * Custom vsscanf which wraps the system sscanf, and is wrapped by bu_sscanf.
 *
 * bu_vsscanf differs notably from the underlying system sscanf in that:
 *
 *  - A maximum field width is required for unsuppressed %s and %[...]
 *    conversions. If a %s or %[...] conversion is encountered which does
 *    not include a maximum field width, the routine bombs in order to avoid
 *    an accidental buffer overrun.
 *
 *  - %V and %#V have been added as valid conversions. Both expect a pointer to
 *    a struct bu_vls as their argument.
 *
 *    %V is comparable to %[^]. It instructs bu_vsscanf to read arbitrary
 *    characters from the source and store them in the vls buffer. The default
 *    maximum field width is infinity.
 *
 *    %#V is comparable to %s. It instructs bu_vsscanf to skip leading
 *    whitespace, and then read characters from the source and store them in the
 *    vls buffer until the next whitespace character is encountered. The default
 *    maximum field width is infinity.
 *
 *  - 0 is always a valid field width for unsuppressed %c, %s, and %[...]
 *    conversions and causes '\0' to be written to the supplied char*
 *    argument.
 *
 *  - a/e/f/g and A/E/F/G are always synonyms for float conversion.
 *
 *  - The C99 conversions hh[diouxX], z[diouxX], and t[diouxX] are always
 *    supported.
 *
 * This routine has an associated test program named test_sscanf, which
 * compares its behavior to the system sscanf.
 */
BU_EXPORT extern int bu_vsscanf(const char *src, const char *fmt, va_list ap);

/**
 * Initializes the va_list, then calls bu_vsscanf.
 *
 * This routine has an associated test program named test_sscanf, which
 * compares its behavior to the system sscanf.
 */
BU_EXPORT extern int bu_sscanf(const char *src, const char *fmt, ...) _BU_ATTR_SCANF23;

/** @file libbu/dirname.c
 *
 * @brief
 * Routines to process file and path names.
 *
 */

/**
 * Given a string containing a hierarchical path, return a dynamic
 * string to the parent path.
 *
 * This function is similar if not identical to most dirname() BSD
 * system function implementations; but that system function cannot be
 * used due to significantly inconsistent behavior across platforms.
 *
 * This function always recognizes paths separated by a '/' (i.e.,
 * geometry paths) as well as whatever the native platform directory
 * separator may be.  It is assumed that all file and directory names
 * in the path will not contain a path separator, even if escaped.
 *
 * It is the caller's responsibility to bu_free() the pointer returned
 * from this routine.
 *
 * Examples of strings returned:
 *
 *	/usr/dir/file	/usr/dir
 * @n	/usr/dir/	/usr
 * @n	/usr/file	/usr
 * @n	/usr/		/
 * @n	/usr		/
 * @n	/		/
 * @n	.		.
 * @n	..		.
 * @n	usr		.
 * @n	a/b		a
 * @n	a/		.
 * @n	../a/b		../a
 *
 * This routine will return "." if other valid results are not available
 * but should never return NULL.
 */
BU_EXPORT extern char *bu_dirname(const char *path);

/**
 * Given a string containing a hierarchical path, return a dynamic
 * string to the portion after the last path separator.
 *
 * This function is similar if not identical to most basename() BSD
 * system function implementations; but that system function cannot be
 * used due to significantly inconsistent behavior across platforms.
 *
 * This function always recognizes paths separated by a '/' (i.e.,
 * geometry paths) as well as whatever the native platform directory
 * separator may be.  It is assumed that all file and directory names
 * in the path will not contain a path separator, even if escaped.
 *
 * It is the caller's responsibility allocate basename with
 * strlen(path).
 *
 * Examples of strings returned:
 *
 *	/usr/dir/file	file
 * @n	/usr/dir/	dir
 * @n	/usr/		usr
 * @n	/usr		usr
 * @n	/		/
 * @n	.		.
 * @n	..		..
 * @n	usr		usr
 * @n	a/b		b
 * @n	a/		a
 * @n	///		/
 */
BU_EXPORT extern void bu_basename(char *basename, const char *path);

/** @file libbu/lex.c
 *
 */

#define BU_LEX_ANY 0	/* pseudo type */
struct bu_lex_t_int {
    int type;
    int value;
};
#define BU_LEX_INT 1
struct bu_lex_t_dbl {
    int type;
    double value;
};
#define BU_LEX_DOUBLE 2
struct bu_lex_t_key {
    int type;
    int value;
};
#define BU_LEX_SYMBOL 3
#define BU_LEX_KEYWORD 4
struct bu_lex_t_id {
    int type;
    char *value;
};
#define BU_LEX_IDENT 5
#define BU_LEX_NUMBER 6	/* Pseudo type */
union bu_lex_token {
    int type;
    struct bu_lex_t_int t_int;
    struct bu_lex_t_dbl t_dbl;
    struct bu_lex_t_key t_key;
    struct bu_lex_t_id t_id;
};
struct bu_lex_key {
    int tok_val;
    char *string;
};
#define BU_LEX_NEED_MORE 0


BU_EXPORT extern int bu_lex(union bu_lex_token *token,
			    struct bu_vls *rtstr,
			    struct bu_lex_key *keywords,
			    struct bu_lex_key *symbols);


/** @file libbu/mread.c
 *
 * multiple-read to fill a buffer
 *
 * Provide a general means to a read some count of items from a file
 * descriptor reading multiple times until the quantity desired is
 * obtained.  This is useful for pipes and network connections that
 * don't necessarily deliver data with the same grouping as it is
 * written with.
 *
 * If a read error occurs, a negative value will be returns and errno
 * should be set (by read()).
 *
 */

/**
 * "Multiple try" read.  Read multiple times until quantity is
 * obtained or an error occurs.  This is useful for pipes.
 */
BU_EXPORT extern long int bu_mread(int fd, void *bufp, long int n);

/** @} */

#endif  /* BU_LOG_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
