/*                            C V . H
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

#ifndef BU_CV_H
#define BU_CV_H

#include "common.h"

#include <stddef.h> /* for size_t */
#include <limits.h> /* for CHAR_MAX */

#include "bu/defines.h"

__BEGIN_DECLS

/*----------------------------------------------------------------------*/

/** @addtogroup conv */
/** @{*/

/**
 * Sizes of "network" format data.  We use the same convention as the
 * TCP/IP specification, namely, big-Endian, IEEE format, twos
 * complement.  This is the BRL-CAD external data representation
 * (XDR).  See also the support routines in libbu/xdr.c
 *
 */
#define SIZEOF_NETWORK_SHORT  2	/* htons(), bu_gshort(), bu_pshort() */
#define SIZEOF_NETWORK_LONG   4	/* htonl(), bu_glong(), bu_plong() */
#define SIZEOF_NETWORK_FLOAT  4	/* htonf() */
#define SIZEOF_NETWORK_DOUBLE 8	/* htond() */

/**
 * provide for 64-bit network/host conversions using ntohl()
 */
#ifndef HAVE_NTOHLL
#  define ntohll(_val) ((bu_byteorder() == BU_LITTLE_ENDIAN) ?				\
			((((uint64_t)ntohl((_val))) << 32) + ntohl((_val) >> 32)) : \
			(_val)) /* sorry pdp-endian */
#endif
#ifndef HAVE_HTONLL
#  define htonll(_val) ntohll(_val)
#endif


#define CV_CHANNEL_MASK 0x00ff
#define CV_HOST_MASK    0x0100
#define CV_SIGNED_MASK  0x0200
#define CV_TYPE_MASK    0x1c00  /* 0001 1100 0000 0000 */
#define CV_CONVERT_MASK 0x6000  /* 0110 0000 0000 0000 */

#define CV_TYPE_SHIFT    10
#define CV_CONVERT_SHIFT 13

#define CV_8  0x0400
#define CV_16 0x0800
#define CV_32 0x0c00
#define CV_64 0x1000
#define CV_D  0x1400

#define CV_CLIP   0x0000
#define CV_NORMAL 0x2000
#define CV_LIT    0x4000

/** deprecated */
#define END_NOTSET 0
#define END_BIG    1	/* PowerPC/MIPS */
#define END_LITTLE 2	/* Intel */
#define END_ILL    3	/* PDP-11 */
#define END_CRAY   4	/* Old Cray */

/** deprecated */
#define IND_NOTSET 0
#define IND_BIG    1
#define IND_LITTLE 2
#define IND_ILL    3
#define IND_CRAY   4


/** @file libbu/convert.c
 *
 * Routines to translate data formats.  The data formats are:
 *
 * \li Host/Network		is the data in host format or local format
 * \li signed/unsigned		Is the data signed?
 * \li char/short/int/long/double
 *				Is the data 8bits, 16bits, 32bits, 64bits
 *				or a double?
 *
 * The method of conversion is to convert up to double then back down the
 * the expected output format.
 *
 */

/**
 * convert from one format to another.
 *
 * @param in		input pointer
 * @param out		output pointer
 * @param count		number of entries to convert
 * @param size		size of output buffer
 * @param infmt		input format
 * @param outfmt	output format
 *
 */
BU_EXPORT extern size_t bu_cv(genptr_t out, char *outfmt, size_t size, genptr_t in, char *infmt, size_t count);

/**
 * Sets a bit vector after parsing an input string.
 *
 * Set up the conversion tables/flags for vert.
 *
 * @param in	format description.
 *
 * @return a 32 bit vector.
 *
 * Format description:
 * [channels][h|n][s|u] c|s|i|l|d|8|16|32|64 [N|C|L]
 *
 * @n channels must be null or 1
 * @n Host | Network
 * @n signed | unsigned
 * @n char | short | integer | long | double | number of bits of integer
 * @n Normalize | Clip | low-order
 */
BU_EXPORT extern int bu_cv_cookie(const char *in);

/**
 * It is always more efficient to handle host data, rather than
 * network.  If host and network formats are the same, and the request
 * was for network format, modify the cookie to request host format.
 */
BU_EXPORT extern int bu_cv_optimize(int cookie);

/**
 * Returns the number of bytes each "item" of type "cookie" occupies.
 */
BU_EXPORT extern size_t bu_cv_itemlen(int cookie);

/**
 * convert with cookie
 *
 * @param in		input pointer
 * @param incookie	input format cookie.
 * @param count		number of entries to convert.
 * @param out		output pointer.
 * @param outcookie	output format cookie.
 * @param size		size of output buffer in bytes;
 *
 *
 * A worst case would be:	ns16 on vax to ns32
 @code
	ns16 	-> hs16
		-> hd
		-> hs32
		-> ns32
 @endcode
 * The worst case is probably the easiest to deal with because all
 * steps are done.  The more difficult cases are when only a subset of
 * steps need to be done.
 *
 * @par Method:
 @code
	HOSTDBL defined as true or false
	if ! hostother then
		hostother = (Endian == END_BIG) ? SAME : DIFFERENT;
	fi
	if (infmt == double) then
		if (HOSTDBL == SAME) {
			inIsHost = host;
		fi
	else
		if (hostother == SAME) {
			inIsHost = host;
		fi
	fi
	if (outfmt == double) then
		if (HOSTDBL == SAME) {
			outIsHost == host;
	else
		if (hostother == SAME) {
			outIsHost = host;
		fi
	fi
	if (infmt == outfmt) {
		if (inIsHost == outIsHost) {
			copy(in, out)
			exit
		else if (inIsHost == net) {
			ntoh?(in, out);
			exit
		else
			hton?(in, out);
			exit
		fi
	fi

	while not done {
		from = in;

		if (inIsHost == net) {
			ntoh?(from, t1);
			from = t1;
		fi
		if (infmt != double) {
			if (outIsHost == host) {
				to = out;
			else
				to = t2;
			fi
			castdbl(from, to);
			from = to;
		fi

		if (outfmt == double) {
			if (outIsHost == net) {
				hton?(from, out);
			fi
		else
			if (outIsHost == host) {
				dblcast(from, out);
			else
				dblcast(from, t3);
				hton?(t3, out);
			fi
		fi
	done
 @endcode
 */
BU_EXPORT extern size_t bu_cv_w_cookie(genptr_t out, int outcookie, size_t size, genptr_t in, int incookie, size_t count);

/**
 * bu_cv_ntohss
 *
 * @brief
 * Network TO Host Signed Short
 *
 * It is assumed that this routine will only be called if there is
 * real work to do.  Ntohs does no checking to see if it is reasonable
 * to do any conversions.
 *
 * @param in	generic pointer for input.
 * @param count	number of shorts to be generated.
 * @param out	short pointer for output
 * @param size	number of bytes of space reserved for out.
 *
 * @return	number of conversions done.
 */
BU_EXPORT extern size_t bu_cv_ntohss(signed short *in, /* FIXME: in/out right? */
				     size_t count,
				     genptr_t out,
				     size_t size);
BU_EXPORT extern size_t bu_cv_ntohus(unsigned short *,
				     size_t,
				     genptr_t,
				     size_t);
BU_EXPORT extern size_t bu_cv_ntohsl(signed long int *,
				     size_t,
				     genptr_t,
				     size_t);
BU_EXPORT extern size_t bu_cv_ntohul(unsigned long int *,
				     size_t,
				     genptr_t,
				     size_t);
BU_EXPORT extern size_t bu_cv_htonss(genptr_t,
				     size_t,
				     signed short *,
				     size_t);
BU_EXPORT extern size_t bu_cv_htonus(genptr_t,
				     size_t,
				     unsigned short *,
				     size_t);
BU_EXPORT extern size_t bu_cv_htonsl(genptr_t,
				     size_t,
				     long *,
				     size_t);
BU_EXPORT extern size_t bu_cv_htonul(genptr_t,
				     size_t,
				     unsigned long *,
				     size_t);

/** @file libbu/htond.c
 *
 * Convert doubles to host/network format.
 *
 * Library routines for conversion between the local host 64-bit
 * ("double precision") representation, and 64-bit IEEE double
 * precision representation, in "network order", i.e., big-endian, the
 * MSB in byte [0], on the left.
 *
 * As a quick review, the IEEE double precision format is as follows:
 * sign bit, 11 bits of exponent (bias 1023), and 52 bits of mantissa,
 * with a hidden leading one (0.1 binary).
 *
 * When the exponent is 0, IEEE defines a "denormalized number", which
 * is not supported here.
 *
 * When the exponent is 2047 (all bits set), and:
 *	all mantissa bits are zero,
 *	value is infinity*sign,
 *	mantissa is non-zero, and:
 *		msb of mantissa=0:  signaling NAN
 *		msb of mantissa=1:  quiet NAN
 *
 * Note that neither the input or output buffers need be word aligned,
 * for greatest flexibility in converting data, even though this
 * imposes a speed penalty here.
 *
 * These subroutines operate on a sequential block of numbers, to save
 * on subroutine linkage execution costs, and to allow some hope for
 * vectorization.
 *
 * On brain-damaged machines like the SGI 3-D, where type "double"
 * allocates only 4 bytes of space, these routines *still* return 8
 * bytes in the IEEE buffer.
 *
 */

/**
 * Host to Network Doubles
 */
BU_EXPORT extern void bu_cv_htond(unsigned char *out,
				  const unsigned char *in,
				  size_t count);

/**
 * Network to Host Doubles
 */
BU_EXPORT extern void bu_cv_ntohd(unsigned char *out,
				  const unsigned char *in,
				  size_t count);

/** @file libbu/htonf.c
 *
 * convert floats to host/network format
 *
 * Host to Network Floats  +  Network to Host Floats.
 *
 */

/**
 * Host to Network Floats
 */
BU_EXPORT extern void bu_cv_htonf(unsigned char *out,
				  const unsigned char *in,
				  size_t count);

/**
 * Network to Host Floats
 */
BU_EXPORT extern void bu_cv_ntohf(unsigned char *out,
				  const unsigned char *in,
				  size_t count);


/** @file libbu/b64.c
 *
 * A base64 encoding algorithm
 * For details, see http://sourceforge.net/projects/libb64
 *
 * Caller is responsible for freeing memory allocated to
 * hold output buffer.
 */
BU_EXPORT extern char *bu_b64_encode(const char *input);

BU_EXPORT extern char *bu_b64_encode_block(const char* input, int length_in);

BU_EXPORT extern int bu_b64_decode(char **output_buffer, const char *input);

BU_EXPORT extern int bu_b64_decode_block(char **output_buffer, const char* input, int length_in);

/** @} */

/* TODO - make a "deprecated.h" file to stuff deprecated things in */
/** @addtogroup hton */
/** @{ */
/** @file libbu/htester.c
 *
 * @brief
 * Test network float conversion.
 *
 * Expected to be used in pipes, or with TTCP links to other machines,
 * or with files RCP'ed between machines.
 *
 */

/** @file libbu/xdr.c
 *
 * DEPRECATED.
 *
 * Routines to implement an external data representation (XDR)
 * compatible with the usual InterNet standards, e.g.:
 * big-endian, twos-compliment fixed point, and IEEE floating point.
 *
 * Routines to insert/extract short/long's into char arrays,
 * independent of machine byte order and word-alignment.
 * Uses encoding compatible with routines found in libpkg,
 * and BSD system routines htonl(), htons(), ntohl(), ntohs().
 *
 */

/**
 * DEPRECATED: use ntohll()
 * Macro version of library routine bu_glonglong()
 * The argument is expected to be of type "unsigned char *"
 */
#define BU_GLONGLONG(_cp)	\
    ((((uint64_t)((_cp)[0])) << 56) |	\
     (((uint64_t)((_cp)[1])) << 48) |	\
     (((uint64_t)((_cp)[2])) << 40) |	\
     (((uint64_t)((_cp)[3])) << 32) |	\
     (((uint64_t)((_cp)[4])) << 24) |	\
     (((uint64_t)((_cp)[5])) << 16) |	\
     (((uint64_t)((_cp)[6])) <<  8) |	\
     ((uint64_t)((_cp)[7])))
/**
 * DEPRECATED: use ntohl()
 * Macro version of library routine bu_glong()
 * The argument is expected to be of type "unsigned char *"
 */
#define BU_GLONG(_cp)	\
    ((((uint32_t)((_cp)[0])) << 24) |	\
     (((uint32_t)((_cp)[1])) << 16) |	\
     (((uint32_t)((_cp)[2])) <<  8) |	\
     ((uint32_t)((_cp)[3])))
/**
 * DEPRECATED: use ntohs()
 * Macro version of library routine bu_gshort()
 * The argument is expected to be of type "unsigned char *"
 */
#define BU_GSHORT(_cp)	\
    ((((uint16_t)((_cp)[0])) << 8) | \
     (_cp)[1])

/**
 * DEPRECATED: use ntohs()
 */
DEPRECATED BU_EXPORT extern uint16_t bu_gshort(const unsigned char *msgp);

/**
 * DEPRECATED: use ntohl()
 */
DEPRECATED BU_EXPORT extern uint32_t bu_glong(const unsigned char *msgp);

/**
 * DEPRECATED: use htons()
 */
DEPRECATED BU_EXPORT extern unsigned char *bu_pshort(unsigned char *msgp, uint16_t s);

/**
 * DEPRECATED: use htonl()
 */
DEPRECATED BU_EXPORT extern unsigned char *bu_plong(unsigned char *msgp, uint32_t l);

/**
 * DEPRECATED: use htonll()
 */
DEPRECATED BU_EXPORT extern unsigned char *bu_plonglong(unsigned char *msgp, uint64_t l);

/** @} */

__END_DECLS

#endif  /* BU_CV_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
