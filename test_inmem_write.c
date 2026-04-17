#include "common.h"
#include <stdio.h>
#include <string.h>
#include "raytrace.h"
#include "rt/primitives/ell.h"
#include "wdb.h"

/* OBJ is globally defined in librt */
extern const struct rt_functab OBJ[];

int main() {
    struct db_i *dbip;
    struct directory *dp;
    struct rt_db_internal internal;
    struct rt_ell_internal *ell;

    /* 1. Create in-memory database */
    dbip = db_create_inmem();
    if (dbip == DBI_NULL) {
        fprintf(stderr, "Failed to create in-memory database\n");
        return 1;
    }

    /* 2. Initialize internal structure and manually set methods from OBJ table */
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_minor_type = ID_ELL;
    internal.idb_meth = &OBJ[ID_ELL];

    /* 3. Prepare test object (ellipsoid) */
    ell = (struct rt_ell_internal *)bu_malloc(sizeof(struct rt_ell_internal), "ell");
    memset(ell, 0, sizeof(struct rt_ell_internal));
    ell->magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell->v, 0, 0, 0);
    VSET(ell->a, 100, 0, 0);
    VSET(ell->b, 0, 100, 0);
    VSET(ell->c, 0, 0, 100);
    internal.idb_ptr = (void *)ell;

    /* 4. Create directory entry */
    /* Use 0 instead of RT_DIR_PHONY_ADDR to avoid the free(-1) crash in db_inmem */
    dp = db_diradd(dbip, "test_ell", 0, 0, RT_DIR_INMEM, &ell->magic);
    if (dp == RT_DIR_NULL) {
        fprintf(stderr, " db_diradd failed\n");
        db_close(dbip);
        return 1;
    }

    /* 5. This is the call that used to fail/crash */
    printf("Attempting rt_db_put_internal\n");
    if (rt_db_put_internal(dp, dbip, &internal, &rt_uniresource) < 0) {
        fprintf(stderr, " rt_db_put_internal failed for in-memory database\n");
        db_close(dbip);
        return 1;
    }

    printf(" rt_db_put_internal succeeded on in-memory database\n");
    
    rt_db_free_internal(&internal);
    db_close(dbip);
    return 0;
}
