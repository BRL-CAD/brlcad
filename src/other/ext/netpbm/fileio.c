/* fileio.c - routines to read elements from Netpbm image files
**
** Copyright (C) 1988 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include <stdio.h>
#include <limits.h>

#include "pm.h"
#include "fileio.h"

char
pm_getc(FILE * const fileP) {
    int ich;
    char ch;

    ich = getc(fileP);
    if (ich == EOF)
        pm_error("EOF / read error reading a byte");
    ch = (char) ich;
    
    if (ch == '#') {
        do {
            ich = getc(fileP);
            if (ich == EOF)
                pm_error("EOF / read error reading a byte");
            ch = (char) ich;
        } while (ch != '\n' && ch != '\r');
    }
    return ch;
}



/* This is useful for PBM files.  It used to be used for PGM and PPM files
   too, since the sample size was always one byte.  Now, use pbm_getrawsample()
   for PGM and PPM files.
*/

unsigned char
pm_getrawbyte(FILE * const file) {
    int iby;

    iby = getc(file);
    if (iby == EOF)
        pm_error("EOF / read error reading a one-byte sample");
    return (unsigned char) iby;
}



unsigned int
pm_getuint(FILE * const ifP) {
/*----------------------------------------------------------------------------
   Read an unsigned integer in ASCII decimal from the file stream
   represented by 'ifP' and return its value.

   If there is nothing at the current position in the file stream that
   can be interpreted as an unsigned integer, issue an error message
   to stderr and abort the program.

   If the number at the current position in the file stream is too
   great to be represented by an 'int' (Yes, I said 'int', not
   'unsigned int'), issue an error message to stderr and abort the
   program.
-----------------------------------------------------------------------------*/
    char ch;
    unsigned int i;

    do {
        ch = pm_getc(ifP);
    } while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');

    if (ch < '0' || ch > '9')
        pm_error("junk in file where an unsigned integer should be");

    i = 0;
    do {
        unsigned int const digitVal = ch - '0';

        if (i > INT_MAX/10)
            pm_error("ASCII decimal integer in file is "
                     "too large to be processed.  ");
        
        i *= 10;

        if (i > INT_MAX - digitVal)
            pm_error("ASCII decimal integer in file is "
                     "too large to be processed.  ");

        i += digitVal;

        ch = pm_getc(ifP);
    } while (ch >= '0' && ch <= '9');

    return i;
}



unsigned int
pm_getraw(FILE *       const file, 
          unsigned int const bytes) {

    unsigned int value;  /* our return value */

    if (bytes == 1) {
        /* Here's a speedup for the common 1-byte sample case: */
        value = getc(file);
        if (value == EOF)
            pm_error("EOF/error reading 1 byte sample from file.");
    } else {
        /* This code works for bytes == 1..4 */
        /* We could speed this up by exploiting knowledge of the format of
           an unsigned integer (i.e. endianness).  Then we could just cast
           the value as an array of characters instead of shifting and
           masking.
           */
        int shift;
        unsigned char inval[4];
        int cursor;
        int n_read;

        n_read = fread(inval, bytes, 1, file);
        if (n_read < 1) 
            pm_error("EOF/error reading %d byte sample from file.", bytes);
        value = 0;  /* initial value */
        cursor = 0;
        for (shift = (bytes-1)*8; shift >= 0; shift-=8) 
            value += inval[cursor++] << shift;
    }
    return(value);
}



void
pm_putraw(FILE *       const file, 
          unsigned int const value, 
          unsigned int const bytes) {

    if (bytes == 1) {
        /* Here's a speedup for the common 1-byte sample case: */
        int rc;
        rc = fputc(value, file);
        if (rc == EOF)
            pm_error("Error writing 1 byte sample to file.");
    } else {
        /* This code works for bytes == 1..4 */
        /* We could speed this up by exploiting knowledge of the format of
           an unsigned integer (i.e. endianness).  Then we could just cast
           the value as an array of characters instead of shifting and
           masking.
           */
        int shift;
        unsigned char outval[4];
        int cursor;
        int n_written;

        cursor = 0;
        for (shift = (bytes-1)*8; shift >= 0; shift-=8) {
            outval[cursor++] = (value >> shift) & 0xFF;
        }
        n_written = fwrite(&outval, bytes, 1, file);
        if (n_written == 0) 
            pm_error("Error writing %d byte sample to file.", bytes);
    }
}
