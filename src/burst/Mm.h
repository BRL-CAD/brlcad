/*                            M M . H
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file Mm.h
	Author:	Gary S. Moss
		U. S. Army Ballistic Research Laboratory
		Aberdeen Proving Ground
		Maryland 21005-5066

	$Header$ (BRL)
 */
/* Emulate MUVES Mm package using malloc. */
#if __STDC__
#include <stdlib.h>
#include <string.h>
#else
extern char	*malloc();
extern char	*strcpy();
#endif
#define MmAllo( typ )		(typ *) malloc( sizeof(typ) )
#define MmFree( typ, ptr )	free( (char *) ptr )
#define MmVAllo( ct, typ )	(typ *) malloc( (ct)*sizeof(typ) )
#define MmVFree( ct, typ, ptr )	free( (char *) ptr )
#define MmStrDup( str )		strcpy( malloc( strlen(str)+1 ), str )
#define MmStrFree( str )	free( str )

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
