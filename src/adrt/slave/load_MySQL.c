/*                         L O A D . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2009 United States Government as represented by
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
/** @file load.c
 *
 */

#ifndef TIE_PRECISION
# define TIE_PRECISION 0
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu.h"

#include "load.h"

#ifdef HAVE_MYSQL
# include <mysql.h>
# define ADRT_MYSQL_USER         "adrt"
# define ADRT_MYSQL_PASS         "adrt"
# define ADRT_MYSQL_DB           "adrt"
MYSQL slave_load_mysql_db;

int
slave_load_MySQL (uint32_t pid, tie_t *tie, const char *hostname)
{
    MYSQL_RES *res;
    MYSQL_ROW row;
    TIE_3 *vlist, **tlist;
    char query[256];
    uint32_t gid, gsize, gind, mnum, tnum, vnum, f32num, *f32list, i, mind;
    uint16_t f16num, *f16list;
    uint8_t c, ftype;
    void *gdata;

    /* establish mysql connection */
    mysql_init (&slave_load_mysql_db);

    /* establish connection to database */

    if ( mysql_real_connect (&slave_load_mysql_db, hostname, ADRT_MYSQL_USER, ADRT_MYSQL_PASS, ADRT_MYSQL_DB, 0, 0, 0) == NULL ) {
	printf("Unable to connect to db: %s\n", mysql_error(&slave_load_mysql_db));
	return -1;
    }


    /* Obtain the geometry id for this project id */
    sprintf (query, "select gid from project where pid = '%d'", pid);
    mysql_query (&slave_load_mysql_db, query);
    res = mysql_use_result (&slave_load_mysql_db);
    if (res == NULL) {
	printf("Unable to get gid... \"%s\"\n", query);
	return -1;
    }
    row = mysql_fetch_row (res);
    gid = atoi (row[0]);
    mysql_free_result (res);

    /*
     * Now that a gid has been obtained, query for the size of the binary data,
     * allocate memory for it, extract it, and process it.
     */
    snprintf (query, 256, "select trinum,meshnum,gsize,gdata from geometry where gid = '%d'", gid);
    mysql_query (&slave_load_mysql_db, query);
    res = mysql_use_result (&slave_load_mysql_db);
    row = mysql_fetch_row (res);

    tnum = atoi (row[0]);
    mnum = atoi (row[1]);
    gsize = atoi (row[2]);
    gdata = row[3];

    /* initialize tie */
    tie_init (tie, tnum, TIE_KDTREE_FAST);

    /*
     * Process geometry data
     */
    slave_load_mesh_num = mnum;
    slave_load_mesh_list = (adrt_mesh_t *) bu_realloc (slave_load_mesh_list, slave_load_mesh_num * sizeof(adrt_mesh_t), "mesh list");
    mind = 0;

    /* skip over endian */
    gind = sizeof(uint16_t);

    /* For each mesh */
    while (gind < gsize) {
	slave_load_mesh_list[mind].texture = NULL;
	slave_load_mesh_list[mind].flags = 0;
	slave_load_mesh_list[mind].attributes = (adrt_mesh_attributes_t *)bu_malloc(sizeof(adrt_mesh_attributes_t), "mesh attributes");

	/* length of name string */
	memcpy(&c, &((char *)gdata)[gind], 1);
	gind += 1;

	/* name */
	memcpy(slave_load_mesh_list[mind].name, &((char *)gdata)[gind], c);
	gind += c;

	/* Assign default attributes */
	VSET(slave_load_mesh_list[mind].attributes->color.v, 0.8, 0.8, 0.8);

	/* vertice num */
	memcpy(&vnum, &((char *)gdata)[gind], sizeof(uint32_t));
	gind += sizeof(uint32_t);

	/* vertice list */
	vlist = (TIE_3 *)&((char *)gdata)[gind];
	gind += vnum * sizeof(TIE_3);

	/* face type */
	memcpy(&ftype, &((char *)gdata)[gind], 1);
	gind += 1;

	if (ftype) {
	    /* face num 32-bit */
	    memcpy(&f32num, &((char *)gdata)[gind], sizeof(uint32_t));
	    gind += sizeof(uint32_t);
	    f32list = (uint32_t *)&((char *)gdata)[gind];
	    gind += 3 * f32num * sizeof(uint32_t);

	    tlist = (TIE_3 **)bu_malloc(3 * f32num * sizeof(TIE_3 *), "tlist32");
	    for (i = 0; i < 3*f32num; i++)
		tlist[i] = &vlist[f32list[i]];

	    /* assign the current mesh to group of triangles */
	    tie_push (tie, tlist, f32num, &slave_load_mesh_list[mind], 0);
	} else {
	    /* face num 16-bit */
	    memcpy(&f16num, &((char *)gdata)[gind], sizeof(uint16_t));
	    gind += sizeof(uint16_t);
	    f16list = (uint16_t *)&((char *)gdata)[gind];
	    gind += 3 * f16num * sizeof(uint16_t);

	    tlist = (TIE_3 **)bu_malloc(3 * f16num * sizeof(TIE_3 *), "tlist16");
	    for (i = 0; i < 3*f16num; i++)
		tlist[i] = &vlist[f16list[i]];

	    /* assign the current mesh to group of triangles */
	    tie_push (tie, tlist, f16num, &slave_load_mesh_list[mind], 0);
	}

	bu_free (tlist, "tlist");
	mind++;
    }

    /* free mysql result */
    mysql_free_result (res);

    /* Query the mesh attributes map for the attribute id */
    for (i = 0; i < slave_load_mesh_num; i++) {
	snprintf (query, 256, "select attr from meshattrmap where mesh='%s' and gid=%d", slave_load_mesh_list[i].name, gid);
	if (!mysql_query (&slave_load_mysql_db, query)) {
	    char attr[48];

	    res = mysql_use_result (&slave_load_mysql_db);
	    row = mysql_fetch_row (res);
	    strncpy (attr, row[0], 48-1);
	    attr[48-1] = '\0'; /* sanity */

/* printf("attr: %s\n", attr); */
	    mysql_free_result (res);

	    snprintf (query, 256, "select data from attribute where gid='%d' and name='%s' and type='color'", gid, attr);
/* printf("query[%d]: %s\n", i, query); */
	    if (!mysql_query (&slave_load_mysql_db, query)) {
		res = mysql_use_result (&slave_load_mysql_db);
		while ((row = mysql_fetch_row (res)))
		    sscanf(row[0], "%f %f %f", &slave_load_mesh_list[i].attributes->color.v[0], &slave_load_mesh_list[i].attributes->color.v[1], &slave_load_mesh_list[i].attributes->color.v[2]);

		mysql_free_result (res);
	    }
	}
    }

    /* Query to see if there is acceleration data for this geometry. */
    snprintf (query, 256, "select asize,adata from geometry where gid='%d'", gid);
    mysql_query (&slave_load_mysql_db, query);
    res = mysql_use_result (&slave_load_mysql_db);
    row = mysql_fetch_row (res);

    if (atoi(row[0]) > 0) {
	printf ("acceleration structure found\n");
	tie_kdtree_cache_load (tie, row[1], atoi(row[0]));
    }

    mysql_free_result (res);
    mysql_close (&slave_load_mysql_db);

    return 0;
}

/********************************************************************
 *******************  stuff to put the .g into the mysql db *********
 *******************************************************************/

#if 0

#ifndef HUGE
#define HUGE 3.40282347e+38F
#endif

#define MYSQL_USER	"adrt"
#define MYSQL_PASS	"adrt"
#define MYSQL_DB	"adrt"
#define M8 (8*1024*1024)


#define TIENET_BUFFER_INIT(_b, _i) { \
        _b.data = NULL; \
        _b.size = 0; \
        _b.index = 0; \
	_b.incr = _i; }

#define TIENET_BUFFER_FREE(_b) free(_b.data);

#define TIENET_BUFFER_SIZE(_b, _s) { \
        if(_s > _b.size) { \
          _b.data = realloc(_b.data, _s + _b.incr); \
          _b.size = _s + _b.incr; \
        } }

#define TIENET_BUFFER_WRITE(_a, _b, _c, _d) { \
	memcpy(&((uint8_t *)_a.data)[_b], _c, _d); \
	_a.index += _d; }

typedef struct tienet_buffer_s {
  void *data;
  uint32_t size;
  uint32_t index;
  uint32_t incr;
} tienet_buffer_t;


typedef struct attributes_s {
  char name[256];
  float color[3];
  float emission;
} attributes_t;


typedef struct regmap_s {
  char name[256];
  int id;
} regmap_t;


typedef struct mesh_map_s {
  char mesh[256];
  char attr[256];
} mesh_map_t;


static struct rt_i *rtip;
int bot_count = 0, attr_num, regmap_num, use_regmap;
attributes_t *attr_list;
regmap_t *regmap_list;
mesh_map_t *mesh_map;
unsigned int total_tri_num, mesh_map_ind;
struct bu_vls*	region_name_from_path(struct db_full_path *pathp);

tienet_buffer_t geom;
tienet_buffer_t attr;


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
static struct bu_vls* region_name_from_path(struct db_full_path *pathp) {
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
   * phong attributes are probably here:
   *	bu_vls_addr(combp->shader)
   */

  /* If no color is found, assign them a 75% gray color */
  color[0] = combp->rgb[0];
  color[1] = combp->rgb[1];
  color[2] = combp->rgb[2];

  if(color[0]+color[1]+color[2] == 0) {
    color[0] = 192;
    color[1] = 192;
    color[2] = 192;
  }

  /* Do a lookup conversion on the name with the region map if used */
  strcpy(name, pathp->fp_names[pathp->fp_len-1]->d_namep);

  /* If name is null, skip it */
  if(!strlen(name))
    return(0);

  /* replace the BRL-CAD name with the regmap name */
  if(use_regmap)
    regmap_lookup(name, tsp->ts_regionid);

  found = 0;
  for(i = 0; i < attr_num; i++) {
    if(!strcmp(attr_list[i].name, name)) {
      found = 1;
      continue;
    }
  }

  if(!found) {
    attr_list = (attributes_t *)realloc(attr_list, sizeof(attributes_t) * (attr_num + 1));
    strcpy(attr_list[attr_num].name, name);
    attr_list[attr_num].color[0] = color[0] / 255.0;
    attr_list[attr_num].color[1] = color[1] / 255.0;
    attr_list[attr_num].color[2] = color[2] / 255.0;
    if(strstr(bu_vls_strgrab((struct vu_vls *)&(combp->shader)), "light")) {
      attr_list[attr_num].emission = 1.0;
    } else {
      attr_list[attr_num].emission = 0.0;
    }
    attr_num++;
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
static union tree *adrt_mysql_leaf_func(struct db_tree_state *tsp,
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
    uint32_t hash, i, n, size;
    char attr_name[256], mesh_name[256];
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

    /* Find the Attributes Name */
    attr_name[0] = 0;


    if(use_regmap) {
      vlsp = region_name_from_path(pathp);
      strcpy(mesh_name, bu_vls_strgrab(vlsp));
      regmap_lookup(mesh_name, tsp->ts_regionid);
      bu_free(vlsp, "vls");
    } else {
      strcpy(mesh_name, db_path_to_string(pathp));
    }

    vlsp = region_name_from_path(pathp);
    strcpy(attr_name, bu_vls_strgrab(vlsp));
    regmap_lookup(attr_name, tsp->ts_regionid);
    bu_free(vlsp, "vls");

    /* if name is null, assign default attribute */
    if(!strlen(attr_name))
      strcpy(attr_name, "default");

    /* Grab the chars from the end till the '/' */
    i = strlen(attr_name)-1;
    if(i >= 0)
      while(attr_name[i] != '/' && i > 0)
        i--;

    if(i != strlen(attr_name))
      strcpy(attr_name, &attr_name[i+1]);

    /* Display Status */
    printf("bots processed: %d\r", ++bot_count);
    fflush(stdout);

    /* this is for librt's benefit */
    BU_GETSTRUCT(stp, soltab);
    VSETALL(stp->st_min, HUGE);
    VSETALL(stp->st_max, -HUGE);

    for(vp = &bot->vertices[bot->num_vertices-1]; vp >= bot->vertices; vp -= 3) {
	VMINMAX(stp->st_min, stp->st_max, vp);
    }
    VMINMAX(rtip->mdl_min, rtip->mdl_max, stp->st_min);
    VMINMAX(rtip->mdl_min, rtip->mdl_max, stp->st_max);

    /* Write bot/mesh name */
    c = strlen(mesh_name) + 1;

    size = geom.index;
    size += 1; /* mesh name length */
    size += c; /* mesh name */
    size += sizeof(uint32_t); /* vertice num */
    size += 3 * bot->num_vertices * 3 * sizeof(float); /* vertice data */

    TIENET_BUFFER_SIZE(geom, size);
    TIENET_BUFFER_WRITE(geom, geom.index, &c, 1);
    TIENET_BUFFER_WRITE(geom, geom.index, mesh_name, c);

    mesh_map = (mesh_map_t *)realloc(mesh_map, sizeof(mesh_map_t) * (mesh_map_ind + 1));

    strcpy(mesh_map[mesh_map_ind].mesh, mesh_name);
    strcpy(mesh_map[mesh_map_ind].attr, attr_name);
    mesh_map_ind++;

    /* Pack number of vertices */
    TIENET_BUFFER_WRITE(geom, geom.index, &bot->num_vertices, sizeof(uint32_t));

    for(i = 0; i < bot->num_vertices; i++) {
      /* Change scale from mm to meters */
      vec[0] = bot->vertices[3*i+0] * 0.001;
      vec[1] = bot->vertices[3*i+1] * 0.001;
      vec[2] = bot->vertices[3*i+2] * 0.001;

      TIENET_BUFFER_WRITE(geom, geom.index, vec, 3 * sizeof(float));
    }

    /* Add to total number of triangles */
    total_tri_num += bot->num_faces;

    if(bot->num_faces < 1<<16) {
      unsigned short ind;

      TIENET_BUFFER_SIZE(geom, geom.index + 1 + (3 * bot->num_faces + 1) * sizeof(uint16_t));

      /* using unsigned 16-bit */
      c = 0;
      TIENET_BUFFER_WRITE(geom, geom.index, &c, 1);

      /* Pack number of faces */
      ind = bot->num_faces;
      TIENET_BUFFER_WRITE(geom, geom.index, &ind, sizeof(uint16_t));
      for(i = 0; i < 3 * bot->num_faces; i++) {
        ind = bot->faces[i];
        TIENET_BUFFER_WRITE(geom, geom.index, &ind, sizeof(uint16_t));
      }
    } else {
      TIENET_BUFFER_SIZE(geom, geom.index + 1 + (3 * bot->num_faces + 1) * sizeof(uint32_t));

      /* using unsigned 32-bit */
      c = 1;
      TIENET_BUFFER_WRITE(geom, geom.index, &c, 1);
      TIENET_BUFFER_WRITE(geom, geom.index, &bot->num_faces, sizeof(uint32_t));
      TIENET_BUFFER_WRITE(geom, geom.index, bot->faces, 3 * bot->num_faces * sizeof(uint32_t));
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


static void adrt_mysql_load_regmap(char *filename) {
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
    printf("using region map: %s\n", filename);
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


int adrt_mysql_shunt(int argc, char *argv[]) {
  MYSQL mysql_db;
  MYSQL_RES *res;
  MYSQL_ROW row;
  struct db_tree_state ts;
  struct directory *dp;
  int i;
  uint16_t s;
  uint32_t gid;
  char idbuf[132], filename[256];
  char query_head[256], query_tail[256], c;
  unsigned char len;


  if(argc <= 4) {
    printf("Usage: g-adrt [-r region.map] file.g mysql_hostname name_for_geometry [region list]\n");
    exit(1);
  }

  /* Initialize MySQL */
  mysql_init(&mysql_db);
  if(!mysql_real_connect(&mysql_db, argv[2], MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0, 0, 0)) {
    printf("Connection failed\n");
    return;
  }

  /* Initialize geom and attr data buffers */
  TIENET_BUFFER_INIT(geom, M8);
  TIENET_BUFFER_INIT(attr, M8);

  /* write 2-byte endian */
  s = 1;
  TIENET_BUFFER_SIZE(geom, geom.index + sizeof(uint16_t));
  TIENET_BUFFER_WRITE(geom, geom.index, &s, sizeof(uint16_t));


  bot_count = 0;
  mesh_map_ind = 0;
  attr_num = 0;
  attr_list = NULL;
  mesh_map = NULL;


  /*
  * Load database.
  * rt_dirbuild() returns an "instance" pointer which describes
  * the database to be ray traced.  It also gives you back the
  * title string in the header (ID) record.
  */

  if((rtip = rt_dirbuild(argv[1], idbuf, sizeof(idbuf))) == RTI_NULL) {
    fprintf(stderr,"rtexample: rt_dirbuild failure\n");
    exit(2);
  }

  ts = rt_initial_tree_state;
  ts.ts_dbip = rtip->rti_dbip;
  ts.ts_rtip = rtip;
  ts.ts_resp = NULL;
  bu_avs_init(&ts.ts_attrs, 1, "avs in tree_state");

  /*
  * Generage geometry data
  */
  db_walk_tree(rtip->rti_dbip, argc - 4, (const char **)&argv[4], 1, &ts, &reg_start_func, &reg_end_func, &adrt_mysql_leaf_func, (void *)0);

  /*
  * Insert geometry data into database
  */
  {
    tienet_buffer_t temp;

    TIENET_BUFFER_INIT(temp, M8);
    TIENET_BUFFER_SIZE(temp, geom.index * 2 + 1);
    temp.index = mysql_real_escape_string(&mysql_db, temp.data, geom.data, geom.index);

    /* The Query */
    sprintf(query_head, "insert into geometry values('', '%s', '%d', '%d', '%d', '", argv[3], total_tri_num, bot_count, geom.index);
    sprintf(query_tail, "', 0, '')");
    TIENET_BUFFER_SIZE(geom, temp.index + strlen(query_head) + strlen(query_tail));
    geom.index = 0;
    TIENET_BUFFER_WRITE(geom, geom.index, query_head, strlen(query_head));
    TIENET_BUFFER_WRITE(geom, geom.index, temp.data, temp.index);
    TIENET_BUFFER_WRITE(geom, geom.index, query_tail, strlen(query_tail));

    mysql_real_query(&mysql_db, (const char *)geom.data, geom.index);
    gid = mysql_insert_id(&mysql_db);

    TIENET_BUFFER_FREE(temp);
  }


  /*
  * Populate attributes table
  */
  sprintf(query_head, "insert into attributes values('', '%d', '%s', '%s', '%.3f %.3f %.3f')", gid, "default", "color", 0.8, 0.8, 0.8);
  mysql_real_query(&mysql_db, (const char *)query_head, strlen(query_head));

  for(i = 0; i < attr_num; i++) {
    sprintf(query_head, "insert into attributes values('', '%d', '%s', '%s', '%.3f %.3f %.3f')", gid, attr_list[i].name, "color", attr_list[i].color[0], attr_list[i].color[1], attr_list[i].color[2]);
    mysql_real_query(&mysql_db, (const char *)query_head, strlen(query_head));
    if(attr_list[i].emission > 0.0) {
      sprintf(query_head, "insert into attributes values('', '%d', '%s', '%s', '%.3f')", gid, attr_list[i].name, "emission", attr_list[i].emission);
      mysql_real_query(&mysql_db, (const char *)query_head, strlen(query_head));
    }
  }


  /*
  * Generate a mesh map file
  */
  for(i = 0; i < mesh_map_ind; i++) {
    sprintf(query_head, "insert into meshattrmap values('', '%d', '%s', '%s')", gid, mesh_map[i].mesh, mesh_map[i].attr);
    mysql_real_query(&mysql_db, (const char *)query_head, strlen(query_head));
  }

  printf("\ncomplete: %d triangles.\n", total_tri_num);

  mysql_close(&mysql_db);
}

#endif

#else
int
slave_load_MySQL (uint32_t pid, tie_t *tie, const char *hostname)
{
	bu_log("MySQL not supported in this build.\n");
	return -1;
}
#endif

