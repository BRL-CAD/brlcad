/*
	SCCS id:	@(#) sgi_dep.c	2.1
	Modified: 	12/9/86 at 15:58:22
	Retrieved: 	12/26/86 at 21:54:43
	SCCS archive:	/vld/moss/src/fbed/s.sgi_dep.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) sgi_dep.c 2.1, modified 12/9/86 at 15:58:22, archive /vld/moss/src/fbed/s.sgi_dep.c";
#endif
#ifdef sgi
#include <device.h>
void
sgi_Init()
	{
	qdevice( KEYBD );
	}

int
sgi_Getchar()
	{	short	val;
	winattach();
	while( qread( &val ) != KEYBD )
		;
	return	(int) val;
	}
#endif
