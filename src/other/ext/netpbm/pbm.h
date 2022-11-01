#ifndef PBM_H_INCLUDED
#define PBM_H_INCLUDED

#include "pm.h"

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif

typedef unsigned char bit;
#define PBM_WHITE 0
#define PBM_BLACK 1


/* Magic constants. */

#define PBM_MAGIC1 'P'
#define PBM_MAGIC2 '1'
#define RPBM_MAGIC2 '4'
#define PBM_FORMAT (PBM_MAGIC1 * 256 + PBM_MAGIC2)
#define RPBM_FORMAT (PBM_MAGIC1 * 256 + RPBM_MAGIC2)
#define PBM_TYPE PBM_FORMAT


/* Macro for turning a format number into a type number. */

#define PBM_FORMAT_TYPE(f) \
  ((f) == PBM_FORMAT || (f) == RPBM_FORMAT ? PBM_TYPE : -1)


/* Declarations of routines. */

void
pbm_init(int *   const argcP,
         char ** const argv);

void
pbm_nextimage(FILE *file, int * const eofP);

bit *
pbm_allocrow(unsigned int const cols);

#define pbm_allocarray(cols, rows) \
  ((bit**) pm_allocarray(cols, rows, sizeof(bit)))
#define pbm_freearray(bits, rows) pm_freearray((char**) bits, rows)
#define pbm_freerow(bitrow) pm_freerow((char*) bitrow)
#define pbm_packed_bytes(cols) (((cols)+7)/8)
#define pbm_allocrow_packed(cols) \
    ((unsigned char *) pm_allocrow(pbm_packed_bytes(cols), \
                                   sizeof(unsigned char)))
#define pbm_freerow_packed(packed_bits) \
    pm_freerow((char *) packed_bits)
#define pbm_allocarray_packed(cols, rows) ((unsigned char **) \
    pm_allocarray(pbm_packed_bytes(cols), rows, sizeof(unsigned char)))
#define pbm_freearray_packed(packed_bits, rows) \
    pm_freearray((char **) packed_bits, rows)

bit**
pbm_readpbm(FILE * const file,
            int  * const colsP,
            int  * const rowsP);

void
pbm_readpbminit(FILE * const file,
                int  * const colsP,
                int  * const rowsP, int * const formatP);

void
pbm_readpbmrow(FILE * const file,
               bit  * const bitrow,
               int    const cols,
               int    const format);

void
pbm_readpbmrow_packed(FILE *          const file, 
                      unsigned char * const packedBits,
                      int             const cols, 
                      int             const format);

void
pbm_readpbmrow_bitoffset(FILE *          const fileP,
                         unsigned char * const packedBits, 
                         int             const cols,
                         int             const format,
                         unsigned int    const offset);

void
pbm_cleanrowend_packed(unsigned char * const packedBits,
                       unsigned int    const cols);

void
pbm_writepbminit(FILE * const fileP, 
                 int    const cols, 
                 int    const rows, 
                 int    const forceplain);

void
pbm_writepbm(FILE * const fileP, 
             bit ** const bits, 
             int    const cols, 
             int    const rows, 
             int    const forceplain);

void
pbm_writepbmrow(FILE *      const fileP, 
                const bit * const bitrow, 
                int         const cols, 
                int         const forceplain);

void
pbm_writepbmrow_packed(FILE *                const fileP, 
                       const unsigned char * const packed_bits,
                       int                   const cols, 
                       int                   const forceplain);

void
pbm_writepbmrow_bitoffset(FILE *          const ifP,
                          unsigned char * const packedBits,
                          unsigned int    const cols,
                          int             const format,
                          unsigned int    const offset);

void
pbm_check(FILE * file, const enum pm_check_type check_type, 
          const int format, const int cols, const int rows,
          enum pm_check_code * const retval_p);

bit
pbm_backgroundbitrow(const unsigned char * const packedBits,
                     unsigned int          const cols,
                     unsigned int          const offset);

#ifdef __cplusplus
}
#endif

#endif
