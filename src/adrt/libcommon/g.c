/*                     G . C
 * BRL-CAD
 *
 * Copyright (C) 2002-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
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
/** @file g.c
 *                     G . C
 *
 *  Common Library - BRLCAD Geometry File Support
 *
 *  Author -
 *      Justin L. Shumaker
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */

#include "g.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "pack.h"

#include "tie.h"

#include "umath.h"
#include "brlcad_config.h"		/* machine specific definitions */
#include "machine.h"		/* machine specific definitions */
#include "vmath.h"		/* vector math macros */
#include "raytrace.h"		/* librt interface definitions */
#include "rtgeom.h"


#ifndef HUGE
#define HUGE 3.40282347e+38F
#endif


void **common_g_app_data;
int *common_g_app_ind;
int common_g_begin;
int common_g_prop_marker;
int common_g_mesh_marker;
int common_g_tri_marker;

static struct rt_i	*rtip;	/* rt_dirbuild returns this */


struct bu_vls*	region_name_from_path(struct db_full_path *pathp);
void		common_pack_g(void **app_data, int *app_ind, char *filename, char *args);


/*
*  r e g i o n _ n a m e _ f r o m _ p a t h
*
*  Walk backwards up the tree to the first region, then
*  make a vls string of everything at or above that level.
*  Basically, it produces the name of the region to which 
*  the leaf of the path contributes. 
*/
struct bu_vls* region_name_from_path(struct db_full_path *pathp) {
    int i, j;
    struct bu_vls *vls;

    /* walk up the path from the bottom looking for the region */
    for (i=pathp->fp_len-1 ; i >= 0 ; i--) {
	if (pathp->fp_names[i]->d_flags & DIR_REGION) {
	    goto found_region;
	}
    }
#if DEBUG
    bu_log("didn't find region on path %s\n", db_path_to_string( pathp ));
#endif
/*    abort(); */

 found_region:
    vls = bu_malloc(sizeof(struct bu_vls), "region name");
    bu_vls_init( vls );

    for (j=0 ; j <= i ; j++) {
	bu_vls_strcat(vls, "/");
	bu_vls_strcat(vls, pathp->fp_names[j]->d_namep);
    }

    return vls;

}



/*
*  r e g _ s t a r t _ f u n c
*  Called by the tree walker when it first encounters a region
*/
static int reg_start_func(struct db_tree_state *tsp,
			  struct db_full_path *pathp,
			  const struct rt_comb_internal *combp,
			  genptr_t client_data)
{
  char c;
  short s;
  common_prop_t prop;

  /* color is here
   *    combp->rgb[3];
   *
   * phong properties are probably here:
   *	bu_vls_addr(combp->shader)
   */

  if(combp->rgb[0] || combp->rgb[1] || combp->rgb[2]) {
    c = strlen(pathp->fp_names[pathp->fp_len-1]->d_namep) + 1;
    common_pack_write(common_g_app_data, common_g_app_ind, &c, sizeof(char));
    common_pack_write(common_g_app_data, common_g_app_ind, pathp->fp_names[pathp->fp_len-1]->d_namep, c);

    prop.color.v[0] = combp->rgb[0] / 255.0;
    prop.color.v[1] = combp->rgb[1] / 255.0;
    prop.color.v[2] = combp->rgb[2] / 255.0;

    prop.density = 0.5;
    prop.gloss = 0.5;
    prop.emission = 0.0;
    prop.ior = 1.0;

    /* pack properties data */
    common_pack_write(common_g_app_data, common_g_app_ind, &prop, sizeof(common_prop_t));

/*    printf("reg_start: -%s- [%d,%d,%d]\n", pathp->fp_names[pathp->fp_len-1]->d_namep, combp->rgb[0], combp->rgb[1], combp->rgb[2]); */
  }
    /* to find the name of the region... it is probably
     * pathp->fp_names[pathp->fp_len-1]
     */

  return(0);
}


/*
*  r e g _ e n d _ f u n c
*
*  Called by the tree walker when it has finished walking the tree for the region
*  ie. all primitives have been processed
*/
static union tree* reg_end_func(struct db_tree_state *tsp,
                                struct db_full_path *pathp,
                                union tree *curtree,
                                genptr_t client_data)
{
  return(NULL);
}


/*
*  l e a f _ f u n c
*
*  process a leaf of the region tree.  called by db_walk_tree()
*
*  Makes sure that this is a BoT primitive, creates the ADRT triangles
*  and some wrapper data structures so that we can ray-trace the BoT in 
*  ADRT
*/
static union tree *leaf_func(struct db_tree_state *tsp,
                             struct db_full_path *pathp,
                             struct rt_db_internal *ip,
                             genptr_t client_data)
{
    register struct soltab	*stp;
    union tree			*curtree;
    struct directory		*dp;
    register matp_t		mat;
    int				face, hash, i, n, size;
    struct rt_i			*rtip;
    struct rt_bot_internal	*bot;

    TIE_3			v0, v1, v2;
    short			s;
    struct meshdata		*md;
    struct triwrap 		*tw;
    vectp_t			vp;
    vect_t			vv;
    struct region_pointer 	*td = (struct region_pointer *)client_data;
    struct bu_vls		*vlsp;
    char			c, prop_name[256], reg_name[256];
    tfloat			matrix[16];


    RT_CK_DBTS(tsp);
    RT_CK_DBI(tsp->ts_dbip);
    RT_CK_FULL_PATH(pathp);
    RT_CK_DB_INTERNAL(ip);
    rtip = tsp->ts_rtip;
    RT_CK_RTI(rtip);
    RT_CK_RESOURCE(tsp->ts_resp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);

    if(common_g_begin) {
      /* END THE PROPERTIES SECTION */
      size = *common_g_app_ind - common_g_prop_marker - sizeof(int);
      common_pack_write(common_g_app_data, &common_g_prop_marker, &size, sizeof(int));
      *common_g_app_ind = common_g_prop_marker + size;


      /* START THE MESH SECTION */
      s = COMMON_PACK_MESH;
      common_pack_write(common_g_app_data, common_g_app_ind, &s, sizeof(short));

      /* Size of mesh data */
      common_g_mesh_marker = *common_g_app_ind;
      *common_g_app_ind += sizeof(int);

      /* Number of triangles */
      common_g_tri_marker = *common_g_app_ind;
      *common_g_app_ind += sizeof(int);

      common_g_begin = 0;
    }


    if (ip->idb_minor_type != ID_BOT) {

#if DEBUG
	fprintf(stderr, "\"%s\" should be a BOT ", pathp->fp_names[pathp->fp_len-1]->d_namep);
#endif

	for (i=pathp->fp_len-1; i >= 0 ; i--) {
	    if (pathp->fp_names[i]->d_flags & DIR_REGION) {
#if DEBUG
		fprintf(stderr, "fix region %s\n", pathp->fp_names[i]->d_namep);
#endif
		goto found;
	    } else if (pathp->fp_names[i]->d_flags & DIR_COMB) {
#if DEBUG
		fprintf(stderr, "not a combination\n");
#endif
	    } else if (pathp->fp_names[i]->d_flags & DIR_SOLID) {
#if DEBUG
		fprintf(stderr, "not a primitive\n");
#endif
	    }
	}
#if DEBUG
	fprintf(stderr, "didn't find region?\n");
#endif
    found:
	return( TREE_NULL );		/* BAD */
    }
    bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);
    rtip->nsolids++;


#if 0
    md->bot_dp = dp;
    md->region_p = (struct region *)NULL; /* XXX these need to get filled in later */
#endif


    /* Find the Properties Name */
    prop_name[0] = 0;

    vlsp = region_name_from_path(pathp);
    strcpy(reg_name, bu_vls_strgrab(vlsp));

    /* Grab the chars from the end till the '/' */
    i = strlen(reg_name)-1;
    while(reg_name[i] != '/' && i > 0)
      i--;

    if(i != strlen(reg_name))
      strcpy(prop_name, &reg_name[i+1]);

/*    printf("prop name: -%s-\n", prop_name); */
    bu_free(vlsp, "vls");



    /* this is for librt's benefit */
    BU_GETSTRUCT( stp, soltab);
    VSETALL(stp->st_min, HUGE);
    VSETALL(stp->st_max, -HUGE);

    for (vp = &bot->vertices[bot->num_vertices-1] ; vp >= bot->vertices ; vp -= 3) {
	VMINMAX(stp->st_min, stp->st_max, vp);
    }
    VMINMAX(rtip->mdl_min, rtip->mdl_max, stp->st_min);
    VMINMAX(rtip->mdl_min, rtip->mdl_max, stp->st_max);

    /* Create a new mesh */
    s = COMMON_PACK_MESH_NEW;
    common_pack_write(common_g_app_data, common_g_app_ind, &s, sizeof(short));

    /* Pack bot/mesh name */
#if 0
    c = strlen(pathp->fp_names[pathp->fp_len-1]->d_namep) + 1;
    common_pack_write(common_g_app_data, common_g_app_ind, &c, sizeof(char));
    common_pack_write(common_g_app_data, common_g_app_ind, pathp->fp_names[pathp->fp_len-1]->d_namep, c);
#else
    c = strlen(reg_name) + 1;
    common_pack_write(common_g_app_data, common_g_app_ind, &c, sizeof(char));
    common_pack_write(common_g_app_data, common_g_app_ind, reg_name, c);
#endif

    /* Set the properties name */
    c = strlen(prop_name) + 1;
    common_pack_write(common_g_app_data, common_g_app_ind, &c, sizeof(char));
    common_pack_write(common_g_app_data, common_g_app_ind, prop_name, c);

    /* Pack number of triangles */
    common_pack_write(common_g_app_data, common_g_app_ind, &bot->num_faces, sizeof(int));    

    /* Expose number of triangles used */
    common_pack_trinum += bot->num_faces;

    for (face = 0 ; face < bot->num_faces ; face++) {
      int vert_idx;

      /* Smooth Triangle - No for now */
      c = 0;
      common_pack_write(common_g_app_data, common_g_app_ind, &c, sizeof(char));

      if(bot->orientation == RT_BOT_CW) {

        vert_idx = bot->faces[face*3];
        VMOVE(v2.v, &bot->vertices[vert_idx*3]);
        vert_idx = bot->faces[face*3+1];
        VMOVE(v1.v, &bot->vertices[vert_idx*3]);
        vert_idx = bot->faces[face*3+2];
        VMOVE(v0.v, &bot->vertices[vert_idx*3]);

      } else if (bot->orientation == RT_BOT_CCW) {

        vert_idx = bot->faces[face*3];
        VMOVE(v0.v, &bot->vertices[vert_idx*3]);
        vert_idx = bot->faces[face*3+1];
        VMOVE(v1.v, &bot->vertices[vert_idx*3]);
        vert_idx = bot->faces[face*3+2];
        VMOVE(v2.v, &bot->vertices[vert_idx*3]);

      } else {

        vert_idx = bot->faces[face*3];
        VMOVE(v0.v, &bot->vertices[vert_idx*3]);
        vert_idx = bot->faces[face*3+1];
        VMOVE(v1.v, &bot->vertices[vert_idx*3]);
        vert_idx = bot->faces[face*3+2];
        VMOVE(v2.v, &bot->vertices[vert_idx*3]);

      }

      /* Change scale from mm to meters */
      math_vec_mul_scalar(v0, v0, 0.001);
      math_vec_mul_scalar(v1, v1, 0.001);
      math_vec_mul_scalar(v2, v2, 0.001);

      /* Pack vertices, transformations assumed to have already been applied */
      common_pack_write(common_g_app_data, common_g_app_ind, &v0, sizeof(TIE_3));
      common_pack_write(common_g_app_data, common_g_app_ind, &v1, sizeof(TIE_3));
      common_pack_write(common_g_app_data, common_g_app_ind, &v2, sizeof(TIE_3));

      /* No smooth normals for now */
#if 0
        printf("[%.3f, %.3f, %.3f] [%.3f, %.3f, %.3f] [%.3f, %.3f, %.3f]\n", v0.v[0], v0.v[1], v0.v[2], v1.v[0], v1.v[1], v1.v[2], v2.v[0], v2.v[1], v2.v[2]);
	TIE_AddTriangle(v0, v1, v2, (void *)&tw[face]);
#endif
    }

    /* Pack Min and Max Data */
    v0.v[0] =  stp->st_min[0];
    v0.v[1] =  stp->st_min[1];
    v0.v[2] =  stp->st_min[2];

    v1.v[0] =  stp->st_max[0];
    v1.v[1] =  stp->st_max[1];
    v1.v[2] =  stp->st_max[2];

    common_pack_write(common_g_app_data, common_g_app_ind, &v0, sizeof(TIE_3));
    common_pack_write(common_g_app_data, common_g_app_ind, &v1, sizeof(TIE_3));


    /* Write Transformation Matrix (Identity for now ) */
    math_mat_ident(matrix, 4);
    common_pack_write(common_g_app_data, common_g_app_ind, matrix, sizeof(tfloat)*16);



    hash = db_dirhash( dp->d_namep );

    /* this piece is for db_walk_tree's benefit */
    RT_GET_TREE( curtree, tsp->ts_resp );
    curtree->magic = RT_TREE_MAGIC;
    curtree->tr_op = OP_SOLID;
    /* regionp will be filled in later by rt_tree_region_assign() */
    curtree->tr_a.tu_regionp = (struct region *)0;


    BU_GETSTRUCT( stp, soltab);
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_rtip = rtip;
    stp->st_dp = dp;
    dp->d_uses++;	/* XXX gratuitous junk */
    stp->st_uses = 1;
    stp->st_matp = (matp_t)0;
    bu_ptbl_init( &stp->st_regions, 0, "regions using solid" ); /* XXX gratuitous junk */

    VSUB2(vv, stp->st_max, stp->st_min); /* bounding box of bot span being computed */

    stp->st_aradius = MAGNITUDE(vv) * 0.5125;
    if (stp->st_aradius <= 0) stp->st_aradius = 1.0;

    /* fake out, */
    /*    stp->st_meth = &adrt_functab; */
    curtree->tr_a.tu_stp = stp;

//    ACQUIRE_SEMAPHORE_TREE(hash);
    BU_LIST_INSERT( &(rtip->rti_solidheads[hash]), &(stp->l) );
    BU_LIST_INSERT( &dp->d_use_hd, &(stp->l2) );
//    RELEASE_SEMAPHORE_TREE(hash);

    /*    bu_log("tree:0x%08x  stp:0x%08x\n", curtree, stp); */

    return( curtree );
}


void common_pack_g(void **app_data, int *app_ind, char *filename, char *args) {
  int argc, size;
  char idbuf[132], *arg_list[32];
  struct db_tree_state ts;
  struct directory *dp;
  short s;

  common_g_app_data = app_data;
  common_g_app_ind = app_ind;

  /*
  *  Load database.
  *  rt_dirbuild() returns an "instance" pointer which describes
  *  the database to be ray traced.  It also gives you back the
  *  title string in the header (ID) record.
  */
  if( (rtip=rt_dirbuild(filename, idbuf, sizeof(idbuf))) == RTI_NULL ) {
    fprintf(stderr,"rtexample: rt_dirbuild failure\n");
    exit(2);
  }

  ts = rt_initial_tree_state;
  ts.ts_dbip = rtip->rti_dbip;
  ts.ts_rtip = rtip;
  ts.ts_resp = NULL;

  ts.ts_attrs.count = 0;
  if (ts.ts_attrs.count == 0)
    bu_avs_init(&ts.ts_attrs, 1, "avs in tree_state");


  arg_list[0] = strtok(args," ");

  if(!arg_list[0]) {
    printf("No regions specified\n");
    exit(1);
  }

  for (argc = 1; argc < 32; argc++) {
    arg_list[argc] = strtok(NULL," ");
    if(arg_list[argc] == NULL)
      break;
  }


  /* START THE PROPERTIES SECTION */

  s = COMMON_PACK_PROP;
  common_pack_write(common_g_app_data, common_g_app_ind, &s, sizeof(short));
  common_g_prop_marker = *common_g_app_ind;
  *common_g_app_ind += sizeof(int);

  common_g_begin = 1;
  db_walk_tree(rtip->rti_dbip, argc, arg_list, 1, &ts, &reg_start_func, &reg_end_func, &leaf_func, (void *)0);


  /* FINNISH THE MESH SECTION */
  size = *app_ind - common_g_mesh_marker - sizeof(int);
  common_pack_write(app_data, &common_g_mesh_marker, &size, sizeof(int));
  common_pack_write(app_data, &common_g_tri_marker, &common_pack_trinum, sizeof(int));
  *app_ind = common_g_mesh_marker + sizeof(int) + size;
}
