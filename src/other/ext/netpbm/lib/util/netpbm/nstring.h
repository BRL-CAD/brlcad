#ifndef _NSTRING_H
#define _NSTRING_H

#include <stdarg.h>
#include <string.h>
#ifdef _MSC_VER
#  define strncasecmp _strnicmp
#  define strcasecmp _stricmp
#else
#  include <strings.h>  /* For strncasecmp */
#endif
#include <ctype.h>

#include "pm_c_util.h"
#include "pm.h"  /* For PM_GNU_PRINTF_ATTR, __inline__ */

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif

/* Here are string functions that respect the size of the array
   into which you are copying -- E.g. STRSCPY truncates the source string as
   required so that it fits, with the terminating null, in the destination
   array.
*/
#define STRSCPY(A,B) \
	(strncpy((A), (B), sizeof(A)), *((A)+sizeof(A)-1) = '\0')
#define STRSCMP(A,B) \
	(strncmp((A), (B), sizeof(A)))
#define STRSCAT(A,B) \
    (strncpy(A+strlen(A), B, sizeof(A)-strlen(A)), *((A)+sizeof(A)-1) = '\0')
#define STRSEQ(A, B) \
	(strneq((A), (B), sizeof(A)))

#define MEMSEQ(a,b) (memeq(a, b, sizeof(*(a))))

#define MEMSSET(a,b) (memset(a, b, sizeof(*(a))))

#define MEMSCPY(a,b) (memcpy(a, b, sizeof(*(a))))

#define MEMSZERO(a) (MEMSSET(a, 0))


static __inline__ int
streq(const char * const comparand,
      const char * const comparator) {

    return strcmp(comparand, comparator) == 0;
}

static __inline__ int
strneq(const char * const comparand,
       const char * const comparator,
       size_t       const size) {

    return strncmp(comparand, comparator, size) == 0;
}

static __inline__ int
memeq(const void * const comparand,
      const void * const comparator,
      size_t       const size) {

    return memcmp(comparand, comparator, size) == 0;
}

/* The Standard C Library may not declare strcasecmp() if the including source
   file doesn't request BSD functions, with _BSD_SOURCE or SUSv2 function,
   with _XOPEN_SOURCE >= 500.  So we don't define functions that use
   strcasecmp() in that case.

   (Actually, _XOPEN_SOURCE 500 is stronger than you need for strcasecmp -
   _XOPEN_SOURCE_EXTENDED, which asks for XPG 4, would do, whereas
   _XOPEN_SOURCE 500 asks for XPG 5, but for simplicity, we don't use
   _XOPEN_SOURCE_EXTENDED in Netpbm.
*/
#if defined(_BSD_SOURCE) || (defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE - 0) >= 500)
static __inline__ int
strcaseeq(const char * const comparand,
          const char * const comparator) {

    return strcasecmp(comparand, comparator) == 0;
}

static __inline__ int
strncaseeq(const char * const comparand,
           const char * const comparator,
           size_t       const size) {

    return strncasecmp(comparand, comparator, size) == 0;
}
#endif


/* The standard C library routines isdigit(), for some weird
   historical reason, does not take a character (type 'char') as its
   argument.  Instead it takes an integer.  When the integer is a whole
   number, it represents a character in the obvious way using the local
   character set encoding.  When the integer is negative, the results
   are undefined.

   Passing a character to isdigit(), which expects an integer, results in
   isdigit() sometimes getting a negative number.

   On some systems, when the integer is negative, it represents exactly
   the character you want it to anyway (e.g. -1 is the character that is
   encoded 0xFF).  But on others, it does not.

   (The same is true of other routines like isdigit()).

   Therefore, we have the substitutes for isdigit() etc. that take an
   actual character (type 'char') as an argument.
*/

#define ISALNUM(C) (isalnum((unsigned char)(C)))
#define ISALPHA(C) (isalpha((unsigned char)(C)))
#define ISCNTRL(C) (iscntrl((unsigned char)(C)))
#define ISDIGIT(C) (isdigit((unsigned char)(C)))
#define ISGRAPH(C) (isgraph((unsigned char)(C)))
#define ISLOWER(C) (islower((unsigned char)(C)))
#define ISPRINT(C) (isprint((unsigned char)(C)))
#define ISPUNCT(C) (ispunct((unsigned char)(C)))
#define ISSPACE(C) (isspace((unsigned char)(C)))
#define ISUPPER(C) (isupper((unsigned char)(C)))
#define ISXDIGIT(C) (isxdigit((unsigned char)(C)))
#define TOUPPER(C) ((char)toupper((unsigned char)(C)))


/* Most of these are private versions of commonly available standard C library
   subroutines whose names are similar.  They're here because not all standard
   C libraries have them.

   The GNU C library has all of them.  All but the oldest standard C libraries
   have snprintf().

   There are slight differences between the asprintf() family and that
   found in other libraries:

     - There is no return value.

     - The returned string is a const char * instead of a char *.  The
       const is more correct.

     - If the function can't get the memory, it returns 'pm_strsol',
       which is a string that is in static memory that contains text
       indicating an out of memory failure has occurred, instead of
       NULL.  This makes it much easier for programs to ignore this
       possibility.

   strfree() is strictly a Netpbm invention, to allow proper type checking
   when freeing storage allocated by the Netpbm pm_asprintf().
*/

extern const char * const pm_strsol;

size_t
pm_strnlen(const char * const s,
           size_t       const maxlen);

int
pm_snprintf(char *       const dest,
            size_t       const maxSize,
            const char * const fmt,
            ...) PM_GNU_PRINTF_ATTR(3,4);

void
pm_vsnprintf(char *       const str,
             size_t       const maxSize,
             const char * const fmt,
             va_list            ap,
             size_t *     const sizeP);

const char *
pm_strdup(const char * const arg);

void
pm_asprintf(const char ** const resultP,
            const char *  const fmt,
            ...) PM_GNU_PRINTF_ATTR(2,3);

void
pm_vasprintf(const char ** const resultP,
             const char *  const format,
             va_list             args);

bool
pm_vasprintf_knows_float(void);

void
pm_strfree(const char * const string);

const char *
pm_strsep(char ** const stringP, const char * const delim);

int
pm_stripeq(const char * const comparand,
           const char * const comparator);

const void *
pm_memmem(const void * const haystackArg,
          size_t       const haystacklen,
          const void * const needleArg,
          size_t       const needlelen);

bool
pm_strishex(const char * const subject);

void
pm_string_to_long(const char *   const string,
                  long *         const longP,
                  const char **  const errorP);

void
pm_string_to_int(const char *   const string,
                 int *          const intP,
                 const char **  const errorP);

void
pm_string_to_uint(const char *   const string,
                  unsigned int * const uintP,
                  const char **  const errorP);

#ifdef __cplusplus
}
#endif

#endif
