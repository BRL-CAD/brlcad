/*
 * snprintf.c - a portable implementation of snprintf
 *

   THIS MODULE WAS ADAPTED FOR NETPBM BY BRYAN HENDERSON ON 2002.03.24.
   Bryan got the base from 
   http://www.ijs.si/software/snprintf/snprintf-2.2.tar.gz, but made
   a lot of changes and additions.

 * AUTHOR
 *   Mark Martinec <mark.martinec@ijs.si>, April 1999.
 *
 *   Copyright 1999, Mark Martinec. All rights reserved.
 *
 * TERMS AND CONDITIONS
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the "Frontier Artistic License" which comes
 *   with this Kit.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *   See the Frontier Artistic License for more details.
 *
 *   You should have received a copy of the Frontier Artistic License
 *   with this Kit in the file named LICENSE.txt .
 *   If not, I'll be glad to provide one.
 *
 * FEATURES
 * - careful adherence to specs regarding flags, field width and precision;
 * - good performance for large string handling (large format, large
 *   argument or large paddings). Performance is similar to system's sprintf
 *   and in several cases significantly better (make sure you compile with
 *   optimizations turned on, tell the compiler the code is strict ANSI
 *   if necessary to give it more freedom for optimizations);
 * - return value semantics per ISO/IEC 9899:1999 ("ISO C99");
 * - written in standard ISO/ANSI C - requires an ANSI C compiler.
 *
 * IMPLEMENTED CONVERSION SPECIFIERS AND DATA TYPES
 *
 * This snprintf implements only the following conversion specifiers:
 * s, c, d, u, o, x, X, p  (and synonyms: i, D, U, O - see below)
 * with flags: '-', '+', ' ', '0' and '#'.
 * An asterisk is acceptable for field width as well as precision.
 *
 * Length modifiers 'h' (short int), 'l' (long int),
 * and 'll' (long long int) are implemented.
 *
 * Conversion of numeric data (conversion specifiers d, u, o, x, X, p)
 * with length modifiers (none or h, l, ll) is left to the system routine
 * sprintf, but all handling of flags, field width and precision as well as
 * c and s conversions is done very carefully by this portable routine.
 * If a string precision (truncation) is specified (e.g. %.8s) it is
 * guaranteed the string beyond the specified precision will not be referenced.
 *
 * Length modifiers h, l and ll are ignored for c and s conversions (you
 * can't use data types wint_t and wchar_t).
 *
 * The following common synonyms for conversion characters are acceptable:
 *   - i is a synonym for d
 *   - D is a synonym for ld, explicit length modifiers are ignored
 *   - U is a synonym for lu, explicit length modifiers are ignored
 *   - O is a synonym for lo, explicit length modifiers are ignored
 * The D, O and U conversion characters are nonstandard, they are accepted
 * for backward compatibility only, and should not be used for new code.
 *
 * The following is specifically NOT implemented:
 *   - flag ' (thousands' grouping character) is recognized but ignored
 *   - numeric conversion specifiers: f, e, E, g, G and synonym F,
 *     as well as the new a and A conversion specifiers
 *   - length modifier 'L' (long double) and 'q' (quad - use 'll' instead)
 *   - wide character/string conversions: lc, ls, and nonstandard
 *     synonyms C and S
 *   - writeback of converted string length: conversion character n
 *   - the n$ specification for direct reference to n-th argument
 *   - locales
 *
 * It is permitted for str_m to be zero, and it is permitted to specify NULL
 * pointer for resulting string argument if str_m is zero (as per ISO C99).
 *
 * The return value is the number of characters which would be generated
 * for the given input, excluding the trailing null. If this value
 * is greater or equal to str_m, not all characters from the result
 * have been stored in str, output bytes beyond the (str_m-1) -th character
 * are discarded. If str_m is greater than zero it is guaranteed
 * the resulting string will be null-terminated.
 *
 * NOTE that this matches the ISO C99, OpenBSD, and GNU C library 2.1,
 * but is different from some older and vendor implementations,
 * and is also different from XPG, XSH5, SUSv2 specifications.
 * For historical discussion on changes in the semantics and standards
 * of snprintf see printf(3) man page in the Linux programmers manual.
 *
 * Routines asprintf and vasprintf return a pointer (in the ptr argument)
 * to a buffer sufficiently large to hold the resulting string. This pointer
 * should be passed to free(3) to release the allocated storage when it is
 * no longer needed. If sufficient space cannot be allocated, these functions
 * will return -1 and set ptr to be a NULL pointer. These two routines are a
 * GNU C library extensions (glibc).
 *
 * Routines asnprintf and vasnprintf are similar to asprintf and vasprintf,
 * yet, like snprintf and vsnprintf counterparts, will write at most str_m-1
 * characters into the allocated output string, the last character in the
 * allocated buffer then gets the terminating null. If the formatted string
 * length (the return value) is greater than or equal to the str_m argument,
 * the resulting string was truncated and some of the formatted characters
 * were discarded. These routines present a handy way to limit the amount
 * of allocated memory to some sane value.
 *
 * AVAILABILITY
 *   http://www.ijs.si/software/snprintf/
 *
 */


#define PORTABLE_SNPRINTF_VERSION_MAJOR 2
#define PORTABLE_SNPRINTF_VERSION_MINOR 2

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

#ifdef isdigit
#undef isdigit
#endif
#define isdigit(c) ((c) >= '0' && (c) <= '9')

/* For copying strings longer or equal to 'breakeven_point'
 * it is more efficient to call memcpy() than to do it inline.
 * The value depends mostly on the processor architecture,
 * but also on the compiler and its optimization capabilities.
 * The value is not critical, some small value greater than zero
 * will be just fine if you don't care to squeeze every drop
 * of performance out of the code.
 *
 * Small values favor memcpy, large values favor inline code.
 */
#if defined(__alpha__) || defined(__alpha)
#  define breakeven_point   2	/* AXP (DEC Alpha)     - gcc or cc or egcs */
#endif
#if defined(__i386__)  || defined(__i386)
#  define breakeven_point  12	/* Intel Pentium/Linux - gcc 2.96 */
#endif
#if defined(__hppa)
#  define breakeven_point  10	/* HP-PA               - gcc */
#endif
#if defined(__sparc__) || defined(__sparc)
#  define breakeven_point  33	/* Sun Sparc 5         - gcc 2.8.1 */
#endif

/* some other values of possible interest: */
/* #define breakeven_point  8 */  /* VAX 4000          - vaxc */
/* #define breakeven_point 19 */  /* VAX 4000          - gcc 2.7.0 */

#ifndef breakeven_point
#  define breakeven_point   6	/* some reasonable one-size-fits-all value */
#endif

#define fast_memcpy(d,s,n) \
  { register size_t nn = (size_t)(n); \
    if (nn >= breakeven_point) memcpy((d), (s), nn); \
    else if (nn > 0) { /* proc call overhead is worth only for large strings*/\
      register char *dd; register const char *ss; \
      for (ss=(s), dd=(d); nn>0; nn--) *dd++ = *ss++; } }

#define fast_memset(d,c,n) \
  { register size_t nn = (size_t)(n); \
    if (nn >= breakeven_point) memset((d), (int)(c), nn); \
    else if (nn > 0) { /* proc call overhead is worth only for large strings*/\
      register char *dd; register const int cc=(int)(c); \
      for (dd=(d); nn>0; nn--) *dd++ = cc; } }

/* declarations */

static char credits[] = "\n\
@(#)snprintf.c, v2.2: Mark Martinec, <mark.martinec@ijs.si>\n\
@(#)snprintf.c, v2.2: Copyright 1999, Mark Martinec. Frontier Artistic License applies.\n\
@(#)snprintf.c, v2.2: http://www.ijs.si/software/snprintf/\n";



void
vsnprintfN(char *       const str,
           size_t       const str_m,
           const char * const fmt,
           va_list            ap,
           size_t *     const sizeP) {

    size_t str_l = 0;
    const char *p = fmt;

    /* In contrast with POSIX, the ISO C99 now says that str can be
       NULL and str_m can be 0.  This is more useful than the old:
       if (str_m < 1) return -1;
    */

    if (!p) p = "";
    while (*p) {
        if (*p != '%') {
            /* if (str_l < str_m) str[str_l++] = *p++; -- this would
               be sufficient but the following code achieves better
               performance for cases * where format string is long and
               contains few conversions
            */
            const char *q = strchr(p + 1,'%');
            size_t n = !q ? strlen(p) : (q - p);
            if (str_l < str_m) {
                size_t avail = str_m - str_l;
                fast_memcpy(str + str_l, p, (n > avail ? avail : n));
            }
            p += n; str_l += n;
        } else {
            const char *starting_p;
            size_t min_field_width = 0, precision = 0;
            int zero_padding = 0, precision_specified = 0, justify_left = 0;
            int alternate_form = 0, force_sign = 0;
            int space_for_positive = 1;
                /* If both the ' ' and '+' flags appear,
                   the ' ' flag should be ignored.
                */
            char length_modifier = '\0';  /* allowed values: \0, h, l, L */
            char tmp[32];
                /* temporary buffer for simple numeric->string conversion */

            const char *str_arg;
                /* string address in case of string argument */
            size_t str_arg_l;
                /* natural field width of arg without padding and sign */
            unsigned char uchar_arg;
                /* unsigned char argument value - only defined for c
                   conversion.  N.B. standard explicitly states the char
                   argument for the c conversion is unsigned.
                */

            size_t number_of_zeros_to_pad = 0;
                /* number of zeros to be inserted for numeric
                   conversions as required by the precision or minimal
                   field width
                */

            size_t zero_padding_insertion_ind = 0;
                /* index into tmp where zero padding is to be inserted */

            char fmt_spec = '\0';
                /* current conversion specifier character */

            str_arg = credits;
                /* just to make compiler happy (defined but not used) */
            str_arg = NULL;
            starting_p = p;
            ++p;  /* skip '%' */

            /* parse flags */
            while (*p == '0' || *p == '-' || *p == '+' ||
                   *p == ' ' || *p == '#' || *p == '\'') {
                switch (*p) {
                case '0': zero_padding = 1; break;
                case '-': justify_left = 1; break;
                case '+': force_sign = 1; space_for_positive = 0; break;
                case ' ': force_sign = 1; break;
                    /* If both the ' ' and '+' flags appear, the ' '
                       flag should be ignored
                    */
                case '#': alternate_form = 1; break;
                case '\'': break;
                }
                ++p;
            }
            /* If the '0' and '-' flags both appear, the '0' flag
               should be ignored.
            */

            /* parse field width */
            if (*p == '*') {
                int j;
                p++; j = va_arg(ap, int);
                if (j >= 0) min_field_width = j;
                else { min_field_width = -j; justify_left = 1; }
            } else if (isdigit((int)(*p))) {
                /* size_t could be wider than unsigned int; make sure
                   we treat argument like common implementations do
                */
                unsigned int uj = *p++ - '0';
                while (isdigit((int)(*p)))
                    uj = 10*uj + (unsigned int)(*p++ - '0');
                min_field_width = uj;
            }
            /* parse precision */
            if (*p == '.') {
                p++; precision_specified = 1;
                if (*p == '*') {
                    int j = va_arg(ap, int);
                    p++;
                    if (j >= 0) precision = j;
                    else {
                        precision_specified = 0; precision = 0;
                        /* NOTE: Solaris 2.6 man page claims that in
                           this case the precision should be set to 0.
                           Digital Unix 4.0, HPUX 10 and BSD man page
                           claim that this case should be treated as
                           unspecified precision, which is what we do
                           here.
                        */
                    }
                } else if (isdigit((int)(*p))) {
                    /* size_t could be wider than unsigned int; make
                       sure we treat argument like common
                       implementations do
                    */
                    unsigned int uj = *p++ - '0';
                    while (isdigit((int)(*p)))
                        uj = 10*uj + (unsigned int)(*p++ - '0');
                    precision = uj;
                }
            }
            /* parse 'h', 'l' and 'll' length modifiers */
            if (*p == 'h' || *p == 'l') {
                length_modifier = *p; p++;
                if (length_modifier == 'l' && *p == 'l') {
                    /* double l = long long */
                    length_modifier = 'l';  /* treat it as a single 'l' */
                    p++;
                }
            }
            fmt_spec = *p;

            /* common synonyms: */
            switch (fmt_spec) {
            case 'i': fmt_spec = 'd'; break;
            case 'D': fmt_spec = 'd'; length_modifier = 'l'; break;
            case 'U': fmt_spec = 'u'; length_modifier = 'l'; break;
            case 'O': fmt_spec = 'o'; length_modifier = 'l'; break;
            default: break;
            }
            /* get parameter value, do initial processing */
            switch (fmt_spec) {
            case '%':
                /* % behaves similar to 's' regarding flags and field widths */
            case 'c':
                /* c behaves similar to 's' regarding flags and field widths */
            case 's':
                /* wint_t and wchar_t not handled */
                length_modifier = '\0';
                /* the result of zero padding flag with non-numeric
                    conversion specifier is undefined. Solaris and
                    HPUX 10 does zero padding in this case, Digital
                    Unix and Linux does not.
                */

                zero_padding = 0;
                    /* turn zero padding off for string conversions */
                str_arg_l = 1;
                switch (fmt_spec) {
                case '%':
                    str_arg = p; break;
                case 'c': {
                    int j = va_arg(ap, int);
                    uchar_arg = (unsigned char) j;
                        /* standard demands unsigned char */
                    str_arg = (const char *) &uchar_arg;
                    break;
                }
                case 's':
                    str_arg = va_arg(ap, const char *);
                    if (!str_arg)
                        /* make sure not to address string beyond the
                           specified precision !!!
                        */
                        str_arg_l = 0;
                    else if (!precision_specified)
                        /* truncate string if necessary as requested by
                           precision 
                        */
                        str_arg_l = strlen(str_arg);
                    else if (precision == 0)
                        str_arg_l = 0;
                    else {
                        /* memchr on HP does not like n > 2^31  !!! */
                        const char * q =
                            memchr(str_arg, '\0',
                                   precision <= 0x7fffffff ?
                                   precision : 0x7fffffff);
                        str_arg_l = !q ? precision : (q-str_arg);
                    }
                    break;
                default: break;
                }
                break;
            case 'd': case 'u': case 'o': case 'x': case 'X': case 'p': {
                /* NOTE: the u, o, x, X and p conversion specifiers imply
                   the value is unsigned;  d implies a signed value
                */
                int arg_sign = 0;
                /* 0  if numeric argument is zero (or if pointer is NULL
                      for 'p'),
                   +1 if greater than zero (or nonzero for unsigned arguments),
                   -1 if negative (unsigned argument is never negative)
                */

                int int_arg = 0;
                unsigned int uint_arg = 0;
                   /* defined only for length modifier h, or for no
                      length modifiers
                   */

                long int long_arg = 0;  unsigned long int ulong_arg = 0;
                /* only defined for length modifier l */

                void *ptr_arg = NULL;
                /* pointer argument value -only defined for p conversion */

                if (fmt_spec == 'p') {
                    /* HPUX 10: An l, h, ll or L before any other
                        conversion character (other than d, i, u, o,
                        x, or X) is ignored.

                      Digital Unix: not specified, but seems to behave
                      as HPUX does.

                      Solaris: If an h, l, or L appears before any
                      other conversion specifier (other than d, i, u,
                      o, x, or X), the behavior is
                      undefined. (Actually %hp converts only 16-bits
                      of address and %llp treats address as 64-bit
                      data which is incompatible with (void *)
                      argument on a 32-bit system). 
                    */

                    length_modifier = '\0';
                    ptr_arg = va_arg(ap, void *);
                    if (ptr_arg != NULL) arg_sign = 1;
                } else if (fmt_spec == 'd') {  /* signed */
                    switch (length_modifier) {
                    case '\0':
                    case 'h':
                        /* It is non-portable to specify a second
                           argument of char or short to va_arg,
                           because arguments seen by the called
                           function are not char or short.  C converts
                           char and short arguments to int before
                           passing them to a function.
                        */
                        int_arg = va_arg(ap, int);
                        if      (int_arg > 0) arg_sign =  1;
                        else if (int_arg < 0) arg_sign = -1;
                        break;
                    case 'l':
                        long_arg = va_arg(ap, long int);
                        if      (long_arg > 0) arg_sign =  1;
                        else if (long_arg < 0) arg_sign = -1;
                        break;
                    }
                } else {  /* unsigned */
                    switch (length_modifier) {
                    case '\0':
                    case 'h':
                        uint_arg = va_arg(ap, unsigned int);
                        if (uint_arg)
                            arg_sign = 1;
                        break;
                    case 'l':
                        ulong_arg = va_arg(ap, unsigned long int);
                        if (ulong_arg)
                            arg_sign = 1;
                        break;
                    }
                }
                str_arg = tmp; str_arg_l = 0;
                /* NOTE: For d, i, u, o, x, and X conversions, if
                   precision is specified, the '0' flag should be
                   ignored. This is so with Solaris 2.6, Digital UNIX
                   4.0, HPUX 10, Linux, FreeBSD, NetBSD; but not with
                   Perl.
                */
                if (precision_specified)
                    zero_padding = 0;
                if (fmt_spec == 'd') {
                    if (force_sign && arg_sign >= 0)
                        tmp[str_arg_l++] = space_for_positive ? ' ' : '+';
                    /* leave negative numbers for sprintf to handle,
                       to avoid handling tricky cases like (short
                       int)(-32768)
                    */
                } else if (alternate_form) {
                    if (arg_sign != 0 && (fmt_spec == 'x' ||
                                          fmt_spec == 'X')) {
                        tmp[str_arg_l++] = '0';
                        tmp[str_arg_l++] = fmt_spec;
                    }
                    /* alternate form should have no effect for p
                       conversion, but ...
                    */
                }
                zero_padding_insertion_ind = str_arg_l;
                if (!precision_specified)
                    precision = 1;   /* default precision is 1 */
                if (precision == 0 && arg_sign == 0) {
                    /* converted to null string */
                    /* When zero value is formatted with an explicit
                       precision 0, the resulting formatted string is
                       empty (d, i, u, o, x, X, p).
                    */
                } else {
                    char f[5]; int f_l = 0;
                    f[f_l++] = '%';
                        /* construct a simple format string for sprintf */
                    if (!length_modifier) { }
                    else if (length_modifier=='2') {
                        f[f_l++] = 'l'; f[f_l++] = 'l';
                    }
                    else
                        f[f_l++] = length_modifier;
                    f[f_l++] = fmt_spec; f[f_l++] = '\0';
                    if (fmt_spec == 'p')
                        str_arg_l += sprintf(tmp+str_arg_l, f, ptr_arg);
                    else if (fmt_spec == 'd') {  /* signed */
                        switch (length_modifier) {
                        case '\0':
                        case 'h':
                            str_arg_l+=sprintf(tmp+str_arg_l, f, int_arg);
                            break;
                        case 'l':
                            str_arg_l+=sprintf(tmp+str_arg_l, f, long_arg);
                            break;
                        }
                    } else {  /* unsigned */
                        switch (length_modifier) {
                        case '\0':
                        case 'h':
                            str_arg_l += sprintf(tmp+str_arg_l, f, uint_arg);
                            break;
                        case 'l':
                            str_arg_l += sprintf(tmp+str_arg_l, f, ulong_arg);
                            break;
                        }
                    }
                    /* include the optional minus sign and possible "0x"
                       in the region before the zero padding insertion point
                    */
                    if (zero_padding_insertion_ind < str_arg_l &&
                        tmp[zero_padding_insertion_ind] == '-') {
                        zero_padding_insertion_ind++;
                    }
                    if (zero_padding_insertion_ind+1 < str_arg_l &&
                        tmp[zero_padding_insertion_ind]   == '0' &&
                        (tmp[zero_padding_insertion_ind+1] == 'x' ||
                         tmp[zero_padding_insertion_ind+1] == 'X') ) {
                        zero_padding_insertion_ind += 2;
                    }
                }
                {
                    size_t num_of_digits =
                        str_arg_l - zero_padding_insertion_ind;
                    if (alternate_form && fmt_spec == 'o'
                        /* unless zero is already the first character */
                        && !(zero_padding_insertion_ind < str_arg_l
                             && tmp[zero_padding_insertion_ind] == '0')) {
                        /* assure leading zero for alternate-form
                           octal numbers 
                        */
                        if (!precision_specified ||
                            precision < num_of_digits+1) {
                            /* precision is increased to force the
                               first character to be zero, except if a
                               zero value is formatted with an
                               explicit precision of zero
                            */
                            precision = num_of_digits+1;
                            precision_specified = 1;
                        }
                    }
                    /* zero padding to specified precision? */
                    if (num_of_digits < precision) 
                        number_of_zeros_to_pad = precision - num_of_digits;
                }
                /* zero padding to specified minimal field width? */
                if (!justify_left && zero_padding) {
                    int n =
                        min_field_width - (str_arg_l+number_of_zeros_to_pad);
                    if (n > 0) number_of_zeros_to_pad += n;
                }
            } break;
            default:
                /* unrecognized conversion specifier, keep format
                   string as-is
                */
                zero_padding = 0;
                    /* turn zero padding off for non-numeric convers. */
                /* reset flags */
                justify_left = 1;
                min_field_width = 0;
                /* discard the unrecognized conversion, just keep the
                   unrecognized conversion character
                */
                str_arg = p;
                str_arg_l = 0;
                if (*p)
                    /* include invalid conversion specifier unchanged
                       if not at end-of-string
                    */
                    ++str_arg_l;
                break;
            }
            if (*p)
                p++;  /* step over the just processed conversion specifier */
            /* insert padding to the left as requested by
               min_field_width; this does not include the zero padding
               in case of numerical conversions
            */

            if (!justify_left) {
                /* left padding with blank or zero */
                int n = min_field_width - (str_arg_l+number_of_zeros_to_pad);
                if (n > 0) {
                    if (str_l < str_m) {
                        size_t avail = str_m-str_l;
                        fast_memset(str+str_l, (zero_padding ? '0' : ' '),
                                    (n > avail ? avail : n));
                    }
                    str_l += n;
                }
            }
            /* zero padding as requested by the precision or by the
               minimal field width for numeric conversions required?
            */
            if (number_of_zeros_to_pad <= 0) {
                /* will not copy first part of numeric right now,
                   force it to be copied later in its entirety
                */
                zero_padding_insertion_ind = 0;
            } else {
                /* insert first part of numerics (sign or '0x') before
                   zero padding
                */
                int n = zero_padding_insertion_ind;
                if (n > 0) {
                    if (str_l < str_m) {
                        size_t avail = str_m-str_l;
                        fast_memcpy(str+str_l, str_arg, (n>avail?avail:n));
                    }
                    str_l += n;
                }
                /* insert zero padding as requested by the precision
                   or min field width
                */
                n = number_of_zeros_to_pad;
                if (n > 0) {
                    if (str_l < str_m) {
                        size_t avail = str_m - str_l;
                        fast_memset(str + str_l, '0', (n > avail ? avail : n));
                    }
                    str_l += n;
                }
            }
            /* insert formatted string (or as-is conversion specifier
               for unknown conversions)
            */
            {
                int n = str_arg_l - zero_padding_insertion_ind;
                if (n > 0) {
                    if (str_l < str_m) {
                        size_t avail = str_m-str_l;
                        fast_memcpy(str + str_l,
                                    str_arg + zero_padding_insertion_ind,
                                    (n > avail ? avail : n));
                    }
                    str_l += n;
                }
            }
            /* insert right padding */
            if (justify_left) {
                /* right blank padding to the field width */
                int n = min_field_width - (str_arg_l+number_of_zeros_to_pad);
                if (n > 0) {
                    if (str_l < str_m) {
                        size_t avail = str_m-str_l;
                        fast_memset(str+str_l, ' ', (n>avail?avail:n));
                    }
                    str_l += n;
                }
            }
        }
    }
    if (str_m > 0) {
        /* make sure the string is null-terminated even at the expense
           of overwriting the last character (shouldn't happen, but
           just in case)
        */
        str[str_l <= str_m-1 ? str_l : str_m-1] = '\0';
    }
    *sizeP = str_l;
}



int
snprintfN(char *       const dest,
          size_t       const str_m,
          const char * const fmt,
          ...) {

    size_t size;
    va_list ap;

    va_start(ap, fmt);
    
    vsnprintfN(dest, str_m, fmt, ap, &size);

    va_end(ap);

    assert(size <= INT_MAX);

    return size;
}



/* When a function that is supposed to return a malloc'ed string cannot
   get the memory for it, it should return 'strsol'.  That has a much
   better effect on the caller, if the caller doesn't explicitly allow for
   the out of memory case, than returning NULL.  Note that it is very
   rare for the system not to have enough memory to return a small string,
   so it's OK to have somewhat nonsensical behavior when it happens.  We
   just don't want catastrophic behavior.

   'strsol' is an external symbol, so if Caller wants to detect the
   out-of-memory failure, he certainly can.
*/
const char * const strsol = "NO MEMORY TO CREATE STRING!";



/* We would like to have vasprintfN(), but it is difficult because you
   can't run through a va_list twice, which we would want to do: once
   to measure the length; once actually to build the string.  On some
   machines, you can simply make two copies of the va_list variable in
   normal C fashion, but on others you need va_copy, which is a
   relatively recent invention.  In particular, the simple va_list copy
   failed on an AMD64 Gcc Linux system in March 2006.
*/

void PM_GNU_PRINTF_ATTR(2,3)
asprintfN(const char ** const resultP,
          const char *  const fmt, 
          ...) {

    va_list varargs;
    
    size_t dryRunLen;
    
    va_start(varargs, fmt);
    
    vsnprintfN(NULL, 0, fmt, varargs, &dryRunLen);

    va_end(varargs);

    if (dryRunLen + 1 < dryRunLen)
        /* arithmetic overflow */
        *resultP = strsol;
    else {
        size_t const allocSize = dryRunLen + 1;
        char * result;
        result = malloc(allocSize);
        if (result == NULL)
            *resultP = strsol;
        else {
            va_list varargs;
            size_t realLen;

            va_start(varargs, fmt);

            vsnprintfN(result, allocSize, fmt, varargs, &realLen);
                
            assert(realLen == dryRunLen);
            va_end(varargs);

            *resultP = result;
        }
    }
}



void
strfree(const char * const string) {

    if (string != strsol)
        free((void *) string);
}



const char *
strsepN(char ** const stringP, const char * const delim) {
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
stripeq(const char * const comparand,
        const char * const comparator) {
/*----------------------------------------------------------------------------
  Compare two strings, ignoring leading and trailing white space.

  Return 1 (true) if the strings are identical; 0 (false) otherwise.
-----------------------------------------------------------------------------*/
    char *p, *q, *px, *qx;
    char equal;
  
    /* Make p and q point to the first non-blank character in each string.
     If there are no non-blank characters, make them point to the terminating
     NULL.
     */

    p = (char *) comparand;
    while (ISSPACE(*p)) p++;
    q = (char *) comparator;
    while (ISSPACE(*q)) q++;

    /* Make px and qx point to the last non-blank character in each string.
       If there are no nonblank characters (which implies the string is
       null), make them point to the terminating NULL.
    */

    if (*p == '\0') px = p;
    else {
        px = p + strlen(p) - 1;
        while (ISSPACE(*px)) px--;
    }

    if (*q == '\0') qx = q;
    else {
        qx = q + strlen(q) - 1;
        while (ISSPACE(*qx)) qx--;
    }

    equal = 1;   /* initial assumption */
  
    /* If the stripped strings aren't the same length, 
       we know they aren't equal 
     */
    if (px - p != qx - q) equal = 0;

    else
    while (p <= px) {
        if (*p != *q) equal = 0;
        p++; q++;
    }
    return equal;
}



const char *
memmemN(const char * const haystack,
        size_t       const haystacklen,
        const char * const needle,
        size_t       const needlelen) {

    /* This does the same as the function of the same name in the GNU
       C library
    */
    const char * p;

    for (p = haystack; p <= haystack + haystacklen - needlelen; ++p)
        if (MEMEQ(p, needle, needlelen))
            return p;

    return NULL;
}
