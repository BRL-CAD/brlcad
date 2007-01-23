/*                      G E T I N P U T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file getinput.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#if ! defined( lint )
static const char RCSid[] = "@(#) getinput.c 2.1, modified 12/9/86 at 15:56:41, archive /vld/moss/src/fbed/s.getinput.c";
#endif

#include "common.h"



#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "fb.h"

#include "./std.h"
#include "./ascii.h"
#include "./font.h"
#include "./try.h"
#include "./extern.h"

extern char *char_To_String(int i);

void
ring_Bell(void)
{
	(void) putchar( BEL );
	return;
	}

/*	g e t _ I n p u t ( )
	Get a line of input.
 */
int
get_Input(char *inbuf, int bufsz, char *msg)
{	static char buffer[BUFSIZ];
		register char *p = buffer;
		register int c;
	if( *cptr != NUL && *cptr != '@' )
		{
		for( ; *cptr != NUL && *cptr != CR && *cptr != LF; cptr++ )
			{
			if( p - buffer >= BUFSIZ )
				{
				ring_Bell();
				fb_log( "get_Input() over-ran internal buffer.\n" );
				prnt_Prompt( "" );
				buffer[BUFSIZ-1] = NUL;
				return 0;
				}
			if( *cptr != Ctrl('V') )
				*p++ = *cptr;
			else
				*p++ = *++cptr;
			}
		if( *cptr == CR || *cptr == LF )
			cptr++;
		*p = NUL;
		(void) strncpy( inbuf, buffer, bufsz );
		return 1;
		}
	else
	/* Skip over '@' and LF, which means "prompt user". */
	if( *cptr == '@' )
		cptr += 2;
	prnt_Prompt( msg );
	*p = NUL;
	do
		{
		(void) fflush( stdout );
		c = get_Char();
		if( remembering )
			{
			*macro_ptr++ = c;
			*macro_ptr = NUL;
			}
		switch( c )
			{
		case Ctrl('A') : /* Cursor to beginning of line. */
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			for( ; p > buffer; p-- )
				(void) putchar( BS );
			break;
		case Ctrl('B') :
		case BS : /* Move cursor back one character. */
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			(void) putchar( BS );
			--p;
			break;
		case Ctrl('D') : /* Delete character under cursor. */
			{	register char *q = p;
			if( *p == NUL )
				{
				ring_Bell();
				break;
				}
			for( ; *q != NUL; ++q )
				{
				*q = *(q+1);
				(void) putchar( *q != NUL ? *q : SP );
				}
			for( ; q > p; --q )
				(void) putchar( BS );
			break;
			}
		case Ctrl('E') : /* Cursor to end of line. */
			if( *p == NUL )
				{
				ring_Bell();
				break;
				}
			(void) printf( "%s", p );
			p += strlen( p );
			break;
		case Ctrl('F') : /* Cursor forward one character. */
			if( *p == NUL || p-buffer >= bufsz-2 )
				{
				ring_Bell();
				break;
				}
			putchar( *p++ );
			break;
		case Ctrl('G') : /* Abort input. */
			ring_Bell();
			fb_log( "Aborted.\n" );
			prnt_Prompt( "" );
			return 0;
		case Ctrl('K') : /* Erase from cursor to end of line. */
			if( *p == NUL )
				{
				ring_Bell();
				break;
				}
			ClrEOL();
			*p = NUL;
			break;
		case Ctrl('P') : /* Yank previous contents of "inbuf". */
			{	register int len = strlen( inbuf );
			if( (p + len) - buffer >= BUFSIZ )
				{
				ring_Bell();
				break;
				}
			(void) strncpy( p, inbuf, bufsz );
			(void) printf( "%s", p );
			p += len;
			break;
			}
		case Ctrl('U') : /* Erase from start of line to cursor. */
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			for( ; p > buffer; --p )
				{	register char *q = p;
				(void) putchar( BS );
				for( ; *(q-1) != NUL; ++q )
					{
					*(q-1) = *q;
					(void) putchar( *q != NUL ? *q : SP );
					}
				for( ; q > p; --q )
					(void) putchar( BS );
				}
			break;
		case Ctrl('R') : /* Print line, cursor doesn't move. */
			{	register int i;
			if( buffer[0] == NUL )
				break;
			for( i = p - buffer; i > 0; i-- )
				(void) putchar( BS );
			(void) printf( "%s", buffer );
			for( i = strlen( buffer ) - (p - buffer); i > 0; i-- )
				(void) putchar( BS );
			break;
			}
		case DEL : /* Delete character behind cursor. */
			{	register char *q = p;
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			(void) putchar( BS );
			for( ; *(q-1) != NUL; ++q )
				{
				*(q-1) = *q;
				(void) putchar( *q != NUL ? *q : SP );
				}
			for( ; q > p; --q )
				(void) putchar( BS );
			p--;
			break;
			}
		case CR :
		case LF :
		case EOF :
			(void) strncpy( inbuf, buffer, bufsz );
			prnt_Prompt( "" );
			return 1;
		case Ctrl('V') :
			/* Escape character, do not process next char. */
			c = get_Char();
			if( remembering )
				{
				*macro_ptr++ = c;
				*macro_ptr = NUL;
				}
			/* Fall through to default case! */
		default : /* Insert character at cursor. */
			{	register char *q = p;
				register int len = strlen( p );
			/* Print control characters as strings. */
			if( c >= NUL && c < SP )
				(void) printf( "%s", char_To_String( c ) );
			else
				(void) putchar( c );
			/* Scroll characters forward. */
			for( ; len >= 0; len--, q++ )
				(void) putchar( *q == NUL ? SP : *q );
			for( ; q > p; q-- )
				{
				(void) putchar( BS );
				*q = *(q-1);
				}
			*p++ = c;
			break;
			}
			} /* End switch. */
		}
	while( strlen( buffer ) < BUFSIZ );
	(void) strncpy( inbuf, buffer, bufsz );
	ring_Bell();
	fb_log( "Buffer full.\n" );
	prnt_Prompt( "" );
	return 0;
	}

/*	g e t _ F u n c _ N a m e ( )
	TENEX-style command completion.
 */
Func_Tab *
get_Func_Name(char *inbuf, int bufsz, char *msg)
{	extern Try	*try_rootp;
		extern Func_Tab	*get_Try(register char *name, register Try *tryp);
		static char buffer[BUFSIZ];
		register char *p = buffer;
		register int c;
		Func_Tab	*ftbl = FT_NULL;
	if( *cptr != NUL && *cptr != '@' )
		{
		for( ; *cptr != NUL && *cptr != CR && *cptr != LF; cptr++ )
			{
			if( p - buffer >= BUFSIZ )
				{
				ring_Bell();
				fb_log( "get_Func_Name() over-ran internal buffer.\n" );
				prnt_Prompt( "" );
				buffer[BUFSIZ-1] = NUL;
				return 0;
				}
			if( *cptr != Ctrl('V') )
				*p++ = *cptr;
			else
				*p++ = *++cptr;
			}
		if( *cptr == CR || *cptr == LF )
			cptr++;
		*p = NUL;
		if( (ftbl = get_Try( buffer, try_rootp )) == FT_NULL )
			(void) putchar( BEL );
		else
			(void) strncpy( inbuf, buffer, bufsz );
		return ftbl;
		}
	else
	/* Skip over '@' and LF, which means "prompt user". */
	if( *cptr == '@' )
		cptr += 2;
	prnt_Prompt( msg );
	*p = NUL;
	do
		{
		(void) fflush( stdout );
		c = get_Char();
		if( remembering )
			{
			*macro_ptr++ = c;
			*macro_ptr = NUL;
			}
		switch( c )
			{
		case SP :
			{
			if( (ftbl = get_Try( buffer, try_rootp )) == FT_NULL )
				(void) putchar( BEL );
			for( ; p > buffer; p-- )
				(void) putchar( BS );
			(void) printf( "%s", buffer );
			(void) ClrEOL();
			(void) fflush( stdout );
			p += strlen( buffer );
			break;
			}
		case Ctrl('A') : /* Cursor to beginning of line. */
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			for( ; p > buffer; p-- )
				(void) putchar( BS );
			break;
		case Ctrl('B') :
		case BS : /* Move cursor back one character. */
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			(void) putchar( BS );
			--p;
			break;
		case Ctrl('D') : /* Delete character under cursor. */
			{	register char *q = p;
			if( *p == NUL )
				{
				ring_Bell();
				break;
				}
			for( ; *q != NUL; ++q )
				{
				*q = *(q+1);
				(void) putchar( *q != NUL ? *q : SP );
				}
			for( ; q > p; --q )
				(void) putchar( BS );
			break;
			}
		case Ctrl('E') : /* Cursor to end of line. */
			if( *p == NUL )
				{
				ring_Bell();
				break;
				}
			(void) printf( "%s", p );
			p += strlen( p );
			break;
		case Ctrl('F') : /* Cursor forward one character. */
			if( *p == NUL || p-buffer >= bufsz-2 )
				{
				ring_Bell();
				break;
				}
			putchar( *p++ );
			break;
		case Ctrl('G') : /* Abort input. */
			ring_Bell();
			fb_log( "Aborted.\n" );
			prnt_Prompt( "" );
			return ftbl;
		case Ctrl('K') : /* Erase from cursor to end of line. */
			if( *p == NUL )
				{
				ring_Bell();
				break;
				}
			ClrEOL();
			*p = NUL;
			break;
		case Ctrl('P') : /* Yank previous contents of "inbuf". */
			{	register int len = strlen( inbuf );
			if( (p + len) - buffer >= BUFSIZ )
				{
				ring_Bell();
				break;
				}
			(void) strncpy( p, inbuf, bufsz );
			(void) printf( "%s", p );
			p += len;
			break;
			}
		case Ctrl('U') : /* Erase from start of line to cursor. */
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			for( ; p > buffer; --p )
				{	register char *q = p;
				(void) putchar( BS );
				for( ; *(q-1) != NUL; ++q )
					{
					*(q-1) = *q;
					(void) putchar( *q != NUL ? *q : SP );
					}
				for( ; q > p; --q )
					(void) putchar( BS );
				}
			break;
		case Ctrl('R') : /* Print line, cursor doesn't move. */
			{	register int i;
			if( buffer[0] == NUL )
				break;
			for( i = p - buffer; i > 0; i-- )
				(void) putchar( BS );
			(void) printf( "%s", buffer );
			for( i = strlen( buffer ) - (p - buffer); i > 0; i-- )
				(void) putchar( BS );
			break;
			}
		case DEL : /* Delete character behind cursor. */
			{	register char *q = p;
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			(void) putchar( BS );
			for( ; *(q-1) != NUL; ++q )
				{
				*(q-1) = *q;
				(void) putchar( *q != NUL ? *q : SP );
				}
			for( ; q > p; --q )
				(void) putchar( BS );
			p--;
			break;
			}
		case CR :
		case LF :
		case EOF :
			if( (ftbl = get_Try( buffer, try_rootp )) == FT_NULL )
				{
				(void) putchar( BEL );
				break;
				}
			else
				{
				(void) strncpy( inbuf, buffer, bufsz );
				prnt_Prompt( "" );
				prnt_Event( "" );
				return ftbl;
				}
		case Ctrl('V') :
			/* Escape character, do not process next char. */
			c = get_Char();
			if( remembering )
				{
				*macro_ptr++ = c;
				*macro_ptr = NUL;
				}
			/* Fall through to default case! */
		default : /* Insert character at cursor. */
			{	register char *q = p;
				register int len = strlen( p );
			/* Scroll characters forward. */
			if( c >= NUL && c < SP )
				(void) printf( "%s", char_To_String( c ) );
			else
				(void) putchar( c );
			for( ; len >= 0; len--, q++ )
				(void) putchar( *q == NUL ? SP : *q );
			for( ; q > p; q-- )
				{
				(void) putchar( BS );
				*q = *(q-1);
				}
			*p++ = c;
			break;
			}
			} /* End switch. */
		}
	while( strlen( buffer ) < BUFSIZ);
	ring_Bell();
	fb_log( "Buffer full.\n" );
	prnt_Prompt( "" );
	return ftbl;
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
