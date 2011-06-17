/* libyywrap - flex run-time support library "yywrap" function */

/* $NetBSD: libyywrap.c,v 1.7 2003/10/27 00:12:43 lukem Exp $ */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>

int yywrap __P((void));

int
yywrap()
	{
	return 1;
	}
