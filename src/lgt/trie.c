/*                          T R I E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file trie.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"

#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./vecmath.h"
#include "./ascii.h"
#include "./tree.h"


#define NewTrie( p ) \
		if( ((p) = (Trie *) malloc( sizeof(Trie) )) == TRIE_NULL ) \
			{ \
			Malloc_Bomb(sizeof(Trie)); \
			fatal_error = TRUE; \
			return	TRIE_NULL; \
			}
#define NewOcList( p ) \
		if( ((p) = (OcList *) malloc( sizeof(OcList) )) == OCLIST_NULL ) \
			{ \
			Malloc_Bomb(sizeof(OcList)); \
			fatal_error = TRUE; \
			return; \
			}
static OcList	*copy_OcList(register OcList *orp);
static OcList	*match_Trie(register Trie *triep);

void
append_Octp(Trie *triep, Octree *octp)
{	register OcList	**opp = &triep->l.t_octp;
	if( octp == OCTREE_NULL )
		return;
	for( ; *opp != OCLIST_NULL; opp = &(*opp)->p_next )
		{
		if( (*opp)->p_octp == octp )
			{
			/* Octree pointer already in list.	*/
			return;
			}
		}
	NewOcList( *opp );
	(*opp)->p_octp = octp;
	(*opp)->p_next = OCLIST_NULL;
	return;
	}

int
delete_Node_OcList(register OcList **oclistp, register Octree *octreep)
{
	for( ; *oclistp != OCLIST_NULL; oclistp = &(*oclistp)->p_next )
		if( (*oclistp)->p_octp == octreep )
			{	register OcList	*tmp = *oclistp;
			*oclistp = (*oclistp)->p_next;
			free( (char *) tmp );
			return	1;
			}
	bu_log( "\"%s\"(%d) Couldn't find node.\n", __FILE__, __LINE__ );
	return	0;
	}
void
delete_OcList(OcList **oclistp)
{	register OcList	*op = *oclistp, *np;
	*oclistp = OCLIST_NULL;
	for( ; op != OCLIST_NULL; op = np )
		{
		np = op->p_next;
		free( (char *) op );
		}
	return;
	}

Trie	*
add_Trie(const char *name, register Trie **triepp)
{	register Trie	*curp;
	if( *name == NUL )
		{ /* See if name already exists.			*/
		if( *triepp == TRIE_NULL )
			{ /* Name does not exist, make leaf node.	*/
			NewTrie( *triepp );
			(*triepp)->l.t_altr = (*triepp)->l.t_next = TRIE_NULL;
			(*triepp)->l.t_octp = OCLIST_NULL;
			}
		else
		if( (*triepp)->n.t_next != TRIE_NULL )
			{ /* Name is subset of another name.		*/
			return	add_Trie( name, &(*triepp)->n.t_altr );
			}
		else
			; /* Name already inserted, so do nothing.	*/
		return	*triepp;
		}
	for(	curp = *triepp;
		curp != TRIE_NULL && *name != curp->n.t_char;
		curp = curp->n.t_altr
		)
		;
	if( curp == TRIE_NULL )
		{ /* No Match, this level, so create new alternate.	*/
		curp = *triepp;
		NewTrie( *triepp );
		(*triepp)->n.t_altr = curp;
		(*triepp)->n.t_char = *name;
		(*triepp)->n.t_next = TRIE_NULL;
		return	add_Trie( ++name, &(*triepp)->n.t_next );
		}
	else
		/* Found matching character.				*/
		return	add_Trie( ++name, &curp->n.t_next );
	}

OcList	*
get_Trie(register char *name, register Trie *triep)
{	register Trie *curp = triep; /* initialize to shutup compiler */
	assert( triep != NULL );
	/* Traverse next links to end of region name.			*/
	for( ; triep != TRIE_NULL; triep = triep->n.t_next )
		{
		curp = triep;
		if( *name == NUL )
			{ /* End of user-typed name.			*/
			if( triep->n.t_altr != TRIE_NULL )
				/* Ambiguous at this point.		*/
				return	OCLIST_NULL;
			else	  /* Complete next character.		*/
				{
				*name++ = triep->n.t_char;
				*name = NUL;
				}
			}
		else
		if( *name == '*' )
			return	match_Trie( triep );
		else	/* Not at end of user-typed name yet, traverse
				alternate list to find current letter.
			 */
			{
			for(	;
				triep != TRIE_NULL && *name != triep->n.t_char;
				triep = triep->n.t_altr
				)
				;
			if( triep == TRIE_NULL )
				/* Non-existant name, truncate bad part.*/
				{
				*name = NUL;
				return	OCLIST_NULL;
				}
			else
				name++;
			}
		}

	/* Clobber key-stroke, and return it. */
	--name;
	*name = NUL;
	/* It is caller's responsibility to free this linked-list. */
	assert( curp != NULL );
	return copy_OcList( curp->l.t_octp );
	}

static OcList	*
match_Trie(register Trie *triep)
{	OcList	*oclist = OCLIST_NULL;
		register OcList	**oclistp = &oclist;
	if( triep == TRIE_NULL )
		return	OCLIST_NULL;
	if( triep->n.t_altr != TRIE_NULL )
		 /* Traverse alternate character pointer.		*/
		*oclistp = match_Trie( triep->n.t_altr );
	/* Advance ptr to end of list.					*/
	for(	;
		*oclistp != OCLIST_NULL;
		oclistp = &(*oclistp)->p_next
		)
		;
	if( triep->n.t_next == TRIE_NULL )
		/* At leaf node, return copy of list.			*/
		*oclistp = copy_OcList( triep->l.t_octp );
	else
		/* Traverse next character pointer.			*/
		*oclistp = match_Trie( triep->n.t_next );
	return	oclist;
	}


static OcList	*
copy_OcList(register OcList *orp)
               	     			/* Input list read pointer.	*/
	{	OcList *oclistp = OCLIST_NULL;	/* Output list pointer.	*/
		register OcList	**owpp = &oclistp; /* Write pointer.	*/
	/* Make copy of Octree pointer list.				*/
	for(	owpp = &oclistp;
		orp != OCLIST_NULL;
		orp = orp->p_next,
		owpp = &(*owpp)->p_next
		)
		{
		if( (*owpp = (OcList *) malloc( sizeof(OcList) )) == OCLIST_NULL )
			{
			Malloc_Bomb(sizeof(OcList));
			fatal_error = TRUE;
			return	OCLIST_NULL;
			}
		(*owpp)->p_octp = orp->p_octp;
		(*owpp)->p_next = OCLIST_NULL;
		}
	return	oclistp;
	}

#define MAX_TRIE_LEVEL	(32*16)

void
prnt_Trie(Trie *triep, int level)
{	register Trie	*tp = triep;
		static char	name_buf[MAX_TRIE_LEVEL+1], *namep;
	/*bu_log( "prnt_Trie(triep=0x%x,level=%d)\n", triep, level );*/
	if( tp == TRIE_NULL )
		return;
	if( tp->n.t_altr != TRIE_NULL )
		prnt_Trie( tp->n.t_altr, level );
	if( level == 0 )
		namep = name_buf;
	*namep = tp->n.t_char;
	if( tp->n.t_next == TRIE_NULL )
		{	register OcList	*op, *ip;
		/* At end of name, so print it out.			*/
		*namep = NUL;
		bu_log( "%s: ", name_buf );
		for( op = tp->l.t_octp; op != OCLIST_NULL; op = op->p_next )
			{
			for( ip = tp->l.t_octp; ip != op; ip = ip->p_next )
				if( ip->p_octp->o_temp == op->p_octp->o_temp )
					/* Already printed this temp.	*/
					break;
			if( ip == op )
				/* This temperature not printed yet.	*/
				bu_log( "%d,", op->p_octp->o_temp );
			}
		bu_log( "\n" );
		}
	else
		{
		namep++;
		prnt_Trie( tp->n.t_next, level+1 );
		namep--;
		}
	return;
	}

int
write_Trie(Trie *triep, int level, FILE *fp)
{	register Trie	*tp = triep;
		static char	name_buf[MAX_TRIE_LEVEL+1], *namep;
	if( tp == TRIE_NULL )
		return	1;
	if( tp->n.t_altr != TRIE_NULL )
		(void) write_Trie( tp->n.t_altr, level, fp );
	if( level == 0 )
		{
		if(	fwrite( (char *) &ir_min, (int) sizeof(int), 1, fp ) != 1
		    ||	fwrite( (char *) &ir_max, (int) sizeof(int), 1, fp ) != 1
			)
			{
			bu_log( "\"%s\"(%d) Write failed!\n", __FILE__, __LINE__ );
			return	0;
			}
		namep = name_buf;
		}
	*namep = tp->n.t_char;
	if( tp->n.t_next == TRIE_NULL )
		{	register OcList	*op;
		/* At end of name, so print it out.			*/
		*namep = NUL;
		for( op = tp->l.t_octp; op != OCLIST_NULL; op = op->p_next )
			{
			(void) fprintf(	fp, "%s\n", name_buf );
			if( ! write_Octree( op->p_octp, fp ) )
				return	0;
			}
		}
	else
		{
		namep++;
		(void) write_Trie( tp->n.t_next, level+1, fp );
		namep--;
		}
	return	1;
	}

int
read_Trie(FILE *fp)
{	static char	name_buf[MAX_TRIE_LEVEL+1];
		register Trie	*triep;
		F_Hdr_Ptlist	hdr_ptlist;
		int		min, max;
		extern Trie	*reg_triep;
	/* Read temperature range information.				*/
	if(	fread( (char *) &min, (int) sizeof(int), 1, fp ) != 1
	     ||	fread( (char *) &max, (int) sizeof(int), 1, fp ) != 1
		)
		{
		bu_log( "Could not read min/max info.\n" );
		rewind( fp );
		}
	else
		{
		bu_log( "IR data base temperature range is %d to %d\n",
			min, max
			);
		if( ir_min == ABSOLUTE_ZERO )
			{ /* Temperature range not set.			*/
			ir_min = min;
			ir_max = max;
			}
		else
			{ /* Merge with existing range.			*/
			ir_min = Min( ir_min, min );
			ir_max = Max( ir_max, max );
			bu_log(	"Global temperature range is %d to %d\n",
				ir_min, ir_max
				);
			(void) fflush( stdout );
			}
		}
	if( ! init_Temp_To_RGB() )
		{
		return	0;
		}
	while( fgets( name_buf, MAX_TRIE_LEVEL, fp ) != NULL )
		{
		name_buf[strlen(name_buf)-1] = '\0'; /* Clobber new-line.*/
		triep = add_Trie( name_buf, &reg_triep );
		if( fread( (char *) &hdr_ptlist, (int) sizeof(F_Hdr_Ptlist), 1, fp )
			!= 1
			)
			{
			bu_log( "\"%s\"(%d) Read failed!\n", __FILE__, __LINE__ );
			return	0;
			}
		for( ; hdr_ptlist.f_length > 0; hdr_ptlist.f_length-- )
			{	fastf_t		point[3];
				float		c_point[3];
				Octree		*octreep;
			if( fread( (char *) c_point, (int) sizeof(c_point), 1, fp ) != 1 )
				{
				bu_log(	"\"%s\"(%d) Read failed.\n",
					__FILE__, __LINE__ );
				return	0;
				}
			VMOVE( point, c_point );
			if( (octreep = add_Region_Octree(	&ir_octree,
								point,
								triep,
								hdr_ptlist.f_temp,
								0
								)
				) != OCTREE_NULL
				)
				append_Octp( triep, octreep );
			}
		}
	return	1;
	}

void
ring_Bell(void)
{
	(void) putchar( BEL );
	return;
	}

char	*
char_To_String(int i)
{	static char	buf[4];
	if( i >= SP && i < DEL )
		{
		buf[0] = i;
		buf[1] = NUL;
		}
	else
	if( i >= NUL && i < SP )
		{
		buf[0] = '^';
		buf[1] = i + 64;
		buf[2] = NUL;
		}
	else
	if( i == DEL )
		return	"DL";
	else
		return	"EOF";
	return	buf;
	}

/*	g e t _ R e g i o n _ N a m e ( )
	TENEX-style name completion.
	Returns a linked-list of pointers to octree leaf nodes.
 */
OcList	*
get_Region_Name(char *inbuf, int bufsz, char *msg)
{	static char	buffer[BUFSIZ];
		register char	*p = buffer;
		register int	c;
		OcList		*oclistp = OCLIST_NULL;
		extern Trie	*reg_triep;
		extern int	tty;
	if( tty )
		{
		save_Tty( 0 );
		set_Raw( 0 );
		clr_Echo( 0 );
		}
	prnt_Prompt( msg );
	*p = NUL;
	do
		{
		(void) fflush( stdout );
		c = hm_getchar();
		switch( c )
			{
		case SP :
			{
			if(	reg_triep == TRIE_NULL
			    ||	(oclistp = get_Trie( buffer, reg_triep ))
				== OCLIST_NULL
				)
				(void) putchar( BEL );
			for( ; p > buffer; p-- )
				(void) putchar( BS );
			(void) printf( "%s", buffer );
			(void) ClrEOL();
			(void) fflush( stdout );
			p += strlen( buffer );
			break;
			}
		case Ctrl('A') : /* Cursor to beginning of line.	*/
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			for( ; p > buffer; p-- )
				(void) putchar( BS );
			break;
		case Ctrl('B') :
		case BS : /* Move cursor back one character.		*/
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			(void) putchar( BS );
			--p;
			break;
		case Ctrl('D') : /* Delete character under cursor.	*/
			{	register char	*q = p;
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
		case Ctrl('E') : /* Cursor to end of line.		*/
			if( *p == NUL )
				{
				ring_Bell();
				break;
				}
			(void) printf( "%s", p );
			p += strlen( p );
			break;
		case Ctrl('F') : /* Cursor forward one character.	*/
			if( *p == NUL || p-buffer >= bufsz-2 )
				{
				ring_Bell();
				break;
				}
			(void) putchar( *p++ );
			break;
		case Ctrl('G') : /* Abort input.			*/
			ring_Bell();
			prnt_Event( "Aborted." );
			goto	clean_return;
		case Ctrl('K') : /* Erase from cursor to end of line.	*/
			if( *p == NUL )
				{
				ring_Bell();
				break;
				}
			ClrEOL();
			*p = NUL;
			break;
		case Ctrl('P') : /* Yank previous contents of "inbuf".	*/
			{	register int	len = strlen( inbuf );
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
		case Ctrl('U') : /* Erase from start of line to cursor.	*/
			if( p == buffer )
				{
				ring_Bell();
				break;
				}
			for( ; p > buffer; --p )
				{	register char	*q = p;
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
		case Ctrl('R') : /* Print line, cursor doesn't move.	*/
			{	register int	i;
			if( buffer[0] == NUL )
				break;
			for( i = p - buffer; i > 0; i-- )
				(void) putchar( BS );
			(void) printf( "%s", buffer );
			for( i = strlen( buffer ) - (p - buffer); i > 0; i-- )
				(void) putchar( BS );
			break;
			}
		case DEL : /* Delete character behind cursor.		*/
			{	register char	*q = p;
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
			if(	reg_triep == TRIE_NULL
			    ||	(oclistp = get_Trie( buffer, reg_triep ))
				== OCLIST_NULL
				)
				{
				(void) putchar( BEL );
				break;
				}
			else
				{
				(void) strncpy( inbuf, buffer, bufsz );
				prnt_Event( "" );
				goto clean_return;
				}
		case Ctrl('V') :
			/* Escape character, do not process next char.	*/
			c = hm_getchar();
			/* Fall through to default case!		*/
		default : /* Insert character at cursor.		*/
			{	register char	*q = p;
				register int	len = strlen( p );
			/* Scroll characters forward.			*/
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
	prnt_Event( "Buffer full." );
clean_return :
	prnt_Prompt( "" );
	if( tty )
		reset_Tty( 0 );
	return	oclistp;
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
