/*                        M A T _ D B . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file mat_db.h
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef INCL_MATDB
#define INCL_MATDB
#define MAX_MAT_DB	100
#define MAX_MAT_NM	81
#define MF_USED		1
#undef	MF_NULL
#define MF_NULL		0
#define TEX_KEYWORD	"texture "
#define TEX_KEYLEN	8		/* strlen( TEX_KEYWORD ) */

typedef struct
	{
	int	id;		/* GED database material id handle.	*/
	int	shine;		/* Shininess parameter.			*/
	double	wgt_specular;	/* Specular reflection weighting coeff.	*/
	double	wgt_diffuse;	/* Diffuse reflection weighting coeff.	*/
	double	transparency;	/* Transparency coefficient.		*/
	double	reflectivity;	/* Mirror reflectivity coefficient.	*/
	double	refrac_index;	/* Refractive index of material.	*/
	unsigned char	df_rgb[3]; /* Diffuse reflection RGB values.	*/
	unsigned char	mode_flag; /* Used flag (MF_USED or MF_NULL)	*/
	char	name[MAX_MAT_NM];  /* Name of material.			*/
	} Mat_Db_Entry;
#define MAT_DB_NULL	(Mat_Db_Entry *) NULL

extern Mat_Db_Entry	*mat_Get_Db_Entry(int id);
extern int		fb_Entry(struct uvcoord *uvp, Mat_Db_Entry *entry);
extern int		mat_Print_Db(int material_id);
extern int		mat_Rd_Db(char *file);
extern int		mat_Save_Db(char *file);
extern int		mat_Edit_Db_Entry(int id);
extern int		mat_Print_Db(int material_id);

extern Mat_Db_Entry	mat_dfl_entry;

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
