/*                           S P M . H
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup sphmap */
/*@{*/
/** @file spm.h
 *@brief
 *  Sphere data structure and function declarations.
 *
 *  @author
 *	Phillip Dykstra
 *
 *  @par Source
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *  $Header$
 */

#ifndef SEEN_SPM_H
#define SEEN_SPM_H seen

#ifndef BN_EXPORT
#   if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#      ifdef BN_EXPORT_DLL
#         define BN_EXPORT __declspec(dllexport)
#      else
#         define BN_EXPORT __declspec(dllimport)
#      endif
#   else
#      define BN_EXPORT
#   endif
#endif

typedef	struct	{
	long	magic;
	int	ny;		/**< @brief  Number of "y" bins */
	int	*nx;		/**< @brief  Number of "x" bins per "y" bin */
	int	elsize;		/**< @brief  Size of each bin element */
	unsigned char **xbin;	/**< @brief  staring addresses of "x" bins */
	unsigned char *_data;	/**< @brief  For freeing purposes, start of data */
} spm_map_t;

#define	SPM_NULL (spm_map_t *)0

#define SPM_MAGIC	0x41278678

#define RT_CK_SPM(smp)		BU_CKMAG(smp, SPM_MAGIC, "spm_map_t" )
#define BN_CK_SPM(smp)		BU_CKMAG(smp, SPM_MAGIC, "spm_map_t" )

/* XXX These should all have bn_ prefixes */

BN_EXPORT extern spm_map_t *spm_init();
BN_EXPORT extern void	spm_free();
BN_EXPORT extern void	spm_read();
BN_EXPORT extern void	spm_write();
BN_EXPORT extern char	*spm_get();
BN_EXPORT extern int	spm_load();
BN_EXPORT extern int	spm_save();
BN_EXPORT extern int	spm_pix_load();
BN_EXPORT extern int	spm_pix_save();
BN_EXPORT extern void	spm_dump();

/*----------------------------------------------------------------------*/
/* sphmap.c */
BN_EXPORT extern int spm_px_load( spm_map_t *mapp, char *filename, int nx, int ny);

#endif /* SEEN_SPM_H */

/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

