#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "brep.h"

#include "opennurbs.h"
#include "libbrep_surfacetree.h"

int
main (int argc, char *argv[])
{
   struct db_i *dbip;
   struct directory *dp;
   struct rt_db_internal intern;
   struct rt_brep_internal *brep_ip;
   struct rt_wdb *wdbp;

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

   if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
      bu_exit(1, "ERROR: object %s does not appear to be of type BREP\n", argv[2]);
   } else {
      brep_ip = (struct rt_brep_internal *)intern.idb_ptr;
   }

    ON_Brep *brep = brep_ip->brep;
    struct bu_vls vls;
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "surfaces:  %d\n", brep->m_S.Count());
    bu_vls_printf(&vls, "3d curve:  %d\n", brep->m_C3.Count());
    bu_vls_printf(&vls, "2d curves: %d\n", brep->m_C2.Count());
    bu_vls_printf(&vls, "vertices:  %d\n", brep->m_V.Count());
    bu_vls_printf(&vls, "edges:     %d\n", brep->m_E.Count());
    bu_vls_printf(&vls, "trims:     %d\n", brep->m_T.Count());
    bu_vls_printf(&vls, "loops:     %d\n", brep->m_L.Count());
    bu_vls_printf(&vls, "faces:     %d\n", brep->m_F.Count());
    printf("%s\n", bu_vls_addr(&vls));
    bu_vls_free(&vls);

    for(int i = 0; i < brep->m_F.Count(); i++) {
       ON_BrepFace& face = brep->m_F[i];
       std::cout << "Face: " << i << "\n";
       ON_SurfaceTree *stree = new ON_SurfaceTree(&face);
       delete stree;
       //std::cout << "Face(old): " << i << "\n";
       //brlcad::SurfaceTree *st = new brlcad::SurfaceTree(&face);
    }

    db_close(dbip);

   return 0;
}
