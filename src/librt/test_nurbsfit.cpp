/*               T E S T _ N U R B S F I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file test_nurbsfit.cpp
 *
 * Test the fitting routines that map a NURBS surface to a BoT patch.
 * *Very* simple - doesn't validate that the BoT projects to a plane
 * without overlaps.
 *
 */

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"

#include "opennurbs_fit.h"

void
BoT2Vector3d(struct rt_bot_internal *ip, on_fit::vector_vec3d &data)
{
  int i = 0;
  for (i = 0; i < ip->num_vertices; i++) {
      //printf("v %f %f %f\n", V3ARGS(&ip->vertices[3*i]));
      data.push_back (ON_3dPoint(V3ARGS(&ip->vertices[3*i])));
  }
}


int
main (int argc, char *argv[])
{
   struct db_i *dbip;
   struct directory *dp;
   struct rt_db_internal intern;
   struct rt_bot_internal *bot_ip;
   struct rt_wdb *wdbp;

   unsigned order (3);
   unsigned refinement (5);
   unsigned iterations (2);
   on_fit::NurbsDataSurface data;
   ON_Brep* brep = ON_Brep::New();
   char *bname;


   if (argc != 3) {
      bu_exit(1, "Usage: %s file.g object", argv[0]);
   }

   //dbip = db_open(argv[1], DB_OPEN_READONLY);
   dbip = db_open(argv[1], DB_OPEN_READWRITE);
   if (dbip == DBI_NULL) {
      bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
   }

   if (db_dirbuild(dbip) < 0)
      bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

   dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
   if (dp == RT_DIR_NULL) {
      bu_exit(1, "ERROR: Unable to look up object %s\n", argv[2]);
   } else {
      bu_log("Got %s\n", dp->d_namep);
   }

   RT_DB_INTERNAL_INIT(&intern)
   if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
      bu_exit(1, "ERROR: Unable to get internal representation of %s\n", argv[2]);
   }

   if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
      bu_exit(1, "ERROR: object %s does not appear to be of type BoT\n", argv[2]);
   } else {
      bot_ip = (struct rt_bot_internal *)intern.idb_ptr;
   }
   RT_BOT_CK_MAGIC(bot_ip);

   BoT2Vector3d(bot_ip, data.interior);

   ON_NurbsSurface nurbs = on_fit::FittingSurface::initNurbsPCABoundingBox (order, &data);
   on_fit::FittingSurface fit (&data, nurbs);
   on_fit::FittingSurface::Parameter params;
   params.interior_smoothness = 0.15;
   params.interior_weight = 1.0;
   params.boundary_smoothness = 0.15;
   params.boundary_weight = 0.0;

   // NURBS refinement
   for (unsigned i = 0; i < refinement; i++)
   {
	   fit.refine (0);
	   fit.refine (1);
   }

   // fitting iterations
   for (unsigned i = 0; i < iterations; i++)
   {
	   bu_log("Assembling params - iteration %d\n", i);
	   fit.assemble (params);
	   bu_log("Solving - iteration %d\n", i);
	   fit.solve ();
   }

   // print resulting surface
   ON_wString wonstr;
   ON_TextLog log(wonstr);
   fit.m_nurbs.Dump(log);
   ON_String onstr = ON_String(wonstr);
   printf("NURBS surface:\n");
   printf("%s\n", onstr.Array());

   brep->NewFace(fit.m_nurbs);
   wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
   bname = (char*)bu_malloc(strlen(argv[2])+6, "char");
   bu_strlcpy(bname, argv[2], strlen(argv[2])+1);
   bu_strlcat(bname, "_brep", strlen(bname)+6);
   if (mk_brep(wdbp, bname, brep) == 0) {
      bu_log("Generated brep object %s\n", bname);
   }
   bu_free(bname, "char");

   return 0;
}
