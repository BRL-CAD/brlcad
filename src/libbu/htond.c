/*                         H T O N D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#include "common.h"

#ifdef HAVE_MEMORY_H
#  include <memory.h>
#endif
#include <stdio.h>
#include <assert.h>

#include "bu.h"


#define OUT_IEEE_ZERO { \
	*out++ = 0; \
	*out++ = 0; \
	*out++ = 0; \
	*out++ = 0; \
	*out++ = 0; \
	*out++ = 0; \
	*out++ = 0; \
	*out++ = 0; \
	continue;   \
}


#define OUT_IEEE_NAN { /* Signaling NAN */	\
	*out++ = 0xFF;				\
	*out++ = 0xF0;				\
	*out++ = 0x0B;				\
	*out++ = 0xAD;				\
	*out++ = 0x0B;				\
	*out++ = 0xAD;				\
	*out++ = 0x0B;				\
	*out++ = 0xAD;				\
	continue;				\
}


void
htond(register unsigned char *out, register const unsigned char *in, size_t count)
{
    register size_t i;

    assert(sizeof(double) == SIZEOF_NETWORK_DOUBLE);

    switch (bu_byteorder()) {
	case BU_BIG_ENDIAN:
	    /*
	     * First, the case where the system already operates in
	     * IEEE format internally, using big-endian order.  These
	     * are the lucky ones.
	     */
	    memcpy(out, in, count*SIZEOF_NETWORK_DOUBLE);
	    return;
	case BU_LITTLE_ENDIAN:
	    /*
	     * This machine uses IEEE, but in little-endian byte order
	     */
	    for (i=count; i > 0; i--) {
		*out++ = in[7];
		*out++ = in[6];
		*out++ = in[5];
		*out++ = in[4];
		*out++ = in[3];
		*out++ = in[2];
		*out++ = in[1];
		*out++ = in[0];
		in += SIZEOF_NETWORK_DOUBLE;
	    }
	    return;
	default:
	    /* nada */
	    break;
    }

    /* Now, for the machine-specific stuff.
     *
     * XXX - Note that the below will not likely work without defining
     * a corresponding pre-processor flag.
     */

#if defined(sgi) && !defined(mips)
    /*
     * Silicon Graphics Iris workstation.  On the 2-D and 3-D, a
     * double is type converted to a float (4 bytes), but IEEE single
     * precision has a different number of exponent bits than double
     * precision, so we have to engage in gyrations here.
     */
    for (i=count; i > 0; i--) {
	/* Brain-damaged 3-D case */
	float small;
	long float big;
	register unsigned char *fp = (unsigned char *)&small;

	*fp++ = *in++;
	*fp++ = *in++;
	*fp++ = *in++;
	*fp++ = *in++;
	big = small;		/* H/W cvt to IEEE double */

	fp = (unsigned char *)&big;
	*out++ = *fp++;
	*out++ = *fp++;
	*out++ = *fp++;
	*out++ = *fp++;
	*out++ = *fp++;
	*out++ = *fp++;
	*out++ = *fp++;
	*out++ = *fp++;
    }
    return;
#endif
#if defined(vax)
    /*
     * Digital Equipment's VAX.
     * VAX order is +6, +4, +2, sign|exp|fraction+0
     * with 8 bits of exponent, excess 128 base 2, exp=0 => zero.
     */
    for (i=count; i > 0; i--) {
	register unsigned long left, right, signbit;
	register int exp;

	left  = (in[1]<<24) | (in[0]<<16) | (in[3]<<8) | in[2];
	right = (in[5]<<24) | (in[4]<<16) | (in[7]<<8) | in[6];
	in += 8;

	exp = (left >> 23) & 0xFF;
	signbit = left & 0x80000000;
	if (exp == 0) {
	    if (signbit) {
		OUT_IEEE_NAN;
	    } else {
		OUT_IEEE_ZERO;
	    }
	}
	exp += 1023 - 129;
	/* Round LSB by adding 4, rather than truncating */
#  ifdef ROUNDING
	right = (left<<(32-3)) | ((right+4)>>3);
#  else
	right = (left<<(32-3)) | (right>>3);
#  endif
	left =  ((left & 0x007FFFFF)>>3) | signbit | (exp<<20);
	*out++ = left>>24;
	*out++ = left>>16;
	*out++ = left>>8;
	*out++ = left;
	*out++ = right>>24;
	*out++ = right>>16;
	*out++ = right>>8;
	*out++ = right;
    }
    return;
#endif
#if defined(ibm) || defined(gould)
    /*
     * IBM Format.
     * 7-bit exponent, base 16.
     * No hidden bits in mantissa (56 bits).
     */
    for (i=count; i > 0; i--) {
	register unsigned long left, right, signbit;
	register int exp;

	left  = (in[0]<<24) | (in[1]<<16) | (in[2]<<8) | in[3];
	right = (in[4]<<24) | (in[5]<<16) | (in[6]<<8) | in[7];
	in += 8;

	exp = (left>>24) & 0x7F;	/* excess 64, base 16 */
	if (left == 0 && right == 0)
	    OUT_IEEE_ZERO;

	signbit = left & 0x80000000;
	left &= 0x00FFFFFF;
	if (signbit) {
	    /* The IBM uses 2's compliment on the mantissa,
	     * and IEEE does not.
	     */
	    left  ^= 0xFFFFFFFF;
	    right ^= 0xFFFFFFFF;
	    if (right & 0x80000000) {
		/* There may be a carry */
		right += 1;
		if ((right & 0x80000000) == 0) {
		    /* There WAS a carry */
		    left += 1;
		}
	    } else {
		/* There will be no carry to worry about */
		right += 1;
	    }
	    left &= 0x00FFFFFF;
	    exp = (~exp) & 0x7F;
	}
	exp -= (64-32+1);		/* excess 32, base 16, + fudge */
	exp *= 4;			/* excess 128, base 2 */
    ibm_normalized:
	if (left & 0x00800000) {
	    /* fix = 0; */
	    exp += 1023-129+1+ 3-0;/* fudge, slide hidden bit */
	} else if (left & 0x00400000) {
	    /* fix = 1; */
	    exp += 1023-129+1+ 3-1;
	    left = (left<<1) |
		((right>>(32-1)) & (0x7FFFFFFF>>(31-1)));
	    right <<= 1;
	} else if (left & 0x00200000) {
	    /* fix = 2; */
	    exp += 1023-129+1+ 3-2;
	    left = (left<<2) |
		((right>>(32-2)) & (0x7FFFFFFF>>(31-2)));
	    right <<= 2;
	} else if (left & 0x00100000) {
	    /* fix = 3; */
	    exp += 1023-129+1+ 3-3;
	    left = (left<<3) |
		((right>>(32-3)) & (0x7FFFFFFF>>(31-3)));
	    right <<= 3;
	} else {
	    /* Encountered 4 consecutive 0 bits of mantissa,
	     * attempt to normalize, and loop.
	     * This case was not expected, but does happen,
	     * at least on the Gould.
	     */
	    exp -= 4;
	    left = (left<<4) | (right>>(32-4));
	    right <<= 4;
	    goto ibm_normalized;
	}

	/* After suitable testing, this check can be deleted */
	if ((left & 0x00800000) == 0) {
	    fprintf(stderr, "ibm->ieee missing 1, left=x%x\n", left);
	    left = (left<<1) | (right>>31);
	    right <<= 1;
	    goto ibm_normalized;
	}

	/* Having nearly VAX format, shift to IEEE, rounding. */
#  ifdef ROUNDING
	right = (left<<(32-3)) | ((right+4)>>3);
#  else
	right = (left<<(32-3)) | (right>>3);
#  endif
	left =  ((left & 0x007FFFFF)>>3) | signbit | (exp<<20);

	*out++ = left>>24;
	*out++ = left>>16;
	*out++ = left>>8;
	*out++ = left;
	*out++ = right>>24;
	*out++ = right>>16;
	*out++ = right>>8;
	*out++ = right;
    }
    return;
#endif
#if defined(CRAY1) || defined(CRAY2) || defined(eta10)
    /*
     * Cray version.  Somewhat easier using 64-bit registers.
     * 15 bit exponent, biased 040000 (octal).  48 mantissa bits.
     * No hidden bits.
     */
    for (i=count; i > 0; i--) {
	register unsigned long word, signbit;
	register int exp;

	word  = (((long)in[0])<<56) | (((long)in[1])<<48) |
	    (((long)in[2])<<40) | (((long)in[3])<<32) |
	    (((long)in[4])<<24) | (((long)in[5])<<16) |
	    (((long)in[6])<<8) | ((long)in[7]);
	in += 8;

	if (word == 0)
	    OUT_IEEE_ZERO;
	exp = (word >> 48) & 0x7FFF;
	signbit = word & 0x8000000000000000L;
#ifdef redundant
	if (exp <= 020001 || exp >= 060000)
	    OUT_IEEE_NAN;
#endif
	exp += 1023 - 040000 - 1;
	if ((exp & ~0x7FF) != 0) {
	    fprintf(stderr, "htond:  Cray exponent too large on x%x\n", word);
	    OUT_IEEE_NAN;
	}

#if defined(CRAY2) && defined(ROUNDING)
	/* Cray-2 seems to round down, XMP rounds up */
	word += 1;
#endif
	word = ((word & 0x00007FFFFFFFFFFFL) << (15-11+1)) |
	    signbit | (((long)exp)<<(64-12));

	*out++ = word>>56;
	*out++ = word>>48;
	*out++ = word>>40;
	*out++ = word>>32;
	*out++ = word>>24;
	*out++ = word>>16;
	*out++ = word>>8;
	*out++ = word;
    }
    return;
#endif
#if defined(convex_NATIVE) || defined(__convex__NATIVE)
    /*
     * Convex C1 version, for Native Convex floating point.
     * (Which seems to be VAX "G" format -- almost IEEE).
     * CC_OPTS = -fn to get this.
     * In modern times, Convex seems to use IEEE by default,
     * so we do too.
     */
    for (i=count; i > 0; i--) {
	register unsigned long long word;
	register int exp;


	word = *((unsigned long long *)in);
	in += 8;

	if (word == 0)
	    OUT_IEEE_ZERO;
	exp = (word >> 52) & 0x7FF;
	/* What value here is a Convex NaN ? */
	exp += 1023 - 1024 - 1;
	if ((exp & ~0x7FF) != 0) {
	    fprintf(stderr, "htond:  Convex exponent too large on x%lx\n", word);
	    OUT_IEEE_NAN;
	}

	word = ((word & 0x800FFFFFFFFFFFFFLL) |
		((long long)exp)<<52);

	*((unsigned long long *)out) = word;
	out += 8;
    }
    return;
#endif

    bu_bomb("htond.c:  ERROR, no HtoND conversion for this machine type\n");
}


void
ntohd(register unsigned char *out, register const unsigned char *in, size_t count)
{
    register size_t i;
    bu_endian_t order;

    assert(sizeof(double) == SIZEOF_NETWORK_DOUBLE);

    order = bu_byteorder();

    switch (order) {
	case BU_BIG_ENDIAN:
	    /*
	     * First, the case where the system already operates in
	     * IEEE format internally, using big-endian order.  These
	     * are the lucky ones.
	     */
	    memcpy(out, in, count*SIZEOF_NETWORK_DOUBLE);
	    return;
	case BU_LITTLE_ENDIAN:
	    /*
	     * This machine uses IEEE, but in little-endian byte order
	     */
	    for (i=count; i > 0; i--) {
		*out++ = in[7];
		*out++ = in[6];
		*out++ = in[5];
		*out++ = in[4];
		*out++ = in[3];
		*out++ = in[2];
		*out++ = in[1];
		*out++ = in[0];
		in += SIZEOF_NETWORK_DOUBLE;
	    }
	    return;
	default:
	    /* nada */
	    break;
    }

    /* Now, for the machine-specific stuff.
     *
     * XXX - Note that the below will not likely work without defining
     * a corresponding pre-processor flag.
     */

#if defined(sgi) && !defined(mips)
    /*
     * Silicon Graphics Iris workstation.
     * See comments in htond() for discussion of the braindamage.
     */
    for (i=count; i > 0; i--) {
	/* Brain-damaged 3-D case */
	float small;
	long float big;
	register unsigned char *fp = (unsigned char *)&big;
	*fp++ = *in++;
	*fp++ = *in++;
	*fp++ = *in++;
	*fp++ = *in++;
	*fp++ = *in++;
	*fp++ = *in++;
	*fp++ = *in++;
	*fp++ = *in++;
	small = big;		/* H/W cvt to IEEE double */
	fp = (unsigned char *)&small;
	*out++ = *fp++;
	*out++ = *fp++;
	*out++ = *fp++;
	*out++ = *fp++;
    }
    return;
#endif
#if defined(vax)
    /*
     * Digital Equipment's VAX.
     * VAX order is +6, +4, +2, sign|exp|fraction+0
     * with 8 bits of exponent, excess 128 base 2, exp=0 => zero.
     */
    for (i=count; i > 0; i--) {
	register unsigned long left, right, signbit;
	register int fix, exp;

	left  = (in[0]<<24) | (in[1]<<16) | (in[2]<<8) | in[3];
	right = (in[4]<<24) | (in[5]<<16) | (in[6]<<8) | in[7];
	in += 8;

	exp = (left >> 20) & 0x7FF;
	signbit = left & 0x80000000;
	if (exp == 0) {
	    *out++ = 0;		/* VAX zero */
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    continue;
	} else if (exp == 0x7FF) {
	vax_undef:
	    *out++ = 0x80;		/* VAX "undefined" */
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    continue;
	}
	exp += 129 - 1023;
	/* Check for exponent out of range */
	if ((exp & ~0xFF) != 0) {
	    fprintf(stderr, "ntohd: VAX exponent overflow\n");
	    goto vax_undef;
	}
	left = ((left & 0x000FFFFF)<<3) | signbit | (exp<<23) |
	    (right >> (32-3));
	right <<= 3;
	out[1] = left>>24;
	out[0] = left>>16;
	out[3] = left>>8;
	out[2] = left;
	out[5] = right>>24;
	out[4] = right>>16;
	out[7] = right>>8;
	out[6] = right;
	out += 8;
    }
    return;
#endif
#if defined(ibm) || defined(gould)
    /*
     * IBM Format.
     * 7-bit exponent, base 16.
     * No hidden bits in mantissa (56 bits).
     */
    for (i=count; i > 0; i--) {
	register unsigned long left, right;
	register int fix, exp, signbit;

	left  = (in[0]<<24) | (in[1]<<16) | (in[2]<<8) | in[3];
	right = (in[4]<<24) | (in[5]<<16) | (in[6]<<8) | in[7];
	in += 8;

	exp = ((left >> 20) & 0x7FF);
	signbit = (left & 0x80000000) >> 24;
	if (exp == 0 || exp == 0x7FF) {
	ibm_undef:
	    *out++ = 0;		/* IBM zero.  No NAN */
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    *out++ = 0;
	    continue;
	}

	left = (left & 0x000FFFFF) | 0x00100000;/* replace "hidden" bit */

	exp += 129 - 1023 -1;	/* fudge, to make /4 and %4 work */
	fix = exp % 4;		/* 2^4 == 16^1;  get fractional exp */
	exp /= 4;		/* excess 32, base 16 */
	exp += (64-32+1);	/* excess 64, base 16, plus fudge */
	if ((exp & ~0xFF) != 0) {
	    fprintf(stderr, "ntohd:  IBM exponent overflow\n");
	    goto ibm_undef;
	}

	if (fix) {
	    left = (left<<fix) | (right >> (32-fix));
	    right <<= fix;
	}

	if (signbit) {
	    /* The IBM actually uses complimented mantissa
	     * and exponent.
	     */
	    left  ^= 0xFFFFFFFF;
	    right ^= 0xFFFFFFFF;
	    if (right & 0x80000000) {
		/* There may be a carry */
		right += 1;
		if ((right & 0x80000000) == 0) {
		    /* There WAS a carry */
		    left += 1;
		}
	    } else {
		/* There will be no carry to worry about */
		right += 1;
	    }
	    left &= 0x00FFFFFF;
	    exp = (~exp) & 0x7F;
	}


	/* Not actually required, but for comparison purposes,
	 * normalize the number.  Remove for production speed.
	 */
	while ((left & 0x00F00000) == 0 && left != 0) {
	    if (signbit && exp <= 0x41) break;

	    left = (left << 4) | (right >> (32-4));
	    right <<= 4;
	    if (signbit) exp--;
	    else exp++;
	}

	*out++ = signbit | exp;
	*out++ = left>>16;
	*out++ = left>>8;
	*out++ = left;
	*out++ = right>>24;
	*out++ = right>>16;
	*out++ = right>>8;
	*out++ = right;
    }
    return;
#endif
#if defined(CRAY1) || defined(CRAY2) || defined(eta10)
    /*
     * Cray version.  Somewhat easier using 64-bit registers.  15 bit
     * exponent, biased 040000 (octal).  48 mantissa bits.  No hidden
     * bits.
     */
    for (i=count; i > 0; i--) {
	register unsigned long word, signbit;
	register int exp;

	word  = (((long)in[0])<<56) | (((long)in[1])<<48) |
	    (((long)in[2])<<40) | (((long)in[3])<<32) |
	    (((long)in[4])<<24) | (((long)in[5])<<16) |
	    (((long)in[6])<<8) | ((long)in[7]);
	in += 8;

	exp = (word>>(64-12)) & 0x7FF;
	signbit = word & 0x8000000000000000L;
	if (exp == 0) {
	    word = 0;
	    goto cray_out;
	}
	if (exp == 0x7FF) {
	    word = 067777L<<48;	/* Cray out of range */
	    goto cray_out;
	}
	exp += 040000 - 1023 + 1;
	word = ((word & 0x000FFFFFFFFFFFFFL) >> (15-11+1)) |
	    0x0000800000000000L | signbit |
	    (((long)exp)<<(64-16));

    cray_out:
	*out++ = word>>56;
	*out++ = word>>48;
	*out++ = word>>40;
	*out++ = word>>32;
	*out++ = word>>24;
	*out++ = word>>16;
	*out++ = word>>8;
	*out++ = word;
    }
    return;
#endif
#if defined(convex_NATIVE) || defined(__convex__NATIVE)
    /*
     * Convex C1 version, for Native Convex floating point.
     */
    for (i=count; i > 0; i--) {
	register unsigned long long word;
	register int exp;

	word = *((unsigned long long *)in);
	in += 8;

	exp = (word >> 52) & 0x7FF;
	if (exp == 0) {
	    word = 0;
	    goto convex_out;
	}
	if (exp == 0x7FF) {
	    /* IEEE NaN = Convex what? */
	    fprintf(stderr, "ntohd: Convex NaN unimplemented\n");
	    word = 0;
	    goto convex_out;
	}
	exp += 1024 - 1023 + 1;
	word = (word & 0x800FFFFFFFFFFFFFLL) |
	    (((long long)exp)<<52);

    convex_out:
	*((unsigned long long *)out) = word;
	out += 8;
    }
    return;
#endif

    bu_bomb("ntohd.c:  ERROR, no NtoHD conversion for this machine type\n");
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
