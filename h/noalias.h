/*
 *			N O A L I A S . H
 *
 *  This header file is intended to be #include'ed in front of any
 *  loop that involves pointers that do not alias each other,
 *  to permit vectorizing compilers to proceed.
 *
 *  This file contains various definitions which can be safely seen
 *  by both ANSI and non-ANSI compilers and preprocessors.
 *  Absolutely NO #pragma's can go in this file;  they spoil backwards
 *  compatability with non-ANSI compilers.
 *  If this file determines that it is being processed by an ANSI
 *  compiler, or one that is known to accept #pragma, then it will #include
 *  the file "noalias-prag.h", which will contain the various
 *  vendor-specific #pragma's.
 *
 *  Authors -
 *	David Becker		Cray
 *	Michael John Muuss	BRL
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 *
 *  @(#)$Header$ (BRL)
 */
#if __STDC__
#	include "noalias-prag.h"
#else
#	if defined(convex) || defined(__convex__)
		/*$dir no_recurrence */
#	endif
#endif
