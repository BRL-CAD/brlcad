/*
 *			S P M . H
 *
 *  Sphere data structure and function declarations.
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
 *
 *  $Header$
 */

typedef	struct	{
	long	magic;
	int	ny;		/* Number of "y" bins */
	int	*nx;		/* Number of "x" bins per "y" bin */
	int	elsize;		/* Size of each bin element */
	unsigned char **xbin;	/* staring addresses of "x" bins */
	unsigned char *_data;	/* For freeing purposes, start of data */
} spm_map_t;

#define	SPM_NULL (spm_map_t *)0

#define SPM_MAGIC	0x41278678

#define RT_CK_SPM(smp)		BU_CKMAG(smp, SPM_MAGIC, "spm_map_t" )
#define BN_CK_SPM(smp)		BU_CKMAG(smp, SPM_MAGIC, "spm_map_t" )

/* XXX These should all have bn_ prefixes */

spm_map_t *spm_init();
void	spm_free();
void	spm_read();
void	spm_write();
char	*spm_get();
int	spm_load();
int	spm_save();
int	spm_pix_load();
int	spm_pix_save();
void	spm_dump();

/*----------------------------------------------------------------------*/
/* sphmap.c */
extern int spm_px_load( spm_map_t *mapp, char *filename, int nx, int ny);

