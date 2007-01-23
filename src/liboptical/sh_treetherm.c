/*                  S H _ T R E E T H E R M . C
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
/** @file sh_treetherm.c
 *
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
#include "rtprivate.h"
#include "rtgeom.h"

#define tthrm_MAGIC 0x7468726d	/* 'thrm' */
#define CK_tthrm_SP(_p) BU_CKMAG(_p, tthrm_MAGIC, "tthrm_specific")


/*
 * The thermal data file starts with a long indicating the number of
 * "cylinder" structures that follow.  The cyliner structure consists
 * of 18 bytes in 5 entries.  First is a 2 byte short integer indicating the
 * number of "segments" that follow.  Max Lorenzo promises this value is
 * always "1".  A segment consists of n tuples.  Max Lorenzo promises that
 * "n" is always 8 (it's a compile-time option to treetherm).  Each tuple
 * consists of 4 (4-byte) float values: x, y, z, temperature.
 *
 * Because there is always 1 segment, the cylinder is a constant 130 bytes.
 *
 * So the file looks like:
 *
 *	long	#_of_cyls		# Beware:  may be 4 or 8 bytes!!!!
 *
 *	short	segs	(always 1)	# 2 bytes
 *	float	x,y,z,temperature	# 4 X 4 = 16 bytes
 *	float	x,y,z,temperature	# 4 X 4 = 16 bytes
 *	float	x,y,z,temperature	# 4 X 4 = 16 bytes
 *	float	x,y,z,temperature	# 4 X 4 = 16 bytes
 *	float	x,y,z,temperature	# 4 X 4 = 16 bytes
 *	float	x,y,z,temperature	# 4 X 4 = 16 bytes
 *	float	x,y,z,temperature	# 4 X 4 = 16 bytes
 *	float	x,y,z,temperature	# 4 X 4 = 16 bytes
 *
 *	short	segs	(always 1)
 *	float	x,y,z,temperature
 *	float	x,y,z,temperature
 *	float	x,y,z,temperature
 *	float	x,y,z,temperature
 *	float	x,y,z,temperature
 *	float	x,y,z,temperature
 *	float	x,y,z,temperature
 *	float	x,y,z,temperature
 *
 *	.
 *	.
 *	.
 *	.
 */
/* return byte offset into file of "float x" in n'th cyl structure */


struct branch_seg {
	struct bu_list		bs_siblings;
	struct branch_seg	*bs_next;	/* toward leaves/ends/tips */
	struct branch_seg	*bs_prev;	/* toward root/source */

	point_t			bs_start;	/* location of segment start */
	vect_t			bs_dir;		/* direction of segment */
	double			bs_length;	/* length of segment */
	double			bs_sradius;	/* start radius */
	double			bs_eradius;	/* end radius */
	double			bs_dist;	/* total distance from root */
	float			*bs_nodes[4];	/* point+temp for nodes */
};


#define NUM_NODES 8
#define THRM_SEG_MAGIC 246127
#define CK_THRM_SEG(_p) BU_CKMAG(_p, THRM_SEG_MAGIC, "thrm_seg")
struct thrm_seg {
	long	magic;
	float	pt[3];			/* center point of nodes */
	float	dir[3];
	float	node[NUM_NODES][3];	/* vectors from center to each node */
	float	vect[NUM_NODES][3];	/* vectors from center to each node */
	float	temperature[NUM_NODES]; /* temperature from treetherm file */
};

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct tthrm_specific {
	long			magic;
	char			tt_name[64];
	long			tt_max_seg;
	fastf_t			tt_min_temp;
	fastf_t			tt_max_temp;
	float			tt_temp_scale;
	struct bu_list		*tt_br;
	struct thrm_seg		*tt_segs;
	mat_t	tthrm_m_to_sh;	/* model to shader space matrix */
};



/* The default values for the variables in the shader specific structure */
#define SHDR_NULL	((struct tthrm_specific *)0)
#define SHDR_O(m)	bu_offsetof(struct tthrm_specific, m)
#define SHDR_AO(m)	bu_offsetofarray(struct tthrm_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse tthrm_parse[] = {
	{"%f",	1, "l",			SHDR_O(tt_min_temp),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "h", 		SHDR_O(tt_max_temp),	BU_STRUCTPARSE_FUNC_NULL },
	{"%s",	64, "file",		SHDR_O(tt_name),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};

HIDDEN int	tthrm_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip), tthrm_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp);
HIDDEN void	tthrm_print(register struct region *rp, char *dp), tthrm_free(char *cp);

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs tthrm_mfuncs[] = {
	{MF_MAGIC,	"tthrm",		0,		MFI_NORMAL|MFI_HIT|MFI_UV,	0,
	tthrm_setup,	tthrm_render,	tthrm_print,	tthrm_free },

	{0,		(char *)0,	0,		0,		0,
	0,		0,		0,		0 }
};
void
print_thrm_seg(struct thrm_seg *ts)
{
	int i;
	CK_THRM_SEG(ts);

	bu_log("Thermal cylinder\n");
	bu_log("Center: (%g %g %g)\n", V3ARGS(ts->pt));
	bu_log("   dir: (%g %g %g)\n", V3ARGS(ts->dir));
	bu_log(" Nodes:\n");
	for (i=0 ; i < NUM_NODES ; i++) {

		bu_log("\t(%g %g %g) %17.14e  (%g %g %g)\n",
			V3ARGS(ts->node[i]),
			ts->temperature[i],
			V3ARGS(ts->vect[i])
			);
	}
}


void
tree_parse(struct bu_list *br, union tree *tr)
{
	switch (tr->tr_b.tb_op) {
		case OP_SOLID: break;
		case OP_UNION: break;
		case OP_INTERSECT: break;
		case OP_SUBTRACT: break;
		case OP_XOR: break;
		case OP_REGION: break;
		case OP_NOP: break;
/* Internal to library routines */
		case OP_NOT: break;
		case OP_GUARD: break;
		case OP_XNOP: break;
		case OP_NMG_TESS: break;
/* LIBWDB import/export interface to combinations */
		case OP_DB_LEAF: break;
	}

}

void
build_tree(struct bu_list *br, struct region *rp)
{
	tree_parse(br, rp->reg_treetop);
}


/*	T R E E T H E R M _ S E T U P
 *
 *	This routine is called (at prep time)
 *	once for each region which uses this shader.
 *	Any shader-specific initialization should be done here.
 */
HIDDEN int
tthrm_setup(register struct region *rp, struct bu_vls *matparm, char **dpp, struct mfuncs *mfp, struct rt_i *rtip)


    			      	/* pointer to reg_udata in *rp */

           		      	/* New since 4.4 release */
{
	register struct tthrm_specific	*tthrm_sp;
	struct bu_mapped_file	*tt_file;
	char			*tt_data;
	long			cyl_tot = 0;
	long			tseg;
	float			*fp;
	float			fv[4];
	double			min_temp;
	double			max_temp;
	point_t			center;
	point_t			pt;
	vect_t			dir;
	static const double	inv_nodes = 1.0/8.0;
	int			node;
	int			i;
	int			long_size = 0;
	int			file_size_long;
	int			file_size_int;

	/* check the arguments */
	RT_CHECK_RTI(rtip);
	BU_CK_VLS( matparm );
	RT_CK_REGION(rp);

	if (rdebug&RDEBUG_SHADE)
		bu_log("tthrm_setup(Region:\"%s\", tthrm(%s))\n",
			rp->reg_name, bu_vls_addr(matparm));

	/* Get memory for the shader parameters and shader-specific data */
	BU_GETSTRUCT( tthrm_sp, tthrm_specific );
	*dpp = (char *)tthrm_sp;
	tthrm_sp->magic = tthrm_MAGIC;

	tthrm_sp->tt_name[0] = '\0';
	tthrm_sp->tt_min_temp = tthrm_sp->tt_max_temp = 0.0;

	if (rdebug&RDEBUG_SHADE)
		bu_log("Parsing: (%s)\n", bu_vls_addr(matparm));

	if (bu_struct_parse( matparm, tthrm_parse, (char *)tthrm_sp ) < 0 ) {
		bu_bomb(__FILE__);
	}
	if (tthrm_sp->tt_name[0] == '\0') {
		bu_log("Must specify file for tthrm shader on %s (got \"%s\"\n",
		       rp->reg_name, bu_vls_addr(matparm));
		bu_bomb(__FILE__);
	}

	tt_file = bu_open_mapped_file( tthrm_sp->tt_name, (char *)NULL);
	if (!tt_file) {
		bu_log("Error mapping \"%s\"\n",  tthrm_sp->tt_name);
		bu_bomb("shader tthrm: can't get thermal data");
	}
	tt_data = tt_file->buf;


	if (rdebug&RDEBUG_SHADE)
		bu_log("tthrm_setup() data: %08lx total\n",
			(unsigned long)tt_data);

	/* Compute how big the file should be, so that we can guess
	 * at the size of the integer at the front of the file
	 */
	file_size_int = sizeof(int) + *((int *)tt_data) *
		(sizeof(short) + sizeof(float) * 4 * NUM_NODES);

	file_size_long = sizeof(long) + *((long *)tt_data) *
		(sizeof(short) + sizeof(float) * 4 * NUM_NODES);

	switch (sizeof(long)) {
	case 8:
		if (tt_file->buflen == file_size_long) {
			/* 64bit data on 64bit host */
			long_size = sizeof(long);
			tthrm_sp->tt_max_seg = cyl_tot = *((long *)tt_data);
		} else if (tt_file->buflen == file_size_int) {
			/* 32bit data on 32bit host */
			long_size = sizeof(int);
			tthrm_sp->tt_max_seg = cyl_tot = *((int *)tt_data);
		}
		break;
	case 4:
		if (tt_file->buflen == file_size_long) {
			/* 32bit data on 32bit host */
			long_size = sizeof(long);
			tthrm_sp->tt_max_seg = cyl_tot = *((long *)tt_data);
		} else if (tt_file->buflen == (file_size_long+4)) {
			/* 64bit data on 32bit host */

			cyl_tot = *((int *)tt_data);
			if (cyl_tot != 0) {
				bu_log("%s:%d thermal data written on 64bit machine with more that 2^32 segs\n", __FILE__, __LINE__);
				bu_bomb("");
			}

			long_size = sizeof(long) + 4;
			tthrm_sp->tt_max_seg = cyl_tot =
				((int *)tt_data)[1];
		}
		break;
	default:
		bu_log("a long int is %d bytes on this machine\n", sizeof(long));
		bu_bomb("I can only handle 4 or 8 byte longs\n");
		break;
	}

	if (rdebug&RDEBUG_SHADE)
		bu_log("cyl_tot = %ld\n", cyl_tot);

	tthrm_sp->tt_segs = (struct thrm_seg *)
		bu_calloc(cyl_tot, sizeof(struct thrm_seg), "thermal segs");

	min_temp = MAX_FASTF;
	max_temp = -MAX_FASTF;

#define CYL_DATA(_n) ((float *) (&tt_data[ \
	 long_size + \
	(_n) * (sizeof(short) + sizeof(float) * 4 * NUM_NODES) + \
	sizeof(short) \
	] ))

	for (tseg = 0 ; tseg < cyl_tot ; tseg++) {

		/* compute centerpoint, min/max temperature values */
		fp = CYL_DATA(tseg);
		VSETALL(center, 0.0);
		for (node=0 ; node < NUM_NODES ; node++, fp+=4) {
			/* this is necessary to assure that all float
			 * values are aligned on 4-byte boundaries
			 */
			memcpy(fv, fp, sizeof(float)*4);

			if (rdebug&RDEBUG_SHADE)
			    bu_log("tthrm_setup() node %d (%g %g %g) %g\n",
					node, fv[0], fv[1], fv[2], fv[3]);

			/* make sure we don't have any "infinity" values */
			for (i=0 ; i < 4 ; i++) {
				if (fv[i] > MAX_FASTF || fv[i] < -MAX_FASTF) {
					bu_log("%s:%d seg %d node %d coord %d out of bounds: %g\n",
						__FILE__, __LINE__, tseg, node, i, fv[i]);
					bu_bomb("choke, gasp, *croak*\n");
				}
			}

			/* copy the values to the segment list, converting
			 * from Meters to Millimeters in the process
			 */
			VSCALE(tthrm_sp->tt_segs[tseg].node[node], fv, 1000.0);
			tthrm_sp->tt_segs[tseg].temperature[node] = fv[3];

			VADD2(center, center, fv);

			if (fv[3] > max_temp) max_temp = fv[3];
			if (fv[3] < min_temp) min_temp = fv[3];
		}

		VSCALE(center, center, 1000.0);
		VSCALE(tthrm_sp->tt_segs[tseg].pt, center, inv_nodes);

		if (rdebug&RDEBUG_SHADE) {
			bu_log("Center: (%g %g %g) (now in mm, not m)\n",
					V3ARGS(tthrm_sp->tt_segs[tseg].pt));
		}

		/* compute vectors from center pt for each node */
		fp = CYL_DATA(tseg);
		for (node=0 ; node < NUM_NODES ; node++, fp+=4) {
			/* this is necessary to assure that all float
			 * values are aligned on 4-byte boundaries
			 */
			memcpy(fv, fp, sizeof(float)*4);

			VSCALE(pt, fv, 1000.0);
			VSUB2(tthrm_sp->tt_segs[tseg].vect[node],
				pt,
				tthrm_sp->tt_segs[tseg].pt
				);
		}

		/* compute a direction vector for the thermal segment */
		VCROSS(dir, tthrm_sp->tt_segs[tseg].vect[0],
				 tthrm_sp->tt_segs[tseg].vect[2]);
		VUNITIZE(dir);
		VMOVE(tthrm_sp->tt_segs[tseg].dir, dir);
		tthrm_sp->tt_segs[tseg].magic = THRM_SEG_MAGIC;
	}

	bu_close_mapped_file(tt_file);

	if (tthrm_sp->tt_min_temp == 0.0 && tthrm_sp->tt_max_temp == 0.0 ) {
		tthrm_sp->tt_min_temp = min_temp;
		tthrm_sp->tt_max_temp = max_temp;
		bu_log("computed temp min/max on %s: %g/%g\n", rp->reg_name, min_temp, max_temp);
	} else {
		min_temp =tthrm_sp->tt_min_temp;
		max_temp = tthrm_sp->tt_max_temp;
		bu_log("taking user specified on %s: min/max %g/%g\n", rp->reg_name, min_temp, max_temp);
	}

	if (max_temp != min_temp) {
		tthrm_sp->tt_temp_scale = 1.0 / (max_temp - min_temp);
	} else {
		/* min and max are equal, maybe zero */
		if (NEAR_ZERO(max_temp, SQRT_SMALL_FASTF))
			tthrm_sp->tt_temp_scale = 0.0;
		else
			tthrm_sp->tt_temp_scale = 255.0/max_temp;
	}
	/* The shader needs to operate in a coordinate system which stays
	 * fixed on the region when the region is moved (as in animation)
	 * we need to get a matrix to perform the appropriate transform(s).
	 *
	 * Shading is done in "region coordinates":
	 */
	db_region_mat(tthrm_sp->tthrm_m_to_sh, rtip->rti_dbip, rp->reg_name, &rt_uniresource);

	if (rdebug&RDEBUG_SHADE) {
		bu_log("min_temp: %17.14e  max_temp %17.14e temp_scale: %17.14e\n",
			tthrm_sp->tt_min_temp,
			tthrm_sp->tt_max_temp,
			tthrm_sp->tt_temp_scale);

		bu_log("tthrm_setup(%s, %s)done\n",
			rp->reg_name, bu_vls_addr(matparm));
		tthrm_print(rp, *dpp);
	}

	return(1);
}

/*
 *	T R E E T H E R M _ P R I N T
 */
HIDDEN void
tthrm_print(register struct region *rp, char *dp)
{
	struct tthrm_specific *tthrm_sp = (struct tthrm_specific *)dp;

	bu_log("%s\n", tthrm_sp->tt_name);
	bn_mat_print( "m_to_sh", tthrm_sp->tthrm_m_to_sh );
#if 0
	bu_struct_print( rp->reg_name, tthrm_print_tab, (char *)dp );
#endif
}

/*
 *	T R E E T H E R M _ F R E E
 */
HIDDEN void
tthrm_free(char *cp)
{
	struct tthrm_specific *tthrm_sp = (struct tthrm_specific *)cp;

	bu_free(tthrm_sp->tt_segs, "thermal segs");
	bu_free(tthrm_sp->tt_name, "bu_vls_strdup");

	tthrm_sp->tt_segs = (struct thrm_seg *)NULL;
	tthrm_sp->tt_name[0] = '\0';
	tthrm_sp->magic = 0;

	bu_free( cp, "tthrm_specific" );
}

/*
 * God help us, we've got to extract the node number from the name
 * of the solid that was hit.
 */
static int
get_solid_number(struct partition *pp)
{
	char *solid_name;
	char *solid_digits;

	solid_name = pp->pt_inseg->seg_stp->st_dp->d_namep;

	if (pp->pt_inseg->seg_stp->st_id != ID_PARTICLE) {
		bu_log("%s:%d solid named %s isn't a particle\n",
			__FILE__, __LINE__, solid_name);
		bu_bomb("Choke! ack! gasp! wheeeeeeze.\n");
	}

	if (! (solid_digits=strrchr(solid_name, (int)'_'))) {
		bu_log("%s:%d solid name %s doesn't have '_'\n",
			__FILE__, __LINE__, solid_name);
		bu_bomb("Choke! ack! gasp! wheeeeeeze.\n");
	}

	solid_digits++;

	if (! strlen(solid_digits)) {
		bu_log("%s:%d solid name %s doesn't have digits after '_'\n",
			__FILE__, __LINE__, solid_name);
		bu_bomb("Choke! ack! gasp! wheeeeeeze.\n");
	}

	return atoi(solid_digits);
}

/*
 *	T R E E T H E R M _ R E N D E R
 *
 *	This is called (from viewshade() in shade.c) once for each hit point
 *	to be shaded.  The purpose here is to fill in values in the shadework
 *	structure.
 */
int
tthrm_render(struct application *ap, struct partition *pp, struct shadework *swp, char *dp)


                	     	/* defined in material.h */
    			    	/* ptr to the shader-specific struct */
{
	register struct tthrm_specific *tthrm_sp =
		(struct tthrm_specific *)dp;
	struct rt_part_internal *part_p;

	point_t pt;
	vect_t pt_v;
	vect_t v;
	int   solid_number;
	struct thrm_seg *thrm_seg;
	int	best_idx;
	double	best_val;
	double	Vdot;
	int node;

	/* check the validity of the arguments we got */
	RT_AP_CHECK(ap);
	RT_CHECK_PT(pp);
	CK_tthrm_SP(tthrm_sp);

	/* We are performing the shading in "region" space.  We must
	 * transform the hit point from "model" space to "region" space.
	 * See the call to db_region_mat in tthrm_setup().
	 */
	MAT4X3PNT(pt, tthrm_sp->tthrm_m_to_sh, swp->sw_hit.hit_point);

	if (rdebug&RDEBUG_SHADE)
		bu_log( "tthrm_render(%s, %g %g %g)\n", tthrm_sp->tt_name,
			V3ARGS(pt));


	solid_number = get_solid_number(pp);

	if (solid_number > tthrm_sp->tt_max_seg ) {
		bu_log("%s:%d solid name %s has solid number higher than %ld\n",
			__FILE__, __LINE__, tthrm_sp->tt_max_seg);
		bu_bomb("Choke! ack! gasp! wheeeeeeze.\n");
	}

	thrm_seg = &tthrm_sp->tt_segs[solid_number];
	CK_THRM_SEG(thrm_seg);



	/* Extract the solid parameters for the particle we hit,
	 * Compare them to the values for the node extracted.  If they
	 * don't match, then we probably have a mis-match between the
	 * geometry and the treetherm output files.
	 */
	if (pp->pt_inseg->seg_stp->st_id != ID_PARTICLE) {
		bu_log("%d != ID_PART\n", pp->pt_inseg->seg_stp->st_id);
		bu_bomb("");
	}
	part_p = (struct rt_part_internal *)pp->pt_inseg->seg_stp->st_specific;
	RT_PART_CK_MAGIC(part_p);
	VSUB2(v, part_p->part_V, thrm_seg->pt);
	if (MAGSQ(v) > 100.0) {
		double dist;
		dist = MAGNITUDE(v);
		/* Distance between particle origin and centroid of themal
		 * segment nodes is > 10.0mm (1cm).  This suggests that
		 * they aren't related
		 */
		bu_log(
"----------------------------- W A R N I N G -----------------------------\n\
%s:%d distance %g between origin of particle and thermal node centroid is\n\
too large.  Probable mis-match between geometry and thermal data\n"
			__FILE__, __LINE__, dist);
		bu_bomb("");
	}


	if (rdebug&RDEBUG_SHADE) {
		vect_t unit_H;
		VMOVE(unit_H, part_p->part_H);
		VUNITIZE(unit_H);

		bu_log("particle rooted at:\n\t(%g %g %g) radius %g\n\tdir: (%g %g %g) (%g %g %g)\n",
			V3ARGS(part_p->part_V), part_p->part_vrad,
			V3ARGS(unit_H),
			V3ARGS(part_p->part_H));

		print_thrm_seg(thrm_seg);
	}

	/* form vector from node center to hit point */
	VSUB2(pt_v, pt, thrm_seg->pt);

	/* The output of treetherm is much too imprecise.  Computed centroid
	 * from truncated floating point values is off.   We'll try to
	 * compensate by doing a double-vector-cross product to get a vector
	 * for our point that is in the plane of the thermal node.
	 */
	VUNITIZE(pt_v);
	VCROSS(v, pt_v, thrm_seg->dir);

	VUNITIZE(v);
	VCROSS(pt_v, thrm_seg->dir, v);

	VUNITIZE(pt_v);

	/* find closest node to hit point by comparing the vectors for the
	 * thermal nodes with the vector for the hit point in the plane
	 * of the nodes
	 */
	best_idx = -1;
	best_val = -2.0;

	for (node=0 ; node < NUM_NODES ; node++) {
		Vdot = VDOT(pt_v, thrm_seg->vect[node]);
		if (Vdot > best_val) {
			best_idx = node;
			best_val = Vdot;
		}
	}


	/* set color to temperature */
	swp->sw_temperature = thrm_seg->temperature[best_idx];
	best_val = (thrm_seg->temperature[best_idx] -
		    tthrm_sp->tt_min_temp) * tthrm_sp->tt_temp_scale;

	/* We do non-grayscale to indicate values
	 * outside the range specified
	 */
	if (best_val > 1.0) {
		/* hotter than maximum */
		best_val -= 1.0;
		if (best_val > 1.0) best_val = 1.0;
		VSET(swp->sw_color, 1.0, best_val, 0.03921568);
	} else if (best_val < 0.0) {
		/* Colder than minimum */
		best_val += 2.0;
		if (best_val < 0.0) best_val = 0.0;

		VSET(swp->sw_color, 0.03921568, best_val, 1.0);
	} else {
		VSET(swp->sw_color, best_val, best_val, best_val);
	}

	if (rdebug&RDEBUG_SHADE) {
		bu_log("closest point is: (%g %g %g) temp: %17.14e\n",
			V3ARGS(thrm_seg->node[best_idx]),
			thrm_seg->temperature[best_idx]);

		bu_log("min_temp: %17.14e  max_temp %17.14e temp_scale: %17.14e\n",
			tthrm_sp->tt_min_temp,
			tthrm_sp->tt_max_temp,
			tthrm_sp->tt_temp_scale);

		bu_log("color: %g (%g)\n", best_val, best_val * 255.0);
	}


	if (rdebug&RDEBUG_SHADE) {
		bu_log("tthrm_render()\n\t  model:(%g %g %g)\n\t shader:(%g %g %g)\n",
		V3ARGS(swp->sw_hit.hit_point),
		V3ARGS(pt) );
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
