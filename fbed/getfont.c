/* 
 * getfont.c - get a file name for a font
 * 
 * Author:	Paul R. Stay
 * 		Ballistics Research Labratory
 * 		APG, Md.
 * Date:	Fri Jan 11 1985
 */
#include <stdio.h>
#include <fb.h>
#include "font.h"

/*	g e t f o n t ( )
	Copy the font name and determine if you need to put a path variable
	on it so it can load correctly.
 */
getfont()
	{
	static char	fname[128];

	if( fontname[0] == '/' )		/* absolute path */
		return	loadfont( fontname );

	(void) sprintf( fname, "/usr/lib/vfont/%s", fontname );
	return	loadfont( fname );
	}
