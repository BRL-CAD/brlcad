/*
 *			S P H . H
 *
 *  Sphere data structure and function declarations.
 *
 *  Author -
 *	Phil Dykstra
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

typedef	struct	{
	int	ny;		/* Number of "y" bins */
	int	*nx;		/* Number of "x" bins per "y" bin */
	unsigned char **xbin;	/* staring addresses of "x" bins */
	unsigned char *_data;	/* For freeing purposes, start of data */
} sph_map_t;

#define	SPH_NULL (sph_map_t *)0

sph_map_t *sph_init();
void	sph_free();
void	sph_read();
void	sph_write();
int	sph_load();
int	sph_save();
int	sph_pix_load();
int	sph_pix_save();
void	sph_dump();
