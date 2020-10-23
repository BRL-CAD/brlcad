#ifndef _NETPBM_FILEIO_H_
#define _NETPBM_FILEIO_H_

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
