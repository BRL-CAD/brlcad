/*
 *			L I B F B - D U M M Y . C
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>

#include "machine.h"
#include "fb.h"

FBIO *
fb_open( name, w, h )
char *name;
{
	return(FBIO_NULL);
}

int
fb_close() {
	return(0);
}
