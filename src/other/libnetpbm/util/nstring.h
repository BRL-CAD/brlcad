#ifndef _NSTRING_H
#define _NSTRING_H

#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "pm.h"  /* For PM_GNU_PRINTF_ATTR, __inline__ */

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif

/* Here is are string functions that respect the size of the array
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

#define STREQ(A, B) \
    (strcmp((A), (B)) == 0)
#define STRNEQ(A, B, C) \
    (strncmp((A), (B), (C)) == 0)
#define STRCASEEQ(A, B) \
    (strcasecmp((A), (B)) == 0)
#define STRNCASEEQ(A, B, C) \
    (strncasecmp((A), (B), (C)) == 0)
#define STRSEQ(A, B) \
	(strncmp((A), (B), sizeof(A)) == 0)

#define MEMEQ(A, B, C) \
    (memcmp((A), (B), (C)) == 0)
#define MEMSZERO(A) \
    bzero((A), sizeof(A))


static __inline__ int
streq(const char * const comparand,
      const char * const comparator) {

    return strcmp(comparand, comparator) == 0;
}



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


/* These are all private versions of commonly available standard C
   library subroutines whose names are the same except with the N at
   the end.  Because not all standard C libraries have them all,
   Netpbm must include them in its own libraries, and because some
   standard C libraries have some of them, Netpbm must use different
   names for them.
   
   The GNU C library has all of them.  All but the oldest standard C libraries
   have snprintf().

   There are slight differences between the asprintf() family and that
   found in other libraries:

     - There is no return value.

     - The returned string is a const char * instead of a char *.  The
       const is more correct.

     - If the function can't get the memory, it returns 'strsol',
       which is a string that is in static memory that contains text
       indicating an out of memory failure has occurred, intead of
       NULL.  This makes it much easier for programs to ignore this
       possibility.

   strfree() is strictly a Netpbm invention, to allow proper type checking
   when freeing storage allocated by the Netpbm asprintfN().
*/

extern const char * const strsol;

int
snprintfN(char *       const dest,
          size_t       const str_m,
          const char * const fmt,
          ...) PM_GNU_PRINTF_ATTR(3,4);

void
vsnprintfN(char *       const str,
           size_t       const str_m,
           const char * const fmt,
           va_list            ap,
           size_t *     const sizeP);

void
asprintfN(const char ** const resultP,
          const char *  const fmt,
          ...) PM_GNU_PRINTF_ATTR(2,3);

void 
strfree(const char * const string);

const char *
strsepN(char ** const stringP, const char * const delim);

int
stripeq(const char * const comparand,
        const char * const comparator);

const char *
memmemN(const char * const haystack,
        size_t       const haystacklen,
        const char * const needle,
        size_t       const needlelen);

#ifdef __cplusplus
}
#endif

#endif
