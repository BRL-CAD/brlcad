#ifndef FILEIO_H_INCLUDED
#define FILEIO_H_INCLUDED

#include <stdio.h>

char
pm_getc(FILE * const file);

unsigned char
pm_getrawbyte(FILE * const file);

unsigned int
pm_getuint(FILE * const file);

unsigned int
pm_getraw(FILE *       const file, 
          unsigned int const bytes);

void
pm_putraw(FILE *       const file, 
          unsigned int const value, 
          unsigned int const bytes);

#endif
