/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
/*
	Originally extracted from SCCS archive:
		SCCS id:	@(#) mat_db.h	1.6
		Modified: 	2/4/87 at 12:18:09
		Retrieved: 	2/4/87 at 12:18:22
		SCCS archive:	/vld/moss/src/libmatdb/s.mat_db.h
*/
#define INCL_MATDB
#if ! defined( NULL )
#include <stdio.h>
#endif
#define MAX_MAT_DB	100
#define MAX_MAT_NM	81
#define MF_USED		1
#undef	MF_NULL
#define MF_NULL		0
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

extern FILE		*mat_Open_Db();
extern Mat_Db_Entry	*fb_Entry();
extern Mat_Db_Entry	*fb_val();
extern Mat_Db_Entry	*mat_Get_Db_Entry();
extern int		mat_Close_Db();
extern int		mat_Print_Db();
extern int		mat_Read_Db();
extern int		mat_Save_Db();
extern int		mat_Put_Db_Entry();
extern int		mat_Print_Db();

extern Mat_Db_Entry	mat_dfl_entry;
