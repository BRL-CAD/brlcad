/*                  S H _ B I L L B O A R D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file sh_billboard.c
 *
 *	A billboard shader for use with RCC geometry
 *
 *	6) Edit shaders.tcl and comb.tcl in the ../tclscripts/mged directory to
 *		add a new gui for this shader.
 */
#include "common.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "rtprivate.h"
#include "plot3.h"


extern int rr_render(struct application	*ap,
		     struct partition	*pp,
		     struct shadework   *swp);
#define bbd_MAGIC 0x62626400	/* "bbd" */
#define CK_bbd_SP(_p) BU_CKMAG(_p, bbd_MAGIC, "bbd_specific")

struct bbd_img {
    struct bu_list	l;	/* must be first */
    plane_t	img_plane;	/* plane eqn for image plane */
    point_t	img_origin;	/* image origin in XYZ */
    vect_t	img_x;		/* u direction in image plane */
    double	img_xlen;	/* length of image in u direction */
    vect_t	img_y;		/* v direction in image plane */
    double	img_ylen;	/* length of image in v direction */
    unsigned long int	img_width;	/* dimension of image */
    unsigned long int 	img_height;
    struct bu_mapped_file *img_mf; /* image data */
};


/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct bbd_specific {
    long	magic;	/* magic # for memory validity check, must come 1st */
    int img_threshold;
    int img_width;
    int img_height;
    double img_scale;
    struct bu_vls img_filename;

    /* these are not initialized by the user */
    unsigned	img_count;
    struct bu_list imgs;

    struct rt_i *rtip;	/* this is needed by structparse */
    plane_t p_A;
    plane_t p_B;

};
#define MAX_IMAGES 64
/* The default values for the variables in the shader specific structure */
const static
struct bbd_specific bbd_defaults = {
    bbd_MAGIC,
    10,		/* img_threshold */
    512L,	/* img_width */
    512L,	/* img_height */
    100.0	/* img_scale */
};

#define SHDR_NULL	((struct bbd_specific *)0)
#define SHDR_O(m)	bu_offsetof(struct bbd_specific, m)
#define SHDR_AO(m)	bu_offsetofarray(struct bbd_specific, m)

void new_image(register const struct bu_structparse	*sdp,
	       register const char			*name,
	       char					*base,
	       const char				*value);


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse bbd_print_tab[] = {
    {"%ld",  1, "w",	SHDR_O(img_width),	BU_STRUCTPARSE_FUNC_NULL },
    {"%ld",  1, "n",	SHDR_O(img_height),	BU_STRUCTPARSE_FUNC_NULL },
    {"%d",  1, "t",	SHDR_O(img_threshold),	BU_STRUCTPARSE_FUNC_NULL },
    {"%f",  1, "h",	SHDR_O(img_scale),	BU_STRUCTPARSE_FUNC_NULL },
    {"%S",  1, "f",	SHDR_O(img_filename),	new_image },
    {"",    0, (char *)0, 0,			BU_STRUCTPARSE_FUNC_NULL }
};
struct bu_structparse bbd_parse_tab[] = {
    {"i",bu_byteoffset(bbd_print_tab[0]), "bbd_print_tab", 0, BU_STRUCTPARSE_FUNC_NULL },
    {"",	0, (char *)0,	0,		BU_STRUCTPARSE_FUNC_NULL }
};

HIDDEN int	bbd_setup(), bbd_render();
HIDDEN void	bbd_print(), bbd_free();

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs bbd_mfuncs[] = {
    {MF_MAGIC,	"bbd",	0,	MFI_NORMAL|MFI_HIT|MFI_UV,	0,
     bbd_setup,	bbd_render,	bbd_print,	bbd_free },

    {0,		(char *)0,	0,		0,		0,
     0,		0,		0,		0 }
};


void new_image(register const struct bu_structparse	*sdp,	/*struct desc*/
	       register const char			*name,	/*member name*/
	       char					*base,	/*struct base*/
	       const char				*value) /*string valu*/
{
    struct bbd_specific *bbd_sp = (struct bbd_specific *)base;
    struct bbd_img *bbdi;

    BU_GETSTRUCT(bbdi, bbd_img);

    bbdi->img_mf = bu_open_mapped_file_with_path(
						 bbd_sp->rtip->rti_dbip->dbi_filepath,
						 bu_vls_addr(&bbd_sp->img_filename),
						 NULL);

    if (!bbdi->img_mf) {
	bu_log("error opening image %s\n", bu_vls_addr(&bbd_sp->img_filename));
	bu_bomb("");
    }
    BU_CK_MAPPED_FILE(bbdi->img_mf);

    bbdi->img_width = bbd_sp->img_width;
    bbdi->img_width = bbd_sp->img_height;

    BU_LIST_APPEND(&bbd_sp->imgs, &(bbdi->l));
    bbd_sp->img_count++;

}



/*	B I L L B O A R D _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 *
 * 	Returns:
 *	1	success
 *	0	success, but delete region
 *	-1	failure
 */
HIDDEN int
bbd_setup( struct region *rp,
	   struct bu_vls *matparm,
	   char **dpp, /* pointer to reg_udata in *rp */
	   struct mfuncs *mfp,
	   struct rt_i *rtip
	   )
{
    register struct bbd_specific	*bbd_sp;
    struct rt_db_internal intern;
    struct rt_tgc_internal *tgc;
    int s;
    mat_t mat;
    struct bbd_img *bi;
    double angle;
    vect_t vtmp;
    int img_num;
    vect_t vv;

    /* check the arguments */
    RT_CHECK_RTI(rtip);
    BU_CK_VLS( matparm );
    RT_CK_REGION(rp);


    if (rdebug&RDEBUG_SHADE) bu_log("bbd_setup(%s)\n", rp->reg_name);

    RT_CK_TREE(rp->reg_treetop);

    if (rp->reg_treetop->tr_a.tu_op != OP_SOLID) {
	bu_log("--- Warning: Region %s shader %s", rp->reg_name, mfp->mf_name);
	bu_bomb("Shader should be used on region of single (rec/rcc) primitive\n");
    }

    RT_CK_SOLTAB(rp->reg_treetop->tr_a.tu_stp);
    if ( rp->reg_treetop->tr_a.tu_stp->st_id != ID_REC) {
	bu_log("--- Warning: Region %s shader %s", rp->reg_name, mfp->mf_name);
	bu_log("Shader should be used on region of single REC/RCC primitive %d\n",
	       rp->reg_treetop->tr_a.tu_stp->st_id);
	bu_bomb("oops\n");
    }


    /* Get memory for the shader parameters and shader-specific data */
    BU_GETSTRUCT( bbd_sp, bbd_specific );
    *dpp = (char *)bbd_sp;

    /* initialize the default values for the shader */
    memcpy(bbd_sp, &bbd_defaults, sizeof(struct bbd_specific) );
    bu_vls_init(&bbd_sp->img_filename);
    BU_LIST_INIT(&bbd_sp->imgs);
    bbd_sp->rtip = rtip; /* because new_image() needs this */
    bbd_sp->img_count = 0;

    /* parse the user's arguments for this use of the shader. */
    if (bu_struct_parse( matparm, bbd_parse_tab, (char *)bbd_sp ) < 0 )
	return(-1);

    if (bbd_sp->img_count > MAX_IMAGES) {
	bu_log("too many images (%d) in shader for %s sb < %d\n",
	       bbd_sp->img_count, rp->reg_name, MAX_IMAGES);
	bu_bomb("excessive image count\n");
    }


    mat_idn(mat);
    RT_INIT_DB_INTERNAL(&intern);
    s = rt_db_get_internal(&intern, rp->reg_treetop->tr_a.tu_stp->st_dp, rtip->rti_dbip,
			   mat, &rt_uniresource);

    if (intern.idb_minor_type != ID_TGC &&
	intern.idb_minor_type != ID_REC) {
	bu_log("What did I get? %d\n", intern.idb_minor_type);
    }

    if (s < 0) {
	bu_log("%s:%d didn't get internal", __FILE__, __LINE__);
	bu_bomb("");
    }
    tgc = (struct rt_tgc_internal *)intern.idb_ptr;
    RT_TGC_CK_MAGIC(tgc);

    angle = M_PI / (double)bbd_sp->img_count;
    img_num = 0;
    VMOVE(vv, tgc->h);
    VUNITIZE(vv);
    for (BU_LIST_FOR(bi, bbd_img, &bbd_sp->imgs)) {
	static const point_t o = { 0.0, 0.0, 0.0 };
	bn_mat_arb_rot(mat, o, vv, angle*img_num);

	/* compute plane equation */
	MAT4X3VEC(bi->img_plane, mat, tgc->a);
	VUNITIZE(bi->img_plane);
	bi->img_plane[H] = VDOT(tgc->v, bi->img_plane);

	MAT4X3VEC(vtmp, mat, tgc->b);
	VADD2(bi->img_origin, tgc->v, vtmp); /* image origin in 3d space */
	/* calculate image u vector */
	VREVERSE(bi->img_x, vtmp);
	VUNITIZE(bi->img_x);
	bi->img_xlen = MAGNITUDE(vtmp) * 2;

	/* calculate image v vector */
	VMOVE(bi->img_y, tgc->h);
	VUNITIZE(bi->img_y);
	bi->img_ylen = MAGNITUDE(tgc->h);

	if (rdebug&RDEBUG_SHADE) {
	    HPRINT("\nimg_plane", bi->img_plane);
	    VPRINT("vtmp", vtmp);
	    VPRINT("img_origin", bi->img_origin);
	    bu_log("img_xlen:%g  ", bi->img_xlen);
	    VPRINT("img_x", bi->img_x);
	    bu_log("img_ylen:%g  ", bi->img_ylen);
	    VPRINT("img_y", bi->img_y);
	}

	img_num++;
    }

    rt_db_free_internal(&intern, &rt_uniresource);

    if (rdebug&RDEBUG_SHADE) {
	bu_struct_print( " Parameters:", bbd_print_tab, (char *)bbd_sp );
    }

    return(1);
}

/*
 *	B I L L B O A R D _ P R I N T
 */
HIDDEN void
bbd_print( struct region *rp, char *dp )
{
    bu_struct_print( rp->reg_name, bbd_print_tab, (char *)dp );
}

/*
 *	B I L L B O A R D _ F R E E
 */
HIDDEN void
bbd_free( char *cp )
{
    bu_free( cp, "bbd_specific" );
}

static void
plot_ray_img(struct application	*ap,
	     struct partition	*pp,
	     double		dist,
	     struct bbd_img	*bi)
{
    static int plot_num;
    FILE *pfd;
    char name[256];
    point_t pt;

    sprintf(name, "bbd_%d.pl", plot_num++);
    bu_log("plotting %s\n", name);
    if ((pfd = fopen(name, "w")) == (FILE *)NULL) {
	bu_bomb("can't open plot file\n");
    }

    /* red line from ray origin to hit point */
    VJOIN1(pp->pt_inhit->hit_point, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
	   ap->a_ray.r_dir);
    if (VAPPROXEQUAL(ap->a_ray.r_pt, pp->pt_inhit->hit_point, 0.125)) {
	/* start and hit point identical, make special allowance */
	vect_t vtmp;

	pl_color(pfd, 255, 0, 128);
	VREVERSE(vtmp, ap->a_ray.r_dir);
	VJOIN1(vtmp, ap->a_ray.r_pt, 5.0, vtmp);
	pdv_3line(pfd, vtmp, pp->pt_inhit->hit_point);
    } else {
	pl_color(pfd, 255, 0, 0);
	pdv_3line(pfd, ap->a_ray.r_pt, pp->pt_inhit->hit_point);
    }

    /* yellow line from hit point to plane point */
    VJOIN1(pt, ap->a_ray.r_pt, dist, ap->a_ray.r_dir); /* point on plane */
    pl_color(pfd, 255, 255, 0);
    pdv_3line(pfd, pp->pt_inhit->hit_point, pt);

    /* green line from image origin to plane point */
    pl_color(pfd, 0, 255, 0);
    pdv_3line(pfd, pt, bi->img_origin);

    fclose(pfd);


}

/*
 *	d o _ r a y _ i m a g e
 *
 *	Handle ray interaction with 1 image
 */
static void
do_ray_image(struct application	*ap,
	     struct partition	*pp,
	     struct shadework	*swp,	/* defined in ../h/shadework.h */
	     struct bbd_specific *bbd_sp,
	     struct bbd_img *bi,
	     double dist)
{
    double x, y, dx, dy, xlo, xhi, ylo, yhi;
    int u, v, ulo, uhi, vlo, vhi;
    unsigned char *pixels, *color;
    int val;
    static const double rgb255 = 1.0 / 256.0;
    vect_t cum_color;
    point_t pt;
    vect_t vpt;
    double radius;
    int tot;
    int color_count;
    double t, opacity;

    if (rdebug&RDEBUG_SHADE) {
	bu_log("do_ray_image\n");
	plot_ray_img(ap, pp, dist, bi);
    }

    VJOIN1(pt, ap->a_ray.r_pt, dist, ap->a_ray.r_dir); /* point on plane */
    VSUB2(vpt, pt, bi->img_origin);	/* vect: origin to pt */
    x = VDOT(vpt, bi->img_x);
    y = VDOT(vpt, bi->img_y);

    if (x < 0.0 || x > bi->img_xlen ||
	y < 0.0 || y > bi->img_ylen) {
	if (rdebug&RDEBUG_SHADE) {
	    bu_log("hit outside bounds, leaving color %g %g %g\n",
		   V3ARGS(swp->sw_color));
	}
	return;
    }


    /* get the radius of the beam at the image plane */
    radius = ap->a_rbeam + dist * ap->a_diverge;

    /*
      dx = radius * VDOT(ap->a_ray.r_dir, bi->img_x);
      dy = radius * VDOT(ap->a_ray.r_dir, bi->img_y);
    */
    dx = radius;
    dy = radius;


    xlo = x - dx;
    xhi = x + dx;

    ylo = y - dy;
    yhi = y + dy;

    CLAMP(xlo, 0.0, bi->img_xlen);
    CLAMP(xhi, 0.0, bi->img_xlen);

    CLAMP(ylo, 0.0, bi->img_ylen);
    CLAMP(yhi, 0.0, bi->img_ylen);

    vlo = (ylo / bi->img_ylen) * (bi->img_height -1);
    vhi = (yhi / bi->img_ylen) * (bi->img_height -1);

    ulo = (xlo / bi->img_xlen) * (bi->img_width -1);
    uhi = (xhi / bi->img_xlen) * (bi->img_width -1);

    if (ulo > uhi) { int i = ulo; ulo = uhi; uhi = i; }
    if (vlo > vhi) { int i = vlo; vlo = vhi; vhi = i; }

    pixels = bi->img_mf->buf;

    if (rdebug&RDEBUG_SHADE) {
	bu_log("u:%d..%d  v:%d..%d\n", ulo, uhi, vlo, vhi);
    }

    tot = (uhi - ulo + 1) * (vhi - vlo + 1); /* total # of pixels */
    color_count = 0; /* */
    VSETALL(cum_color, 0.0);
    for (v = vlo ; v <= vhi ; v++) {
	for (u = ulo ; u <= uhi ; u++) {
	    color = &pixels[v*bi->img_width*3 + u*3];
	    val = color[0]+color[1]+color[2];
	    if (val > bbd_sp->img_threshold) {
		color_count++;
		VJOIN1(cum_color, cum_color, rgb255, color);
		if (rdebug&RDEBUG_SHADE) {
		    bu_log("%d %d %d\n", V3ARGS(color));
		    VPRINT("cum_color", cum_color);
		}
	    }
	}
    }
    if (rdebug&RDEBUG_SHADE)
	bu_log("tot:%d color_count: %d\n", tot, color_count);

    if (color_count == 0)  {
	if (rdebug&RDEBUG_SHADE)
	    bu_log("no color contribution, leaving color as %g %g %g\n",
		   V3ARGS(swp->sw_color));
	return;
    }
    /* get average color: scale color by the # of contributions */
    t = 1.0 / (double)color_count;
    VSCALE(cum_color, cum_color, t);
    if (rdebug&RDEBUG_SHADE) {
	int c[3];

	VSCALE(c, cum_color, 256);
	bu_log("average color: %d %d %d\n", V3ARGS(c));
    }

    /* compute residual transmission */
    opacity = ((double)color_count / tot); /* opacity */

    t = swp->sw_transmit*opacity;
    VJOIN1(swp->sw_color, swp->sw_color, t, cum_color);

    swp->sw_transmit -= opacity;
    if (swp->sw_transmit < 0.0) swp->sw_transmit = 0.0;


}


struct imgdist {
    int status;
    double dist;
    struct bbd_img *bi;
    int index;
};

int
imgdist_compare(const void *a, const void *b)
{
    return (int)(((struct imgdist *)a)->dist - ((struct imgdist *)b)->dist);
}
/*
 *	B I L L B O A R D _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 *
 *  dp is a pointer to the shader-specific struct
 */
int
bbd_render( struct application *ap, struct partition *pp, struct shadework *swp, char *dp )
{
    register struct bbd_specific *bbd_sp = (struct bbd_specific *)dp;
    union tree *tp;
    struct bbd_img *bi;
    struct imgdist id[MAX_IMAGES];
    int i;

    /* check the validity of the arguments we got */
    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);
    CK_bbd_SP(bbd_sp);

    if (rdebug&RDEBUG_SHADE) {
	bu_struct_print( "bbd_render Parameters:",
			 bbd_print_tab, (char *)bbd_sp );
	bu_log("pixel %d %d\n", ap->a_x, ap->a_y);
	bu_log("bbd region: %s\n", pp->pt_regionp->reg_name);
    }
    tp = pp->pt_regionp->reg_treetop;
    if (tp->tr_a.tu_op != OP_SOLID) {
	bu_log("%s:%d region %s rendering bbd should have found OP_SOLID, not %d\n",
	       __FILE__, __LINE__, pp->pt_regionp->reg_name, tp->tr_a.tu_op);
	bu_bomb("\n");
    }


    swp->sw_transmit = 1.0;
    VSETALL(swp->sw_color, 0.0);
    VSETALL(swp->sw_basecolor, 1.0);

    i = 0;
    for (BU_LIST_FOR(bi, bbd_img, &bbd_sp->imgs)) {
	/* find out if the ray hits the plane */
	id[i].index = i;
	id[i].bi = bi;
	id[i].status = bn_isect_line3_plane(&id[i].dist,
					 ap->a_ray.r_pt, ap->a_ray.r_dir,
					 bi->img_plane, &ap->a_rt_i->rti_tol);
	i++;
    }

    qsort(id, bbd_sp->img_count, sizeof(id[0]), &imgdist_compare);

    for (i=0 ; i < bbd_sp->img_count && swp->sw_transmit > 0.0 ; i++) {
	if (id[i].status > 0) do_ray_image(ap, pp, swp, bbd_sp, id[i].bi, id[i].dist);
    }
    if (rdebug&RDEBUG_SHADE) {
	bu_log("color %g %g %g\n", V3ARGS(swp->sw_color));
    }
    /* shader must perform transmission/reflection calculations
     *
     * 0 < swp->sw_transmit <= 1 causes transmission computations
     * 0 < swp->sw_reflect <= 1 causes reflection computations
     */
    if (swp->sw_reflect > 0 || swp->sw_transmit > 0 ) {
	int level = ap->a_level;
	ap->a_level = 0; /* Bogus hack to keep rr_render from giving up */
	(void)rr_render( ap, pp, swp );
	ap->a_level = level;
    }
    if (rdebug&RDEBUG_SHADE) {
	bu_log("color %g %g %g\n", V3ARGS(swp->sw_color));
    }
    return(1);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
