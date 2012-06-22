#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "plot3.h"


#define LINE_PLOT(p1, p2) pdv_3line(plot, p1, p2) 
#define COLOR_PLOT(r, g, b) pl_color(plot, (r), (g), (b))

int
main (int argc, char *argv[])
{
   struct db_i *dbip;
   struct directory *dp;
   struct rt_db_internal intern;
   struct rt_bot_internal *bot_ip = NULL;

   static FILE* plot = NULL;

   vect_t from_xplus = {-1, 0, 0};
   vect_t from_xminus = {1, 0, 0};
   vect_t from_yplus = {0, -1, 0};
   vect_t from_yminus = {0, 1, 0};
   vect_t from_zplus = {0, 0, -1};
   vect_t from_zminus = {0, 0, 1};

   int i = 0;
   int j = 0;
   point_t min, max;

   if (argc != 3) {
      bu_exit(1, "Usage: %s file.g object", argv[0]);
   }

   dbip = db_open(argv[1], "r");
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

  plot = fopen("out.pl", "w");
  VSET(min, -2048, -2048, -2048);
  VSET(max, 2048, 2048, 2048);
  pdv_3space(plot, min, max);

  /*printf("num_vertices: %d\n", bot_ip->num_vertices);
  printf("num_normals: %d\n", bot_ip->num_normals);
  printf("num_faces: %d\n", bot_ip->num_faces);
  printf("num_face_normals: %d\n", bot_ip->num_face_normals);*/

  for (i = 0; i < (int)bot_ip->num_faces; i++) {
      vect_t a, b, norm_dir;
      fastf_t results[6];
      int result_max = 0;
      fastf_t tmp = 0.0;
      VSETALLN(results, 0, 6);
      VSUB2(a, &bot_ip->vertices[bot_ip->faces[i*3+1]*3], &bot_ip->vertices[bot_ip->faces[i*3]*3]);
      VSUB2(b, &bot_ip->vertices[bot_ip->faces[i*3+2]*3], &bot_ip->vertices[bot_ip->faces[i*3]*3]);
      VCROSS(norm_dir, a, b);
      VUNITIZE(norm_dir);
      results[0] = VDOT(from_xplus, norm_dir);
      results[1] = VDOT(from_xminus, norm_dir);
      results[2] = VDOT(from_yplus, norm_dir);
      results[3] = VDOT(from_yminus, norm_dir);
      results[4] = VDOT(from_zplus, norm_dir);
      results[5] = VDOT(from_zminus, norm_dir);
      VSCALE(norm_dir, norm_dir, 100);
      VADD2(norm_dir, &bot_ip->vertices[bot_ip->faces[i*3+1]*3], norm_dir);
      /*printf("vert %d (%f,%f,%f,%f,%f,%f): ", i, results[0], results[1], results[2], results[3], results[4], results[5]);*/
      for(j = 0; j < 6; j++) {
         if (results[j] > tmp) {
            result_max = j;
            tmp = results[j];
         }
      }

      if (result_max == 0) {
        COLOR_PLOT(255, 0, 0);
	pdv_3move(plot, &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
        LINE_PLOT(&bot_ip->vertices[bot_ip->faces[i*3+1]*3], norm_dir);
        /*printf(" xplus ");*/
      }
      if (result_max == 1) {
        COLOR_PLOT(50, 0, 0);
	pdv_3move(plot, &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
        LINE_PLOT(&bot_ip->vertices[bot_ip->faces[i*3+1]*3], norm_dir);
        /*printf(" xminus ");*/
      }
      if (result_max == 2) {
        COLOR_PLOT(0, 255, 0);
	pdv_3move(plot, &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
        LINE_PLOT(&bot_ip->vertices[bot_ip->faces[i*3+1]*3], norm_dir);
        /* printf(" yplus "); */
      }
      if (result_max == 3) {
        COLOR_PLOT(0, 50, 0);
	pdv_3move(plot, &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
        LINE_PLOT(&bot_ip->vertices[bot_ip->faces[i*3+1]*3], norm_dir);
        /* printf(" yminus "); */
      }
      if (result_max == 4) {
        COLOR_PLOT(0, 0, 255);
	pdv_3move(plot, &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
        LINE_PLOT(&bot_ip->vertices[bot_ip->faces[i*3+1]*3], norm_dir);
        /* printf(" zplus "); */
      }
      if (result_max == 5) {
        COLOR_PLOT(0, 0, 50);
	pdv_3move(plot, &bot_ip->vertices[bot_ip->faces[i*3+1]*3]);
        LINE_PLOT(&bot_ip->vertices[bot_ip->faces[i*3+1]*3], norm_dir);
        /* printf(" zminus "); */
      }
      /*printf("\n");*/
  }
  return 0;
}
