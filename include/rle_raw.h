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
/*@{*/
/**
 * @file rle_raw.h - Definitions for rle_getraw/rle_putraw.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Jul  7 1986
 * Copyright (c) 1986, Spencer W. Thomas
 */

#ifndef RLE_RAW_H
#define RLE_RAW_H


#ifdef __cplusplus        /* Cfront 2.0  or g++ */
#ifndef c_plusplus
#define c_plusplus        
#endif
extern "C" {
#endif

#include "rle_code.h"

/*****************************************************************
 * TAG( rle_op )
 *
 * Struct representing one rle opcode.
 */

typedef
struct rle_op {
    int opcode;			/* one of RByteDataOp or RRunDataOp */
    int xloc;			/* X location this op starts at */
    int length;			/* length of run or data */
    union a {
	rle_pixel * pixels;	/* for ByteData */
	int run_val;		/* for RunData */
    } u;
} rle_op;

#ifdef USE_PROTOTYPES
    /*****************************************************************
     * TAG( rle_raw_alloc )
     * 
     * Allocate buffer space for use by rle_getraw and rle_putraw.
     */
    extern int
    rle_raw_alloc( rle_hdr *the_hdr, rle_op ***scanp, int **nrawp );

    /*****************************************************************
     * TAG( rle_raw_free )
     *
     * Free buffer space allocated by rle_raw_alloc.
     */
    extern void rle_raw_free( rle_hdr *the_hdr, rle_op **scanp, int *nrawp );

    /*****************************************************************
     * TAG( rle_getraw )
     * 
     * Get a raw scanline from the input file.
     */
    extern unsigned int
    rle_getraw( rle_hdr *the_hdr, rle_op *scanraw[], int nraw[] );

    /*****************************************************************
     * TAG( rle_freeraw )
     * 
     * Free all the pixel arrays in the raw scan struct.
     */
    extern void
    rle_freeraw( rle_hdr * the_hdr, rle_op *scanraw[], int nraw[] );

    /*****************************************************************
     * TAG( rle_putraw )
     *
     * Put raw scanline data to the output file.
     */
    extern void
    rle_putraw( rle_op **scanraw, int *nraw, rle_hdr *the_hdr );

    /*****************************************************************
     * TAG( rle_rawtorow )
     *
     * Convert raw data to "row" type scanline data.
     */
    extern void
    rle_rawtorow( rle_hdr *the_hdr, rle_op **scanraw, int *nraw,
		  rle_pixel **outrows );
#else
    /* Return value decls only.  See above for detailed declarations. */
    /* From rle_getraw.c. */
    extern unsigned int rle_getraw();
    extern void rle_freeraw();
    /* From rle_putraw.c. */
    extern void rle_putraw();
    /* From rle_raw_alc.c. */
    extern int rle_raw_alloc();
    extern void rle_raw_free();
    /* From rle_rawrow.c. */
    extern void rle_rawtorow();

#endif

#ifdef __cplusplus
}
#endif

#endif /* RLE_RAW_H */
/*@}*/
