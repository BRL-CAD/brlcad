/*                        B I T V . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @defgroup container Data Containers */
/**   @defgroup bitv Bit Vectors */
#ifndef BU_BITV_H
#define BU_BITV_H

#include "common.h"

#include "./defines.h"
#include "./list.h"
#include "./vls.h"

/*----------------------------------------------------------------------*/
/** @addtogroup bitv */
/** @ingroup container */
/** @{*/
/** @file libbu/bitv.c
 *
 * Routines for managing efficient high-performance bit vectors of
 * arbitrary length.
 *
 * The basic type "bitv_t" is defined in include/bu.h; it is the
 * widest integer datatype for which efficient hardware support
 * exists.  BU_BITV_SHIFT and BU_BITV_MASK are also defined in bu.h
 *
 * These bit vectors are "little endian", bit 0 is in the right hand
 * side of the [0] word.
 *
 */

/**
 * bitv_t should be a fast integer type for implementing bit vectors.
 *
 * On many machines, this is a 32-bit "long", but on some machines a
 * compiler/vendor-specific type such as "long long" or even 'char'
 * can give access to faster integers.
 *
 * THE SIZE OF bitv_t MUST MATCH BU_BITV_SHIFT.
 */
typedef unsigned char bitv_t;

/**
 * Bit vector shift size
 *
 * Should equal to: log2(sizeof(bitv_t)*8.0).  Using bu_bitv_shift()
 * will return a run-time computed shift size if the size of a bitv_t
 * changes.  Performance impact is rather minimal for most models but
 * disabled for a handful of primitives that heavily rely on bit
 * vectors.
 *
 * (8-bit type: 3, 16-bit type: 4, 32-bit type: 5, 64-bit type: 6)
 */
#ifdef CHAR_BIT
#  if CHAR_BIT == 8
#    define BU_BITV_SHIFT 3
#  elif CHAR_BIT == 16
#    define BU_BITV_SHIFT 4
#  elif CHAR_BIT == 32
#    define BU_BITV_SHIFT 5
#  elif CHAR_BIT == 64
#    define BU_BITV_SHIFT 6
#  endif
#else
#  define BU_BITV_SHIFT bu_bitv_shift()
#endif

/** Bit vector mask */
#define BU_BITV_MASK ((1<<BU_BITV_SHIFT)-1)

/**
 * Bit vector data structure.
 *
 * bu_bitv uses a little-endian encoding, placing bit 0 on the right
 * side of the 0th word.
 *
 * This is done only because left-shifting a 1 can be done in an
 * efficient word-length-independent manner; going the other way would
 * require a compile-time constant with only the sign bit set, and an
 * unsigned right shift, which some machines don't have in hardware,
 * or an extra subtraction.
 *
 * Application code should *never* peek at the bit-buffer; use the
 * macros.  The external hex form is most significant byte first (bit
 * 0 is at the right).  Note that MUVES does it differently.
 */
struct bu_bitv {
    struct bu_list l;		/**< linked list for caller's use */
    size_t nbits;		/**< actual size of bits[], in bits */
    bitv_t bits[2];	/**< variable size array */
};
typedef struct bu_bitv bu_bitv_t;
#define BU_BITV_NULL ((struct bu_bitv *)0)

/**
 * asserts the integrity of a non-head node bu_bitv struct.
 */
#define BU_CK_BITV(_bp) BU_CKMAG(_bp, BU_BITV_MAGIC, "bu_bitv")

/**
 * initializes a bu_bitv struct without allocating any memory.  this
 * macro is not suitable for initializing a head list node.
 */
#define BU_BITV_INIT(_bp) { \
	BU_LIST_INIT_MAGIC(&(_bp)->l, BU_BITV_MAGIC); \
	(_bp)->nbits = 0; \
	(_bp)->bits[0] = 0; \
	(_bp)->bits[1] = 0; \
    }

/**
 * macro suitable for declaration statement initialization of a bu_bitv
 * struct.  does not allocate memory.  not suitable for a head node.
 */
#define BU_BITV_INIT_ZERO { {BU_BITV_MAGIC, BU_LIST_NULL, BU_LIST_NULL}, 0, {0, 0} }

/**
 * returns truthfully whether a bu_bitv has been initialized
 */
#define BU_BITV_IS_INITIALIZED(_bp) (((struct bu_bitv *)(_bp) != BU_BITV_NULL) && LIKELY((_bp)->l.magic == BU_BITV_MAGIC))

/**
 * returns floor(log2(sizeof(bitv_t)*8.0)), i.e. the number of bits
 * required with base-2 encoding to index any bit in an array of
 * length sizeof(bitv_t)*8.0 bits long.  users should not call this
 * directly, instead calling the BU_BITV_SHIFT macro instead.
 */
BU_EXPORT extern size_t bu_bitv_shift(void);

/**
 * Convert a number of words into the corresponding (bitv_t type) size
 * as a bit-vector size.
 */
#define BU_WORDS2BITS(_nw)	((size_t)(_nw>0?_nw:0)*sizeof(bitv_t)*8)

/**
 * Convert a bit-vector (stored in a bitv_t array) size into the
 * corresponding word size.
 */
#define BU_BITS2WORDS(_nb)	(((size_t)(_nb>0?_nb:0)+BU_BITV_MASK)>>BU_BITV_SHIFT)

/**
 * Convert a bit-vector (stored in a bitv_t array) size into the
 * corresponding total memory size (in bytes) of the bitv_t array.
 */
#define BU_BITS2BYTES(_nb)	(BU_BITS2WORDS(_nb)*sizeof(bitv_t))


#if 1
#define BU_BITTEST(_bv, bit)	\
    (((_bv)->bits[(bit)>>BU_BITV_SHIFT] & (((bitv_t)1)<<((bit)&BU_BITV_MASK)))!=0)
#else
static __inline__ int BU_BITTEST(volatile void * addr, int nr)
{
    int oldbit;

    __asm__ __volatile__(
	"btl %2, %1\n\tsbbl %0, %0"
	:"=r" (oldbit)
	:"m" (addr), "Ir" (nr));
    return oldbit;
}
#endif

#define BU_BITSET(_bv, bit)	\
    ((_bv)->bits[(bit)>>BU_BITV_SHIFT] |= (((bitv_t)1)<<((bit)&BU_BITV_MASK)))
#define BU_BITCLR(_bv, bit)	\
    ((_bv)->bits[(bit)>>BU_BITV_SHIFT] &= ~(((bitv_t)1)<<((bit)&BU_BITV_MASK)))

/**
 * zeros all of the internal storage bytes in a bit vector array
 */
#define BU_BITV_ZEROALL(_bv)	\
    { \
	if (LIKELY((_bv) && (_bv)->nbits != 0)) { \
	    unsigned char *bvp = (unsigned char *)(_bv)->bits; \
	    size_t nbytes = BU_BITS2BYTES((_bv)->nbits); \
	    do { \
		*bvp++ = (unsigned char)0; \
	    } while (--nbytes != 0); \
	} \
    }


/* This is not done by default for performance reasons */
#ifdef NO_BOMBING_MACROS
#  define BU_BITV_BITNUM_CHECK(_bv, _bit) BU_IGNORE((_bv))
#else
#  define BU_BITV_BITNUM_CHECK(_bv, _bit)	/* Validate bit number */ \
    if (UNLIKELY(((unsigned)(_bit)) >= (_bv)->nbits)) {\
	bu_log("BU_BITV_BITNUM_CHECK bit number (%u) out of range (0..%u)\n", \
	       ((unsigned)(_bit)), (_bv)->nbits); \
	bu_bomb("process self-terminating\n");\
    }
#endif

#ifdef NO_BOMBING_MACROS
#  define BU_BITV_NBITS_CHECK(_bv, _nbits) BU_IGNORE((_bv))
#else
#  define BU_BITV_NBITS_CHECK(_bv, _nbits)	/* Validate number of bits */ \
    if (UNLIKELY(((unsigned)(_nbits)) > (_bv)->nbits)) {\
	bu_log("BU_BITV_NBITS_CHECK number of bits (%u) out of range (> %u)", \
	       ((unsigned)(_nbits)), (_bv)->nbits); \
	bu_bomb("process self-terminating"); \
    }
#endif


/**
 * Macros to efficiently find all the ONE bits in a bit vector.
 * Counts words down, counts bits in words going up, for speed &
 * portability.  It does not matter if the shift causes the sign bit
 * to smear to the right.
 *
 * @par Example:
 @code

 BU_BITV_LOOP_START(bv) {
   fiddle(BU_BITV_LOOP_INDEX);
 } BU_BITV_LOOP_END;

 @endcode
 *
 */
#define BU_BITV_LOOP_START(_bv)	\
    { \
	int _wd;	/* Current word number */  \
	BU_CK_BITV(_bv); \
	for (_wd=BU_BITS2WORDS((_bv)->nbits)-1; _wd>=0; _wd--) {  \
	    int _b;	/* Current bit-in-word number */  \
	    bitv_t _val;	/* Current word value */  \
	    if ((_val = (_bv)->bits[_wd])==0) continue;  \
	    for (_b=0; _b < BU_BITV_MASK+1; _b++, _val >>= 1) { \
		if (!(_val & 1)) continue;

/**
 * This macro is valid only between a BU_BITV_LOOP_START/LOOP_END
 * pair, and gives the bit number of the current iteration.
 */
#define BU_BITV_LOOP_INDEX ((_wd << BU_BITV_SHIFT) | _b)

/**
 * Paired with BU_BITV_LOOP_START()
 */
#define BU_BITV_LOOP_END	\
    } /* end for (_b) */ \
	    } /* end for (_wd) */ \
	} /* end block */

/**
 * Allocate storage for a new bit vector of at least 'nbits' in
 * length.  The bit vector itself is guaranteed to be initialized to
 * all zero.
 */
BU_EXPORT extern struct bu_bitv *bu_bitv_new(size_t nbits);

/**
 * Release all internal storage for this bit vector.
 *
 * It is the caller's responsibility to not use the pointer 'bv' any
 * longer.  It is the caller's responsibility to dequeue from any
 * linked list first.
 */
BU_EXPORT extern void bu_bitv_free(struct bu_bitv *bv);

/**
 * Set all the bits in the bit vector to zero.
 *
 * Also available as a BU_BITV_ZEROALL macro if you don't desire the
 * pointer checking.
 */
BU_EXPORT extern void bu_bitv_clear(struct bu_bitv *bv);

/**
 * TBD
 */
BU_EXPORT extern void bu_bitv_or(struct bu_bitv *ov,  const struct bu_bitv *iv);

/**
 * TBD
 */
BU_EXPORT extern void bu_bitv_and(struct bu_bitv *ov, const struct bu_bitv *iv);

/**
 * Print the bits set in a bit vector.
 */
BU_EXPORT extern void bu_bitv_vls(struct bu_vls *v, const struct bu_bitv *bv);

/**
 * Print the bits set in a bit vector.  Use bu_vls stuff, to make only
 * a single call to bu_log().
 */
BU_EXPORT extern void bu_pr_bitv(const char *str, const struct bu_bitv *bv);

/**
 * Convert a bit vector to an ascii string of hex digits.  The string
 * is from MSB to LSB (bytes and bits).
 */
BU_EXPORT extern void bu_bitv_to_hex(struct bu_vls *v, const struct bu_bitv *bv);

/**
 * Convert a string of HEX digits (as produced by bu_bitv_to_hex) into
 * a bit vector.
 */
BU_EXPORT extern struct bu_bitv *bu_hex_to_bitv(const char *str);

/**
 * Convert a bit vector to an ascii string of binary digits in the GCC
 * format ("0bn...").  The string is from MSB to LSB (bytes and bits).
 */
BU_EXPORT extern void bu_bitv_to_binary(struct bu_vls *v, const struct bu_bitv *bv);

/**
 * Convert a string of BINARY digits (as produced by
 * bu_bitv_to_binary) into a bit vector.
 */
BU_EXPORT extern struct bu_bitv *bu_binary_to_bitv(const char *str);

/**
 * Convert a string of BINARY digits (as produced by
 * bu_bitv_to_binary) into a bit vector.  The "nbytes" argument may be
 * zero if the user has no minimum length preference.
 */
BU_EXPORT extern struct bu_bitv *bu_binary_to_bitv2(const char *str, const int nbytes);

/**
 * Compare two bit vectors for equality. They are considered equal iff
 * their lengths and each bit are equal.  Returns 1 for true, zero for
 * false.
 */
BU_EXPORT extern int bu_bitv_compare_equal(const struct bu_bitv *, const struct bu_bitv *);

/**
 * Compare two bit vectors for equality. They are considered equal iff
 * their non-zero bits are equal (leading zero bits are ignored so
 * lengths are not considered explicitly). Returns 1 for true, 0 for
 * false.
 */
BU_EXPORT extern int bu_bitv_compare_equal2(const struct bu_bitv *, const struct bu_bitv *);

/**
 * Make a copy of a bit vector
 */
BU_EXPORT extern struct bu_bitv *bu_bitv_dup(const struct bu_bitv *bv);


/**
 * Convert a string of hex characters to an equivalent string of
 * binary characters.
 *
 * The input hex string may have an optional prefix of '0x' or '0X' in
 * which case the resulting binary string will be prefixed with '0b'.
 *
 * The input string is expected to represent an integral number of
 * bytes but will have leading zeroes prepended as necessary to fulfill
 * that requirement.
 *
 * Returns BRLCAD_OK for success, BRLCAD_ERROR for errors.
 */
BU_EXPORT extern int bu_hexstr_to_binstr(const char *hexstr, struct bu_vls *b);


/**
 * Convert a string of binary characters to an equivalent string of
 * hex characters.
 *
 * The input binary string may have an optional prefix of '0b' or '0B'
 * in which case the resulting hex string will be prefixed with '0x'.
 *
 * The input string is expected to represent an integral number of
 * bytes but will have leading zeroes prepended as necessary to fulfill
 * that requirement.
 *
 * Returns BRLCAD_OK for success, BRLCAD_ERROR for errors.
 *
 */
BU_EXPORT extern int bu_binstr_to_hexstr(const char *binstr, struct bu_vls *h);

/** @addtogroup bitv */
/** @ingroup container */
/** @{ */
/** @file libbu/printb.c
 *
 * print bitfields
 *
 */

/**
 * Format a value a la the %b format of the kernel's printf
 *
 * @param   vls	variable length string to put output in
 * @param    s		title string
 * @param   v		the integer with the bits in it
 * @param   bits	a string which starts with the desired base (8 or 16)
 * as \\010 or \\020, followed by
 * words preceded with embedded low-value bytes indicating
 * bit number plus one,
 * in little-endian order, e.g.:
 * "\010\2Bit_one\1BIT_zero"
 */
BU_EXPORT extern void bu_vls_printb(struct bu_vls *vls,
				    const char *s, unsigned long v,
				    const char *bits);

/**
 * Format and print, like bu_vls_printb().
 */
BU_EXPORT extern void bu_printb(const char *s,
				unsigned long v,
				const char *bits);

/** @} */

#endif  /* BU_BITV_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
