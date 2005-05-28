/*
 *	L I N E B U F . C
 *
 *	A portable way of doing setlinebuf().
 *
 */

#include "common.h"



#include <stdio.h>

#include "machine.h"

void
port_setlinebuf(FILE *fp)
{
#ifdef _WIN32
	(void) setvbuf( fp, (char *) NULL, _IOLBF, BUFSIZ );
#else

#ifdef BSD
	setlinebuf( fp );
#else
#	if defined( SYSV ) && !defined( sgi ) && !defined(CRAY2) && !defined(n16)
		(void) setvbuf( fp, (char *) NULL, _IOLBF, BUFSIZ );
#	endif
#	if defined(sgi) && defined(mips)
		if( setlinebuf( fp ) != 0 )
			perror("setlinebuf(fp)");
#	endif
#endif
#endif
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
