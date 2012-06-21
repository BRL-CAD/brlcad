#include "common.h"
#include "vmath.h"
#include "raytrace.h"

#include "opennurbs_fit.h"

void
BoT2Vector3d(struct rt_bot_internal *ip, on_fit::vector_vec3d &data)
{ 
  int i = 0;
  fastf_t *vertices = ip->vertices;
  for (i = 0; i < ip->num_vertices; i++) {
      printf("v %f %f %f\n", V3ARGS(&vertices[3*i]));
      data.push_back (Eigen::Vector3d (V3ARGS(&vertices[3*i])));
  }
}


int
main (int argc, char *argv[])
{
   struct db_i *dbip;
   struct directory *dp;
   struct rt_db_internal internal;
   struct rt_bot_internal *bot_ip;
   mat_t mat;

   unsigned order (3);
   unsigned refinement (5);
   unsigned iterations (2);
   on_fit::NurbsDataSurface data;


   if (argc != 3) {
      bu_exit(1, "Usage: %s file.g object", argv[0]);
   }

   dbip = db_open(argv[1], "r");
   //dbip = db_open(argv[1], "r+w");
   if (dbip == DBI_NULL) { 
      bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
   }

   if (db_dirbuild(dbip) < 0)
      bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
  
   dp = db_lookup(dbip, argv[2], LOOKUP_QUIET); 
   if (dp == RT_DIR_NULL) { 
      bu_exit(1, "ERROR: Unable to look up object %s\n", argv[2]);
   }

   if (rt_db_get_internal(&internal, dp, dbip, mat, &rt_uniresource) < 0) {
      bu_exit(1, "ERROR: Unable to get internal representation of %s\n", argv[2]);
   }

   if (internal.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
      bu_exit(1, "ERROR: object %s does not appear to be of type BoT\n", argv[2]);
   } else {
      bot_ip = (struct rt_bot_internal *)(&internal)->idb_ptr;   
   }
 
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
	   fit.assemble (params);
	   fit.solve ();
   }

   // print resulting surface
   ON_wString wonstr;
   ON_TextLog log(wonstr);
   fit.m_nurbs.Dump(log);
   ON_String onstr = ON_String(wonstr);
   printf("NURBS surface:\n");
   printf("%s\n", onstr.Array());

   return 0;
}
