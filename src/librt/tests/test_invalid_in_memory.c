#include "common.h"
#include "vmath.h"
#include "bu/vls.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "bn/tol.h"

int test_rpc() {
    struct rt_db_internal intern;
    struct rt_rpc_internal rpc;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    int issues;
    int ret = 0;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_RPC;
    intern.idb_ptr = &rpc;
    intern.idb_meth = &OBJ[ID_RPC];

    VSET(rpc.rpc_V, 0, 0, 0);
    VSET(rpc.rpc_H, 0, 0, 1);
    VSET(rpc.rpc_B, 1, 0, 0);
    rpc.rpc_r = 5.0;
    rpc.rpc_magic = RT_RPC_INTERNAL_MAGIC;

    VSET(rpc.rpc_B, 0.5, 0, 1.0);

    issues = OBJ[ID_RPC].ft_validate(&msg, &intern, &tol);
    if (issues == 0) {
        bu_log("RPC validate failed to catch non-perpendicular B and H!\n");
        ret = 1;
    }

    if (EDOBJ[ID_RPC].ft_repair) {
        bu_vls_trunc(&msg, 0);
        int rep = EDOBJ[ID_RPC].ft_repair(&msg, &intern, &tol, 0, NULL);
        if (rep < 0) {
            bu_log("RPC repair returned error!\n");
            ret = 1;
        }

        bu_vls_trunc(&msg, 0);
        issues = OBJ[ID_RPC].ft_validate(&msg, &intern, &tol);
        if (issues > 0) {
            bu_log("RPC validation still failed after repair!\n");
            ret = 1;
        }
    } else {
        bu_log("RPC repair missing!\n");
        ret = 1;
    }

    bu_vls_free(&msg);
    return ret;
}

int test_eto() {
    struct rt_db_internal intern;
    struct rt_eto_internal eto;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    int issues;
    int ret = 0;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_ETO;
    intern.idb_ptr = &eto;
    intern.idb_meth = &OBJ[ID_ETO];

    VSET(eto.eto_V, 0, 0, 0);
    VSET(eto.eto_N, 0, 0, 1);
    VSET(eto.eto_C, 5, 0, 0);
    eto.eto_r = 5.0;
    eto.eto_rd = 2.0;
    eto.eto_magic = RT_ETO_INTERNAL_MAGIC;

    VSET(eto.eto_C, 5, 0, 1);

    issues = OBJ[ID_ETO].ft_validate(&msg, &intern, &tol);
    if (issues == 0) {
        bu_log("ETO validate failed to catch non-perpendicular N and C!\n");
        ret = 1;
    }

    if (EDOBJ[ID_ETO].ft_repair) {
        bu_vls_trunc(&msg, 0);
        int rep = EDOBJ[ID_ETO].ft_repair(&msg, &intern, &tol, 0, NULL);
        if (rep < 0) {
            bu_log("ETO repair returned error!\n");
            ret = 1;
        }

        bu_vls_trunc(&msg, 0);
        issues = OBJ[ID_ETO].ft_validate(&msg, &intern, &tol);
        if (issues > 0) {
            bu_log("ETO validation still failed after repair!\n");
            ret = 1;
        }
    } else {
        bu_log("ETO repair missing!\n");
        ret = 1;
    }

    bu_vls_free(&msg);
    return ret;
}


int test_ehy() {
    struct rt_db_internal intern;
    struct rt_ehy_internal ehy;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    int issues, ret = 0;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_EHY;
    intern.idb_ptr = &ehy;
    intern.idb_meth = &OBJ[ID_EHY];

    VSET(ehy.ehy_V, 0, 0, 0);
    VSET(ehy.ehy_H, 0, 0, 1);
    VSET(ehy.ehy_Au, 1, 0, 0);
    ehy.ehy_r1 = 5.0;
    ehy.ehy_r2 = 5.0;
    ehy.ehy_c = 1.0;
    ehy.ehy_magic = RT_EHY_INTERNAL_MAGIC;

    VSET(ehy.ehy_Au, 0.5, 0, 1.0);

    issues = OBJ[ID_EHY].ft_validate(&msg, &intern, &tol);
    if (issues == 0) {
        bu_log("EHY validate failed to catch non-perpendicular Au and H!\n");
        ret = 1;
    }

    if (EDOBJ[ID_EHY].ft_repair) {
        bu_vls_trunc(&msg, 0);
        int rep = EDOBJ[ID_EHY].ft_repair(&msg, &intern, &tol, 0, NULL);
        if (rep < 0) {
            bu_log("EHY repair returned error!\n");
            ret = 1;
        }

        bu_vls_trunc(&msg, 0);
        issues = OBJ[ID_EHY].ft_validate(&msg, &intern, &tol);
        if (issues > 0) {
            bu_log("EHY validation still failed after repair!\n");
            ret = 1;
        }
    } else {
        bu_log("EHY repair missing!\n");
        ret = 1;
    }
    bu_vls_free(&msg);
    return ret;
}

int test_epa() {
    struct rt_db_internal intern;
    struct rt_epa_internal epa;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    int issues, ret = 0;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_EPA;
    intern.idb_ptr = &epa;
    intern.idb_meth = &OBJ[ID_EPA];

    VSET(epa.epa_V, 0, 0, 0);
    VSET(epa.epa_H, 0, 0, 1);
    VSET(epa.epa_Au, 1, 0, 0);
    epa.epa_r1 = 5.0;
    epa.epa_r2 = 5.0;
    epa.epa_magic = RT_EPA_INTERNAL_MAGIC;

    VSET(epa.epa_Au, 0.5, 0, 1.0);

    issues = OBJ[ID_EPA].ft_validate(&msg, &intern, &tol);
    if (issues == 0) {
        bu_log("EPA validate failed to catch non-perpendicular Au and H!\n");
        ret = 1;
    }

    if (EDOBJ[ID_EPA].ft_repair) {
        bu_vls_trunc(&msg, 0);
        int rep = EDOBJ[ID_EPA].ft_repair(&msg, &intern, &tol, 0, NULL);
        if (rep < 0) {
            bu_log("EPA repair returned error!\n");
            ret = 1;
        }

        bu_vls_trunc(&msg, 0);
        issues = OBJ[ID_EPA].ft_validate(&msg, &intern, &tol);
        if (issues > 0) {
            bu_log("EPA validation still failed after repair!\n");
            ret = 1;
        }
    } else {
        bu_log("EPA repair missing!\n");
        ret = 1;
    }
    bu_vls_free(&msg);
    return ret;
}

int test_rhc() {
    struct rt_db_internal intern;
    struct rt_rhc_internal rhc;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    int issues, ret = 0;

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = ID_RHC;
    intern.idb_ptr = &rhc;
    intern.idb_meth = &OBJ[ID_RHC];

    VSET(rhc.rhc_V, 0, 0, 0);
    VSET(rhc.rhc_H, 0, 0, 1);
    VSET(rhc.rhc_B, 1, 0, 0);
    rhc.rhc_r = 5.0;
    rhc.rhc_c = 1.0;
    rhc.rhc_magic = RT_RHC_INTERNAL_MAGIC;

    VSET(rhc.rhc_B, 0.5, 0, 1.0);

    issues = OBJ[ID_RHC].ft_validate(&msg, &intern, &tol);
    if (issues == 0) {
        bu_log("RHC validate failed to catch non-perpendicular B and H!\n");
        ret = 1;
    }

    if (EDOBJ[ID_RHC].ft_repair) {
        bu_vls_trunc(&msg, 0);
        int rep = EDOBJ[ID_RHC].ft_repair(&msg, &intern, &tol, 0, NULL);
        if (rep < 0) {
            bu_log("RHC repair returned error!\n");
            ret = 1;
        }

        bu_vls_trunc(&msg, 0);
        issues = OBJ[ID_RHC].ft_validate(&msg, &intern, &tol);
        if (issues > 0) {
            bu_log("RHC validation still failed after repair!\n");
            ret = 1;
        }
    } else {
        bu_log("RHC repair missing!\n");
        ret = 1;
    }
    bu_vls_free(&msg);
    return ret;
}

int main() {
    int errors = 0;
    
    bu_log("Testing RPC...\n");
    errors += test_rpc();

    bu_log("Testing ETO...\n");
    errors += test_eto();

    bu_log("Testing EHY...\n");
    errors += test_ehy();

    bu_log("Testing EPA...\n");
    errors += test_epa();

    bu_log("Testing RHC...\n");
    errors += test_rhc();

    if (errors > 0) {
        bu_log("FAILED\n");
        return 1;
    }
    bu_log("SUCCESS\n");
    return 0;
}
