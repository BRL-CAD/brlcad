/*
 *			S H O R T V E C T - P R A G . H
 *
 *  This header file is intended to be #include'ed by "shortvect.h",
 *  and may contain vendor-specific vectorization directives which
 *  use #pragma and other ANSI-C constructions.
 *
 *  This file will never be included when the compilation is not being
 *  processed by an ANSI-C compiler.
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
#	if defined(CRAY)
#		pragma shortloop
#	endif
#	if defined(alliant)
		/* ??? */
#	endif
#endif
