/*
	SCCS id:	@(#) loadfont.c	1.14
	Modified: 	5/6/86 at 15:33:42 Gary S. Moss
	Retrieved: 	12/26/86 at 21:54:27
	SCCS archive:	/vld/moss/src/fbed/s.loadfont.c
*/
#if ! defined( lint )
static
char sccsTag[] = "@(#) loadfont.c 1.14, modified 5/6/86 at 15:33:42, archive /vld/moss/src/fbed/s.loadfont.c";
#endif
/* 
 * loadfont.c - Load a new font by reading in the header and directory.
 * 
 * Original Author:	Paul R. Stay
 * 			Ballistics Research Labratory
 * 			APG, Md.
 * Date:		Tue Jan  8 1985
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>

#include "machine.h"
#include "externs.h"
#include "fb.h"

#include "./std.h"
#include "./ascii.h"
#include "./font.h"
#include "./try.h"
#include "./extern.h"

loadfont(char *ff)
{
	FILE		*newff;
	struct header	lochdr;

	/* Open the file and read in the header information. */
	if( (newff = fopen( ff, "r" )) == NULL )
		{
		MvCursor( 1, 4 );
		prnt_Debug( "Error opening font file '%s'", ff );
		ffdes = NULL;
		return 0;
    		}
	if( ffdes != NULL )
		(void) fclose(ffdes);
	ffdes = newff;
	if( fread( (char *) &lochdr, (int) sizeof(struct header), 1, ffdes ) < 1 )
		{
		(void) fprintf( stderr, "loadfont() read failed!\n" );
		ffdes = NULL;
		return 0;
		}

	if( lochdr.magic != 0436 )
    		{
		prnt_Debug( "Not a font file: %s", ff );
		ffdes = NULL;
		return 0;
		}
	hdr = lochdr;

	/* Read in the directory for the font. */
	if( fread( (char *) dir, (int) sizeof(struct dispatch), 256, ffdes ) < 256 )
		{
		(void) fprintf( stderr, "loadfont() read failed!\n" );
		ffdes = NULL;
		return 0;
		}
	/* Addresses of characters in the file are relative to
		point in the file after the directory, so grab the
		current position.
	 */
 	offset = ftell( ffdes );
	return 1;
	}
