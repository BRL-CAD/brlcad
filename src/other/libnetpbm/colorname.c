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

#include "pm_c_util.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "colorname.h"

static int lineNo;

void
pm_canonstr(char * const str) {

    char * p;
    for (p = str; *p; ) {
        if (isspace(*p)) {
            strcpy(p, &(p[1]));
        } else {
            *p = tolower(*p);
            ++p;
        }
    }
}



FILE *
pm_openColornameFile(const char * const fileName, const int must_open) {
/*----------------------------------------------------------------------------
   Open the colorname dictionary file.  Its file name is 'fileName', unless
   'fileName' is NULL.  In that case, its file name is the value of the
   environment variable whose name is RGB_ENV (e.g. "RGBDEF").  Except
   if that environment variable is not set, it is RGB_DB1, RGB_DB2,
   or RGB_DB3 (e.g. "/usr/lib/X11/rgb.txt"), whichever exists.
   
   'must_open' is a logical: we must get the file open or die.  If
   'must_open' is true and we can't open the file (e.g. it doesn't
   exist), exit the program with an error message.  If 'must_open' is
   false and we can't open the file, just return a null pointer.
-----------------------------------------------------------------------------*/
    const char *rgbdef;
    FILE *f;

    if (fileName == NULL) {
        if ((rgbdef = getenv(RGBENV))==NULL) {
            /* The environment variable isn't set, so try the hardcoded
               default color name dictionary locations.
            */
            if ((f = fopen(RGB_DB1, "r")) == NULL &&
                (f = fopen(RGB_DB2, "r")) == NULL &&
                (f = fopen(RGB_DB3, "r")) == NULL && must_open) {
                pm_error("can't open color names dictionary file named "
                         "%s, %s, or %s "
                         "and Environment variable %s not set.  Set %s to "
                         "the pathname of your rgb.txt file or don't use "
                         "color names.", 
                         RGB_DB1, RGB_DB2, RGB_DB3, RGBENV, RGBENV);
            }
        } else {            
            /* The environment variable is set */
            if ((f = fopen(rgbdef, "r")) == NULL && must_open)
                pm_error("Can't open the color names dictionary file "
                         "named %s, per the %s environment variable.  "
                         "errno = %d (%s)",
                         rgbdef, RGBENV, errno, strerror(errno));
        }
    } else {
        f = fopen(fileName, "r");
        if (f == NULL && must_open)
            pm_error("Can't open the color names dictionary file '%s'.  "
                     "errno = %d (%s)", fileName, errno, strerror(errno));
        
    }
    lineNo = 0;
    return(f);
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
    canoncolor = strdup(colorname);
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



