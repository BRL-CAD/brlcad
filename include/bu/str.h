/*                         S T R . H
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

#ifndef BU_STR_H
#define BU_STR_H

#include "common.h"
#include "bu/defines.h"

__BEGIN_DECLS

/** @addtogroup bu_str
 *
 * @brief
 * Compatibility routines to various string processing functions
 * including strlcat and strlcpy.
 *
 */
/** @{ */
/** @file bu/str.h */

/**
 * concatenate one string onto the end of another, returning the
 * length of the dst string after the concatenation.
 *
 * bu_strlcat() is a macro to bu_strlcatm() so that we can report the
 * file name and line number of any erroneous callers.
 */
BU_EXPORT extern size_t bu_strlcatm(char *dst, const char *src, size_t size, const char *label);
#define bu_strlcat(dst, src, size) bu_strlcatm(dst, src, size, CPP_FILELINE)

/**
 * copies one string into another, returning the length of the dst
 * string after the copy.
 *
 * bu_strlcpy() is a macro to bu_strlcpym() so that we can report the
 * file name and line number of any erroneous callers.
 */
BU_EXPORT extern size_t bu_strlcpym(char *dst, const char *src, size_t size, const char *label);
#define bu_strlcpy(dst, src, size) bu_strlcpym(dst, src, size, CPP_FILELINE)

/**
 * Given a string, allocate enough memory to hold it using
 * bu_malloc(), duplicate the strings, returns a pointer to the new
 * string.
 *
 * bu_strdup() is a macro that includes the current file name and line
 * number that can be used when bu debugging is enabled.
 */
BU_EXPORT extern char *bu_strdupm(const char *cp, const char *label);
#define bu_strdup(s) bu_strdupm(s, CPP_FILELINE)

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


/**
 * @brief
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
 * Backslash escape sequences are not allowed (e.g., \\t or \\x01) as
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
 @code
    char *result;
    char buf[128];
    result = bu_str_escape("my fair lady", " ", buf, 128);
    :: result == buf == "my\ fair\ lady"
    result = bu_str_escape(buf, "\", NULL, 0);
    :: result == "my\\ fair\\ lady"
    :: buf == "my\ fair\ lady"
    bu_free(result, "bu_str_escape");
    result = bu_str_escape(buf, "a-zA-Z", buf, 128);
    :: result == buf == "\m\y\ \f\a\i\r\ \l\a\d\y"
 @endcode
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
 @code
    char *result;
    char buf[128];
    result = bu_str_unescape("\m\y\\ \f\a\i\r\\ \l\a\d\y", buf, 128);
    :: result == buf == "my\ fair\ lady"
    result = bu_str_unescape(buf, NULL, 0);
    :: result == "my fair lady"
    :: buf == "my\ fair\ lady"
    bu_free(result, "bu_str_unescape");
 @endcode
 *
 * This function should be thread safe and re-entrant if the
 * input/output buffers are not shared (and strlen() is threadsafe).
 */
BU_EXPORT extern char *bu_str_unescape(const char *input, char *output, size_t size);


/** @brief Routines for checking ctypes. */

BU_EXPORT extern int bu_str_isprint(const char *cp);


/** @brief routines for parsing boolean values from strings */

/**
 * Returns truthfully if a given input string represents an
 * "affirmative string".
 *
 * Input values that are case-insensitively NULL, empty (i.e., ""
 * after disregarding whitespace), "n", "no", "false", "off",
 * "(null)", or are 0-valued return as false.  Any other input value
 * returns true.
 *
 * Strings strongly indicating true such as "y", "yes", "true", "on",
 * or are 1-valued will return as 1, other non-empty strings still
 * return as true but may be greater than 1.
 */
BU_EXPORT extern int bu_str_true(const char *str);

/**
 * Returns truthfully if a given input string represents a
 * "negative string".
 *
 * Input values that are case-insensitively NULL, empty (i.e., ""
 * after disregarding whitespace), "n", "no", "false", "off",
 * "(null)", or are 0-valued return as true.  Any other input value
 * returns as false.
 */
BU_EXPORT extern int bu_str_false(const char *str);


/** @brief Functions related to argv processing. */

/**
 * Build argv[] array from input buffer, by splitting whitespace
 * separated "words" into null terminated strings.
 *
 * 'lim' indicates the maximum number of elements that can be stored
 * in the argv[] array not including a terminating NULL.
 *
 * The 'lp' input buffer is altered by this process.  The argv[] array
 * points into the input buffer.
 *
 * The argv[] array needs to have at least lim+1 pointers allocated
 * for lim items plus a terminating pointer to NULL.  The input buffer
 * should not be freed until argv has been freed or passes out of
 * scope.
 *
 * Returns -
 * 0    no words in input
 * argc number of words of input, now in argv[]
 */
BU_EXPORT extern size_t bu_argv_from_string(char *argv[],
					    size_t lim,
					    char *lp);


/**
 * libbu replacement for Tcl's ascii list to argc/argv functionality
 * (Note: function signature duplicates that of Tcl_SplitList)
 */
BU_EXPORT extern int bu_argv_from_tcl_list(const char *list_str,
					   int *argc,
					   const char ***argv);

/**
 * Deallocate all strings in a given argv array and the array itself
 *
 * This call presumes the array has been allocated with bu_argv_dup()
 * or bu_path_to_argv().
 */
BU_EXPORT extern void bu_argv_free(size_t argc, char *argv[]);

/**
 * free up to argc elements of memory allocated to an array without
 * free'ing the array itself.
 */
BU_EXPORT extern void bu_free_args(size_t argc, char *argv[], const char *str);

/**
 * Dynamically duplicate an argv array and all elements in the array
 *
 * Duplicate an argv array by duplicating all strings and the array
 * itself.  It is the caller's responsibility to free the array
 * returned including all elements in the array by calling bu_free()
 * or bu_argv_free().
 */
BU_EXPORT extern char **bu_argv_dup(size_t argc, const char *argv[]);

/**
 * Combine two argv arrays into one new (duplicated) argv array.
 *
 * If insert is negative, the insertArgv array elements will be
 * prepended into the new argv array.  If insert is greater than or
 * equal to argc, the insertArgv array elements will be appended after
 * all duplicated elements in the specified argv array.  Otherwise,
 * the insert argument is the position where the insertArgv array
 * elements will be merged with the specified argv array.
 */
BU_EXPORT extern char **bu_argv_dupinsert(int insert, size_t insertArgc, const char *insertArgv[], size_t argc, const char *argv[]);

/**
 * Calculate the Damarau Levenshtein edit distance between two strings.  When
 * max_dist is defined, calculation will terminate once that distance is reached
 * and max_dist will be returned.  When max_dist is 0, the calculation will
 * proceed up to an internally defined calculation limit. */
BU_EXPORT unsigned long
bu_editdist(const char *s1, const char *s2);


__END_DECLS

/** @} */

#endif  /* BU_STR_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
