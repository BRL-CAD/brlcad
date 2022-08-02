/*                         L O G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

#ifndef BU_LOG_H
#define BU_LOG_H

#include "common.h"

#include <stdio.h> /* For FILE */
#include <stdarg.h> /* For va_list */

#include "bu/defines.h"
#include "bu/magic.h"
#include "bu/parse.h"
#include "bu/hook.h"

#include "bu/exit.h" /* XXX not used here, not intended to be kept included here */


__BEGIN_DECLS

/** @addtogroup bu_log
 *
 * @brief
 * BRL-CAD support library, error logging routines.
 *
 * Note that the user may provide his own logging routine, by replacing these
 * functions.  That is why this is in file of its own.  For example, LGT and
 * RTSRV take advantage of this.
 *
 * Here is an example of how to set up a custom logging callback.  While bu_log
 * presently writes to STDERR by default, this behavior should not be relied
 * upon and may be changed to STDOUT in the future without notice.
 *
 * @code
 * --- BEGIN EXAMPLE ---
 *
 * int log_output_to_file(void *data, void *str)
 * {
 *   FILE *fp = (FILE *)data;
 *   fprintf(fp, "LOG: %s", str);
 *   return 0;
 * }
 *
 * int main(int ac, char *av[])
 * {
 *   FILE *fp = fopen("whatever.log", "w+");
 *   bu_log_add_hook(log_output_to_file, (void *)fp);
 *   bu_log("Logging to file.\n");
 *   bu_log_delete_hook(log_output_to_file, (void *)fp);
 *   bu_log("Logging to stderr.\n");
 *   fclose(fp);
 *   return 0;
 * }
 *
 * --- END EXAMPLE ---
 * @endcode
 *
 */
/** @{ */
/** @file bu/log.h */

/**
 * @brief
 * fgets replacement function that also handles CR as an EOL marker
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

/** @brief A portable way of doing setlinebuf(). */

BU_EXPORT extern void bu_setlinebuf(FILE *fp);

/** @brief parallel safe version of fprintf for logging */

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
BU_EXPORT extern void bu_log_add_hook(bu_hook_t func, void *clientdata);

/**
 * Removes the hook matching the function and clientdata parameters from
 * the hook list.  Note that it is not necessarily the active (top) hook.
 */
BU_EXPORT extern void bu_log_delete_hook(bu_hook_t func, void *clientdata);

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
 * indentation options.
 */
BU_EXPORT extern int bu_log(const char *, ...) _BU_ATTR_PRINTF12;

/**
 * Just like bu_log() except that you can send output to a specified
 * file pointer.
 */
BU_EXPORT extern int bu_flog(FILE *, const char *, ...) _BU_ATTR_PRINTF23;

/**
 * @brief
 * libbu implementations of vsscanf/sscanf() with extra format
 * specifiers.
 */

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
 *  - %V and %\#V have been added as valid conversions. Both expect a
 *    pointer to a struct bu_vls as their argument.
 *
 *    %V is comparable to %[^]. It instructs bu_vsscanf to read arbitrary
 *    characters from the source and store them in the vls buffer. The default
 *    maximum field width is infinity.
 *
 *    %\#V is comparable to %s. It instructs bu_vsscanf to skip
 *    leading whitespace, and then read characters from the source and
 *    store them in the vls buffer until the next whitespace character
 *    is encountered. The default maximum field width is infinity.
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

/** Routines for scanning certain kinds of data. */

/**
 * Scans a sequence of fastf_t numbers from a string or stdin
 *
 * Scanning fastf_t numbers with bu_sscanf() is difficult, because
 * doing so requires scanning to some intermediate type like double
 * and then assigning to the fastf_t variable to convert the value to
 * whatever type fastf_t really is.  This function makes it possible
 * to scan a series of fastf_t numbers separated by some character(s)
 * easily, by doing the required conversion internally to the
 * functions.  As series of delimiter characters will be skipped,
 * empty scan fields are not supported (e.g., "0.0,,0.0,1.0" will scan
 * as 3 fields, not 4 with the 2nd skipped).
 *
 * @param[out] c Returns number of characters scanned by the function
 * @param[in] src A source string to scan from, or NULL to read from stdin
 * @param[in] delim Any delimiter character(s) to skip between scan values
 * @param[in] n Number of fastf_t values to scan from the src input string
 * @param[out] ... Pointers to fastf_t for storing scanned values (optional)
 *
 */
BU_EXPORT extern int bu_scan_fastf_t(int *c, const char *src, const char *delim, size_t n, ...);



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


/** @brief multiple-read to fill a buffer */

/**
 * Provide a general means to a read some count of items from a file
 * descriptor reading multiple times until the quantity desired is
 * obtained.  This is useful for pipes and network connections that
 * don't necessarily deliver data with the same grouping as it is
 * written with.
 *
 * If a read error occurs, a negative value will be returns and errno
 * should be set (by read()).
 * "Multiple try" read.  Read multiple times until quantity is
 * obtained or an error occurs.  This is useful for pipes.
 */
BU_EXPORT extern long int bu_mread(int fd, void *bufp, long int n);

/** @} */

__END_DECLS

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
