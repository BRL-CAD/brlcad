/*
 *			N O A L I A S - P R A G . H
 *
 *  This header file is intended to be #include'ed by "noalis.h",
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
#		pragma ivdep
#	endif
#	if defined(alliant)
#		pragma noeqvchk
#	endif
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
