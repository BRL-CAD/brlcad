/*
 *			L I B F B - D U M M Y . C
 *
 *  Merely for the use of RT, a libfb-compatible
 *  interface library that does nothing.  Used for benchmarks, etc.
 */
#include "fb.h"

fbsetsize( size )  {}

fbopen( file, mode )
{
	return( 1 );	/* "Success" */
}

fbclose()  {}

fbclear()  {}

fb_wmap()  {}

fbzoom( xpts, ypts )  {}

fbwindow( xpts, ypts )  {}

fbwrite( x, y, p, count )
register Pixel *p;
{
}
