/*                       F O R T R A Y . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup fort */
/** @{ */
/** @file librt/fortray.c
 *
 *  A general-purpose set of FORTRAN-callable interface routines to
 *  permit any FORTRAN program to use LIBRT, the ray-tracing library
 *  of the BRL-CAD Package.
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "raytrace.h"


static struct partition fr_global_head;


HIDDEN int
fr_hit(struct application *ap, struct partition *headp, struct seg *segp)
{
    RT_CK_AP(ap);
    RT_CK_PT_HD(headp);
    if (segp) RT_CK_SEG(segp);

    if ( headp->pt_forw == headp )  return 0;

    /* Steal the linked list, hang it off a global header */
    fr_global_head.pt_forw = headp->pt_forw;
    fr_global_head.pt_back = headp->pt_back;
    fr_global_head.pt_back->pt_forw = &fr_global_head;
    fr_global_head.pt_forw->pt_back = &fr_global_head;

    headp->pt_forw = headp->pt_back = headp;
    return 1;
}

HIDDEN int
fr_miss(struct application *ap)
{
    RT_CK_AP(ap);

    fr_global_head.pt_forw = fr_global_head.pt_back = &fr_global_head;
    return 0;
}


/**
 * F R _ S T R I N G _ C 2 F
 *
 * Take a null-terminated C string, and place it with space padding on
 * the right into a FORTRAN string of given length.
 */
void
fr_string_c2f(register char *fstr, register char *cstr, register int flen)
{
    register int	i;

    for ( i=0; i < flen; i++ )  {
	if ( (fstr[i] = cstr[i]) == '\0' )  break;
    }
    for (; i < flen; i++ )
	fstr[i] = ' ';
}


/**
 * F R _ S T R I N G _ F 2 C
 *
 * Take a FORTRAN string with a length, and return a pointer to null
 * terminated copy of that string in a STATIC buffer.
 */
static char *
fr_string_f2c(char *str, int maxlen)
{
    static char	buf[512];
    int	len;
    int	i;

    len = sizeof(buf);
    if (maxlen < len)
	len = maxlen;

    bu_strlcpy( buf, str, len );

    /* Remove any trailing blanks */
    for ( i=(int)strlen(buf)-1; i >= 0; i-- )  {
	if ( buf[i] != ' ' && buf[i] != '\n' )  break;
	buf[i] = '\0';
    }
    return buf;
}


/**
 * F R D I R
 *
 * FORTRAN to RT interface for rt_dirbuild()
 *
 * XXX NOTE Apollo FORTRAN passes string length as extra (invisible)
 * argument, by value.  Other systems probably are different.
 *
 * XXX On some systems, the C standard I/O library may need to be
 * initialized here (eg, Cray).
 */
void
BU_FORTRAN(frdir, FRDIR)(struct rt_i **rtip, char *filename, int *filelen)
{
    char	*file;

    file = fr_string_f2c( filename, *filelen );
    *rtip = rt_dirbuild( file, (char *)0, 0 );
}


/**
 * F R T R E E
 *
 * Add another top-level tree to the in-core geometry.
 */
void
BU_FORTRAN(frtree, FRTREE)(int		*fail,
			   struct rt_i	**rtip,
			   char		*objname,
			   int		*objlen)
{
    char	*obj;

    RT_CHECK_RTI(*rtip);

    obj = fr_string_f2c( objname, *objlen );
    *fail = rt_gettree( *rtip, obj );
}


/**
 * F R P R E P
 */
void
BU_FORTRAN(frprep, FRPREP)(struct rt_i	**rtip)
{
    RT_CHECK_RTI(*rtip);
    rt_prep(*rtip);
}


#define CONTEXT_LEN	6	/* Reserve this many FORTRAN Doubles for each */
struct context {
    double		co_vpriv[3];
    struct soltab	*co_stp;
    char		*co_priv;
    int		co_inflip;
};


/**
 * F R S H O T
 *
 * NOTE that the [0] element here corresponds with the caller's (1) element.
 */
void
BU_FORTRAN(frshot, FRSHOT)(int			*nloc,		/* input & output */
			   double		*indist,	/* output only */
			   double		*outdist,
			   int			*region_ids,
			   struct context	*context,
			   struct rt_i		**rtip,		/* input only */
			   double		*pt,
			   double		*dir)
{
    struct application	ap;
    register struct partition *pp;
    int		ret;
    register int	i;

    RT_CHECK_RTI(*rtip);

    if ( *nloc <= 0 )  {
	bu_log("ERROR frshot: nloc=%d\n", *nloc);
	*nloc = 0;
	return;
    }

    RT_APPLICATION_INIT(&ap);
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
    ap.a_onehit = *nloc * 2;
    ap.a_resource = &rt_uniresource;
    rt_uniresource.re_magic = RESOURCE_MAGIC;
    ap.a_purpose = "frshot";
    ap.a_rt_i = *rtip;

    /*
     * Actually fire the ray.  The list of results will be linked to
     * fr_global_head by fr_hit(), for further use below.
     *
     * It is a bit risky to rely on the segment structures pointed to
     * by the partition list to still be valid, because rt_shootray
     * has already put them back on the free segment queue.  However,
     * they will remain unchanged until the next call to
     * rt_shootray(), so copying out the data here will work fine.
     */
    ret = rt_shootray( &ap );

    if ( ret <= 0 )  {
	/* Signal no hits */
	*nloc = 0;
	return;
    }

    /* Copy hit information from linked list to argument arrays */
    pp = fr_global_head.pt_forw;
    if ( pp == &fr_global_head )  {
	*nloc = 0;
	return;
    }
    for ( i=0; i < *nloc; i++, pp=pp->pt_forw )  {
	register struct context	*ctp;

	if ( pp == &fr_global_head )  break;
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
    for ( pp = fr_global_head.pt_forw; pp != &fr_global_head;  )  {
	register struct partition *newpp;

	newpp = pp;
	pp = pp->pt_forw;
	FREE_PT(newpp, (&rt_uniresource));
    }
}


/**
 * F R N O R M
 *
 * Given the data returned by a previous call to frshot(), compute the
 * surface normal at the entry point to the indicated solid.
 *
 * In order to save storage, and copying time, frshot() saved only the
 * minimum amount of data required.  Here, the hit and xray structures
 * are reconstructed, suitable for passing to RT_HIT_NORMAL.
 */
void
BU_FORTRAN(frnorm, FRNORM)(double		*normal,	/* output only */
			   int			*idx,		/* input only */
			   double		*indist,
			   struct context	*context,
			   double		*UNUSED(pt),
			   double		*UNUSED(dir))
{
    register struct context	*ctp;
    struct hit	hit;
    struct soltab	*stp;
    register int	i;

    i = *idx - 1; /* Selects which inhit is used */

    /* Reconstruct the hit structure */
    hit.hit_dist = indist[i];
    ctp = &context[i];
    stp = ctp->co_stp;
    VMOVE( hit.hit_vpriv, ctp->co_vpriv );
    hit.hit_private = ctp->co_priv;

    /* The new macro doesn't use ray argument */
    RT_HIT_NORMAL( normal, &hit, stp, NULL, ctp->co_inflip );
}


/**
 * F R N R E G
 *
 * Return the number of regions that exist in the model
 */
void
BU_FORTRAN(frnreg, FRNREG)(int *nreg, struct rt_i **rtip)
{
    *nreg = (*rtip)->nregions;
}


/**
 * F R N A M E
 *
 * Given a region number (range 1..nregions), return the right-hand
 * portion of the name in the provided buffer.
 *
 * XXX buflen is provided "automaticly" on the Apollo.
 */
void
BU_FORTRAN(frname, FRNAME)(char		*fbuf,
			   int		*region_num,
			   struct rt_i	**rtip,
			   int		fbuflen)
{
    register struct region *rp;
    int	i;
    int	len;
    int	offset;
    size_t rnum;
    char buf[512];

    rnum = *region_num-1;
    if ( rnum > (*rtip)->nregions )  {
	sprintf( buf, "Region id %d out of range, max=%ld",
		 *region_num, (long)((*rtip)->nregions) );
	fr_string_c2f( fbuf, buf, fbuflen );
	return;
    }
    for ( BU_LIST_FOR( rp, region, &((*rtip)->HeadRegion) ) )  {
	if ( (size_t)rp->reg_bit != rnum )  continue;
	len = (int)strlen( rp->reg_name );
	offset = 0;
	if ( len >= fbuflen )  {
	    offset = len-(fbuflen+1);
	    len -= (fbuflen+1);
	}
	bu_strlcpy( fbuf, rp->reg_name+offset, len );
	fbuf[len] = ' '; /* replace null with space, needs testing */
	for ( i=offset+len; i < fbuflen; i++ )
	    fbuf[i] = ' ';
	return;
    }
    sprintf(fbuf, "Unable to find region %d", *region_num );
    fr_string_c2f( fbuf, buf, fbuflen );
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
