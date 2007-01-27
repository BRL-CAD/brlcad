/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is
 * preserved on all copies.
 *
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/** @addtogroup utahrle */
/** @{ */
/** @file colorquant.h
 *
 * Definitions for colorquant.
 *
 * @author	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 *
 * Date:	Thu Jan  3 1991
 * Copyright (c) 1991, University of Michigan
 */


/* Define values for the accum_hist argument:
 *
 * If non-zero the histogram will accumulate and reflect pixels from
 * multiple images.  When 1, the histogram will be initialized and
 * summed, but not thrown away OR processed.  When 2, the image RGB will
 * be added to it.  When 3, Boxes are cut and a colormap and rgbmap are
 * be returned, Histogram is freed too.  When zero, all code is
 * executed as per normal.
 */
#define INIT_HIST 1
#define USE_HIST 2
#define PROCESS_HIST 3

/*
 * Flag bits.
 *
 * CQ_FAST:	If set, the rgbmap will be constructed quickly.  If
 *		not set, the rgbmap will be built much slower, but
 *		more accurately.  In most cases, CQ_FAST should be
 *		set, as the error introduced by the approximation is
 *		usually small.
 * CQ_QUANTIZE:	If set, the data in red, green, and blue is not
 * 		pre-quantized, and will be quantized "on the fly".
 * 		This slows the routine slightly.  If not set, the data
 * 		has been prequantized to 'bits' significant bits.
 * CQ_NO_RGBMAP:If set, rgbmap will not be built.
 *
 */
#define CQ_FAST		1
#define CQ_QUANTIZE	2
#define CQ_NO_RGBMAP	4

/* Declare the function. */
#ifdef USE_PROTOTYPES
extern int  colorquant( unsigned char *red,
			unsigned char *green,
			unsigned char *blue,
			unsigned long pixels,
			unsigned char *colormap[3],
			int colors,
			int bits,
			unsigned char *rgbmap,
			int flags,
			int accum_hist );
#else
extern int colorquant();
#endif

/** @} */
