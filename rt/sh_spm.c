/*
 *			S P M . C
 *
 *  Spherical Data Structures/Texture Maps
 *
 *  Author -
 *	Phillip Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "spm.h"
#include "./material.h"
#include "./rdebug.h"

#define SPM_NAME_LEN 128
struct spm_specific {
	char	sp_file[SPM_NAME_LEN];	/* Filename */
	int	sp_w;		/* Width: number of pixels around equator */
	spm_map_t *sp_map;	/* stuff */
};
#define SP_NULL	((struct spm_specific *)0)
#define SP_O(m)	offsetof(struct spm_specific, m)

struct structparse spm_parse[] = {
	"%s",	SPM_NAME_LEN, "file",		offsetofarray(struct spm_specific, sp_file),	FUNC_NULL,
	"%d",	1, "w",		SP_O(sp_w),	FUNC_NULL,
	"%d",	1, "n",		SP_O(sp_w),	FUNC_NULL,	/*compat*/
	"",	0, (char *)0,	0,		FUNC_NULL
};

HIDDEN int	spm_setup(), spm_render();
HIDDEN void	spm_print(), spm_mfree();

struct mfuncs spm_mfuncs[] = {
	"spm",		0,		0,		MFI_UV,
	spm_setup,	spm_render,	spm_print,	spm_mfree,

	(char *)0,	0,		0,
	0,		0,		0,		0
};

/*
 *  			S P M _ R E N D E R
 *  
 *  Given a u,v coordinate within the texture ( 0 <= u,v <= 1.0 ),
 *  return a pointer to the relevant pixel.
 */
HIDDEN int
spm_render( ap, pp, swp, dp )
struct application *ap;
struct partition *pp;
struct shadework	*swp;
char	*dp;
{
	register struct spm_specific *spp =
		(struct spm_specific *)dp;
	int	x, y;
	register unsigned char *cp;

	/** spm_read( spp->sp_map, xxx ); **/
	/* Limits checking? */
	y = swp->sw_uv.uv_v * spp->sp_map->ny;
	x = swp->sw_uv.uv_u * spp->sp_map->nx[y];
	cp = &(spp->sp_map->xbin[y][x*3]);
	VSET( swp->sw_color, cp[RED]/256., cp[GRN]/256., cp[BLU]/256. );
	return(1);
}

/*
 *			S P M _ S E T U P
 *
 *  Returns -
 *	<0	failed
 *	>0	success
 */
HIDDEN int
spm_setup( rp, matparm, dpp )
register struct region *rp;
struct rt_vls	*matparm;
char	**dpp;
{
	register struct spm_specific *spp;

	RT_VLS_CHECK( matparm );
	GETSTRUCT( spp, spm_specific );
	*dpp = (char *)spp;

	spp->sp_file[0] = '\0';
	spp->sp_w = -1;
	rt_structparse( matparm, spm_parse, (char *)spp );
	if( spp->sp_w < 0 )  spp->sp_w = 512;
	if( spp->sp_file[0] == '\0' )
		goto fail;
	if( (spp->sp_map = spm_init( spp->sp_w, sizeof(RGBpixel) )) == SPM_NULL )
		goto fail;
	if( spm_load( spp->sp_map, spp->sp_file ) < 0 )
		goto fail;
	return(1);
fail:
	rt_free( (char *)spp, "spm_specific" );
	return(-1);
}

/*
 *			S P M _ P R I N T
 */
HIDDEN void
spm_print( rp, dp )
register struct region *rp;
char	*dp;
{
	rt_structprint("spm_setup", spm_parse, (char *)dp);
	/* Should be more here */
}

HIDDEN void
spm_mfree( cp )
char *cp;
{
	spm_free( (spm_map_t *)cp );
}
