/*                            B U . H
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

/** @file bu.h
 *
 * Main header file for the BRL-CAD Utility Library, LIBBU.
 *
 * The two letters "BU" stand for "BRL-CAD" and "Utility".  This
 * library provides several layers of low-level utility routines,
 * providing features that make cross-platform coding easier.
 *
 * Parallel processing support:  threads, semaphores, parallel-malloc.
 * Consolidated logging support:  bu_log(), bu_exit(), and bu_bomb().
 *
 * The intention is that these routines are general extensions to the
 * data types offered by the C language itself, and to the basic C
 * runtime support provided by the system LIBC.  All routines in LIBBU
 * are designed to be "parallel-safe" (sometimes called "mp-safe" or
 * "thread-safe" if parallelism is via threading) to greatly ease code
 * development for multiprocessor systems.
 *
 * All of the data types provided by this library are defined in bu.h
 * or appropriate included files from the ./bu subdirectory; none of
 * the routines in this library will depend on data types defined in
 * other BRL-CAD header files, such as vmath.h.  Look for those
 * routines in LIBBN.
 *
 * All truly fatal errors detected by the library use bu_bomb() to
 * exit with a status of 12.  The LIBBU variants of system calls
 * (e.g., bu_malloc()) do not return to the caller (unless there's a
 * bomb hook defined) unless they succeed, thus sparing the programmer
 * from constantly having to check for NULL return codes.
 *
 */
#ifndef BU_H
#define BU_H

#include "common.h"

#include <stdlib.h>
#include <sys/types.h>
#include <stdarg.h>

__BEGIN_DECLS

#include "./bu/defines.h"

/* system interface headers */
#include <setjmp.h> /* for bu_setjmp */
#include <stddef.h> /* for size_t */
#include <limits.h> /* for CHAR_BIT */

#ifdef HAVE_STDINT_H
#  include <stdint.h> /* for [u]int[16|32|64]_t */
#endif

#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>	/* for RTLD_* */
#endif

/* common interface headers */
#include "tcl.h"	/* Included for Tcl_Interp definition */
#include "bu/magic.h"

/* FIXME Temporary global interp.  Remove me.  */
BU_EXPORT extern Tcl_Interp *brlcad_interp;


/** @file libbu/vers.c
 *
 * version information about LIBBU
 *
 */

/**
 * returns the compile-time version of libbu
 */
BU_EXPORT extern const char *bu_version(void);

/**
 * MAX_PSW - The maximum number of processors that can be expected on
 * this hardware.  Used to allocate application-specific per-processor
 * tables at compile-time and represent a hard limit on the number of
 * processors/threads that may be spawned. The actual number of
 * available processors is found at runtime by calling bu_avail_cpus()
 */
#define MAX_PSW 1024


#include "./bu/cv.h"
#include "./bu/endian.h"
#include "./bu/list.h"
#include "./bu/bitv.h"
#include "./bu/hist.h"
#include "./bu/ptbl.h"
#include "./bu/mapped_file.h"
#include "./bu/vls.h"
#include "./bu/avs.h"
#include "./bu/vlb.h"
#include "./bu/debug.h"
#include "./bu/parse.h"
#include "./bu/color.h"
#include "./bu/rb.h"

/**
 * TBD
 */
struct bu_observer {
    struct bu_list l;
    struct bu_vls observer;
    struct bu_vls cmd;
};
typedef struct bu_observer bu_observer_t;
#define BU_OBSERVER_NULL ((struct bu_observer *)0)

/**
 * asserts the integrity of a non-head node bu_observer struct.
 */
#define BU_CK_OBSERVER(_op) BU_CKMAG(_op, BU_OBSERVER_MAGIC, "bu_observer magic")

/**
 * initializes a bu_observer struct without allocating any memory.
 */
#define BU_OBSERVER_INIT(_op) { \
	BU_LIST_INIT_MAGIC(&(_op)->l, BU_OBSERVER_MAGIC); \
	BU_VLS_INIT(&(_op)->observer); \
	BU_VLS_INIT(&(_op)->cmd); \
    }

/**
 * macro suitable for declaration statement initialization of a bu_observer
 * struct.  does not allocate memory.  not suitable for a head node.
 */
#define BU_OBSERVER_INIT_ZERO { {BU_OBSERVER_MAGIC, BU_LIST_NULL, BU_LIST_NULL}, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO }

/**
 * returns truthfully whether a bu_observer has been initialized.
 */
#define BU_OBSERVER_IS_INITIALIZED(_op) (((struct bu_observer *)(_op) != BU_OBSERVER_NULL) && LIKELY((_op)->magic == BU_OBSERVER_MAGIC))

/*----------------------------------------------------------------------*/

#include "./bu/log.h"
#include "./bu/file.h"

/*----------------------------------------------------------------------*/

/** @addtogroup getopt */
/** @{ */

/** @file libbu/getopt.c
 *
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
 * returns the next known option character in ostr.  If bu_getopt()
 * encounters a character not found in ostr or if it detects a missing
 * option argument, it returns `?' (question mark).  If ostr has a
 * leading `:' then a missing option argument causes `:' to be
 * returned instead of `?'.  In either case, the variable bu_optopt is
 * set to the character that caused the error.  The bu_getopt()
 * function returns -1 when the argument list is exhausted.
 */
BU_EXPORT extern int bu_getopt(int nargc, char * const nargv[], const char *ostr);

/** @} */

#include "./bu/parallel.h"
#include "./bu/malloc.h"

/** @file libbu/str.c
 *
 * Compatibility routines to various string processing functions
 * including strlcat and strlcpy.
 *
 */

/**
 * concatenate one string onto the end of another, returning the
 * length of the dst string after the concatenation.
 *
 * bu_strlcat() is a macro to bu_strlcatm() so that we can report the
 * file name and line number of any erroneous callers.
 */
BU_EXPORT extern size_t bu_strlcatm(char *dst, const char *src, size_t size, const char *label);
#define bu_strlcat(dst, src, size) bu_strlcatm(dst, src, size, BU_FLSTR)

/**
 * copies one string into another, returning the length of the dst
 * string after the copy.
 *
 * bu_strlcpy() is a macro to bu_strlcpym() so that we can report the
 * file name and line number of any erroneous callers.
 */
BU_EXPORT extern size_t bu_strlcpym(char *dst, const char *src, size_t size, const char *label);
#define bu_strlcpy(dst, src, size) bu_strlcpym(dst, src, size, BU_FLSTR)

/**
 * Given a string, allocate enough memory to hold it using
 * bu_malloc(), duplicate the strings, returns a pointer to the new
 * string.
 *
 * bu_strdup() is a macro that includes the current file name and line
 * number that can be used when bu debugging is enabled.
 */
BU_EXPORT extern char *bu_strdupm(const char *cp, const char *label);
#define bu_strdup(s) bu_strdupm(s, BU_FLSTR)

/**
 * Compares two strings for equality.  It performs the comparison more
 * robustly than the standard library's strcmp() function by defining
 * consistent behavior for NULL and empty strings.  It accepts NULL as
 * valid input values and considers "" and NULL as equal.  Returns 0
 * if the strings match.
 */
BU_EXPORT extern int bu_strcmp(const char *string1, const char *string2);

/**
 * Compares two strings for equality.  No more than n-characters are
 * compared.  It performs the comparison more robustly than the
 * standard library's strncmp() function by defining consistent
 * behavior for NULL and empty strings.  It accepts NULL as valid
 * input values and considers "" and NULL as equal.  Returns 0 if the
 * strings match.
 */
BU_EXPORT extern int bu_strncmp(const char *string1, const char *string2, size_t n);

/**
 * Compares two strings for equality without regard for the case in
 * the string.  It performs the comparison more robustly than the
 * standard strcasecmp()/stricmp() function by defining consistent
 * behavior for NULL and empty strings.  It accepts NULL as valid
 * input values and considers "" and NULL as equal.  Returns 0 if the
 * strings match.
 */
BU_EXPORT extern int bu_strcasecmp(const char *string1, const char *string2);

/**
 * Compares two strings for equality without regard for the case in
 * the string.  No more than n-characters are compared.  It performs
 * the comparison more robustly than the standard
 * strncasecmp()/strnicmp() function by defining consistent behavior
 * for NULL and empty strings.  It accepts NULL as valid input values
 * and considers "" and NULL as equal.  Returns 0 if the strings
 * match.
 */
BU_EXPORT extern int bu_strncasecmp(const char *string1, const char *string2, size_t n);

/**
 * BU_STR_EMPTY() is a convenience macro that tests a string for
 * emptiness, i.e. "" or NULL.
 */
#define BU_STR_EMPTY(s) (bu_strcmp((s), "") == 0)

/**
 * BU_STR_EQUAL() is a convenience macro for testing two
 * null-terminated strings for equality.  It is equivalent to
 * (bu_strcmp(s1, s2) == 0) whereby NULL strings are allowed and
 * equivalent to an empty string.  Evaluates true when the strings
 * match and false if they do not.
 */
#define BU_STR_EQUAL(s1, s2) (bu_strcmp((s1), (s2)) == 0)

/**
 * BU_STR_EQUIV() is a convenience macro that compares two
 * null-terminated strings for equality without regard for case.  Two
 * strings are equivalent if they are a case-insensitive match.  NULL
 * strings are allowed and equivalent to an empty string.  Evaluates
 * true if the strings are similar and false if they are not.
 */
#define BU_STR_EQUIV(s1, s2) (bu_strcasecmp((s1), (s2)) == 0)


/** @file escape.c
 *
 * These routines implement support for escaping and unescaping
 * generalized strings that may represent filesystem paths, URLs,
 * object lists, and more.
 *
 */

/**
 * Escapes an input string with preceding '\'s for any characters
 * defined in the 'expression' string.  The input string is written to the
 * specified output buffer of 'size' capacity.  The input and output
 * pointers may overlap or be the same memory (assuming adequate space
 * is available).  If 'output' is NULL, then dynamic memory will be
 * allocated and returned.
 *
 * The 'expression' parameter is a regular "bracket expression"
 * commonly used in globbing and POSIX regular expression character
 * matching.  An expression can be either a matching list (default) or
 * a non-matching list (starting with a circumflex '^' character).
 * For example, "abc" matches any of the characters 'a', 'b', or 'c'.
 * Specifying a non-matching list expression matches any character
 * except for the ones listed after the circumflex.  For example,
 * "^abc" matches any character except 'a', 'b', or 'c'.
 *
 * Backslash escape sequences are not allowed (e.g., \t or \x01) as
 * '\' will be matched literally.
 *
 * A range expression consists of two characters separated by a hyphen
 * and will match any single character between the two characters.
 * For example, "0-9a-c" is equivalent to "0123456789abc".  To match a
 * '-' dash literally, include it as the last or first (after any '^')
 * character within the expression.
 *
 * The expression may also contain named character classes but only
 * for ASCII input strings:
 *
 * [:alnum:] Alphanumeric characters: a-zA-Z0-9
 * [:alpha:] Alphabetic characters: a-zA-Z
 * [:blank:] Space and TAB characters
 * [:cntrl:] Control characters: ACSII 0x00-0X7F
 * [:digit:] Numeric characters: 0-9
 * [:graph:] Characters that are both printable and visible: ASCII 0x21-0X7E
 * [:lower:] Lowercase alphabetic characters: a-z
 * [:print:] Visible and space characters (not control characters): ASCII 0x20-0X7E
 * [:punct:] Punctuation characters (not letters, digits, control, or space): ][!"#$%&'()*+,./:;<=>?@^_`{|}~-\
 * [:upper:] Uppercase alphabetic characters: A-Z
 * [:xdigit:] Hexadecimal digits: a-fA-F0-9
 * [:word:] (non-POSIX) Alphanumeric plus underscore: a-zA-Z0-9_
 *
 * A non-NULL output string is always returned.  This allows
 * expression chaining and embedding as function arguments but care
 * should be taken to free the dynamic memory being returned when
 * 'output' is NULL.
 *
 * If output 'size' is inadequate for holding the escaped input
 * string, bu_bomb() is called.
 *
 * Example:
 *   char *result;
 *   char buf[128];
 *   result = bu_str_escape("my fair lady", " ", buf, 128);
 *   :: result == buf == "my\ fair\ lady"
 *   result = bu_str_escape(buf, "\", NULL, 0);
 *   :: result == "my\\ fair\\ lady"
 *   :: buf == "my\ fair\ lady"
 *   bu_free(result, "bu_str_escape");
 *   result = bu_str_escape(buf, "a-zA-Z", buf, 128);
 *   :: result == buf == "\m\y\ \f\a\i\r\ \l\a\d\y"
 *
 * This function should be thread safe and re-entrant if the
 * input/output buffers are not shared (and strlen() is threadsafe).
 */
BU_EXPORT extern char *bu_str_escape(const char *input, const char *expression, char *output, size_t size);

/**
 * Removes one level of '\' escapes from an input string.  The input
 * string is written to the specified output buffer of 'size'
 * capacity.  The input and output pointers may overlap or be the same
 * memory.  If 'output' is NULL, then dynamic memory will be allocated
 * and returned.
 *
 * A non-NULL output string is always returned.  This allows
 * expression chaining and embedding as function arguments but care
 * should be taken to free the dynamic memory being returned when
 * 'output' is NULL.
 *
 * If output 'size' is inadequate for holding the unescaped input
 * string, bu_bomb() is called.
 *
 * Example:
 *   char *result;
 *   char buf[128];
 *   result = bu_str_unescape("\m\y\\ \f\a\i\r\\ \l\a\d\y", buf, 128);
 *   :: result == buf == "my\ fair\ lady"
 *   result = bu_str_unescape(buf, NULL, 0);
 *   :: result == "my fair lady"
 *   :: buf == "my\ fair\ lady"
 *   bu_free(result, "bu_str_unescape");
 *
 * This function should be thread safe and re-entrant if the
 * input/output buffers are not shared (and strlen() is threadsafe).
 */
BU_EXPORT extern char *bu_str_unescape(const char *input, char *output, size_t size);


/** @} */


/** @addtogroup hton */
/** @{ */
/** @file libbu/htester.c
 *
 * @brief
 * Test network float conversion.
 *
 * Expected to be used in pipes, or with TTCP links to other machines,
 * or with files RCP'ed between machines.
 *
 */

/** @file libbu/xdr.c
 *
 * DEPRECATED.
 *
 * Routines to implement an external data representation (XDR)
 * compatible with the usual InterNet standards, e.g.:
 * big-endian, twos-compliment fixed point, and IEEE floating point.
 *
 * Routines to insert/extract short/long's into char arrays,
 * independent of machine byte order and word-alignment.
 * Uses encoding compatible with routines found in libpkg,
 * and BSD system routines htonl(), htons(), ntohl(), ntohs().
 *
 */

/**
 * DEPRECATED: use ntohll()
 * Macro version of library routine bu_glonglong()
 * The argument is expected to be of type "unsigned char *"
 */
#define BU_GLONGLONG(_cp)	\
    ((((uint64_t)((_cp)[0])) << 56) |	\
     (((uint64_t)((_cp)[1])) << 48) |	\
     (((uint64_t)((_cp)[2])) << 40) |	\
     (((uint64_t)((_cp)[3])) << 32) |	\
     (((uint64_t)((_cp)[4])) << 24) |	\
     (((uint64_t)((_cp)[5])) << 16) |	\
     (((uint64_t)((_cp)[6])) <<  8) |	\
     ((uint64_t)((_cp)[7])))
/**
 * DEPRECATED: use ntohl()
 * Macro version of library routine bu_glong()
 * The argument is expected to be of type "unsigned char *"
 */
#define BU_GLONG(_cp)	\
    ((((uint32_t)((_cp)[0])) << 24) |	\
     (((uint32_t)((_cp)[1])) << 16) |	\
     (((uint32_t)((_cp)[2])) <<  8) |	\
     ((uint32_t)((_cp)[3])))
/**
 * DEPRECATED: use ntohs()
 * Macro version of library routine bu_gshort()
 * The argument is expected to be of type "unsigned char *"
 */
#define BU_GSHORT(_cp)	\
    ((((uint16_t)((_cp)[0])) << 8) | \
     (_cp)[1])

/**
 * DEPRECATED: use ntohs()
 */
DEPRECATED BU_EXPORT extern uint16_t bu_gshort(const unsigned char *msgp);

/**
 * DEPRECATED: use ntohl()
 */
DEPRECATED BU_EXPORT extern uint32_t bu_glong(const unsigned char *msgp);

/**
 * DEPRECATED: use htons()
 */
DEPRECATED BU_EXPORT extern unsigned char *bu_pshort(unsigned char *msgp, uint16_t s);

/**
 * DEPRECATED: use htonl()
 */
DEPRECATED BU_EXPORT extern unsigned char *bu_plong(unsigned char *msgp, uint32_t l);

/**
 * DEPRECATED: use htonll()
 */
DEPRECATED BU_EXPORT extern unsigned char *bu_plonglong(unsigned char *msgp, uint64_t l);

/** @} */

/** @addtogroup tcl */
/** @{ */
/** @file libbu/observer.c
 *
 * @brief
 * Routines for implementing the observer pattern.
 *
 */

/**
 * runs a given command, calling the corresponding observer callback
 * if it matches.
 */
BU_EXPORT extern int bu_observer_cmd(void *clientData, int argc, const char *argv[]);

/**
 * Notify observers.
 */
BU_EXPORT extern void bu_observer_notify(Tcl_Interp *interp, struct bu_observer *headp, char *self);

/**
 * Free observers.
 */
BU_EXPORT extern void bu_observer_free(struct bu_observer *);

/**
 * Bu_Init
 *
 * Allows LIBBU to be dynamically loaded to a vanilla tclsh/wish with
 * "load /usr/brlcad/lib/libbu.so"
 *
 * @param interp	- tcl interpreter in which this command was registered.
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
BU_EXPORT extern int Bu_Init(void *interp);


/** @} */

#include "bu/hash.h"

/** @file libbu/ctype.c
 *
 * Routines for checking ctypes.
 *
 */
BU_EXPORT extern int bu_str_isprint(const char *cp);

/**
 * Get the current operating host's name.  This is usually also the
 * network name of the current host.  The name is written into the
 * provided hostname buffer of at least len size.  The hostname is
 * always null-terminated and should be sized accordingly.
 */
BU_EXPORT extern int bu_gethostname(char *hostname, size_t len);

__END_DECLS

#endif  /* BU_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
