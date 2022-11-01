/* libpbm3.c - pbm utility library part 3
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

#include <assert.h>

#include "pm_c_util.h"

#include "pbm.h"

#ifndef PACKBITS_SSE
#if WANT_SSE && defined(__SSE2__) && HAVE_GCC_BSWAP
  #define PACKBITS_SSE 2
#else
  #define PACKBITS_SSE 0
#endif
#endif

/* WANT_SSE means we want to use SSE CPU facilities to make PBM raster
   processing faster.  This implies it's actually possible - i.e. the
   build environment has <emmintrin.h>.

   The GNU Compiler -msse2 option makes SSE/SSE2 available, and is
   evidenced by __SSE2__.
   For x86-32 with SSE, "-msse2" must be explicitly given.
   For x86-64 and AMD64, "-msse2" is the default (from Gcc v.4.)
*/

#if PACKBITS_SSE == 2
  #include <emmintrin.h>
#endif


void
pbm_writepbminit(FILE * const fileP, 
                 int    const cols, 
                 int    const rows, 
                 int    const forceplain) {

    if (!forceplain && !pm_plain_output) {
        fprintf(fileP, "%c%c\n%d %d\n", PBM_MAGIC1, RPBM_MAGIC2, cols, rows);
    } else
        fprintf(fileP, "%c%c\n%d %d\n", PBM_MAGIC1, PBM_MAGIC2, cols, rows);
}



static void
writePackedRawRow(FILE *                const fileP,
                  const unsigned char * const packed_bits,
                  int                   const cols) {

    int bytesWritten;
    bytesWritten = fwrite(packed_bits, 1, pbm_packed_bytes(cols), fileP);
    if (bytesWritten < pbm_packed_bytes(cols)) 
        pm_error("I/O error writing packed row to raw PBM file.");
} 



#if PACKBITS_SSE == 2
static void
packBitsWithSse2(  FILE *          const fileP,
                   const bit *     const bitrow,
                   unsigned char * const packedBits,
                   unsigned int    const cols) {
/*----------------------------------------------------------------------------
    Pack the bits of bitrow[] into bytes at 'packedBits'.

    Use the SSE2 facilities to pack the bits quickly, but
    perform the exact same function as the simpler
    packBitsGeneric() + packPartialBytes()

    Unlike packBitsGeneric(), the whole row is converted.
-----------------------------------------------------------------------------*/
    /*
      We use 2 SSE registers.
    
      The key machine instructions are:
        
      PCMPGTB128  Packed CoMPare Greater Than Byte
    
        Compares 16 bytes in parallel
        Result is x00 if greater than, xFF if not for each byte

    
      PMOVMSKB128 Packed MOVe MaSK Byte 
    
        Result is 16 bits, the MSBs of 16 bytes
        x00 xFF x00 xFF xFF xFF x00 x00 xFF xFF xFF xFF x00 x00 x00 x00 
        --> 0101110011110000B = 0x5CF0
        
        The result is actually a 64 bit int, but the higher bits are
        always 0.

      We use SSE instructions in "_mm_" form in favor of "__builtin_".
      In GCC the "__builtin_" form is documented but "_mm_" is not.
      Former versions of this source file used "__builtin_".  This was
      changed to make possible compilation with clang, which does not
      implement some "__builtin_" forms.

      __builtin_ia32_pcmpgtb128 :  _mm_cmpgt_epi8
      __builtin_ia32_pmovmskb128 : _mm_movemask_epi8

      The conversion requires <emmintrin.h> .
    */

    typedef char v16qi __attribute__ ((vector_size(16)));

    unsigned int col;
    union {
        v16qi    v16;
        uint64_t i64[2];
        unsigned char byte[16];
    } bit128;

    v16qi zero128;
    zero128 = zero128 ^ zero128;   /* clear to zero */

    for (col = 0; col + 15 < cols; col += 16) {
        bit128.i64[0]=__builtin_bswap64( *(uint64_t*) &bitrow[col]);
        bit128.i64[1]=__builtin_bswap64( *(uint64_t*) &bitrow[col+8]);

        {
            v16qi const compare = (v16qi)
                _mm_cmpgt_epi8((__m128i)bit128.v16, (__m128i) zero128);
            uint16_t const blackMask = _mm_movemask_epi8 ((__m128i)compare);
            
            *(uint16_t *) & packedBits[col/8] = blackMask;
        }
    }

    if (cols % 16 > 0) {
        unsigned int i, j;

        bit128.v16 = bit128.v16 ^ bit128.v16;
    
        for (i = 0, j = col ; j < cols; ++i, ++j) 
            bit128.byte[ (i&8) + 7-(i&7) ] = bitrow[j];
      
        {
            v16qi const compare = (v16qi)
                _mm_cmpgt_epi8((__m128i)bit128.v16, (__m128i) zero128);
            uint16_t const blackMask = _mm_movemask_epi8 ((__m128i)compare);

            if ( cols%16 >8 )  /* Two partial bytes */
                *(uint16_t *) & packedBits[col/8] = blackMask;
            else              /* One partial byte */
                packedBits[col/8] = (unsigned char) blackMask ;
        }
    }
}
#else
/* Avoid undefined function warning; never actually called */

#define packBitsWithSse2(a,b,c,d) packBitsGeneric((a),(b),(c),(d),NULL)
#endif



static unsigned int
bitValue(unsigned char const byteValue) {

    return byteValue == 0 ? 0 : 1;
}



static void
packBitsGeneric(FILE *          const fileP,
                const bit *     const bitrow,
                unsigned char * const packedBits,
                unsigned int    const cols,
                unsigned int *  const nextColP) {
/*----------------------------------------------------------------------------
   Pack the bits of bitrow[] into bytes at 'packedBits'.  Going left to right,
   stop when there aren't enough bits left to fill a whole byte.  Return
   as *nextColP the number of the next column after the rightmost one we
   packed.

   Don't use any special CPU facilities to do the packing.
-----------------------------------------------------------------------------*/
    unsigned int col;

    for (col = 0; col + 7 < cols; col += 8)
        packedBits[col/8] = (
            bitValue(bitrow[col+0]) << 7 |
            bitValue(bitrow[col+1]) << 6 |
            bitValue(bitrow[col+2]) << 5 |
            bitValue(bitrow[col+3]) << 4 |
            bitValue(bitrow[col+4]) << 3 |
            bitValue(bitrow[col+5]) << 2 |
            bitValue(bitrow[col+6]) << 1 |
            bitValue(bitrow[col+7]) << 0
            );
    *nextColP = col;
}



static void
packPartialBytes(const bit *     const bitrow,
                 unsigned int    const cols,
                 unsigned int    const nextCol,
                 unsigned char * const packedBits) {
              
    /* routine for partial byte at the end of packedBits[]
       Prior to addition of the above enhancement,
       this method was used for the entire process
    */                   
    
    unsigned int col;
    int bitshift;
    unsigned char item;
    
    bitshift = 7;  /* initial value */
    item = 0;      /* initial value */
    for (col = nextCol; col < cols; ++col, --bitshift)
        if (bitrow[col] != 0)
            item |= 1 << bitshift;
    
    packedBits[col/8] = item;
}



static void
writePbmRowRaw(FILE *      const fileP,
               const bit * const bitrow,
               int         const cols) {

    jmp_buf jmpbuf;
    jmp_buf * origJmpbufP;
    unsigned char * packedBits;

    packedBits = pbm_allocrow_packed(cols);

    if (setjmp(jmpbuf) != 0) {
        pbm_freerow_packed(packedBits);
        pm_setjmpbuf(origJmpbufP);
        pm_longjmp();
    } else {

        pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

        switch (PACKBITS_SSE) {
        case 2: 
            packBitsWithSse2(fileP, bitrow, packedBits, cols);
            break;
        default: {
            unsigned int nextCol;
            packBitsGeneric(fileP, bitrow, packedBits, cols, &nextCol);
            if (cols % 8 > 0)
                packPartialBytes(bitrow, cols, nextCol, packedBits);
        }
        }
        writePackedRawRow(fileP, packedBits, cols);

        pm_setjmpbuf(origJmpbufP);
    }
    pbm_freerow_packed(packedBits);
}



static void
writePbmRowPlain(FILE *      const fileP,
                 const bit * const bitrow, 
                 int         const cols) {
    
    int col, charcount;

    charcount = 0;
    for (col = 0; col < cols; ++col) {
        if (charcount >= 70) {
            putc('\n', fileP);
            charcount = 0;
        }
        putc(bitrow[col] ? '1' : '0', fileP);
        ++charcount;
    }
    putc('\n', fileP);
}



void
pbm_writepbmrow(FILE *       const fileP, 
                const bit *  const bitrow, 
                int          const cols, 
                int          const forceplain) {

    if (!forceplain && !pm_plain_output)
        writePbmRowRaw(fileP, bitrow, cols);
    else
        writePbmRowPlain(fileP, bitrow, cols);
}



void
pbm_writepbmrow_packed(FILE *                const fileP, 
                       const unsigned char * const packedBits,
                       int                   const cols, 
                       int                   const forceplain) {

    if (!forceplain && !pm_plain_output)
        writePackedRawRow(fileP, packedBits, cols);
    else {
        jmp_buf jmpbuf;
        jmp_buf * origJmpbufP;
        bit * bitrow;

        bitrow = pbm_allocrow(cols);

        if (setjmp(jmpbuf) != 0) {
            pbm_freerow(bitrow);
            pm_setjmpbuf(origJmpbufP);
            pm_longjmp();
        } else {
            unsigned int col;
            
            pm_setjmpbufsave(&jmpbuf, &origJmpbufP);

            for (col = 0; col < cols; ++col) 
                bitrow[col] = 
                    packedBits[col/8] & (0x80 >> (col%8)) ?
                    PBM_BLACK : PBM_WHITE;

            writePbmRowPlain(fileP, bitrow, cols);

            pm_setjmpbuf(origJmpbufP);
        }
        pbm_freerow(bitrow);
    }
}



static unsigned char
leftBits(unsigned char const x,
         unsigned int  const n) {
/*----------------------------------------------------------------------------
   Clear rightmost (8-n) bits, retain leftmost (=high) n bits.
-----------------------------------------------------------------------------*/
    unsigned char buffer;

    assert(n < 8);

    buffer = x;

    buffer >>= (8-n);
    buffer <<= (8-n);

    return buffer;
}



void
pbm_writepbmrow_bitoffset(FILE *          const fileP,
                          unsigned char * const packedBits,
                          unsigned int    const cols,
                          int             const format,
                          unsigned int    const offset) {
/*----------------------------------------------------------------------------
   Write PBM row from a packed bit buffer 'packedBits, starting at the
   specified offset 'offset' in the buffer.

   We destroy the buffer.
-----------------------------------------------------------------------------*/
    unsigned int const rsh = offset % 8;
    unsigned int const lsh = (8 - rsh) % 8;
    unsigned int const csh = cols % 8;
    unsigned char * const window = &packedBits[offset/8];
        /* Area of packed row buffer from which we take the image data.
           Aligned to nearest byte boundary to the left, so the first
           few bits might be irrelvant.

           Also our work buffer, in which we shift bits and from which we
           ultimately write the bits to the file.
        */
    unsigned int const colByteCnt = pbm_packed_bytes(cols);
    unsigned int const last = colByteCnt - 1;
        /* Position within window of rightmost byte after shift */

    bool const carryover = (csh == 0 || rsh + csh > 8);
        /* TRUE:  Input comes from colByteCnt bytes and one extra byte.
           FALSE: Input comes from colByteCnt bytes.  For example:
           TRUE:  xxxxxxii iiiiiiii iiiiiiii iiixxxxx  cols=21, offset=6 
           FALSE: xiiiiiii iiiiiiii iiiiiixx ________  cols=21, offset=1

           We treat these differently for in the FALSE case the byte after
           last (indicated by ________) may not exist.
        */
       
    if (rsh > 0) {
        unsigned int const shiftBytes =  carryover ? colByteCnt : colByteCnt-1;

        unsigned int i;
        for (i = 0; i < shiftBytes; ++i)
            window[i] = window[i] << rsh | window[i+1] >> lsh;

        if (!carryover)
            window[last] = window[last] << rsh;
    }
      
    if (csh > 0)
        window[last] = leftBits(window[last], csh);
          
    pbm_writepbmrow_packed(fileP, window, cols, 0);
}



void
pbm_writepbm(FILE * const fileP, 
             bit ** const bits, 
             int    const cols, 
             int    const rows, 
             int    const forceplain) {

    int row;

    pbm_writepbminit(fileP, cols, rows, forceplain);
    
    for (row = 0; row < rows; ++row)
        pbm_writepbmrow(fileP, bits[row], cols, forceplain);
}
