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

#include "tienet.h"
#include "umath.h"

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
	MATH_VEC_SET(slave_load_mesh_list[mind].attributes->color, 0.8, 0.8, 0.8);

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
#else
int
slave_load_MySQL (uint32_t pid, tie_t *tie, const char *hostname)
{
	bu_log("MySQL not supported in this build.\n");
	return -1;
}
#endif

