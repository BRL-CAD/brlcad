/*
 *			D M D F B . H
 *
 *  Author -
 *	Gary S. Moss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 *
 *  $Header$
 */

#define BLACK		0
#define WHITE		1
#define CLR_BIT( word, bit )	word &= ~(bit)
#define SET_BIT( word, bit )	word |= (bit)
#define TST_BIT( word, bit )	((word)&(bit))

#define PT_ANIMATE	'a'
#define PT_CLEAR	'e'
#define PT_CURSOR	'c'
#define PT_EOL		'\r'
#define PT_QUIT		'q'
#define PT_READ		'r'
#define PT_SEEK		's'
#define PT_SETSIZE	'S'
#define PT_VIEWPORT	'v'
#define PT_WINDOW	'w'
#define PT_WRITE	'p'
#define PT_ZOOM		'z'

#define PR_NOT_PENDING	-1
#define PR_ERROR	0
#define PR_END_OF_RUN	1
#define PR_END_OF_LINE	2
#define PR_SUCCESS	3

#define LAYER_BORDER	4
#define SMALL_LAYER_SZ	51
#define MIN_LAYER_SZ	80
