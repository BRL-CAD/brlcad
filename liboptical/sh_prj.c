/*
 *	S H _ P R J . C
 *
 *	Projection shader
 *
 *	The one parameter to this shader is a filename.  The named file
 *	contains the REAL parameters to the shader.  The v4 database format
 *	is far too anemic to support this sort of shader.
 *
 */
#include "conf.h"

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "shadefuncs.h"
#include "shadework.h"
#include "../rt/mathtab.h"
#include "../rt/rdebug.h"

#define prj_MAGIC 0x1834    /* make this a unique number for each shader */
#define CK_prj_SP(_p) RT_CKMAG(_p, prj_MAGIC, "prj_specific")

struct image {
	bu_list	l;
	struct bu_vls	i_filename;
	unsigned char	*i_image;
	unsigned	i_width;
	unsigned	i_height;
	fastf_t		i_viewsize;
	point_t		i_eye;
	quat_t		i_orient;
	mat_t		i_m;
}

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct prj_specific {
	long	magic;	/* magic # for memory validity check, must come 1st */
	struct bu_vls	*prj_img_filename;
	unsigned char	*prj_image;
	int		prj_img_width;
	int		prj_img_height;
	fastf_t		prj_viewsize;
	point_t		prj_eye;
	mat_t		prj_sh_to_v;
	mat_t		prj_m;
	mat_t		prj_m_inv;
	vect_t		prj_dir;	/* direction of projection */
	point_t		prj_min;	/* bounding box min */
	point_t		prj_max;	/* bounding box max */
	mat_t		prj_m_to_sh;	/* region space xform */
	FILE 		*prj_plfd;
};

/* The default values for the variables in the shader specific structure */
CONST static
struct prj_specific prj_defaults = {
	prj_MAGIC,
	(struct bu_vls *)0,
	(unsigned char *)0,
	512,
	512,
	1.0,
	{ 1.0, 1.0, 1.0 },		/* prj_eye */
	{	0.0, 0.0, 0.0, 0.0,	/* prj_sh_to_v */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 },
	{	0.0, 0.0, 0.0, 0.0,	/* prj_m */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 },
	{	0.0, 0.0, 0.0, 0.0,	/* prj_m_inv */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 },
	{ 0.0, 0.0, 0.0 },		/* prj_dir */
	{ 0.0, 0.0, 0.0 },		/* prj_min */
	{ 0.0, 0.0, 0.0 },		/* prj_max */
	{	0.0, 0.0, 0.0, 0.0,	/* prj_m_to_sh */
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0 },
	(FILE *)0			/* prj_plfd */
	};

#define SHDR_NULL	((struct prj_specific *)0)
#define SHDR_O(m)	offsetof(struct prj_specific, m)
#define SHDR_AO(m)	offsetofarray(struct prj_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse prj_print_tab[] = {
	{"%d",  1, "img_width",		SHDR_O(prj_img_width),	FUNC_NULL },
	{"%d",  1, "img_height",	SHDR_O(prj_img_height),	FUNC_NULL },
	{"%f",  1, "view_size",		SHDR_O(prj_viewsize),	FUNC_NULL },
	{"%f",  3, "view_eye",		SHDR_AO(prj_eye),	FUNC_NULL },
	{"%f",  3, "view_dir",		SHDR_AO(prj_dir),	FUNC_NULL },
	{"%f",  16,"view_sh_to_v",	SHDR_AO(prj_sh_to_v),	FUNC_NULL },
	{"%f",  16,"view_m",		SHDR_AO(prj_m),	FUNC_NULL },
	{"%f",  3, "shader_max",	SHDR_AO(prj_min),	FUNC_NULL },
	{"%f",  3, "shader_min",	SHDR_AO(prj_max),	FUNC_NULL },
	{"%f",  16,"shader_mat",	SHDR_AO(prj_m_to_sh),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }

};
struct bu_structparse prj_parse_tab[] = {
	{"i",	bu_byteoffset(prj_print_tab[0]), "prj_print_tab", 0, FUNC_NULL },
	{"%d",  1, "w",			SHDR_O(prj_img_width),	FUNC_NULL },
	{"%d",  1, "n",			SHDR_O(prj_img_height),	FUNC_NULL },
	{"%f",  1, "vs",		SHDR_O(prj_viewsize),	FUNC_NULL },
	{"%f",  3, "ve",		SHDR_AO(prj_eye),	FUNC_NULL },
	{"%f",  16, "vm",		SHDR_AO(prj_sh_to_v),	FUNC_NULL },
	{"%f",  16, "sm",		SHDR_AO(prj_m_to_sh),	FUNC_NULL },
	{"",	0, (char *)0,		0,			FUNC_NULL }
};

HIDDEN int	prj_setup(), prj_render();
HIDDEN void	prj_print(), prj_free();

/* The "mfuncs" structure defines the external interface to the shader.
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

static void
mp(s, m)
char *s;
mat_t m;
{
	int i;
	fprintf(stderr, "Matrix %s:\n", s);
	fprintf(stderr, "%lg %lg %lg %lg\n", m[0], m[1], m[2], m[3]);
	fprintf(stderr, "%lg %lg %lg %lg\n", m[4], m[5], m[6], m[7]);
	fprintf(stderr, "%lg %lg %lg %lg\n", m[8], m[9], m[10], m[11]);
	fprintf(stderr, "%lg %lg %lg %lg\n", m[12], m[13], m[14], m[15]);
}
static int
get_actual_parameters(prj_sp, matparm)
struct prj_specific *prj_sp;
struct rt_vls		*matparm;
{
	FILE *fd;
	char *fname = "prj.txt";
	char *img_file = "../pix/m35.pix";
	int i, n;

	
	if( rdebug&RDEBUG_SHADE) {
		bu_log("debug matparm:%s\n", bu_vls_addr(matparm));
	}


	bu_log("setting image file %s\n", img_file);


	prj_sp->prj_img_filename = bu_vls_vlsinit();

	bu_vls_strcpy(prj_sp->prj_img_filename, img_file);
	bu_log("set image file %s\n", bu_vls_addr(prj_sp->prj_img_filename));


	bu_semaphore_acquire( BU_SEM_SYSCALL );
	if ((fd=fopen(fname, "r")) == NULL) {
		fprintf(stderr, "Error opening parameter file \"%s\": %s\n",
			fname, strerror(errno));
		bu_semaphore_release( BU_SEM_SYSCALL );
		return(-1);
	}

	if (fscanf(fd, "%lg", &prj_sp->prj_viewsize) != 1) {
		fprintf(stderr, "error scanning viewsize\n");
		bu_semaphore_release( BU_SEM_SYSCALL );
		bu_bomb("prj shader error");
	}
	if (fscanf(fd, "%lg %lg %lg", &prj_sp->prj_eye[0], 
		&prj_sp->prj_eye[1], &prj_sp->prj_eye[2]) != 3) {
		fprintf(stderr, "error scanning eye point\n");
		bu_semaphore_release( BU_SEM_SYSCALL );
		bu_bomb("prj shader error");
	}

	for (i=0 ; i < ELEMENTS_PER_MAT ; i++) {
		if (fscanf(fd, "%lg", &prj_sp->prj_m[i]) != 1) {
			fprintf(stderr, "error scanning matrix element %d\n", i);
			bu_semaphore_release( BU_SEM_SYSCALL );
			bu_bomb("prj shader error");
		}
	}

	fclose(fd);

	fprintf(stderr, "opening %dx%dimage file %s\n",
		prj_sp->prj_img_width,
		prj_sp->prj_img_height,
		bu_vls_addr(prj_sp->prj_img_filename));

	if ( (fd=fopen(bu_vls_addr(prj_sp->prj_img_filename), "r")) == NULL) {
		fprintf(stderr, "Error opening image file \"%s\": %s\n",
			fname, strerror(errno));
		bu_semaphore_release( BU_SEM_SYSCALL );
		return(-1);
	}
	
	n = prj_sp->prj_img_width * prj_sp->prj_img_height;

	bu_semaphore_release( BU_SEM_SYSCALL );
	prj_sp->prj_image = (unsigned char *)bu_malloc(n * 3, "prj image");
	bu_semaphore_acquire( BU_SEM_SYSCALL );

	fprintf(stderr, "reading %dx%dimage file %s\n",
		prj_sp->prj_img_width,
		prj_sp->prj_img_height,
		bu_vls_addr(prj_sp->prj_img_filename));

	i = fread(prj_sp->prj_image, 3, n, fd);

	if (i != n) {
		fprintf(stderr, "Error reading %dx%d image file\n\t(got %d pixels, not %d)\n",
			 prj_sp->prj_img_width, prj_sp->prj_img_height, i, n);
		bu_semaphore_release( BU_SEM_SYSCALL );
		return(-1);
	}

	fclose(fd);


	if( rdebug&RDEBUG_SHADE) {
		prj_sp->prj_plfd = fopen("prj.pl", "w");
		if ( ! prj_sp->prj_plfd ) {
			fprintf(stderr, "Error opening plot file \"%s\": %s\n",
				"prj.pl", strerror(errno));
			bu_semaphore_release( BU_SEM_SYSCALL );
			return(-1);
		}
	}


	bu_semaphore_release( BU_SEM_SYSCALL );


	return 0;
}

/*	P R J _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
prj_setup( rp, matparm, dpp, mfp, rtip)
register struct region	*rp;
struct rt_vls		*matparm;
char			**dpp;	/* pointer to reg_udata in *rp */
struct mfuncs		*mfp;
struct rt_i		*rtip;	/* New since 4.4 release */
{
	register struct prj_specific	*prj_sp;
	mat_t	trans, rot, scale, tmp;
	vect_t	bb_min, bb_max, v_tmp;
	struct stat sb;
	int i;
	struct bu_vls	param_buf;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	RT_VLS_CHECK( matparm );
	RT_CK_REGION(rp);

	if( rdebug&RDEBUG_SHADE)
		rt_log("prj_setup(%s)\n", rp->reg_name);

	/* Get memory for the shader parameters and shader-specific data */
	GETSTRUCT( prj_sp, prj_specific );
	*dpp = (char *)prj_sp;

	/* initialize the default values for the shader */
	memcpy(prj_sp, &prj_defaults, sizeof(struct prj_specific) );

	if (get_actual_parameters(prj_sp, matparm)) return -1;


	/* The shader needs to operate in a coordinate system which stays
	 * fixed on the region when the region is moved (as in animation)
	 * we need to get a matrix to perform the appropriate transform(s).
	 *
	 * db_region_mat returns a matrix which maps points on/in the region
	 * as it exists where the region is defined (as opposed to the 
	 * (possibly transformed) one we are rendering.
	 */
	db_region_mat(prj_sp->prj_m_to_sh, rtip->rti_dbip, rp->reg_name);


	/* compute a direction vector indicating the direction toward
	 * the plane of projection.
	 */
	VSET(v_tmp, 0.0, 0.0, 1.0);
	mat_inv(prj_sp->prj_m_inv, prj_sp->prj_m);	
	MAT4X3VEC(prj_sp->prj_dir, prj_sp->prj_m_inv, v_tmp);


	/* compute matrix to transform shader coordinates into projection 
	 * coordinates
	 */
	bn_mat_idn(trans);
	MAT_DELTAS_VEC_NEG(trans, prj_sp->prj_eye);

	bn_mat_idn(scale);
	MAT_SCALE_ALL(scale, prj_sp->prj_viewsize);

	mat_mul(tmp, prj_sp->prj_m, trans);
	mat_mul(prj_sp->prj_sh_to_v, scale, tmp);


	if( rdebug&RDEBUG_SHADE) {
		bu_struct_print( " Parameters:", prj_print_tab, (char *)prj_sp );
		mat_print( "m_to_sh", prj_sp->prj_m_to_sh );
	}

	return(1);
}

/*
 *	P R J _ P R I N T
 */
HIDDEN void
prj_print( rp, dp )
register struct region *rp;
char	*dp;
{
	bu_struct_print( rp->reg_name, prj_print_tab, (char *)dp );
}

/*
 *	P R J _ F R E E
 */
HIDDEN void
prj_free( cp )
char *cp;
{
	struct prj_specific *prj_sp = (struct prj_specific *)cp;

	if ( prj_sp->prj_plfd ) {
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		fclose( prj_sp->prj_plfd );
		bu_semaphore_release( BU_SEM_SYSCALL );
	}

	rt_free( cp, "prj_specific" );
}

/*
 *	P R J _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
prj_render( ap, pp, swp, dp )
struct application	*ap;
struct partition	*pp;
struct shadework	*swp;	/* defined in material.h */
char			*dp;	/* ptr to the shader-specific struct */
{
	register struct prj_specific *prj_sp =
		(struct prj_specific *)dp;
	point_t r_pt;
	vect_t	r_N;
	point_t	dir_pt;	/* for plotting prj_dir */
	point_t sh_pt;
	vect_t	sh_N;
	vect_t	sh_color;
	static const double	cs = (1.0/255.0);
	char buf[32];
	static int fileno = 0;
	FILE *fd;
	mat_t	trans_o, trans_p, rot, scale, tmp, xform;
	point_t	img_v;
	const static point_t delta = {0.5, 0.5, 0.0};
	int x, y;
	unsigned char *pixel;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_prj_SP(prj_sp);

	if( rdebug&RDEBUG_SHADE)
		bu_struct_print( "prj_render Parameters:", prj_print_tab, (char *)prj_sp );

	/* If we are performing the shading in "region" space, we must 
	 * transform the hit point from "model" space to "region" space.
	 * See the call to db_region_mat in prj_setup().
	 */
	MAT4X3PNT(r_pt, prj_sp->prj_m_to_sh, swp->sw_hit.hit_point);
	MAT4X3VEC(r_N, prj_sp->prj_m_to_sh, swp->sw_hit.hit_normal);

	


	if( rdebug&RDEBUG_SHADE) {
		rt_log("prj_render()  model:(%g %g %g) shader:(%g %g %g)\n", 
		V3ARGS(swp->sw_hit.hit_point),
		V3ARGS(r_pt) );
	}


	/* If the normal is not even vaguely in the direction of the 
	 * projection plane, do nothing.
	 */
	if (VDOT(r_N, prj_sp->prj_dir) < 0.0) {
		if( rdebug&RDEBUG_SHADE)
			bu_log("vectors oppose, shader abort\n");
		VSET(swp->sw_color, 1.0, 1.0, 0.0);


		if (prj_sp->prj_plfd) {
			/* plot hit normal */
			pl_color(prj_sp->prj_plfd, 255, 255, 255);
			pdv_3move(prj_sp->prj_plfd, r_pt);
			VADD2(dir_pt, r_pt, r_N);
			pdv_3cont(prj_sp->prj_plfd, dir_pt);

			/* plot projection direction */
			pl_color(prj_sp->prj_plfd, 255, 255, 0);
			pdv_3move(prj_sp->prj_plfd, r_pt);
			VADD2(dir_pt, r_pt, prj_sp->prj_dir);
			pdv_3cont(prj_sp->prj_plfd, dir_pt);
		}
		return 1;
	}

	MAT4X3PNT(sh_pt, prj_sp->prj_sh_to_v, r_pt);

	if( rdebug&RDEBUG_SHADE) {
		VPRINT("sh_pt", sh_pt);
	}

	VADD2(img_v, sh_pt, delta);

	x = img_v[X] * (prj_sp->prj_img_width-1);
	y = img_v[Y] * (prj_sp->prj_img_height-1);

	pixel = &prj_sp->prj_image[x*3 + y*prj_sp->prj_img_width*3];

	if (x >= prj_sp->prj_img_width || x < 0) {
		VSET(swp->sw_color, 1.0, .0, .0);

		if( rdebug&RDEBUG_SHADE)
			bu_log("x coord test failed 0 > %d <= %u, shader abort\n",
				x, prj_sp->prj_img_width);
		
		if (prj_sp->prj_plfd) {
			/* plot projection direction */
			pl_color(prj_sp->prj_plfd, 255, 0, 0);
			pdv_3move(prj_sp->prj_plfd, r_pt);
			VADD2(dir_pt, r_pt, prj_sp->prj_dir);
			pdv_3cont(prj_sp->prj_plfd, dir_pt);
		}
		return 1;
	}
	if (y >= prj_sp->prj_img_height || y < 0) {
		VSET(swp->sw_color, 1.0, 0.0, 1.0);
		if( rdebug&RDEBUG_SHADE)
			bu_log("y coord test failed 0 > %d <= %u, shader abort\n",
				y, prj_sp->prj_img_height);

		if (prj_sp->prj_plfd) {
			/* plot projection direction */
			pl_color(prj_sp->prj_plfd, 255, 0, 255);
			pdv_3move(prj_sp->prj_plfd, r_pt);
			VADD2(dir_pt, r_pt, prj_sp->prj_dir);
			pdv_3cont(prj_sp->prj_plfd, dir_pt);
		}
		return 1;
	}


	if( rdebug&RDEBUG_SHADE) {
		bu_log("pixel (%g,%g) %d,%d  (%u %u %u)\n",
			 img_v[X], img_v[Y], x, y, 
			 (unsigned)pixel[0],
			 (unsigned)pixel[1],
			 (unsigned)pixel[2]);

	}

	VMOVE(sh_color, pixel);	/* int/float conversion */
	VSCALE(swp->sw_color, sh_color, cs);

	return 1;
}
