/*
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
