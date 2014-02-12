/*                           V L S . H
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

/** @defgroup container Data Containers */
/**   @defgroup vls Variable-length Strings */

/** @file vls.h
 */
#ifndef BU_VLS_H
#define BU_VLS_H

#include "common.h"
#include <stdio.h> /* For FILE */
#include <sys/types.h> /* for off_t */
#include <stddef.h> /* for size_t */
#ifdef HAVE_STDINT_H
#  include <stdint.h> /* for [u]int[16|32|64]_t */
#endif

#include "./defines.h"

/*----------------------------------------------------------------------*/
/** @addtogroup vls */
/** @ingroup container */
/** @{ */
/** @file libbu/vls.c
 *
 @brief
 * Variable Length Strings
 *
 * This structure provides support for variable length strings,
 * freeing the programmer from concerns about having character arrays
 * large enough to hold strings.
 *
 * Assumption:  libc-provided sprintf() function is safe to use in parallel,
 * on parallel systems.
 */

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
 * DEPRECATED: use if (!vls) bu_vls_init(vls)
 *
 * If a VLS is uninitialized, initialize it.  If it is already
 * initialized, leave it alone, caller wants to append to it.
 */
DEPRECATED BU_EXPORT extern void bu_vls_init_if_uninit(struct bu_vls *vp);

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
				    off_t len);

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
				     char *str);


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
