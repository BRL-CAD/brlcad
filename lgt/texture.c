/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif
/*
	Originally extracted from SCCS archive:
		SCCS id:	@(#) texture.c	2.1
		Modified: 	12/10/86 at 16:03:53	G S M
		Retrieved: 	2/4/87 at 08:53:42
		SCCS archive:	/vld/moss/src/lgt/s.texture.c
*/

#include <stdio.h>
#include <fcntl.h>
#include "machine.h"
#include "fb.h"
#include "vmath.h"
#include "raytrace.h"
#include "./mat_db.h"
#include "./extern.h"
#define BITS_PER_BYTE	8
#define BITS_WIDE	48
#define FB_MAP		"map.fb"
	
FBIO	*txtr_ifp = FBIO_NULL;

#if ! defined( cray )
short	texture[BITS_WIDE][BITS_WIDE/(sizeof(short)*BITS_PER_BYTE)] =
		{
#include "./texture.h"
		};
#endif

#if ! defined( cray )
txtr_Val( uvp )
register struct uvcoord	*uvp;
	{	register int	ui = uvp->uv_u * BITS_WIDE;
		register int	vi = uvp->uv_v * BITS_WIDE;
		static int	word_sz = sizeof(short) * BITS_PER_BYTE;
		short		word;
	word = texture[vi-1][ui/word_sz];
	ui = ui % word_sz;
/*	rt_log( "word=0x%x ui=%d\n", word, ui );
	rt_log( "word >> ui = 0x%x\n", (int) word >> ui );*/
	return	(int) word & (1 << ui) ? 1 : 0;
	}
#endif

init_Fb_Val( fbfile )
char	*fbfile;
	{
	if( fbfile == NULL )
		fbfile = FB_MAP;
	if( txtr_ifp != FBIO_NULL )
		(void) fb_close( txtr_ifp );
	if( (txtr_ifp = fb_open( fbfile, fb_width, fb_width ))
		== FBIO_NULL
		) 
		return	0;
	(void) fb_ioinit( txtr_ifp );
	return	1;
	}

Mat_Db_Entry *
fb_Entry( uvp )
register struct uvcoord	*uvp;
	{	register int	ui = uvp->uv_u * fb_ulen;
		register int	vi = uvp->uv_v * fb_vlen;
		RGBpixel	pixel;
		static Mat_Db_Entry entry =
				{
				0,		/* Material id.		*/
				1,		/* Shininess.		*/
				0.1,		/* Specular weight.	*/
				0.9,		/* Diffuse weight.	*/
				0.0,		/* Transparency.	*/
				0.0,		/* Reflectivity.	*/
				1.0,		/* Refractive index.	*/
				0, 0, 0,	/* Diffuse RGB values.	*/
				MF_USED,	/* Mode flag.		*/
				"fb mapping"	/* Material name.	*/
				};
	if(	fb_seek( txtr_ifp, ui, (fb_vlen-1)-vi ) == -1
	    ||	fb_rpixel( txtr_ifp, pixel ) == -1
		)
		return	MAT_DB_NULL;
	/*rt_log( "uv_u=%g uv_v=%g ui=%d vi=%d\n", uvp->uv_u, uvp->uv_v, ui, vi );
	rt_log( "pixel=(%u,%u,%u)\n", pixel[RED], pixel[GRN], pixel[BLU] );*/
	COPYRGB( entry.df_rgb, pixel );
	return	&entry;
	}
