/*
 *			F O R T R A Y . C
 *
 *  A general-purpose set of FORTRAN-callable interface routines to
 *  permit any FORTRAN program to use LIBRT, the ray-tracing library
 *  of the BRL CAD Package.
 *
 *  
 *  Author -
 *	Michael John Muuss
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
static const char RCSid[] = "@(#)$Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <string.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

/*
 *  Macro 'F' is used to take the 'C' function name,
 *  and convert it to the convention used for a particular system.
 *  Both lower-case and upper-case alternatives have to be provided
 *  because there is no way to get the C preprocessor to change the
 *  case of a token.
 *
 *  See also the code in ../libplot3/fortran.c
 */
#if CRAY
#	define	F(lc,uc)	uc
#endif
#if defined(apollo) || defined(mips) || defined(aux)
	/* Lower case, with a trailing underscore */
#ifdef __STDC__
#	define	F(lc,uc)	lc ## _
#else
#	define	F(lc,uc)	lc/**/_
#endif
#endif
#if !defined(F)
#	define	F(lc,uc)	lc
#endif

extern struct resource rt_uniresource;	/* From librt/shoot.c */

int			fr_hit(), fr_miss();
struct partition	fr_global_head;

/*
 *			F R _ S T R I N G _ C 2 F
 *
 *  Take a null-terminated C string, and place it with space padding
 *  on the right into a FORTRAN string of given length.
 */
void
fr_string_c2f( fstr, cstr, flen )
register char	*fstr;
register char	*cstr;
register int	flen;
{
	register int	i;
	
	for( i=0; i < flen; i++ )  {
		if( (fstr[i] = cstr[i]) == '\0' )  break;
	}
	for( ; i < flen; i++ )
		fstr[i] = ' ';
}

/*
 *			F R _ S T R I N G _ F 2 C
 *
 *  Take a FORTRAN string with a length, and return a pointer to
 *  null terminated copy of that string in a STATIC buffer.
 */
static char *
fr_string_f2c( str, maxlen )
char	*str;
int	maxlen;
{
	static char	buf[512];
	int	len;
	int	i;

	len = sizeof(buf)-1;
	buf[len] = '\0';
	if( maxlen < len )  len = maxlen;
	strncpy( buf, str, len );

	/* Remove any trailing blanks */
	for( i=strlen(buf)-1; i >= 0; i-- )  {
		if( buf[i] != ' ' && buf[i] != '\n' )  break;
		buf[i] = '\0';
	}
	return(buf);
}

/*
 *			F R D I R
 *
 *  FORTRAN to RT interface for rt_dirbuild()
 *
 *  XXX NOTE Apollo FORTRAN passes string length as extra (invisible)
 *  argument, by value.  Other systems probably are different.
 *
 *  XXX On some systems, the C standard I/O library may need to be
 *  initialized here (eg, Cray).
 */
void
F(frdir,FRDIR)( rtip, filename, filelen )
struct rt_i	**rtip;
char		*filename;
int		filelen;
{
	char	*file;

	file = fr_string_f2c( filename, filelen );
	*rtip = rt_dirbuild( file, (char *)0, 0 );
}

/*
 *			F R T R E E
 *
 *  Add another top-level tree to the in-core geometry.
 */
void
F(frtree,FRTREE)( fail, rtip, objname, objlen )
int		*fail;
struct rt_i	**rtip;
char		*objname;
int		objlen;
{
	char	*obj;

	obj = fr_string_f2c( objname, objlen );
	*fail = rt_gettree( *rtip, obj );
}

#define CONTEXT_LEN	6	/* Reserve this many FORTRAN Doubles for each */
struct context {
	double		co_vpriv[3];
	struct soltab	*co_stp;
	char		*co_priv;
	int		co_inflip;
};

/*
 *			F R S H O T
 *
 * NOTE that the [0] element here corresponds with the caller's (1) element. 
 */
void
F(frshot,FRSHOT)( nloc, indist, outdist, region_ids, context, rtip, pt, dir )
int		*nloc;		/* input & output */
double		*indist;	/* output only */
double		*outdist;
int		*region_ids;
struct context	*context;
struct rt_i	**rtip;		/* input only */
double		*pt;
double		*dir;
{
	struct application	ap;
	register struct partition *pp;
	int		ret;
	register int	i;

	if( *nloc <= 0 )  {
		bu_log("ERROR frshot: nloc=%d\n", *nloc);
		*nloc = 0;
		return;
	}

	bzero( (char *)&ap, sizeof(ap));
	ap.a_ray.r_pt[X] = pt[0];
	ap.a_ray.r_pt[Y] = pt[1];
	ap.a_ray.r_pt[Z] = pt[2];
	ap.a_ray.r_dir[X] = dir[0];
	ap.a_ray.r_dir[Y] = dir[1];
	ap.a_ray.r_dir[Z] = dir[2];
	VUNITIZE( ap.a_ray.r_dir );
	ap.a_hit = fr_hit;
	ap.a_miss = fr_miss;
	ap.a_level = 0;
	ap.a_onehit = 1;		/* Should be parameter XXX */
	ap.a_resource = &rt_uniresource;
	rt_uniresource.re_magic = RESOURCE_MAGIC;
	ap.a_purpose = "frshot";
	ap.a_rt_i = *rtip;

	/*
	 *  Actually fire the ray
	 *  The list of results will be linked to fr_global_head
	 *  by fr_hit(), for further use below.
	 *
	 *  It is a bit risky to rely on the segment structures
	 *  pointed to by the partition list to still be valid,
	 *  because rt_shootray has already put them back on the
	 *  free segment queue.  However, they will remain unchanged
	 *  until the next call to rt_shootray(), so copying out the
	 *  data here will work fine.
	 */
	ret = rt_shootray( &ap );

	if( ret <= 0 )  {
		/* Signal no hits */
		*nloc = 0;
		return;
	}

	/* Copy hit information from linked list to argument arrays */
	pp = fr_global_head.pt_forw;
	if( pp == &fr_global_head )  {
		*nloc = 0;
		return;
	}
	for( i=0; i < *nloc; i++, pp=pp->pt_forw )  {
		register struct context	*ctp;

		if( pp == &fr_global_head )  break;
		indist[i] = pp->pt_inhit->hit_dist;
		outdist[i] = pp->pt_outhit->hit_dist;
		/* This might instead be reg_regionid ?? */
		region_ids[i] = pp->pt_regionp->reg_bit+1;
		ctp = &context[i];
		ctp->co_stp = pp->pt_inseg->seg_stp;
		VMOVE( ctp->co_vpriv, pp->pt_inhit->hit_vpriv);
		ctp->co_priv = pp->pt_inhit->hit_private;
		ctp->co_inflip = pp->pt_inflip;
	}
	*nloc = i;	/* Will have been incremented above, if successful */

	/* Free linked list storage */
	for( pp = fr_global_head.pt_forw; pp != &fr_global_head;  )  {
		register struct partition *newpp;

		newpp = pp;
		pp = pp->pt_forw;
		FREE_PT(newpp, (&rt_uniresource));
	}
}

int
fr_hit( ap, headp )
struct application	*ap;
struct partition	*headp;
{
	if( headp->pt_forw == headp )  return(0);

	/* Steal the linked list, hang it off a global header */
	fr_global_head.pt_forw = headp->pt_forw;
	fr_global_head.pt_back = headp->pt_back;
	fr_global_head.pt_back->pt_forw = &fr_global_head;
	fr_global_head.pt_forw->pt_back = &fr_global_head;

	headp->pt_forw = headp->pt_back = headp;
	return(1);
}

int
fr_miss( ap )
struct application	*ap;
{
	fr_global_head.pt_forw = fr_global_head.pt_back = &fr_global_head;
	return(0);
}
/*
 *			F R N O R M
 *
 *  Given the data returned by a previous call to frshot(),
 *  compute the surface normal at the entry point to the indicated solid.
 *
 *  In order to save storage, and copying time, frshot() saved only
 *  the minimum amount of data required.  Here, the hit and xray
 *  structures are reconstructed, suitable for passing to RT_HIT_NORM.
 */
void
F(frnorm,FRNORM)( normal, index, indist, context, pt, dir )
double		*normal;	/* output only */
int		*index;		/* input only */
double		*indist;
struct context	*context;
double		*pt;
double		*dir;
{
	register struct context	*ctp;
	struct hit	hit;
#if 0
	struct xray	ray;
#endif
	struct soltab	*stp;
	register int	i;
	
	i = *index-1;		/* Selects which inhit is used */

#if 0
	/* Reconstruct the ray structure */
	ray.r_pt[X] = pt[0];
	ray.r_pt[Y] = pt[1];
	ray.r_pt[Z] = pt[2];
	ray.r_dir[X] = dir[0];
	ray.r_dir[Y] = dir[1];
	ray.r_dir[Z] = dir[2];
	/* Unitize r_dir? */
#endif

	/* Reconstruct the hit structure */
	hit.hit_dist = indist[i];
	ctp = &context[i];
	stp = ctp->co_stp;
	VMOVE( hit.hit_vpriv, ctp->co_vpriv );
	hit.hit_private = ctp->co_priv;
	
#if 0
	RT_HIT_NORMAL( normal, &hit, stp, &ray, ctp->co_inflip );
#else
	/* The new macro doesn't use ray argument */
	RT_HIT_NORMAL( normal, &hit, stp, NULL, ctp->co_inflip );
#endif
}

/*
 *			F R N R E G
 *
 *  Return the number of regions that exist in the model
 */
void
F(frnreg,FRNREG)( nreg, rtip )
int		*nreg;
struct rt_i	**rtip;
{
	*nreg = (*rtip)->nregions;
}

/*
 *			F R N A M E
 *
 *  Given a region number (range 1..nregions), return the
 *  right-hand portion of the name in the provided buffer.
 *
 *  XXX buflen is provided "automaticly" on the Apollo.
 */
void
F(frname,FRNAME)( fbuf, region_num, rtip, fbuflen )
char		*fbuf;
int		*region_num;
struct rt_i	**rtip;
int		fbuflen;
{
	register struct region *rp;
	int	i;
	int	len;
	int	offset;
	int	rnum;
	char	buf[512];

	rnum = *region_num-1;
	if( rnum < 0 || rnum > (*rtip)->nregions )  {
		sprintf( buf, "Region id %d out of range, max=%ld",
			*region_num, (long)((*rtip)->nregions) );
		fr_string_c2f( fbuf, buf, fbuflen );
		return;
	}
	for( BU_LIST_FOR( rp, region, &((*rtip)->HeadRegion) ) )  {
		if( rp->reg_bit != rnum )  continue;
		len = strlen( rp->reg_name );
		offset = 0;
		if( len >= fbuflen )  {
			offset = len-(fbuflen+1);
			len -= (fbuflen+1);
		}
		strncpy( fbuf, rp->reg_name+offset, len );
		for( i=offset+len; i < fbuflen; i++ )
			fbuf[i] = ' ';
		return;
	}
	sprintf(fbuf, "Unable to find region %d", *region_num );
	fr_string_c2f( fbuf, buf, fbuflen );
}
