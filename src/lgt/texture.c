/*                       T E X T U R E . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file texture.c
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



#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./mat_db.h"

#define DEBUG_TEXTURE	0

typedef unsigned short	icon_t;
#define BIT_TEST( w, b )	(b == 0 ? (int)(w)&1 : (int)(w)&(1<<(b)))
#define Fb_Lookup( fbp, u, v )	(fbp->map + (v) * fbp->wid + (u))
#define BITS_PER_BYTE	8
#define BYTES_WIDE	(wid/BITS_PER_BYTE)
#define ITEMS_WIDE	(BYTES_WIDE/sizeof(icon_t))
#define ICON_SUFFIX	"icon"
#define FB_SUFFIX	"pix"

struct icon_texture
	{
	char			*filenm;
	icon_t			*map;
	int			wid;
	int			hgt;
	struct icon_texture	*next;
	}
*icons = NULL;

struct fb_texture
	{
	char			*filenm;
	RGBpixel		*map;
	int			wid;
	int			hgt;
	struct fb_texture	*next;
	}
*fbs = NULL;

STATIC char	*
suffix(register char *str)
{	register char	*p = str + strlen( str ) - 1;
	while( *p != '.' && p != str )
		p--;
	if( *p == '.' )
		return	p+1;
	else
		return	p;
	}

STATIC RGBpixel	*
icon_Lookup( iconp, u, v )
struct icon_texture	*iconp;
int	u, v;
	{	static int	word_sz = sizeof(icon_t) * BITS_PER_BYTE;
		static RGBpixel	black_pixel = { 0, 0, 0 };
		static RGBpixel	white_pixel = { 255, 255, 255 };
		int	offset = (iconp->hgt-1-v)*iconp->wid/word_sz + u/word_sz;
		int	bit = (word_sz-1) - (u % word_sz);	
		icon_t	word = iconp->map[offset];
	if( BIT_TEST( word, bit ) )
		return	(RGBpixel *) black_pixel;
	else
		return	(RGBpixel *) white_pixel;
	}

STATIC struct icon_texture	*
init_Icon_Texture(char *file, Mat_Db_Entry *entry)
{	FILE	*iconfp;
		register struct icon_texture	*iconp;
		icon_t	*iconmap;
		int	wid = entry->df_rgb[0] << 3;
		int	hgt = entry->df_rgb[1] << 3;
	if( (iconfp = fopen( file, "r" )) == NULL )
		{
		bu_log( "Can't open icon texture \"%s\" for reading.\n",
			file );
		return	NULL;
		}
	if(	(iconmap =
		(icon_t *) malloc( BYTES_WIDE*hgt ))
		== (icon_t *) 0
		)
		{
		bu_log( "No space for icon texture map.\n" );
		return	NULL;
		}
#if DEBUG_TEXTURE
	bu_log( "init_Icon_Texture(%s) wid=%d hgt=%d\n", file, wid, hgt );
	bu_log( "%d bytes allocated for texture map.\n",
		BYTES_WIDE*hgt );
#endif
	if( fread( iconmap, sizeof(icon_t), ITEMS_WIDE*hgt, iconfp )
		== -1 )
		{
		bu_log( "Read of icon texture map failed.\n" );
		return	NULL;
		}
	if(	(iconp =
		(struct icon_texture *) malloc( sizeof( struct icon_texture ) ))
	    ==	(struct icon_texture *) 0
		)
		{
		bu_log( "No space for icon texture.\n" );
		return	NULL;
		}
	if( (iconp->filenm = malloc( strlen(file)+1 )) == NULL )
		{
		bu_log( "init_Icon_Texture: no space for file name.\n" );
		return	NULL;
		}
	(void) strcpy( iconp->filenm, file );
	iconp->map = iconmap;
	iconp->wid = wid;
	iconp->hgt = hgt;
	iconp->next = icons;
	icons = iconp;
#if DEBUG_TEXTURE
	{ register int	u, v;
	for( v = 0; v < hgt; v++ )
		for( u = 0; u < wid; u++ )
			{	RGBpixel	*pixel;
			pixel = icon_Lookup( iconp, u, v );
			prnt_Pixel( *pixel, u, v );
			}
	}
#endif
	(void) fclose( iconfp );
	return	iconp;
	}

STATIC struct fb_texture	*
init_Fb_Texture(char *file, Mat_Db_Entry *entry)
{	FBIO		*txfbiop;
		register struct fb_texture	*fbp;
		RGBpixel	*fbmap;
		int		wid = entry->df_rgb[0] << 3;
		int		hgt = entry->df_rgb[1] << 3;
	if( (txfbiop = fb_open( file, wid, hgt )) == FBIO_NULL )
		return	NULL;
	if(	(fbmap =
		(RGBpixel *) malloc( wid*hgt*sizeof(RGBpixel) ))
		== (RGBpixel *) 0
		)
		{
		bu_log( "No space for frame buffer texture map.\n" );
		return	NULL;
		}
#if DEBUG_TEXTURE
	bu_log( "init_Fb_Texture(%s) wid=%d hgt=%d\n", file, wid, hgt );
	bu_log( "%d bytes allocated for texture map.\n",
		wid*hgt*sizeof(RGBpixel) );
#endif
	if( fb_read( txfbiop, 0, 0, (unsigned char *)fbmap, wid*hgt ) == -1 )
		{
		bu_log( "Read of frame buffer texture failed.\n" );
		return	NULL;
		}
	if(	(fbp =
		(struct fb_texture *) malloc( sizeof( struct fb_texture ) ))
	    ==	(struct fb_texture *) 0
		)
		{
		bu_log( "No space for fb texture.\n" );
		return	NULL;
		}
	if( (fbp->filenm = malloc( strlen(file)+1 )) == NULL )
		{
		bu_log( "init_Fb_Texture: no space for file name.\n" );
		return	NULL;
		}
	(void) strcpy( fbp->filenm, file );
	fbp->map = fbmap;
	fbp->wid = wid;
	fbp->hgt = hgt;
	fbp->next = fbs;
	fbs = fbp;
#if DEBUG_TEXTURE
	{ register int	u, v;
	for( v = 0; v < hgt; v++ )
		for( u = 0; u < wid; u++ )
			{	RGBpixel	*pixel;
			pixel = Fb_Lookup( fbp, u, v );
			prnt_Pixel( pixel, u, v );
			}
	}
#endif
	(void) fb_close( txfbiop );
	return	fbp;
	}

int
tex_Entry(struct uvcoord *uvp, Mat_Db_Entry *entry)
{
	if( strcmp( ICON_SUFFIX, suffix( entry->name ) ) == 0 )
		return	icon_Entry( uvp, entry );
	else
	if( strcmp( FB_SUFFIX, suffix( entry->name ) ) == 0 )
		return	fb_Entry( uvp, entry );
	else
		return	0;
	}

int
icon_Entry(struct uvcoord *uvp, Mat_Db_Entry *entry)
{	int	ui;
		int	vi;
		register RGBpixel		*pixel;
		register struct icon_texture	*iconp;
		char				*file = entry->name + TEX_KEYLEN;
	bu_semaphore_acquire( RT_SEM_RESULTS );
	for(	iconp = icons;
		iconp != NULL && strcmp( iconp->filenm, file ) != 0;
		iconp = iconp->next )
		;
	if( iconp == NULL )
		iconp = init_Icon_Texture( file, entry );
	bu_semaphore_release( RT_SEM_RESULTS );
	if( iconp == NULL )
		return	0;
	if(	uvp->uv_u > 1.0 || uvp->uv_u < 0.0
	     ||	uvp->uv_v > 1.0 || uvp->uv_v < 0.0
		)
		{
		bu_log( "uv coordinates out of range: u=%g v=%g\n",
			uvp->uv_u, uvp->uv_v );
		return	0;
		}
	ui = uvp->uv_u * iconp->wid;
	vi = uvp->uv_v * iconp->hgt;
#if DEBUG_TEXTURE
	bu_log( "uvp->uv_u=%g ui=%d uvp->uv_v=%g vi=%d\n",
		uvp->uv_u, ui, uvp->uv_v, vi );
#endif
	pixel = icon_Lookup( iconp, ui, vi );
	COPYRGB( entry->df_rgb, *pixel );
	return	1;
	}

int
fb_Entry(struct uvcoord *uvp, Mat_Db_Entry *entry)
{	int				ui;
		int				vi;
		register RGBpixel		*pixel;
		register struct fb_texture	*fbp;
		char				*file = entry->name + TEX_KEYLEN;
	bu_semaphore_acquire( RT_SEM_RESULTS );
	for(	fbp = fbs;
		fbp != NULL && strcmp( fbp->filenm, file ) != 0 ;
		fbp = fbp->next )
		;
	if( fbp == NULL )
		fbp = init_Fb_Texture( file, entry );
	bu_semaphore_release( RT_SEM_RESULTS );
	if( fbp == NULL )
		return	0;
	if(	uvp->uv_u > 1.0 || uvp->uv_u < 0.0
	     ||	uvp->uv_v > 1.0 || uvp->uv_v < 0.0
		)
		{
		bu_log( "uv coordinates out of range: u=%g v=%g\n",
			uvp->uv_u, uvp->uv_v );
		return	0;
		}
	ui = uvp->uv_u * fbp->wid;
	vi = uvp->uv_v * fbp->hgt;
	pixel = Fb_Lookup( fbp, ui, vi );
	COPYRGB( entry->df_rgb, *pixel );
#if DEBUG_TEXTURE
	bu_log( "uvp->uv_u=%g uvp->uv_v=%g\n",
		uvp->uv_u, uvp->uv_v );
	bu_log( "fbp->map[%d]=<%d,%d,%d>\n",
		vi*fbp->wid + ui,
		(*(fbp->map+vi*fbp->wid+ui))[0],	
		(*(fbp->map+vi*fbp->wid+ui))[1],	
		(*(fbp->map+vi*fbp->wid+ui))[2] );	
	prnt_Pixel( pixel, ui, vi );
#endif
	return	1;
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
