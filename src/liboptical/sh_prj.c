/*                        S H _ P R J . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file liboptical/sh_prj.c
 *
 * Projection shader
 *
 * The one parameter to this shader is a filename.  The named file
 * contains the REAL parameters to the shader.  The v4 database format
 * is far too anemic to support this sort of shader.
 *
 */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>

#include "vmath.h"
#include "raytrace.h"
#include "optical.h"
#include "bn/plot3.h"


#define prj_MAGIC 0x70726a00	/* "prj" */
#define CK_prj_SP(_p) BU_CKMAG(_p, prj_MAGIC, "prj_specific")

struct img_specific {
    struct bu_list l;
    unsigned long junk;
    struct bu_vls i_name; /* name of object or file (depending on i_datasrc flag) */
#define IMG_SRC_FILE 'f'
#define IMG_SRC_OBJECT 'o'
#define IMG_SRC_AUTO 0
    char i_datasrc; /* is the datasource a file/object or automatic */
    struct bu_mapped_file *i_data; /* mapped file when IMG_SRC_FILE */
    struct rt_binunif_internal *i_binunifp;  /* db internal object when IMG_SRC_OBJECT */
    unsigned char *i_img;
    int i_width;
    int i_height;
    fastf_t i_viewsize;
    point_t i_eye_pt;
    quat_t i_orient;
    mat_t i_mat;		/* computed from i_orient */
    mat_t i_bn_mat_inv;	/* computed (for debug) */
    plane_t i_plane;	/* dir/plane of projection */
    mat_t i_sh_to_img;	/* transform used in prj_render() */
    char i_through;	/* ignore surface normal */
    char i_antialias;	/* anti-alias texture */
    char i_behind;	/* shade points behind img plane */
    fastf_t i_perspective;	/* perspective angle 0=ortho */
};
#define IMG_MAGIC 0x696d6700	/* "img" */
#define IMG_SPECIFIC_INIT_ZERO {BU_LIST_INIT_ZERO, 0, BU_VLS_INIT_ZERO, '\0', NULL, NULL, NULL, 0, 0, 0.0, VINIT_ZERO, HINIT_ZERO, MAT_INIT_IDN, MAT_INIT_IDN, HINIT_ZERO, MAT_INIT_IDN, '\0', '\0', '\0', 0.0}


/**
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct prj_specific {
    uint32_t magic;
    struct img_specific prj_images;
    mat_t prj_m_to_sh;
    FILE *prj_plfd;
};


/**
 * img_source_hook() is used to record where the image datasource is coming from
 * so that the image may be loaded automatically as needed from either a file or
 * from a database-embedded binary object.
 */
HIDDEN void
img_source_hook(const struct bu_structparse *UNUSED(sdp),
		const char *sp_name,
		void *base,
		const char *UNUSED(value),
		void *UNUSED(data))
{
    struct img_specific *imageSpecific = (struct img_specific *)base;
    if (bu_strncmp(sp_name, "file", 4) == 0) {
	imageSpecific->i_datasrc=IMG_SRC_FILE;
    } else if (bu_strncmp(sp_name, "obj", 3) == 0) {
	imageSpecific->i_datasrc=IMG_SRC_OBJECT;
    } else {
	imageSpecific->i_datasrc=IMG_SRC_AUTO;
    }
}


/**
 * This is a helper routine used in prj_setup() to load a projection image
 * either from a file or from a db object.
 */
HIDDEN int
img_load_datasource(struct img_specific *image, struct db_i *dbInstance, const unsigned long int size)
{
    struct directory *dirEntry;

    RT_CK_DBI(dbInstance);

    if (image == (struct img_specific *)NULL) {
	bu_bomb("ERROR: img_load_datasource() received NULL arg (struct img_specific *)\n");
    }

    bu_log("Loading image %s [%s]...", image->i_datasrc==IMG_SRC_AUTO?"from auto-determined datasource":image->i_datasrc==IMG_SRC_OBJECT?"from a database object":image->i_datasrc==IMG_SRC_FILE?"from a file":"from an unknown source (ERROR)", bu_vls_addr(&image->i_name));

    /* if the source is auto or object, we try to load the object */
    if ((image->i_datasrc==IMG_SRC_AUTO) || (image->i_datasrc==IMG_SRC_OBJECT)) {

	/* see if the object exists */
	if ((dirEntry=db_lookup(dbInstance, bu_vls_addr(&image->i_name), LOOKUP_QUIET)) == RT_DIR_NULL) {

	    /* unable to find the image object */
	    if (image->i_datasrc!=IMG_SRC_AUTO) {
		return -1;
	    }
	} else {
	    struct rt_db_internal *dbip;

	    BU_ALLOC(dbip, struct rt_db_internal);

	    RT_DB_INTERNAL_INIT(dbip);
	    RT_CK_DB_INTERNAL(dbip);
	    RT_CK_DIR(dirEntry);

	    /* the object was in the directory, so go get it */
	    if (rt_db_get_internal(dbip, dirEntry, dbInstance, NULL, NULL) <= 0) {
		/* unable to load/create the image database record object */
		return -1;
	    }

	    RT_CK_DB_INTERNAL(dbip);
	    RT_CK_BINUNIF(dbip->idb_ptr);

	    /* keep the binary object pointer */
	    image->i_binunifp=(struct rt_binunif_internal *)dbip->idb_ptr; /* make it so */

	    /* release the database struct we created */
	    RT_DB_INTERNAL_INIT(dbip);
	    bu_free(dbip, "img_load_datasource");

	    /* check size of object */
	    if (image->i_binunifp->count < size) {
		bu_log("\nWARNING: %s needs %d bytes, binary object only has %zu\n", bu_vls_addr(&image->i_name), size, image->i_binunifp->count);
	    } else if (image->i_binunifp->count > size) {
		bu_log("\nWARNING: Binary object is larger than specified image size\n\tBinary Object: %lu pixels\n"
		       "\tSpecified Image Size: %zu pixels\n...continuing to load using image subsection...",
		       (unsigned long)image->i_binunifp->count);
	    }
	    image->i_img = (unsigned char *) image->i_binunifp->u.uint8;

	}
    }

    /* if we are auto and we couldn't find a database object match, or if source
     * is explicitly a file then we load the file.
     */
    if (((image->i_datasrc==IMG_SRC_AUTO) && (image->i_binunifp==NULL)) || (image->i_datasrc==IMG_SRC_FILE)) {


	image->i_data = bu_open_mapped_file_with_path(dbInstance->dbi_filepath,	bu_vls_addr(&image->i_name), NULL);

	if (image->i_data==NULL)
	    return -1;				/* FAIL */

	if (image->i_data->buflen < size) {
	    bu_log("\nWARNING: %s needs %lu bytes, file only has %lu\n", bu_vls_addr(&image->i_name), size, image->i_data->buflen);
	} else if (image->i_data->buflen > size) {
	    bu_log("\nWARNING: Image file size is larger than specified image size\n\tInput File: %zu pixels\n\tSpecified Image Size: %lu pixels\n...continuing to load using image subsection...", image->i_data->buflen, size);
	}

	image->i_img = (unsigned char *) image->i_data->buf;
    }

    bu_log("done.\n");

    return 0;
}


/**
 * Bounds checking on perspective angle
 */
HIDDEN void
persp_hook(const struct bu_structparse *UNUSED(sdp),
	   const char *UNUSED(name),
	   void *base,
	   const char *value,
	   void *UNUSED(data))
/* structure description */
/* struct member name */
/* beginning of structure */
/* string containing value */
{
    struct img_specific *img_sp = (struct img_specific *)base;

    if (img_sp->i_perspective < 0.0) {
	bu_log("perspective %s < 0.0\n", value);
	bu_bomb("");
    }

    if (img_sp->i_perspective > 180.0) {
	bu_log("perspective %s > 180.\n", value);
	bu_bomb("");
    }

    if (!ZERO(img_sp->i_perspective))
	bu_bomb("non-ortho perspective not yet implemented!\n");
}


/**
 * Check for value < 0.0
 */
HIDDEN void
dimen_hook(const struct bu_structparse *sdp,
	   const char *UNUSED(name),
	   void *base,
	   const char *value,
	   void *UNUSED(data))
/* structure description */
/* struct member name */
/* beginning of structure */
/* string containing value */
{
    if (BU_STR_EQUAL("%f", sdp->sp_fmt)) {
	fastf_t *f;
	f = (fastf_t *)((char *)base + sdp->sp_offset);
	if (*f < 0.0) {
	    bu_log("%s value %g(%s) < 0.0\n",
		   sdp->sp_name, *f, value);
	    bu_bomb("");
	}
    } else if (BU_STR_EQUAL("%d", sdp->sp_fmt)) {
	int *i;
	i = (int *)((char *)base + sdp->sp_offset);
	if (*i < 0) {
	    bu_log("%s value %d(%s) < 0.0\n",
		   sdp->sp_name, *i, value);
	    bu_bomb("");
	}
    }
}


/**
 * This routine is responsible for duplicating the image list head to make
 * a new list element.  It used to read in pixel data for an image (this is
 * now done in the prj_setup() routine), and computes the matrix from the view
 * quaternion.
 *
 * XXX "orient" MUST ALWAYS BE THE LAST PARAMETER SPECIFIED FOR EACH IMAGE.
 */
static void
orient_hook(const struct bu_structparse *UNUSED(sdp),
	    const char *UNUSED(name),
	    void *base,
	    const char *UNUSED(value),
	    void *UNUSED(data))
/* structure description */
/* struct member name */
/* beginning of structure */
/* string containing value */
{
    struct prj_specific *prj_sp;
    struct img_specific *img_sp = (struct img_specific *)base;
    struct img_specific *img_new;
    mat_t trans, scale, tmp, xform;
    vect_t v_tmp;
    point_t p_tmp;

    BU_CK_LIST_HEAD(&img_sp->l);

    /* create a new img_specific struct,
     * copy the parameters from the "head" into the new
     * img_specific struct
     */
    BU_GET(img_new, struct img_specific);
    memcpy(img_new, img_sp, sizeof(struct img_specific));
    BU_LIST_MAGIC_SET(&img_new->l, IMG_MAGIC);
    BU_CK_VLS(&img_sp->i_name);

    /* zero the filename for the next iteration */
    bu_vls_init(&img_sp->i_name);

    /* Generate matrix from the quaternion */
    quat_quat2mat(img_new->i_mat, img_new->i_orient);


    /* compute matrix to transform region coordinates into
     * shader projection coordinates:
     *
     * prj_coord = scale * rot * translate * region_coord
     */
    MAT_IDN(trans);
    MAT_DELTAS_VEC_NEG(trans, img_new->i_eye_pt);

    MAT_IDN(scale);
    MAT_SCALE_ALL(scale, img_new->i_viewsize);

    bn_mat_mul(tmp, img_new->i_mat, trans);
    bn_mat_mul(img_new->i_sh_to_img, scale, tmp);


    VSET(v_tmp, 0.0, 0.0, 1.0);

    /* compute inverse */
    bn_mat_inv(img_new->i_bn_mat_inv, img_new->i_mat);
    bn_mat_inv(xform, img_new->i_sh_to_img);

    MAT4X3VEC(img_new->i_plane, xform, v_tmp);
    VUNITIZE(img_new->i_plane);

    if (rdebug&RDEBUG_SHADE) {
	point_t pt;

	prj_sp = (struct prj_specific *)
	    ((struct prj_specific *)base - (bu_offsetof(struct prj_specific, prj_images)));
	CK_prj_SP(prj_sp);

	if (!prj_sp->prj_plfd)
	    bu_bomb("prj shader prj_plfd should be open\n");

	/* plot out the extent of the image frame */
	pl_color(prj_sp->prj_plfd, 255, 0, 0);

	VSET(v_tmp, -0.5, -0.5, 0.0);
	MAT4X3PNT(pt, xform, v_tmp);
	pdv_3move(prj_sp->prj_plfd, pt);

	VSET(v_tmp, 0.5, -0.5, 0.0);
	MAT4X3PNT(p_tmp, xform, v_tmp);
	pdv_3cont(prj_sp->prj_plfd, p_tmp);

	VSET(v_tmp, 0.5, 0.5, 0.0);
	MAT4X3PNT(p_tmp, xform, v_tmp);
	pdv_3cont(prj_sp->prj_plfd, p_tmp);

	VSET(v_tmp, -0.5, 0.5, 0.0);
	MAT4X3PNT(p_tmp, xform, v_tmp);
	pdv_3cont(prj_sp->prj_plfd, p_tmp);

	pdv_3cont(prj_sp->prj_plfd, pt);

	VSET(v_tmp, 0.0, 0.0, 0.0);
	MAT4X3PNT(p_tmp, xform, v_tmp);
	pdv_3move(prj_sp->prj_plfd, p_tmp);
	VREVERSE(pt, img_new->i_plane);
	VADD2(p_tmp, p_tmp, pt);
	pdv_3cont(prj_sp->prj_plfd, p_tmp);

    }

    /* read in the pixel data now happens in prj_setup() */
    /* we add an image to the list of images regardless of whether the data is valid or not */
    BU_LIST_APPEND(&img_sp->l, &img_new->l);
}


#define IMG_O(m) bu_offsetof(struct img_specific, m)


/** description of how to parse/print the arguments to the shader.
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse img_parse_tab[] = {
    {"%V",	1, "image",		IMG_O(i_name),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%V",	1, "file",		IMG_O(i_name),		img_source_hook, NULL, NULL },
    {"%V",	1, "obj",		IMG_O(i_name),		img_source_hook, NULL, NULL },
    {"%V",	1, "object",		IMG_O(i_name),		img_source_hook, NULL, NULL },
    {"%d",	1, "w",			IMG_O(i_width),		dimen_hook, NULL, NULL },
    {"%d",	1, "n",			IMG_O(i_height),	dimen_hook, NULL, NULL },
    {"%f",	1, "viewsize",		IMG_O(i_viewsize),	dimen_hook, NULL, NULL },
    {"%f",	3, "eye_pt",		IMG_O(i_eye_pt),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	4, "orientation",	IMG_O(i_orient),	orient_hook, NULL, NULL },
    {"%c",	1, "through",		IMG_O(i_through),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%c",	1, "antialias",		IMG_O(i_antialias),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%c",	1, "behind",		IMG_O(i_behind),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1, "perspective",	IMG_O(i_perspective),	persp_hook, NULL, NULL },
    {"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};
struct bu_structparse img_print_tab[] = {
    {"%p", 1, "img_parse_tab", bu_byteoffset(img_parse_tab[0]), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	4, "i_plane",		IMG_O(i_plane),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


HIDDEN int prj_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *mfp, struct rt_i *rtip);
HIDDEN int prj_render(struct application *ap, const struct partition *pp, struct shadework *swp, void *dp);
HIDDEN void prj_print(register struct region *rp, void *dp);
HIDDEN void prj_free(void *cp);

/**
 * The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs prj_mfuncs[] = {
    {MF_MAGIC,	"prj",		0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
     prj_setup,	prj_render,	prj_print,	prj_free },

    {0,		(char *)0,	0,		0,		0,
     0,		0,		0,		0 }
};


/**
 * This routine is called (at prep time)
 * once for each region which uses this shader.
 * Any shader-specific initialization should be done here.
 */
HIDDEN int
prj_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *rtip)
/* pointer to reg_udata in *rp */
/* New since 4.4 release */
{
    /* we use this to initialize new img_specific objects */
    static const struct img_specific IMG_INIT = IMG_SPECIFIC_INIT_ZERO;

    struct prj_specific *prj_sp;
    struct img_specific *img_sp;

    struct bu_vls parameter_data = BU_VLS_INIT_ZERO;
    struct bu_mapped_file *parameter_file;

    /* check the arguments */
    RT_CHECK_RTI(rtip);
    BU_CK_VLS(matparm);
    RT_CK_REGION(rp);

    if (rdebug&RDEBUG_SHADE)
	bu_log("prj_setup(%s) matparm:\"%s\"\n",
	       rp->reg_name, bu_vls_addr(matparm));

    /* Get memory for the shader parameters and shader-specific data */
    BU_GET(prj_sp, struct prj_specific);
    *dpp = prj_sp;

    prj_sp->magic = prj_MAGIC;
    memcpy(&prj_sp->prj_images, &IMG_INIT, sizeof(struct img_specific));
    BU_LIST_INIT(&prj_sp->prj_images.l);

    if (rdebug&RDEBUG_SHADE) {
	if ((prj_sp->prj_plfd=fopen("prj.plot3", "wb")) == (FILE *)NULL) {
	    bu_log("ERROR creating plot3 file prj.plot3");
	}
    } else {
	prj_sp->prj_plfd = (FILE *)NULL;
    }

    if (! *(bu_vls_addr(matparm))) {
	bu_log("ERROR: Null projection shader file or options?\n");
	return -1;
    }

    /* first we try to open the specified argument as a file, as previously implemented.
     * if it succeeds, then the file contents become the parameter data.  Otherwise, the
     * argument string considered the parameter data.
     */

    parameter_file = bu_open_mapped_file(bu_vls_addr(matparm), (char *)NULL);

    if (parameter_file) {
	/* the file loaded, so the contents become the parameter string */
	bu_log("Filename: %s\n", bu_vls_addr(matparm));

	bu_vls_strncpy(&parameter_data, (char *)parameter_file->buf,
		       parameter_file->buflen);

	if (rdebug&RDEBUG_SHADE) {
	    bu_log("parsing: %s\n", bu_vls_addr(&parameter_data));
	}

	bu_close_mapped_file(parameter_file);
    } else {
	/* the file did not load, so the shader args become the param string */
	bu_log("Parameters: %s\n", bu_vls_addr(matparm));

	bu_vls_strncpy (&parameter_data, bu_vls_addr(matparm), bu_vls_strlen(matparm));
    }

    /* set defaults on img_specific struct */
    prj_sp->prj_images.i_width = prj_sp->prj_images.i_height = 512;
    prj_sp->prj_images.i_antialias = '1';
    prj_sp->prj_images.i_through = '0';
    prj_sp->prj_images.i_behind = '0';
    prj_sp->prj_images.i_datasrc = IMG_SRC_AUTO;

    if (bu_struct_parse(&parameter_data, img_parse_tab,
			(char *)&prj_sp->prj_images, NULL) < 0) {
	bu_log("ERROR: Unable to properly parse projection shader parameters\n");
	return -1;
    }

    bu_vls_free(&parameter_data);

    /* load the image data for any specified images */
    for (BU_LIST_FOR(img_sp, img_specific, &prj_sp->prj_images.l)) {
	if (img_load_datasource(img_sp, rtip->rti_dbip, img_sp->i_width * img_sp->i_height * 3) < 0) {
	    bu_log("\nERROR: prj_setup() %s %s could not be loaded [source was %s]\n",
		   rp->reg_name, bu_vls_addr(&img_sp->i_name),
		   img_sp->i_datasrc==IMG_SRC_OBJECT?"object":img_sp->i_datasrc==IMG_SRC_FILE?"file":"auto");

	    /* skip this one */
	    img_sp->i_through='0';
	    HREVERSE(img_sp->i_plane, img_sp->i_plane);

	    return -1;
	}
    }

    /* if even one of the images is to be anti-aliased, then we need
     * to set the rti_prismtrace flag so that we can compute the exact
     * extent of the pixel.
     */
    for (BU_LIST_FOR(img_sp, img_specific, &prj_sp->prj_images.l)) {
	if (img_sp->i_antialias != '0') {
	    if (rdebug&RDEBUG_SHADE)
		bu_log("prj_setup(%s) setting prismtrace 1\n", rp->reg_name);
	    rtip->rti_prismtrace = 1;
	    break;
	}
    }

    /* The shader needs to operate in a coordinate system which stays
     * fixed on the region when the region is moved (as in animation)
     * we need to get a matrix to perform the appropriate transform(s).
     *
     * db_region_mat returns a matrix which maps points on/in the region
     * as it exists where the region is defined (as opposed to the
     * (possibly transformed) one we are rendering.
     *
     * Non-PARALLEL, which is OK, because shaders are prepped serially.
     */
    db_region_mat(prj_sp->prj_m_to_sh, rtip->rti_dbip, rp->reg_name, &rt_uniresource);


    if (rdebug&RDEBUG_SHADE) {

	prj_print(rp, (char *)prj_sp);
    }

    return 1;
}


HIDDEN void
prj_print(register struct region *rp, void *dp)
{
    struct prj_specific *prj_sp = (struct prj_specific *)dp;
    struct img_specific *img_sp;

    for (BU_LIST_FOR(img_sp, img_specific, &prj_sp->prj_images.l)) {
	bu_struct_print(rp->reg_name, img_print_tab, (char *)img_sp);
    }
}


HIDDEN void
prj_free(void *cp)
{
    struct prj_specific *prj_sp = (struct prj_specific *)cp;

    struct img_specific *img_sp;

    while (BU_LIST_WHILE(img_sp, img_specific, &prj_sp->prj_images.l)) {

	img_sp->i_img = (unsigned char *)0;
	if (img_sp->i_data) bu_close_mapped_file(img_sp->i_data);
	img_sp->i_data = (struct bu_mapped_file *)NULL; /* sanity */
	if (img_sp->i_binunifp) rt_binunif_free(img_sp->i_binunifp);
	img_sp->i_binunifp = (struct rt_binunif_internal *)NULL; /* sanity */
	bu_vls_vlsfree(&img_sp->i_name);

	BU_LIST_DEQUEUE(&img_sp->l);
	BU_PUT(img_sp, struct img_specific);
    }

    if (prj_sp->prj_plfd) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fclose(prj_sp->prj_plfd);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    BU_PUT(cp, struct prj_specific);
}
HIDDEN const double cs = (1.0/255.0);
HIDDEN const point_t delta = {0.5, 0.5, 0.0};


HIDDEN int
project_point(point_t sh_color, struct img_specific *img_sp, struct prj_specific *prj_sp, point_t r_pt)
{
    int x, y;
    point_t sh_pt;
    point_t tmp_pt;
    unsigned char *pixel;

    MAT4X3PNT(sh_pt, img_sp->i_sh_to_img, r_pt);
    VADD2(sh_pt, sh_pt, delta);

    if (rdebug&RDEBUG_SHADE) {
	VPRINT("sh_pt", sh_pt);
    }

    /* make sure we have an image to project into */
    if (img_sp->i_width <= 0 || img_sp->i_height <= 0) {
	return 1;
    }

    x = sh_pt[X] * (img_sp->i_width-1);
    y = sh_pt[Y] * (img_sp->i_height-1);

    if (x<0 || y<0
	|| (x*3 + y*img_sp->i_width*3) < 0
	|| (x*3 + y*img_sp->i_width*3) > (img_sp->i_width * img_sp->i_height * 3))
    {
	static int count = 0;
	static int suppressed = 0;
	if (count++ < 10) {
	    bu_log("INTERNAL ERROR: projection point is invalid\n");
	} else {
	    if (!suppressed) {
		suppressed++;
		bu_log("INTERNAL ERROR: suppressing further project point error messages\n");
	    }
	}
	return 1;
    }

    pixel = &img_sp->i_img[x*3 + y*img_sp->i_width*3];

    if (x >= img_sp->i_width || x < 0 ||
	y >= img_sp->i_height || y < 0 ||
	((img_sp->i_behind == '0' && sh_pt[Z] > 0.0))) {
	/* we're out of bounds,
	 * leave the color alone
	 */
	return 1;
    }

    if (rdebug&RDEBUG_SHADE && prj_sp->prj_plfd) {
	/* plot projection direction */
	pl_color(prj_sp->prj_plfd, V3ARGS(pixel));
	pdv_3move(prj_sp->prj_plfd, r_pt);
	VMOVE(tmp_pt, r_pt);

	VSCALE(tmp_pt, img_sp->i_plane, -sh_pt[Z]);
	VADD2(tmp_pt, r_pt, tmp_pt);
	pdv_3cont(prj_sp->prj_plfd, tmp_pt);
    }
    VMOVE(sh_color, pixel);	/* int/float conversion */
    return 0;
}


/**
 * This is called (from viewshade() in shade.c) once for each hit point
 * to be shaded.  The purpose here is to fill in values in the shadework
 * structure.
 */
int
prj_render(struct application *ap, const struct partition *pp, struct shadework *swp, void *dp)
/* defined in material.h */
/* ptr to the shader-specific struct */
{
    register struct prj_specific *prj_sp =
	(struct prj_specific *)dp;
    point_t r_pt;
    plane_t r_N;
    int i, status;
    struct img_specific *img_sp;
    point_t sh_color;
    point_t final_color;
    point_t tmp_pt;
    fastf_t divisor;
    struct pixel_ext r_pe;	/* region coord version of ap->a_pixelext */
    fastf_t dist;
    fastf_t weight;


    /* check the validity of the arguments we got */
    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);
    CK_prj_SP(prj_sp);

    if (rdebug&RDEBUG_SHADE) {
	bu_log("shading with prj\n");
	prj_print(pp->pt_regionp, dp);
    }
    /* If we are performing the shading in "region" space, we must
     * transform the hit point from "model" space to "region" space.
     * See the call to db_region_mat in prj_setup().
     */
    MAT4X3PNT(r_pt, prj_sp->prj_m_to_sh, swp->sw_hit.hit_point);
    MAT4X3VEC(r_N, prj_sp->prj_m_to_sh, swp->sw_hit.hit_normal);


    if (rdebug&RDEBUG_SHADE) {
	bu_log("prj_render() model:(%g %g %g) shader:(%g %g %g)\n",
	       V3ARGS(swp->sw_hit.hit_point),
	       V3ARGS(r_pt));
    }


    VSET(final_color, 0.0, 0.0, 0.0);
    divisor = 0.0;

    if (ap->a_pixelext) {

	BU_CK_PIXEL_EXT(ap->a_pixelext);

	/* We need to compute the extent of the pixel on an
	 * imaginary plane through the hit point with the same
	 * normal as the surface normal at the hit point.  Later
	 * this quadrilateral will be projected onto the image
	 * plane(s) to facilitate anti-aliasing.
	 */

	/* compute region coordinates for pixel extent */
	for (i = 0; i < CORNER_PTS; i++) {
	    MAT4X3PNT(r_pe.corner[i].r_pt,
		      prj_sp->prj_m_to_sh,
		      ap->a_pixelext->corner[i].r_pt);
	    MAT4X3VEC(r_pe.corner[i].r_dir,
		      prj_sp->prj_m_to_sh,
		      ap->a_pixelext->corner[i].r_dir);
	}

	/* compute plane of hit point */
	VUNITIZE(r_N);
	r_N[H] = VDOT(r_N, r_pt);

	/* project corner points into plane of hit point */
	for (i = 0; i < CORNER_PTS; i++) {
	    dist = 0.0;
	    status = bn_isect_line3_plane(
		&dist,
		r_pe.corner[i].r_pt,
		r_pe.corner[i].r_dir,
		r_N,
		&(ap->a_rt_i->rti_tol));

	    if (rdebug&RDEBUG_SHADE) {
		/* status will be <= 0 when the image was not loaded */
		if (status <= 0) {
		    /* XXX What to do if we don't
		     * hit plane?
		     */
		    bu_log("%s:%d The unthinkable has happened\n",
			   __FILE__, __LINE__);
		}
	    }

	    VJOIN1(r_pe.corner[i].r_pt,
		   r_pe.corner[i].r_pt,
		   dist,
		   r_pe.corner[i].r_dir);
	}
    }


    for (BU_LIST_FOR(img_sp, img_specific, &prj_sp->prj_images.l)) {
	if (img_sp->i_through == '0' && VDOT(r_N, img_sp->i_plane) < 0.0) {
	    /* normal and projection dir don't match, skip on */

	    if (rdebug&RDEBUG_SHADE && prj_sp->prj_plfd) {
		/* plot hit normal */
		pl_color(prj_sp->prj_plfd, 255, 255, 255);
		pdv_3move(prj_sp->prj_plfd, r_pt);
		VADD2(tmp_pt, r_pt, r_N);
		pdv_3cont(prj_sp->prj_plfd, tmp_pt);

		/* plot projection direction */
		pl_color(prj_sp->prj_plfd, 255, 255, 0);
		pdv_3move(prj_sp->prj_plfd, r_pt);
		VADD2(tmp_pt, r_pt, img_sp->i_plane);
		pdv_3cont(prj_sp->prj_plfd, tmp_pt);
	    }
	    continue;
	}

	if (project_point(sh_color, img_sp, prj_sp, r_pt))
	    continue;

	VSCALE(sh_color, sh_color, cs);
	weight = VDOT(r_N, img_sp->i_plane);
	if (img_sp->i_through != '0')
	    weight = (weight < 0.0 ? -weight : weight);
	if (weight > 0.0) {
	    VJOIN1(final_color, final_color, weight, sh_color);
	    divisor += weight;
	}
    }

    if (divisor > 0.0) {
	divisor = 1.0 / divisor;
	VSCALE(swp->sw_color, final_color, divisor);
    }
    return 1;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
