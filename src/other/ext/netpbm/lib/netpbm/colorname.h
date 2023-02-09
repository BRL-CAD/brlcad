#ifndef COLORNAME_H
#define COLORNAME_H

#include <string.h>
#include <stdio.h>
#include <netpbm/ppm.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif

enum colornameFormat {PAM_COLORNAME_ENGLISH = 0,
                      PAM_COLORNAME_HEXOK   = 1};

struct colorfile_entry {
    long r, g, b;
    char * colorname;
};



void 
pm_canonstr(char * const str);

FILE *
pm_openColornameFile(const char * const fileName, const int must_open);

struct colorfile_entry
pm_colorget(FILE * const f);

void
pm_parse_dictionary_name(const char       colorname[], 
                         pixval     const maxval,
                         int        const closeOk,
                         pixel *    const colorP);

#ifdef __cplusplus
}
#endif
#endif
