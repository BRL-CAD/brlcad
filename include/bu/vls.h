/*                           V L S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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

#ifndef BU_VLS_H
#define BU_VLS_H

#include "common.h"
#include <stdio.h> /* for FILE */
#include <stddef.h> /* for size_t */
#include <stdarg.h> /* For va_list */


#include "bu/defines.h"
#include "bu/magic.h"

__BEGIN_DECLS

/*----------------------------------------------------------------------*/
/** @addtogroup bu_vls
 *
 * @brief
 * Variable length strings provide a way for programmers to easily handling
 * dynamic strings - they serve a function similar to that of std::string in
 * C++.
 *
 * This frees the programmer from concerns about having character arrays large
 * enough to hold strings.
 *
 * Assumption:  libc-provided sprintf() function is safe to use in parallel, on
 * parallel systems.
 */
/** @{ */
/** @file bu/vls.h */

/** Primary bu_vls container */
struct bu_vls {
    uint32_t vls_magic;
    char *vls_str;	/**< Dynamic memory for buffer */
    size_t vls_offset;	/**< Positive index into vls_str where string begins */
    size_t vls_len;	/**< Length, not counting the null */
    size_t vls_max;
};
typedef struct bu_vls bu_vls_t;
#define BU_VLS_NULL ((struct bu_vls *)0)

/**
 * assert the integrity of a bu_vls struct.
 */
#define BU_CK_VLS(_vp) BU_CKMAG(_vp, BU_VLS_MAGIC, "bu_vls")

/**
 * initializes a bu_vls struct without allocating any memory.
 */
#define BU_VLS_INIT(_vp) { \
	(_vp)->vls_magic = BU_VLS_MAGIC; \
	(_vp)->vls_str = NULL; \
	(_vp)->vls_offset = (_vp)->vls_len = (_vp)->vls_max = 0; \
    }

/**
 * macro suitable for declaration statement initialization of a bu_vls
 * struct.  does not allocate memory.
 */
#define BU_VLS_INIT_ZERO { BU_VLS_MAGIC, NULL, 0, 0, 0 }

/**
 * returns truthfully whether a bu_vls struct has been initialized.
 * is not reliable unless the struct has been allocated with
 * BU_ALLOC(), bu_calloc(), or a previous call to bu_vls_init() or
 * BU_VLS_INIT() has been made.
 */
#define BU_VLS_IS_INITIALIZED(_vp) (((struct bu_vls *)(_vp) != BU_VLS_NULL) && ((_vp)->vls_magic == BU_VLS_MAGIC))

/**
 * No storage should be allocated at this point, and bu_vls_addr()
 * must be able to live with that.
 */
BU_EXPORT extern void bu_vls_init(struct bu_vls *vp);

/**
 * Allocate storage for a struct bu_vls, call bu_vls_init on it, and
 * return the result.  Allows for creation of dynamically allocated
 * VLS strings.
 */
BU_EXPORT extern struct bu_vls *bu_vls_vlsinit(void);

/**
 * Return a pointer to the null-terminated string in the vls array.
 * If no storage has been allocated yet, give back a valid string.
 */
BU_EXPORT extern char *bu_vls_addr(const struct bu_vls *vp);

/**
 * Return a pointer to the null-terminated string in the vls array.
 * If no storage has been allocated yet, give back a valid string.
 * (At the moment this function is a mnemonically-named convenience
 * function which returns a call to bu_vls_addr.)
 */
BU_EXPORT extern const char *bu_vls_cstr(const struct bu_vls *vp);

/**
 * Ensure that the provided VLS has at least 'extra' characters of
 * space available.  Additional space is allocated in minimum step
 * sized amounts and may allocate more than requested.
 */
BU_EXPORT extern void bu_vls_extend(struct bu_vls *vp,
				    size_t extra);

/**
 * Ensure that the vls has a length of at least 'newlen', and make
 * that the current length.
 *
 * Useful for subroutines that are planning on mucking with the data
 * array themselves.  Not advisable, but occasionally useful.
 *
 * Does not change the offset from the front of the buffer, if any.
 * Does not initialize the value of any of the new bytes.
 */
BU_EXPORT extern void bu_vls_setlen(struct bu_vls *vp,
				    size_t newlen);
/**
 * Return length of the string, in bytes, not including the null
 * terminator.
 */
BU_EXPORT extern size_t bu_vls_strlen(const struct bu_vls *vp);

/**
 * Truncate string to at most 'len' characters.  If 'len' is negative,
 * trim off that many from the end.  If 'len' is zero, don't release
 * storage -- user is probably just going to refill it again,
 * e.g. with bu_vls_gets().
 */
BU_EXPORT extern void bu_vls_trunc(struct bu_vls *vp,
				   int len);

/**
 * "Nibble" 'len' characters off the front of the string.  Changes the
 * length and offset; no data is copied.
 *
 * 'len' may be positive or negative. If negative, characters are
 * un-nibbled.
 */
BU_EXPORT extern void bu_vls_nibble(struct bu_vls *vp,
				    b_off_t len);

/**
 * Releases the memory used for the string buffer.
 */
BU_EXPORT extern void bu_vls_free(struct bu_vls *vp);

/**
 * Releases the memory used for the string buffer and the memory for
 * the vls structure
 */
BU_EXPORT extern void bu_vls_vlsfree(struct bu_vls *vp);
/**
 * Return a dynamic copy of a vls.  Memory for the string being
 * returned is acquired using bu_malloc() implying the caller must
 * bu_free() the returned string.
 */
BU_EXPORT extern char *bu_vls_strdup(const struct bu_vls *vp);

/**
 * Like bu_vls_strdup(), but destructively grab the string from the
 * source argument 'vp'.  This is more efficient than bu_vls_strdup()
 * for those instances where the source argument 'vp' is no longer
 * needed by the caller, as it avoids a potentially long buffer copy.
 *
 * The source string is destroyed, as if bu_vls_free() had been
 * called.
 */
BU_EXPORT extern char *bu_vls_strgrab(struct bu_vls *vp);

/**
 * Empty the vls string, and copy in a regular string.
 */
BU_EXPORT extern void bu_vls_strcpy(struct bu_vls *vp,
				    const char *s);

/**
 * Empty the vls string, and copy in a regular string, up to N bytes
 * long.
 */
BU_EXPORT extern void bu_vls_strncpy(struct bu_vls *vp,
				     const char *s,
				     size_t n);

/**
 * Concatenate a new string onto the end of the existing vls string.
 */
BU_EXPORT extern void bu_vls_strcat(struct bu_vls *vp,
				    const char *s);

/**
 * Concatenate a new string onto the end of the existing vls string.
 */
BU_EXPORT extern void bu_vls_strncat(struct bu_vls *vp,
				     const char *s,
				     size_t n);

/**
 * Concatenate a new vls string onto the end of an existing vls
 * string.  The storage of the source string is not affected.
 */
BU_EXPORT extern void bu_vls_vlscat(struct bu_vls *dest,
				    const struct bu_vls *src);

/**
 * Concatenate a new vls string onto the end of an existing vls
 * string.  The storage of the source string is released (zapped).
 */
BU_EXPORT extern void bu_vls_vlscatzap(struct bu_vls *dest,
				       struct bu_vls *src);

/**
 * Lexicographically compare two vls strings.  Returns an integer
 * greater than, equal to, or less than 0, according as the string s1
 * is greater than, equal to, or less than the string s2.
 */
BU_EXPORT extern int bu_vls_strcmp(struct bu_vls *s1,
				   struct bu_vls *s2);

/**
 * Lexicographically compare two vls strings up to n characters.
 * Returns an integer greater than, equal to, or less than 0,
 * according as the string s1 is greater than, equal to, or less than
 * the string s2.
 */
BU_EXPORT extern int bu_vls_strncmp(struct bu_vls *s1,
				    struct bu_vls *s2,
				    size_t n);

/**
 * Given and argc & argv pair, convert them into a vls string of
 * space-separated words.
 */
BU_EXPORT extern void bu_vls_from_argv(struct bu_vls *vp,
				       int argc,
				       const char *argv[]);

/**
 * Write the VLS to the provided file pointer.
 */
BU_EXPORT extern void bu_vls_fwrite(FILE *fp,
				    const struct bu_vls *vp);

/**
 * Write the VLS to the provided file descriptor.
 */
BU_EXPORT extern void bu_vls_write(int fd,
				   const struct bu_vls *vp);

/**
 * Read the remainder of a UNIX file onto the end of a vls.
 *
 * Returns -
 * nread number of characters read
 *  0 if EOF encountered immediately
 * -1 read error
 */
BU_EXPORT extern int bu_vls_read(struct bu_vls *vp,
				 int fd);

/**
 * Append a newline-terminated string from the file pointed to by "fp"
 * to the end of the vls pointed to by "vp".  The newline from the
 * file is read, but not stored into the vls.
 *
 * The most common error is to forget to bu_vls_trunc(vp, 0) before
 * reading the next line into the vls.
 *
 * Returns -
 *   >=0  the length of the resulting vls
 *   -1   on EOF where no characters were read or added to the vls
 */
BU_EXPORT extern int bu_vls_gets(struct bu_vls *vp,
				 FILE *fp);

/**
 * Append the given character to the vls.
 */
BU_EXPORT extern void bu_vls_putc(struct bu_vls *vp,
				  int c);

/**
 * Remove leading and trailing white space from a vls string.
 */
BU_EXPORT extern void bu_vls_trimspace(struct bu_vls *vp);


/**
 * Format a string into a vls using standard variable arguments.
 *
 * %s continues to be a regular null-terminated 'C' string (char *).
 * %V is a libbu variable-length string (struct bu_vls *).
 *
 * Other format specifiers should behave identical to printf().
 *
 * This routine appends to the given vls similar to how vprintf
 * appends to stdout (see bu_vls_sprintf for overwriting the vls).
 * The implementation ends up calling bu_vls_vprintf().
 */
BU_EXPORT extern void bu_vls_printf(struct bu_vls *vls,
				    const char *fmt, ...) _BU_ATTR_PRINTF23;

/**
 * Format a string into a vls, setting the vls to the given print
 * specifier expansion.  This routine truncates any existing vls
 * contents beforehand (i.e. it doesn't append, see bu_vls_vprintf for
 * appending to the vls).
 *
 * %s continues to be a regular 'C' string, null terminated.
 * %V is a pointer to a (struct bu_vls *) string.
 */
BU_EXPORT extern void bu_vls_sprintf(struct bu_vls *vls,
				     const char *fmt, ...) _BU_ATTR_PRINTF23;

/**
 * Efficiently append 'cnt' spaces to the current vls.
 */
BU_EXPORT extern void bu_vls_spaces(struct bu_vls *vp,
				    size_t cnt);

/**
 * Returns number of printed spaces used on final output line of a
 * potentially multi-line vls.  Useful for making decisions on when to
 * line-wrap.
 *
 * Accounts for normal UNIX tab-expansion:
 *	         1         2         3         4
 *	1234567890123456789012345678901234567890
 *	        x       x       x       x
 *
 *	0-7 --> 8, 8-15 --> 16, 16-23 --> 24, etc.
 */
BU_EXPORT extern size_t bu_vls_print_positions_used(const struct bu_vls *vp);

/**
 * Given a vls, return a version of that string which has had all
 * "tab" characters converted to the appropriate number of spaces
 * according to the UNIX tab convention.
 */
BU_EXPORT extern void bu_vls_detab(struct bu_vls *vp);


/**
 * Add a string to the beginning of the vls.
 */
BU_EXPORT extern void bu_vls_prepend(struct bu_vls *vp,
				     const char *str);


/**
 * Copy a substring from a source vls into a destination vls
 *
 *   where:
 *
 *     begin  - the index (0-based) of the beginning character position
 *              in the source vls
 *     nchars - the number of characters to copy
 *
 */
BU_EXPORT extern void bu_vls_substr(struct bu_vls *dest, const struct bu_vls *src,
				    size_t begin, size_t nchars);

/** @brief bu_vls_vprintf implementation */

/**
 * Format a string into a vls using a varargs list.
 *
 * %s continues to be a regular null-terminated 'C' string (char *).
 * %V is a libbu variable-length string (struct bu_vls *).
 *
 * Other format specifiers should behave identical to printf().
 *
 * This routine appends to the given vls similar to how vprintf
 * appends to stdout (see bu_vls_sprintf for overwriting the vls).
 */
BU_EXPORT extern void bu_vls_vprintf(struct bu_vls *vls,
				     const char *fmt,
				     va_list ap);


/** @brief Routines to encode/decode strings into bu_vls structures. */

/**
 * given an input string, wrap the string in double quotes if there is
 * a space and append it to the provided bu_vls.  escape any existing
 * double quotes.
 *
 * TODO: consider a specifiable quote character and octal encoding
 * instead of double quote wrapping.  perhaps specifiable encode type:
 *   BU_ENCODE_QUOTE
 *   BU_ENCODE_OCTAL
 *   BU_ENCODE_XML
 *
 * More thoughts on encode/decode - the nature of "quoting" is going to
 * vary depending on the usage context and the language.  For some
 * applications, HEX or BASE64 may be appropriate.  For others (like
 * the problems with arbitrary strings in Tcl which initially motivated
 * these functions) such wholesale encoding is not needed and it is just
 * a subset of characters that must be escaped or otherwise identified.
 *
 * Given the large set of possible scenarios, it definitely makes sense
 * to allow an encoding specifying variable, and probably other optional
 * variables (which may be NULL, depending on the encoding type) specifying
 * active characters (that need quoting) and an escape character (or
 * characters?  does it take more than one in some scenarios?  perhaps start
 * and end of escape strings would be the most general?)
 *
 * This probably makes sense as its own header given that is is really
 * a feature on top of vls rather than something integral to vls
 * itself - it would be workable (maybe even practical, if the final
 * length of the encoded data can be pre-determined) to just work with
 * char arrays: see bu_str_escape()
 *
 * the behavior of this routine is subject to change but should remain
 * a reversible operation when used in conjunction with
 * bu_vls_decode().
 *
 * returns a pointer to the encoded string (i.e., the substring held
 * within the bu_vls)
 */
BU_EXPORT extern const char *bu_vls_encode(struct bu_vls *vp, const char *str);


/**
 * given an encoded input string, unwrap the string from any
 * surrounding double quotes and unescape any embedded double quotes.
 *
 * the behavior of this routine is subject to change but should remain
 * the reverse operation of bu_vls_encode().
 *
 * returns a pointer to the decoded string (i.e., the substring held
 * within the bu_vls)
 */
BU_EXPORT extern const char *bu_vls_decode(struct bu_vls *vp, const char *str);

/**
   @brief
   Automatic string generation routines.

   There are many situations where a calling program, given an input string,
   needs to produce automatically derived output strings that satisfy some
   criteria (incremented, special characters removed, etc.)  The functions
   below perform this type of work.
*/


/**
 * A problem frequently encountered when working with strings in BRL-CAD
 * is the presence of special characters, characters active in the scripting
 * language being applied, or other problematic contents that interfere with
 * processing or readability of strings.  This function takes a vls as an
 * input and simplifies it as follows:
 *
 * 1) Reduce characters present to alphanumeric characters and those
 *    characters supplied to the routine in the "keep" string.  Substitute
 *    is performed as follows:
 *
 *    * Skip any character in the "keep" string
 *
 *    * Replace diacritic characters(code >= 192) from the extended ASCII set
 *      with a specific mapping from the standard ASCII set. See discussion at:
 *      http://stackoverflow.com/questions/14094621/ for more about this.
 *
 *    * Replace any non-keep characters that aren't replaced by other
 *      approaches with the '_' underscore character
 *
 * 2) Collapses duplicate characters in the "de_dup" string.
 *
 * 3) Remove leading and trailing characters in the "trim' string.
 *
 * Returns 0 if string was not altered, 1 otherwise.
 */
BU_EXPORT extern int bu_vls_simplify(struct bu_vls *vp, const char *keep, const char *de_dup, const char *trim);


/**
 * callback type for bu_vls_incr()
 */
typedef int (*bu_vls_uniq_t)(struct bu_vls *v, void *data);


/**
 * A problem frequently encountered when generating names is
 * generating names that are in some sense structured but avoid
 * colliding.  For example, given a geometry object named:
 *
 * @verbatim
 * engine_part.s
 * @endverbatim
 *
 * An application wanting to make multiple copies of engine_part.s
 * automatically might want to produce a list of names such as:
 *
 * @verbatim
 * engine_part.s-1, engine_part.s-2, engine_part.s-3, ...
 * @endverbatim
 *
 * However, it is equally plausible that the desired pattern might be:
 *
 * @verbatim
 * engine_part_0010.s, engine_part_0020.s, engine_part_0030.s, ...
 * @endverbatim
 *
 * This function implements an engine for generating the "next" name
 * in a sequence, given an initial name supplied by the caller and
 * (optionally) information to identify the incrementor in the string
 * and the incrementing behavior desired.  bu_vls_incr does not track
 * any "state" for individual strings - all information is contained
 * either in the current state of the input string itself or the
 * incrementation specifier (more details on the latter can be found
 * below.)
 *
 * @param[in,out] name Contains the "seed" string for the name
 * generation.  Upon return the old string is cleared and the new one
 * resides in name
 *
 * @param[in] regex_str Optional - user supplied regular expression
 * for locating the incrementer substring.
 *
 * @param[in] incr_spec Optional - string of colon separated
 * parameters defining function behavior.
 *
 * @param[in] uniq_test Optional - uniqueness testing function.
 * 
 * @param[in] data Optional - data to pass to the uniq_test
 * function call.
 * 
 *
 * @section bu_vls_incr_regexp Incrementer Substring Identification
 *
 * bu_vls_incr uses regular expressions to identify the numerical part
 * of a supplied string.  By default, if no regular expression is
 * supplied by the caller, bu_vls_incr will use a numerical sequence
 * at the end of the string (more precisely, it will use the last
 * sequence of numbers if and only if there are no non-numerical
 * characters between that sequence and the end of the string.) If no
 * appropriate match is found, the incrementer will be appended to the
 * end of the string.
 *
 * When no pre-existing number is found to start the sequence, the
 * default behavior is to treat the existing string as the "0"-th item
 * in the sequence.  In such cases bu_vls_incr will thus return one,
 * not zero, as the first incremented name in the sequence.
 *
 * If the caller wishes to be more sophisticated about where
 * incrementers are found in strings, they may supply their own
 * regular expression that uses parenthesis bounded matchers to
 * identify numerical identifiers.  For example, the regular
 * expression:
 *
 * @verbatim
 * ([-_:]*[0-9]+[-_:]*)[^0-9]*$
 * @endverbatim
 *
 * will instruct bu_vls_incr to define not just the last number but
 * the last number and any immediately surrounding separator
 * characters as the subset of the string to process.  Combined with a
 * custom incrementation specifier, this option allows calling
 * programs to exercise broad flexibility in how they process strings.
 *
 * @section bu_vls_incr_inc Specifying Incrementor Behavior
 *
 * The caller may optionally specify incrementation behavior with an
 * incrementer specification string, which has the following form:
 * "minwidth:init:max:step[:left_sepchar:right_sepchar]"
 *
 * The table below explains the individual elements.
 *
 * <table>
 * <tr><th colspan="2">"minwidth:init:max:step[:left_sepchar:right_sepchar]"</th></tr>
 * <tr><td>minwidth</td>     <td>specifies the minimum number of digits used when printing the incrementer substring</td>
 * <tr><td>init</td>         <td>specifies the initial minimum value to use when returning an incremented string. Overrides the string-based value if that value is less than the init value.</td>
 * <tr><td>max</td>          <td>specifies the maximum value of the incrementer - if the step takes the value past this number, the counter "rolls over" to the init value.</td>
 * <tr><td>step</td>         <td>value by which the incrementor value is increased</td>
 * <tr><td>left_sepchar</td> <td>optional - specify a character to insert to the left of the numerical substring</td>
 * <tr><td>right_sepchar</td><td>optional - specify a character to insert to the right of the numerical substring</td>
 * </table>
 *
 * In the case of minwidth, init, max, and step a '0' always indicates
 * unspecified (i.e. "default") behavior.  So, an incrementer
 * specified as 0:0:0:0 would behave as follows:
 *
 * minwidth: width found in string, or standard printf handling if
 * nothing useful is found.  For example, engine_part-001.s would by
 * default be incremented to engine_part-002.s, preserving the prefix
 * zeros.
 *
 * init: from initial name string, or 0 if not found
 *
 * max: LONG_MAX
 *
 * step: 1
 *
 * The last two separator chars are optional - they are useful if the
 * caller wants to guarantee a separation between the active
 * incremented substring and its surroundings. For example, the
 * following would prefix the incrementer output with an underscore
 * and add a suffix with a dash:
 *
 * @verbatim
 * 0:0:0:0:_:-
 * @endverbatim
 *
 * @section bu_vls_incr_ex Examples
 *
 * To generate a suffix pattern, e.g.,:
 *
 * @verbatim
 * engine_part.s-1, engine_part.s-2, engine_part.s-3, ...
 * @endverbatim
 *
 * Example code is as follows:
 *
 * @code
 * struct bu_vls name = BU_VLS_INIT_ZERO;
 * bu_vls_sprintf(&name, "engine_part.s");
 * for (int i = 0; i < 10; i++) {
 *   bu_vls_incr(&name, NULL, "0:0:0:0:-", NULL, NULL);
 *   bu_log("%s\n", bu_vls_cstr(&name));
 * }
 * bu_vls_free(&name);
 * @endcode
 *
 * Here we show an infix case.  There is no number present in the
 * original string, we want the number *before* the .s suffix, we're
 * incrementing by more than 1, we want four numerical digits in the
 * string, and we want an underscore prefix spacer before the number:
 *
 * @verbatim
 * engine_part_0010.s, engine_part_0020.s, engine_part_0030.s, ...
 * @endverbatim
 *
 * To set this up correctly we take advantage of the bu_path_component
 * function and construct an initial "seed" string with a zero
 * incrementer where we need it:
 *
 * @code
 * const char *estr = "engine_part.s"
 * struct bu_vls name = BU_VLS_INIT_ZERO;
 * bu_vls_sprintf(&name, "%s0.%s",
 *                bu_path_component(estr, BU_PATH_EXTLESS),
 *                bu_path_component(estr, BU_PATH_EXT));
 * for (int i = 0; i < 10; i++) {
 *   bu_vls_incr(&name, NULL, "4:10:0:10:_", NULL, NULL);
 *   bu_log("%s\n", bu_vls_cstr(&name));
 * }
 * bu_vls_free(&name);
 * @endcode
 *
 */
BU_EXPORT extern int bu_vls_incr(struct bu_vls *name, const char *regex_str, const char *incr_spec, bu_vls_uniq_t uniq_test, void *data);

__END_DECLS

/** @} */

#endif  /* BU_VLS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
