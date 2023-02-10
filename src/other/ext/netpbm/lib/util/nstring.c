/*=============================================================================
                               nstring.c
===============================================================================

  pm_snprintf (and pm_vsnprintf) in this file used to be derived from
  'portable_snprintf' from
  http://www.ijs.si/software/snprintf/snprintf-2.2.tar.gz, because not all
  system C libraries had snprintf.  But in 2013, we extended that snprintf to
  implement %f by calling 'snprintf' in the system C library, just to see if
  it caused any build failures.  As of August 2022, there had been no
  complaints of problems caused by this reliance on the system providing
  snprintf, so we just made pm_snprintf a wrapper of snprintf for everything.

  Eventually we will remove pm_snprintf and pm_vsnprintf altogether and their
  callers will call 'snprintf' and 'vsnprintf' instead

  Note that snprintf is required by the C99 standard.

  The code from which pm_snprintf was formerly derived was protected by
  copyright and licensed to the public under GPL.  A user in August 2022 noted
  that GPL was insufficient for his use of it, making him unable to use
  libnetpbm.

  Code in this file is contributed to the public domain by its authors.
=============================================================================*/
#define _DEFAULT_SOURCE /* New name for SVID & BSD source defines */
#define _XOPEN_SOURCE 500  /* Make sure strdup() is in string.h */
#define _BSD_SOURCE  /* Make sure strdup() is in string.h */
#define _DARWIN_C_SOURCE   /* Make sure strdup() is in string.h */
#define _GNU_SOURCE
   /* Because of conditional compilation, this is GNU source only if the C
      library is GNU.
   */
#include <sys/types.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>

#include "pm.h"
#include "pm_c_util.h"

#include "nstring.h"

/* MacOS X before 10.7, for one, does not have strnlen */
size_t
pm_strnlen(const char * const s,
           size_t       const maxlen) {

    unsigned int i;

    for (i = 0; i < maxlen && s[i]; ++i) {}

    return i;
}



void
pm_vsnprintf(char *       const str,
             size_t       const maxSize,
             const char * const fmt,
             va_list            ap,
             size_t *     const sizeP) {

    int rc;

    rc = vsnprintf(str, maxSize, fmt, ap);

    assert((size_t)rc == rc);

    *sizeP = (size_t)rc;
}



int
pm_snprintf(char *       const dest,
            size_t       const maxSize,
            const char * const fmt,
            ...) {

    size_t size;
    va_list ap;

    va_start(ap, fmt);

    pm_vsnprintf(dest, maxSize, fmt, ap, &size);

    va_end(ap);

    assert(size <= INT_MAX);

    return size;
}



/* When a function that is supposed to return a malloc'ed string cannot
   get the memory for it, it should return 'pm_strsol'.  That has a much
   better effect on the caller, if the caller doesn't explicitly allow for
   the out of memory case, than returning NULL.  Note that it is very
   rare for the system not to have enough memory to return a small string,
   so it's OK to have somewhat nonsensical behavior when it happens.  We
   just don't want catastrophic behavior.

   'pm_strsol' is an external symbol, so if Caller wants to detect the
   out-of-memory failure, he certainly can.
*/
const char * const pm_strsol = "NO MEMORY TO CREATE STRING!";



const char *
pm_strdup(const char * const arg) {

    const char * const dup = strdup(arg);

    return dup ? dup : pm_strsol;
}



void PM_GNU_PRINTF_ATTR(2,3)
pm_asprintf(const char ** const resultP,
            const char *  const fmt,
            ...) {

    const char * result;
    va_list varargs;

#if HAVE_VASPRINTF
    int rc;
    va_start(varargs, fmt);
    rc = vasprintf((char **)&result, fmt, varargs);
    va_end(varargs);
    if (rc < 0)
        result = pm_strsol;
#else
    size_t dryRunLen;

    va_start(varargs, fmt);

    pm_vsnprintf(NULL, 0, fmt, varargs, &dryRunLen);

    va_end(varargs);

    if (dryRunLen + 1 < dryRunLen)
        /* arithmetic overflow */
        result = pm_strsol;
    else {
        size_t const allocSize = dryRunLen + 1;
        char * buffer;
        buffer = malloc(allocSize);
        if (buffer != NULL) {
            va_list varargs;
            size_t realLen;

            va_start(varargs, fmt);

            pm_vsnprintf(buffer, allocSize, fmt, varargs, &realLen);

            assert(realLen == dryRunLen);
            va_end(varargs);

            result = buffer;
        }
    }
#endif

    if (result == NULL)
        *resultP = pm_strsol;
    else
        *resultP = result;
}



void
pm_strfree(const char * const string) {

    if (string != pm_strsol)
        free((void *) string);
}



const char *
pm_strsep(char ** const stringP, const char * const delim) {
    const char * retval;

    if (stringP == NULL || *stringP == NULL)
        retval = NULL;
    else {
        char * p;

        retval = *stringP;

        for (p = *stringP; *p && strchr(delim, *p) == NULL; ++p);

        if (*p) {
            /* We hit a delimiter, not end-of-string.  So null out the
               delimiter and advance user's pointer to the next token
            */
            *p++ = '\0';
            *stringP = p;
        } else {
            /* We ran out of string.  So the end-of-string delimiter is
               already there, and we set the user's pointer to NULL to
               indicate there are no more tokens.
            */
            *stringP = NULL;
        }
    }
    return retval;
}



int
pm_stripeq(const char * const comparand,
           const char * const comparator) {
/*----------------------------------------------------------------------------
  Compare two strings, ignoring leading and trailing white space.

  Return 1 (true) if the strings are identical; 0 (false) otherwise.
-----------------------------------------------------------------------------*/
    const char * p;
    const char * q;
    const char * px;
    const char * qx;
    bool equal;

    /* Make p and q point to the first non-blank character in each string.
       If there are no non-blank characters, make them point to the terminating
       NUL.
    */

    p = &comparand[0];
    while (ISSPACE(*p))
        p++;
    q = &comparator[0];
    while (ISSPACE(*q))
        q++;

    /* Make px and qx point to the last non-blank character in each string.
       If there are no nonblank characters (which implies the string is
       null), make them point to the terminating NUL.
    */

    if (*p == '\0')
        px = p;
    else {
        px = p + strlen(p) - 1;
        while (ISSPACE(*px))
            --px;
    }

    if (*q == '\0')
        qx = q;
    else {
        qx = q + strlen(q) - 1;
        while (ISSPACE(*qx))
            --qx;
    }

    if (px - p != qx - q) {
        /* The stripped strings aren't the same length, so we know they aren't
           equal.
        */
        equal = false;
    } else {
        /* Move p and q through the nonblank characters, comparing. */
        for (equal = true; p <= px; ++p, ++q) {
            assert(q <= qx);  /* Because stripped strings are same length */
            if (*p != *q)
                equal = false;
        }
    }
    return equal ? 1 : 0;
}



const void *
pm_memmem(const void * const haystackArg,
          size_t       const haystacklen,
          const void * const needleArg,
          size_t       const needlelen) {

    const unsigned char * const haystack = haystackArg;
    const unsigned char * const needle   = needleArg;

    /* This does the same as the function of the same name in the GNU
       C library
    */
    const unsigned char * p;

    for (p = haystack; p <= haystack + haystacklen - needlelen; ++p)
        if (memeq(p, needle, needlelen))
            return p;

    return NULL;
}



bool
pm_strishex(const char * const subject) {

    bool retval;
    unsigned int i;

    retval = TRUE;  /* initial assumption */

    for (i = 0; i < strlen(subject); ++i)
        if (!ISXDIGIT(subject[i]))
            retval = FALSE;

    return retval;
}



void
pm_string_to_long(const char *   const string,
                  long *         const longP,
                  const char **  const errorP) {

    if (strlen(string) == 0)
        pm_asprintf(errorP, "Value is a null string");
    else {
        char * tailptr;

        /* strtol() does a bizarre thing where if the number is out
           of range, it returns a clamped value but tells you about it
           by setting errno = ERANGE.  If it is not out of range,
           strtol() leaves errno alone.
        */
        errno = 0;  /* So we can tell if strtol() overflowed */

        *longP = strtol(string, &tailptr, 10);

        if (*tailptr != '\0')
            pm_asprintf(errorP, "Non-numeric crap in string: '%s'", tailptr);
        else {
             if (errno == ERANGE)
                 pm_asprintf(errorP, "Number is too large for computation");
             else
                 *errorP = NULL;
        }
    }
}



void
pm_string_to_int(const char *   const string,
                 int *          const intP,
                 const char **  const errorP) {

    long longValue;

    pm_string_to_long(string, &longValue, errorP);

    if (!*errorP) {
        if ((int)longValue != longValue)
            pm_asprintf(errorP,
                        "Number is too large for computation");
        else {
            *intP = (int)longValue;
            *errorP = NULL;
        }
    }
}



void
pm_string_to_uint(const char *   const string,
                  unsigned int * const uintP,
                  const char **  const errorP) {

    /* We can't use 'strtoul'.  Contrary to expectations, though as
       designed, it returns junk if there is a minus sign.
    */

    long longValue;

    pm_string_to_long(string, &longValue, errorP);

    if (!*errorP) {
        if (longValue < 0)
            pm_asprintf(errorP, "Number is negative");
        else {
            if ((unsigned int)longValue != longValue)
                pm_asprintf(errorP,
                            "Number is too large for computation");
            else {
                *uintP = (unsigned int)longValue;
                *errorP = NULL;
            }
        }
    }
}



