/*
 *			B N . H
 *
 *  Header file for the BRL-CAD Numerical Computation Library, LIBBN.
 *
 *  The library provides a broad assortment of numerical algorithms
 *  and computational routines, including random number generation,
 *  vector math, matrix math, quaternion math, complex math,
 *  synthetic division, root finding, etc.
 *
 *  This header file depends on vmath.h
 *  This header file depends on bu.h and LIBBU
 *  ??Should complex.h and plane.h and polyno.h get absorbed in here??
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 *
 *  Include Sequencing -
 *	#include "conf.h"
 *	#include <stdio.h>
 *	#include <math.h>
 *	#include "machine.h"	/_* For fastf_t definition on this machine *_/
 *	#include "bu.h"
 *	#include "vmath.h"
 *	#include "bn.h"
 *
 *  Libraries Used -
 *	-lm -lc
 *
 *  $Header$
 */

#ifndef SEEN_RTLIST_H
# include "rtlist.h"
#endif


#ifndef SEEN_COMPAT4_H
# include "compat4.h"
#endif

#ifndef SEEN_BN_H
#define SEEN_BN_H seen
#ifdef __cplusplus
extern "C" {
#endif

#define BN_H_VERSION	"@(#)$Header$ (BRL)"





#ifdef __cplusplus
}
#endif
#endif /* SEEN_BN_H */
