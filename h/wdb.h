/*
 *			W D B . H
 *
 *  Interface structures and routines for libwdb
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 *
 *  $Header$
 */

#ifndef WDB_H
#define WDB_H seen

struct wmember  {
	int	wm_magic;
	char	wm_name[16+3];	/* NAMESIZE */
	char	wm_op;		/* Boolean operation */
	mat_t	wm_mat;
	struct wmember *wm_forw;
	struct wmember *wm_back;
};
#define WMEMBER_NULL	((struct wmember *)0)
#define WMEMBER_MAGIC	0x43128912

/* Values for wm_op.  These must track db.h */
#define WMOP_INTERSECT	'+'
#define WMOP_SUBTRACT	'-'
#define WMOP_UNION		'u'

/* Convienient definitions */
#define mk_lfcomb(fp,name,headp,region)		mk_lcomb( fp, name, headp, \
	region, (char *)0, (char *)0, (char *)0, 0 );

extern struct wmember	*mk_addmember();

/*
 * This internal variable should not be directly modified;
 * call mk_conversion() or mk_set_conversion() instead.
 */
extern double	mk_conv2mm;		/* Conversion factor to mm */

extern int		mk_conversion();
extern double		mk_cvt_factor();
extern int		mk_set_conversion();

#endif /* WDB_H */
