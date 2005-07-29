/*                     G - A D R T . C
 *
 * @file g-adrt.c
 *
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
 *
 *  BRLCAD -> ADRT Converter
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

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"		/* machine specific definitions */
#include "vmath.h"		/* vector math macros */
#include "raytrace.h"		/* librt interface definitions */
#include "rtgeom.h"


#ifndef HUGE
#define HUGE 3.40282347e+38F
#endif
#define ADRT_GEOMETRY_REVISION 1


typedef struct property_s {
  char name[256];
  float color[3];
  float emission;
} property_t;


typedef struct regmap_s {
  char name[256];
  int id;
} regmap_t;


static struct rt_i *rtip;	/* rt_dirbuild returns this */
FILE *adrt_fh;
int region_count = 0;
int prop_num;
property_t *prop_list;
int regmap_num;
regmap_t *regmap_list;
int use_regmap;

struct bu_vls*	region_name_from_path(struct db_full_path *pathp);


void regmap_lookup(char *name, int id) {
  int i;

  for(i = 0; i < regmap_num; i++) {
    if(id == regmap_list[i].id) {
      strcpy(name, regmap_list[i].name);
      continue;
    }
  }
}


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

    for(j = 0; j <= i; j++) {
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
  char name[256], found;
  unsigned color[3];
  int i;

  /* color is here
   *    combp->rgb[3];
   *
   * phong properties are probably here:
   *	bu_vls_addr(combp->shader)
   */

  /* If no color is found, assign them a 75% gray color */
  color[0] = combp->rgb[0];
  color[1] = combp->rgb[1];
  color[2] = combp->rgb[2];

  if(!color[0] && !color[1] && !color[2]) {
    color[0] = 192;
    color[1] = 192;
    color[2] = 192;
  }

  /* Do a lookup conversion on the name with the region map if used */
  strcpy(name, pathp->fp_names[pathp->fp_len-1]->d_namep);

  /* replace the brl-cad name with the regmap name */
  if(use_regmap)
    regmap_lookup(name, tsp->ts_regionid);

  found = 0;
  for(i = 0; i < prop_num; i++) {
    if(!strcmp(prop_list[i].name, name)) {
      found = 1;
      continue;
    }
  }

  if(!found) {
    prop_list = (property_t *)realloc(prop_list, sizeof(property_t) * (prop_num + 1));
    strcpy(prop_list[prop_num].name, name);
    prop_list[prop_num].color[0] = color[0] / 255.0;
    prop_list[prop_num].color[1] = color[1] / 255.0;
    prop_list[prop_num].color[2] = color[2] / 255.0;
    if(strstr(bu_vls_strgrab((struct vu_vls *)&(combp->shader)), "light")) {
      prop_list[prop_num].emission = 1.0;
    } else {
      prop_list[prop_num].emission = 0.0;
    }
    prop_num++;
  }
/*    printf("reg_start: -%s- [%d,%d,%d]\n", name, combp->rgb[0], combp->rgb[1], combp->rgb[2]); */

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
    register struct soltab *stp;
    union tree *curtree;
    struct directory *dp;
    struct region_pointer *td = (struct region_pointer *)client_data;
    struct bu_vls *vlsp;
    struct rt_i *rtip;
    struct rt_bot_internal *bot;
    matp_t mat;
    vect_t vv;
    vectp_t vp;
    float vec[3];
    int hash, i, n, size;
    char prop_name[256], reg_name[256];
    unsigned char c;


    RT_CK_DBTS(tsp);
    RT_CK_DBI(tsp->ts_dbip);
    RT_CK_FULL_PATH(pathp);
    RT_CK_DB_INTERNAL(ip);
    rtip = tsp->ts_rtip;
    RT_CK_RTI(rtip);
    RT_CK_RESOURCE(tsp->ts_resp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);


    if(ip->idb_minor_type != ID_BOT) {
#if DEBUG
	fprintf(stderr, "\"%s\" should be a BOT ", pathp->fp_names[pathp->fp_len-1]->d_namep);
	for (i=pathp->fp_len-1; i >= 0 ; i--) {
	    if (pathp->fp_names[i]->d_flags & DIR_REGION) {
		fprintf(stderr, "fix region %s\n", pathp->fp_names[i]->d_namep);
		goto found;
	    } else if (pathp->fp_names[i]->d_flags & DIR_COMB) {
		fprintf(stderr, "not a combination\n");
	    } else if (pathp->fp_names[i]->d_flags & DIR_SOLID) {
		fprintf(stderr, "not a primitive\n");
	    }
	}
	fprintf(stderr, "didn't find region?\n");
    found:
#endif
      return(TREE_NULL); /* BAD */
    }

/*    printf("region_id: %d\n", tsp->ts_regionid); */

    bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);
    rtip->nsolids++;

    /* Find the Properties Name */
    prop_name[0] = 0;


    vlsp = region_name_from_path(pathp);
    strcpy(reg_name, bu_vls_strgrab(vlsp));

    /* Grab the chars from the end till the '/' */
    i = strlen(reg_name)-1;
    while(reg_name[i] != '/' && i > 0)
      i--;

    if(use_regmap)
      regmap_lookup(reg_name, tsp->ts_regionid);

    /* Display Status */
    printf("regions processed: %d\r", ++region_count);
    fflush(stdout);

    if(i != strlen(reg_name))
      strcpy(prop_name, &reg_name[i+1]);
/*    printf("prop name: -%s-\n", prop_name); */
    bu_free(vlsp, "vls");


    /* this is for librt's benefit */
    BU_GETSTRUCT(stp, soltab);
    VSETALL(stp->st_min, HUGE);
    VSETALL(stp->st_max, -HUGE);

    for(vp = &bot->vertices[bot->num_vertices-1]; vp >= bot->vertices; vp -= 3) {
	VMINMAX(stp->st_min, stp->st_max, vp);
    }
    VMINMAX(rtip->mdl_min, rtip->mdl_max, stp->st_min);
    VMINMAX(rtip->mdl_min, rtip->mdl_max, stp->st_max);

    /* Pack bot/mesh name */
#if 0
    c = strlen(pathp->fp_names[pathp->fp_len-1]->d_namep) + 1;
    common_pack_write(common_g_app_data, common_g_app_ind, &c, sizeof(char));
    common_pack_write(common_g_app_data, common_g_app_ind, pathp->fp_names[pathp->fp_len-1]->d_namep, c);
#else
    c = strlen(reg_name) + 1;
    fwrite(&c, 1, 1, adrt_fh);
    fwrite(reg_name, 1, c, adrt_fh);
#endif

    /* Set the properties name */
    c = strlen(prop_name) + 1;
    fwrite(&c, 1, 1, adrt_fh);
    fwrite(prop_name, 1, c, adrt_fh);

    /* Pack number of vertices */
    fwrite(&bot->num_vertices, sizeof(int), 1, adrt_fh);

    for(i = 0; i < bot->num_vertices; i++) {
      /* Change scale from mm to meters */
      vec[0] = bot->vertices[3*i+0] * 0.001;
      vec[1] = bot->vertices[3*i+1] * 0.001;
      vec[2] = bot->vertices[3*i+2] * 0.001;
      fwrite(vec, sizeof(float), 3, adrt_fh);
    }

    if(bot->num_faces < 1<<16) {
      unsigned short ind;

      c = 0; /* using unsigned shorts */
      fwrite(&c, 1, 1, adrt_fh);

      /* Pack number of faces */
      ind = bot->num_faces;
      fwrite(&ind, sizeof(unsigned short), 1, adrt_fh);
      for(i = 0; i < 3 * bot->num_faces; i++) {
        ind = bot->faces[i];
        fwrite(&ind, sizeof(unsigned short), 1, adrt_fh);
      }
    } else {
      c = 1; /* using ints */
      fwrite(&c, 1, 1, adrt_fh);

      fwrite(&bot->num_faces, sizeof(int), 1, adrt_fh);
      fwrite(bot->faces, sizeof(int), 3*bot->num_faces, adrt_fh);
    }

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

/*    ACQUIRE_SEMAPHORE_TREE(hash); */
#if 0
    BU_LIST_INSERT( &(rtip->rti_solidheads[hash]), &(stp->l) );
    BU_LIST_INSERT( &dp->d_use_hd, &(stp->l2) );
#endif
/*    RELEASE_SEMAPHORE_TREE(hash); */

    /*    bu_log("tree:0x%08x  stp:0x%08x\n", curtree, stp); */

    return( curtree );
}


void load_regmap(char *filename) {
  FILE *fh;
  char line[256], name[256], idstr[20], *ptr;
  int i, ind, id;

  regmap_num = 0;
  regmap_list = NULL;

  fh = fopen(filename, "r");
  if(!fh) {
    printf("region map file \"%s\" not found.\n", filename);
    return;
  } else {
    printf("using region map: %s\n", optarg);
  }

  while(!feof(fh)) {
    /* read in the line */
    fgets(line, 256, fh);

    /* strip off the new line */
    line[strlen(line)-1] = 0;

    /* replace any tabs with spaces */
    for(i = 0; i < strlen(line); i++)
      if(line[i] == '\t')
        line[i] = ' ';

    /* advance to the first non-space */
    ind = 0;
    while(line[ind] == ' ')
      ind++;

    /* If there is a '#' comment here then continue */
    if(line[ind] == '#')
      continue;

    /* advance to the first space while reading the name */
    i = 0;
    while(i < 256 && line[ind] != ' ' && ind < strlen(line))
      name[i++] = line[ind++];
    name[i] = 0;

    /* skip any lines that are empty */
    if(!strlen(name))
      continue;

    /* begin parsing the id's */
    while(ind < strlen(line)) {
      /* advance to the first id */
      while(line[ind] == ' ')
        ind++;

      /* check for comment */
      if(line[ind] == '#')
        break;

      /* advance to the first space while reading the id string */
      i = 0;
      while(i < 256 && line[ind] != ' ' && ind < strlen(line))
        idstr[i++] = line[ind++];
      idstr[i] = 0;

      /* chesk that there were no spaces after the last id */
      if(!strlen(idstr))
        break;

      /* if the id string contains a ':' then it's a range */
      if(strstr(idstr, ":")) {
        int hi, lo;

        ptr = strchr(idstr, ':');
        ptr++;
        hi = atoi(ptr);
        strchr(idstr, ':')[0] = 0;
        lo = atoi(idstr);

        /* insert an entry for the whole range */
        for(i = 0; i <= hi-lo; i++) {
          /* Insert one entry into the regmap_list */
          regmap_list = (regmap_t *)realloc(regmap_list, sizeof(regmap_t) * (regmap_num + 1));
          strcpy(regmap_list[regmap_num].name, name);
          regmap_list[regmap_num].id = lo + i;
          regmap_num++;
        }
      } else {
        /* insert one entry into the regmap_list */
        regmap_list = (regmap_t *)realloc(regmap_list, sizeof(regmap_t) * (regmap_num + 1));
        strcpy(regmap_list[regmap_num].name, name);
        regmap_list[regmap_num].id = atoi(idstr);
        regmap_num++;
      }
    }
  }
}


int main(int argc, char *argv[]) {
  int i;
  char idbuf[132], filename[256];
  char shortopts[] = "r:", c;
  struct db_tree_state ts;
  struct directory *dp;
  short s;


  if(argc <= 3) {
    printf("Usage: g-adrt [-r region.map] file.g adrt_project_name [region list]\n");
    exit(1);
  }

  /* Process command line arguments */
  use_regmap = 0;
  while((c = bu_getopt(argc, argv, shortopts)) != -1) {
    switch(c) {
      case 'r':
        use_regmap = 1;
        load_regmap(optarg);
        break;

      default:
        break;
    }
  }
  argc -= bu_optind;
  argv += bu_optind;

  /* Open the adrt file */
  strcpy(filename, argv[1]);
  strcat(filename, ".adrt");
  adrt_fh = fopen(filename, "w");
  printf("converting: %s\n", argv[0]);

  /* write 2-byte endian */
  s = 1;
  fwrite(&s, sizeof(short), 1, adrt_fh);

  /* write geometry revision number */
  s = ADRT_GEOMETRY_REVISION;
  fwrite(&s, sizeof(short), 1, adrt_fh);

  region_count = 0;
  prop_num = 0;
  prop_list = NULL;

  /*
  * Load database.
  * rt_dirbuild() returns an "instance" pointer which describes
  * the database to be ray traced.  It also gives you back the
  * title string in the header (ID) record.
  */

  if((rtip = rt_dirbuild(argv[0], idbuf, sizeof(idbuf))) == RTI_NULL) {
    fprintf(stderr,"rtexample: rt_dirbuild failure\n");
    exit(2);
  }

  ts = rt_initial_tree_state;
  ts.ts_dbip = rtip->rti_dbip;
  ts.ts_rtip = rtip;
  ts.ts_resp = NULL;
  bu_avs_init(&ts.ts_attrs, 1, "avs in tree_state");

  /*
  * Generage a geometry file
  */
  db_walk_tree(rtip->rti_dbip, argc - 2, (const char **)&argv[2], 1, &ts, &reg_start_func, &reg_end_func, &leaf_func, (void *)0);
  fclose(adrt_fh);

  /*
  * Generate an environment file
  */
  strcpy(filename, argv[1]);
  strcat(filename, ".env");
  adrt_fh = fopen(filename, "w");

  strcpy(filename, argv[1]);
  strcat(filename, ".adrt");
  fprintf(adrt_fh, "geometry_file,%s\n", filename);
  fprintf(adrt_fh, "properties_file,properties.db\n");
  fprintf(adrt_fh, "textures_file,textures.db\n");
  fprintf(adrt_fh, "frames_file,frames.db\n");
  fprintf(adrt_fh, "image_size,640,480,80,80\n");
  fprintf(adrt_fh, "rendering_method,path,128\n");

  fclose(adrt_fh);

  /*
  * Generate a properties file
  */
  adrt_fh = fopen("properties.db", "w");
  for(i = 0; i < prop_num; i++) {
    fprintf(adrt_fh, "properties,%s\n", prop_list[i].name);
    fprintf(adrt_fh, "color,%f,%f,%f\n", prop_list[i].color[0], prop_list[i].color[1], prop_list[i].color[2]);
    if(prop_list[i].emission > 0.0)
      fprintf(adrt_fh, "emission,%f\n", prop_list[i].emission);
  }
  fclose(adrt_fh);

  /*
  * Generate a textures file
  */
  adrt_fh = fopen("textures.db", "w");
  fclose(adrt_fh);

  /*
  * Generate a frames file
  */
  adrt_fh = fopen("frames.db", "w");
  fprintf(adrt_fh, "frame,1\n");
  fprintf(adrt_fh, "camera,10.0,10.0,10.0,0.0,0.0,0.0,0.0,20.0,0.0\n");
  fclose(adrt_fh);

  printf("\ncomplete.\n");
}
