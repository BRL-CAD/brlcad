/* colorname.c - colorname routines, not dependent on Netpbm formats
**
** Taken from libppm4.c May 2002.

** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#define _BSD_SOURCE 1      /* Make sure strdup() is in string.h */
#define _XOPEN_SOURCE 500  /* Make sure strdup() is in string.h */

#include "netpbm/pm_c_util.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "netpbm/nstring.h"

#include "colorname.h"

static int lineNo;



void 
pm_canonstr(char * const arg) {
/*----------------------------------------------------------------------------
   Modify string 'arg' to canonical form: lower case, no white space.
-----------------------------------------------------------------------------*/
    const char * srcCursor;
    char * dstCursor;

    for (srcCursor = arg, dstCursor = arg; *srcCursor; ++srcCursor) {
        if (!ISSPACE(*srcCursor)) {
            *dstCursor++ =
                ISUPPER(*srcCursor) ? tolower(*srcCursor) : *srcCursor;
        }
    }
}



static void
openColornameFileSearch(const char * const searchPath,
                        FILE **      const filePP) {
/*----------------------------------------------------------------------------
   Open the color name file, finding it via the search path 'searchPath'.

   Return as *filePP the stream handle for it, but if we don't find it
   (or just can open it) anywhere, return *filePP == NULL.
-----------------------------------------------------------------------------*/
    char * buffer;

    buffer = strdup(searchPath);

    if (buffer) {
        char * cursor;
        bool eol;

        cursor = &buffer[0];
        eol = FALSE;    /* initial value */
        *filePP = NULL;  /* initial value */
        while (!eol && !*filePP) {
            const char * token;
            token = pm_strsep(&cursor, ":");
            if (token) {
                *filePP = fopen(token, "r");
            } else
                eol = TRUE;
        }
        free(buffer);
    } else
        *filePP = NULL;
}


struct colorfile_entry
pm_colorget(FILE * const f) {
/*----------------------------------------------------------------------------
   Get next color entry from the color name dictionary file 'f'.

   If eof or error, return a color entry with NULL for the color name.

   Otherwise, return color name in static storage within.
-----------------------------------------------------------------------------*/
    char buf[200];
    static char colorname[200];
    bool gotOne;
    bool eof;
    struct colorfile_entry retval;
    char * rc;
    
    gotOne = FALSE;  /* initial value */
    eof = FALSE;
    while (!gotOne && !eof) {
        lineNo++;
        rc = fgets(buf, sizeof(buf), f);
        if (rc == NULL)
            eof = TRUE;
        else {
            if (buf[0] != '#' && buf[0] != '\n' && buf[0] != '!' &&
                buf[0] != '\0') {
                if (sscanf(buf, "%ld %ld %ld %[^\n]", 
                           &retval.r, &retval.g, &retval.b, colorname) 
                    == 4 )
                    gotOne = TRUE;
                else {
                    if (buf[strlen(buf)-1] == '\n')
                        buf[strlen(buf)-1] = '\0';
                    pm_message("can't parse color names dictionary Line %d:  "
                               "'%s'", 
                               lineNo, buf);
                }
            }
        }
    }
    if (gotOne)
        retval.colorname = colorname;
    else
        retval.colorname = NULL;
    return retval;
}



void
pm_parse_dictionary_name(char    const colorname[], 
                         pixval  const maxval,
                         int     const closeOk,
                         pixel * const colorP) {

    FILE* f;
    bool gotit;
    bool colorfileExhausted;
    struct colorfile_entry colorfile_entry;
    char * canoncolor;
    pixval r,g,b;

    f = pm_openColornameFile(NULL, TRUE);  /* exits if error */
    canoncolor = pm_strdup(colorname);

    if (!canoncolor)
        pm_error("Failed to allocate memory for %u-byte color name",
                 (unsigned)strlen(colorname));

    pm_canonstr(canoncolor);
    gotit = FALSE;
    colorfileExhausted = FALSE;
    while (!gotit && !colorfileExhausted) {
        colorfile_entry = pm_colorget(f);
        if (colorfile_entry.colorname) {
            pm_canonstr(colorfile_entry.colorname);
            if (strcmp( canoncolor, colorfile_entry.colorname) == 0)
                gotit = TRUE;
        } else
            colorfileExhausted = TRUE;
    }
    fclose(f);
    
    if (!gotit)
        pm_error("unknown color '%s'", colorname);
    
    /* Rescale from [0..255] if necessary. */
    if (maxval != 255) {
        r = colorfile_entry.r * maxval / 255;
        g = colorfile_entry.g * maxval / 255;
        b = colorfile_entry.b * maxval / 255;

        if (!closeOk) {
            if (r * 255 / maxval != colorfile_entry.r ||
                g * 255 / maxval != colorfile_entry.g ||
                b * 255 / maxval != colorfile_entry.b)
                pm_message("WARNING: color '%s' cannot be represented "
                           "exactly with a maxval of %u.  "
                           "Approximating as (%u,%u,%u).  "
                           "The color dictionary uses maxval 255, so that "
                           "maxval will always work.",
                           colorname, maxval, r, g, b);
        }
    } else {
        r = colorfile_entry.r;
        g = colorfile_entry.g;
        b = colorfile_entry.b;
    }
    free(canoncolor);

    PPM_ASSIGN(*colorP, r, g, b);
}



