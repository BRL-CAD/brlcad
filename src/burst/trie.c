/*                          T R I E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2009 United States Government as represented by
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
 *
 */
/** @file trie.c
 *	Author:		Gary S. Moss
 */

#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <signal.h>

#include "bu.h"

#include "./burst.h"
#include "./trie.h"
#include "./ascii.h"
#include "./extern.h"


static Func *matchTrie();

/*
  Trie *addTrie( char *name, Trie **triepp )

  Insert the name in the trie specified by triepp, if it
  doesn't already exist there.
  Return pointer to leaf node associated with name.
*/
Trie	*
addTrie( name, triepp )
    char *name;
    Trie **triepp;
{
    Trie *curp;
    if ( *name == NUL )
    {
	/* End of name, see if name already exists. */
	if ( *triepp == TRIE_NULL )
	{
	    /* Name does not exist, make leaf node. */
	    NewTrie( *triepp );
	    (*triepp)->l.t_altr = (*triepp)->l.t_next
		= TRIE_NULL;
	    (*triepp)->l.t_func = NULL_FUNC;
	}
	else
	    if ( (*triepp)->n.t_next != TRIE_NULL )
		/* Name is subset of another name. */
		return	addTrie( name, &(*triepp)->n.t_altr );
	/* else -- Name already inserted, so do nothing. */
	return	*triepp;
    }
    /* Find matching letter, this level.  */
    for (	curp = *triepp;
		curp != TRIE_NULL && *name != curp->n.t_char;
		curp = curp->n.t_altr
	)
	;
    if ( curp == TRIE_NULL )
    {
	/* No Match, this level, so create new alternate. */
	curp = *triepp;
	NewTrie( *triepp );
	(*triepp)->n.t_altr = curp;
	(*triepp)->n.t_char = *name;
	(*triepp)->n.t_next = TRIE_NULL;
	return	addTrie( ++name, &(*triepp)->n.t_next );
    }
    else
	/* Found matching character. */
	return	addTrie( ++name, &curp->n.t_next );
}

Func	*
getTrie( name, triep )
    char	*name;
    Trie	*triep;
{
    Trie *curp = NULL;
    assert( triep != TRIE_NULL );

    /* Traverse next links to end of region name. */
    for (; triep != TRIE_NULL; triep = triep->n.t_next )
    {
	curp = triep;
	if ( *name == NUL )
	{
	    /* End of user-typed name. */
	    if ( triep->n.t_altr != TRIE_NULL )
		/* Ambiguous at this point. */
		return	NULL_FUNC;
	    else	  /* Complete next character. */
	    {
		*name++ = triep->n.t_char;
		*name = NUL;
	    }
	}
	else
	    if ( *name == '*' )
		return	matchTrie( triep );
	    else	/* Not at end of user-typed name yet, traverse
			   alternate list to find current letter.
			*/
	    {
		for (	;
			triep != TRIE_NULL
			    &&	*name != triep->n.t_char;
			triep = triep->n.t_altr
		    )
		    ;
		if ( triep == TRIE_NULL )
		    /* Non-existant name, truncate bad part. */
		{
		    *name = NUL;
		    return	NULL_FUNC;
		}
		else
		    name++;
	    }
    }
    /* Clobber key-stroke, and return it. */
    --name;
    *name = NUL;
    assert( curp != TRIE_NULL );
    return	curp->l.t_func;
}

#define MAX_TRIE_LEVEL	(32*16)

static Func	*
matchTrie( triep )
    Trie	*triep;
{
    Func	*func;
    if ( triep == TRIE_NULL )
	func = NULL_FUNC;
    else
	if ( triep->n.t_altr != TRIE_NULL )
	    func = NULL_FUNC;	/* Ambiguous root, no match.  */
	else
	    if ( triep->n.t_next == TRIE_NULL )
		func = triep->l.t_func;	/* At leaf node, return datum.  */
	    else				/* Keep going to leaf.  */
		func = matchTrie( triep->n.t_next );
    return	func;
}

void
prntTrie( triep, level )
    Trie	*triep;
    int	level;
{
    Trie	*tp = triep;
    static char	name_buf[MAX_TRIE_LEVEL+1], *namep;
#if DEBUG_TRIE
    brst_log( "prntTrie(triep=0x%x, level=%d)\n", triep, level );
#endif
    if ( tp == TRIE_NULL )
	return;
    if ( tp->n.t_altr != TRIE_NULL )
	prntTrie( tp->n.t_altr, level );
    if ( level == 0 )
	namep = name_buf;
    *namep = tp->n.t_char;
    if ( tp->n.t_next == TRIE_NULL )
    {
	/* At end of name, so print it out. */
	*namep = NUL;
	brst_log( "%s\n", name_buf );
    }
    else
    {
	namep++;
	prntTrie( tp->n.t_next, level+1 );
	namep--;
    }
    return;
}

int
writeTrie( triep, level, fp )
    Trie	*triep;
    int	level;
    FILE	*fp;
{
    Trie	*tp = triep;
    static char	name_buf[MAX_TRIE_LEVEL+1], *namep;
    if ( tp == TRIE_NULL )
	return	1;
    if ( tp->n.t_altr != TRIE_NULL )
	(void) writeTrie( tp->n.t_altr, level, fp );
    if ( level == 0 )
	namep = name_buf;
    *namep = tp->n.t_char;
    if ( tp->n.t_next == TRIE_NULL )
    {
	/* At end of name, so print it out. */
	*namep = NUL;
	(void) fprintf(	fp, "%s\n", name_buf );
    }
    else
    {
	namep++;
	(void) writeTrie( tp->n.t_next, level+1, fp );
	namep--;
    }
    return	1;
}

int
readTrie( fp, triepp )
    FILE	*fp;
    Trie	**triepp;
{
    static char	name_buf[MAX_TRIE_LEVEL+1];
    while ( bu_fgets( name_buf, MAX_TRIE_LEVEL, fp ) != NULL )
    {
	name_buf[strlen(name_buf)-1] = '\0'; /* Clobber new-line. */
	(void) addTrie( name_buf, triepp );
    }
    return	1;
}

void
ring_Bell()
{
    (void) putchar( BEL );
    return;
}

char	*
char_To_String( i )
    int	i;
{
    static char	buf[4];
    if ( i >= SP && i < DEL )
    {
	buf[0] = i;
	buf[1] = NUL;
    }
    else
	if ( i >= NUL && i < SP )
	{
	    buf[0] = '^';
	    buf[1] = i + 64;
	    buf[2] = NUL;
	}
	else
	    if ( i == DEL )
		return	"DL";
	    else
		return	"EOF";
    return	buf;
}

#if false
/*
  Func *getFuncNm( char *inbuf, int bufsz, char *msg, Trie **triepp )
  TENEX-style name completion.
  Returns a function pointer.
*/
Func	*
getFuncNm( inbuf, bufsz, msg, triepp )
    char	 *inbuf;
    int	 bufsz;
    char	*msg;
    Trie	**triepp;
{
    static char	buffer[BUFSIZ];
    char	*p = buffer;
    int	c;
    Func		*funcp;
    if ( tty )
    {
	save_Tty( 0 );
	set_Raw( 0 );
	clr_Echo( 0 );
    }
    prompt( msg );
    *p = NUL;
    do
    {
	(void) fflush( stdout );
	c = hm_getchar();
	switch ( c )
	{
	    case SP :
	    {
		if (	*triepp == TRIE_NULL
			||	(funcp = getTrie( buffer, *triepp ))
			== NULL_FUNC
		    )
		    (void) putchar( BEL );
		for (; p > buffer; p-- )
		    (void) putchar( BS );
		(void) printf( "%s", buffer );
		(void) ClrEOL();
		(void) fflush( stdout );
		p += strlen( buffer );
		break;
	    }
	    case Ctrl('A') : /* Cursor to beginning of line. */
		if ( p == buffer )
		{
		    ring_Bell();
		    break;
		}
		for (; p > buffer; p-- )
		    (void) putchar( BS );
		break;
	    case Ctrl('B') :
	    case BS : /* Move cursor back one character. */
		if ( p == buffer )
		{
		    ring_Bell();
		    break;
		}
		(void) putchar( BS );
		--p;
		break;
	    case Ctrl('D') : /* Delete character under cursor. */
	    {
		char	*q = p;
		if ( *p == NUL )
		{
		    ring_Bell();
		    break;
		}
		for (; *q != NUL; ++q )
		{
		    *q = *(q+1);
		    (void) putchar( *q != NUL ? *q : SP );
		}
		for (; q > p; --q )
		    (void) putchar( BS );
		break;
	    }
	    case Ctrl('E') : /* Cursor to end of line. */
		if ( *p == NUL )
		{
		    ring_Bell();
		    break;
		}
		(void) printf( "%s", p );
		p += strlen( p );
		break;
	    case Ctrl('F') : /* Cursor forward one character. */
		if ( *p == NUL || p-buffer >= bufsz-2 )
		{
		    ring_Bell();
		    break;
		}
		(void) putchar( *p++ );
		break;
	    case Ctrl('G') : /* Abort input. */
		ring_Bell();
		notify( "Aborted.", NOTIFY_APPEND );
		goto	clean_return;
	    case Ctrl('K') : /* Erase from cursor to end of line. */
		if ( *p == NUL )
		{
		    ring_Bell();
		    break;
		}
		ClrEOL();
		*p = NUL;
		break;
	    case Ctrl('P') : /* Yank previous contents of "inbuf". */
	    {
		int	len = strlen( inbuf );
		if ( (p + len) - buffer >= BUFSIZ )
		{
		    ring_Bell();
		    break;
		}
		bu_strlcpy( p, inbuf, bufsz );
		printf( "%s", p );
		p += len;
		break;
	    }
	    case Ctrl('U') : /* Erase from start of line to cursor. */
		if ( p == buffer )
		{
		    ring_Bell();
		    break;
		}
		for (; p > buffer; --p )
		{
		    char	*q = p;
		    (void) putchar( BS );
		    for (; *(q-1) != NUL; ++q )
		    {
			*(q-1) = *q;
			(void) putchar( *q != NUL ? *q : SP );
		    }
		    for (; q > p; --q )
			(void) putchar( BS );
		}
		break;
	    case Ctrl('R') : /* Print line, cursor doesn't move. */
	    {
		int	i;
		if ( buffer[0] == NUL )
		    break;
		for ( i = p - buffer; i > 0; i-- )
		    (void) putchar( BS );
		(void) printf( "%s", buffer );
		for ( i = strlen( buffer ) - (p - buffer); i > 0; i-- )
		    (void) putchar( BS );
		break;
	    }
	    case DEL : /* Delete character behind cursor. */
	    {
		char	*q = p;
		if ( p == buffer )
		{
		    ring_Bell();
		    break;
		}
		(void) putchar( BS );
		for (; *(q-1) != NUL; ++q )
		{
		    *(q-1) = *q;
		    (void) putchar( *q != NUL ? *q : SP );
		}
		for (; q > p; --q )
		    (void) putchar( BS );
		p--;
		break;
	    }
	    case CR :
	    case LF :
	    case EOF :
		if (	*triepp == TRIE_NULL
			||	(funcp = getTrie( buffer, *triepp ))
			== NULL_FUNC
		    )
		{
		    (void) putchar( BEL );
		    break;
		}
		else
		{
		    bu_strlcpy( inbuf, buffer, bufsz );
		    notify( NULL, NOTIFY_DELETE );
		    goto clean_return;
		}
	    case Ctrl('V') :
		/* Escape character, do not process next char. */
		c = hm_getchar();
		/* Fall through to default case! */
	    default : /* Insert character at cursor. */
	    {
		char	*q = p;
		int	len = strlen( p );
		/* Scroll characters forward. */
		if ( c >= NUL && c < SP )
		    (void) printf( "%s", char_To_String( c ) );
		else
		    (void) putchar( c );
		for (; len >= 0; len--, q++ )
		    (void) putchar( *q == NUL ? SP : *q );
		for (; q > p; q-- )
		{
		    (void) putchar( BS );
		    *q = *(q-1);
		}
		*p++ = c;
		break;
	    }
	} /* End switch.  */
    }
    while ( strlen( buffer ) < BUFSIZ);
    ring_Bell();
    notify( "Buffer full.", NOTIFY_APPEND );
    clean_return :
	prompt( "" );
    if ( tty )
	reset_Tty( 0 );
    return	funcp;
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
