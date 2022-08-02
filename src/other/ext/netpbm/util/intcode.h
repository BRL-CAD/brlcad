#ifndef INTCODE_H_INCLUDED
#define INTCODE_H_INCLUDED

#include "pm_config.h"  /* For uint32_t, BYTE_ORDER */

typedef struct {
/*----------------------------------------------------------------------------
   This is a big-endian representation of a 32 bit integer.  I.e.
   bytes[0] is the most significant 8 bits; bytes[3] is the least
   significant 8 bits of the number in pure binary.

   On a big-endian machines, this is bit for bit identical to uint32_t.
   On a little-endian machine, it isn't.

   This is an important data type because decent file formats use
   big-endian -- they don't care if some CPU happens to use some other
   code for its own work.
-----------------------------------------------------------------------------*/
    unsigned char bytes[4];
} bigend32;


unsigned int const pm_byteOrder = BYTE_ORDER;


static __inline__ uint32_t
pm_uintFromBigend32(bigend32 const arg) {

    uint32_t retval;

    switch (pm_byteOrder) {
    case BIG_ENDIAN: {
        union {
            bigend32 bigend;
            uint32_t native;
        } converter;
        
        converter.bigend = arg;
        
        retval = converter.native;
    }; break;
    case LITTLE_ENDIAN: {
        retval =
            (arg.bytes[0] << 24) |
            (arg.bytes[1] << 16) |
            (arg.bytes[2] <<  8) |
            (arg.bytes[3] <<  0);
    } break;
    }
    return retval;
}



static __inline__ bigend32
pm_bigendFromUint32(uint32_t const arg) {

    bigend32 retval;

    switch (pm_byteOrder) {
    case BIG_ENDIAN: {
        union {
            bigend32 bigend;
            uint32_t native;
        } converter;
        
        converter.native = arg;

        retval = converter.bigend;
    } break;
    case LITTLE_ENDIAN: {
        uint32_t shift;
        shift = arg;
        retval.bytes[3] = shift;  /* Takes lower 8 bits */
        shift >>= 8;
        retval.bytes[2] = shift;  /* Takes lower 8 bits */
        shift >>= 8;
        retval.bytes[1] = shift;  /* Takes lower 8 bits */
        shift >>= 8;
        retval.bytes[0] = shift;  /* Takes lower 8 bits */
    } break;
    }
    return retval;
}

#endif
