/* libyywrap - flex run-time support library "yywrap" function */

/* $NetBSD: libyywrap.c,v 1.7 2003/10/27 00:12:43 lukem Exp $ */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#if !defined(_WIN32) & !defined(__CYGWIN__)
int yywrap __P((void));
#endif

int
yywrap()
	{
	return 1;
	}
