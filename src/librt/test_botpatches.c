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
   struct rt_bot_list *headRblp = NULL;
   struct rt_bot_list *split_bots = NULL;
   struct rt_db_internal bot_intern;
   struct rt_bot_list *rblp;
   struct rt_bot_list *splitlp;
   struct bu_vls name;

   int i = 0;
   int j = 0;

   bu_vls_init(&name);

   if (argc != 3) {
      bu_exit(1, "Usage: %s file.g object", argv[0]);
   }

   dbip = db_open(argv[1], "r+w");
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

   headRblp = rt_bot_patches(bot_ip);

   i = 0;
   j = 0;
   for (BU_LIST_FOR(rblp, rt_bot_list, &headRblp->l)) {
	   split_bots = rt_bot_split(rblp->bot);
	   for (BU_LIST_FOR(splitlp, rt_bot_list, &split_bots->l)) {
		   bu_vls_sprintf(&name, "auto-%d_%d.bot", i, j);
		   RT_DB_INTERNAL_INIT(&bot_intern);
		   bot_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		   bot_intern.idb_type = ID_BOT;
		   bot_intern.idb_meth = &rt_functab[ID_BOT];
		   bot_intern.idb_ptr = (genptr_t)splitlp->bot;
		   dp = db_diradd(dbip, bu_vls_addr(&name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&bot_intern.idb_type);
		   if (dp == RT_DIR_NULL) {
			   printf("auuugh - dp says diradd failed!\n");
		   } else {
			   if (rt_db_put_internal(dp, dbip, &bot_intern, &rt_uniresource) < 0) {
				   printf("auuugh - put_internal failed!\n");
			   }
		   }
                   j++;
	   }
	   i++;
   }

   rt_bot_list_free(headRblp, 0);
   rt_db_free_internal(&intern);
   bu_vls_free(&name); 

   return 0;
}
