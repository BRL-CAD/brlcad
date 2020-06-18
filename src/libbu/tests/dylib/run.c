/*                           R U N . C
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file run.c
 *
 * Program to load libdylib plugins and test that they
 * work correctly.
 *
 */

#include "common.h"
#include "bu.h"
#include "dylib.h"

int main(int UNUSED(ac), const char *av[]) {
    int expected_plugins = 2;
    struct bu_ptbl plugins = BU_PTBL_INIT_ZERO;
    struct bu_ptbl handles = BU_PTBL_INIT_ZERO;

    bu_setprogname(av[0]);

    int pcnt = dylib_load_plugins(&plugins, &handles);
    if (pcnt != expected_plugins) {
	bu_log("Expected %d plugins, found %d.\n", expected_plugins, pcnt);
	bu_ptbl_free(&plugins);
	(void)dylib_close_plugins(&handles);
	bu_ptbl_free(&handles);
	return -1;
    } else {
	bu_log("Found %d plugins.\n", pcnt);
    }

    int expected_results = 1;
    for (size_t i = 0; i < BU_PTBL_LEN(&plugins); i++) {
	const struct dylib_contents *p = (const struct dylib_contents *)BU_PTBL_GET(&plugins, i);
	if (BU_STR_EQUAL(p->name, "Plugin 1")) {
	    // Check that plugin 1 does what we expect
	    double eversion = 1.0;
	    if (!NEAR_EQUAL(p->version, eversion, SMALL_FASTF)) {
		bu_log("%s: expected version %f plugins, found %f.\n", p->name, eversion, p->version);
		expected_results = 0;
	    } else {
		bu_log("%s: got expected plugin version: %f.\n", p->name, p->version);
	    }
	    int rstr_len = 10;
	    char *cresult = (char *)bu_calloc(rstr_len, sizeof(char), "result buffer");
	    int calc_test = p->calc((char **)&cresult, rstr_len, 2);
	    if (calc_test) {
	    	bu_log("%s: plugin reports insufficient space in results buffer.\n", p->name);
		expected_results = 0;
	    }
	    const char *ecalc = "4";
	    if (!BU_STR_EQUAL(cresult, ecalc)) {
		bu_log("%s: expected to calculate %s, got %s.\n", p->name, ecalc, cresult);
		expected_results = 0;
	    } else {
		bu_log("%s: got expected result: %s.\n", p->name, cresult);
	    }
	    bu_free(cresult, "result container");
	}
	if (BU_STR_EQUAL(p->name, "Plugin 2")) {
	    // Check that plugin 2 does what we expect
	    double eversion = 2.3;
	    if (!NEAR_EQUAL(p->version, eversion, SMALL_FASTF)) {
		bu_log("%s: expected version %f plugins, found %f.\n", p->name, eversion, p->version);
		expected_results = 0;
	    }
	    int rstr_len = 10;
	    char *cresult = (char *)bu_calloc(rstr_len, sizeof(char), "result buffer");
	    int calc_test = p->calc((char **)&cresult, rstr_len, 4);
	    if (calc_test) {
	    	bu_log("%s: plugin reports insufficient space in results buffer.\n", p->name);
		expected_results = 0;
	    }
	    const char *ecalc = "400";
	    if (!BU_STR_EQUAL(cresult, ecalc)) {
		bu_log("%s: expected to calculate %s, got %s.\n", p->name, ecalc, cresult);
		expected_results = 0;
	    } else {
		bu_log("%s: got expected result: %s.\n", p->name, cresult);
	    }
	    bu_free(cresult, "result container");
	}
    }

    bu_ptbl_free(&plugins);

    if (dylib_close_plugins(&handles)) {
	bu_log("bu_dlclose failed to unload plugins.\n");
	expected_results = 0;
    }
    bu_ptbl_free(&handles);

    // If everything is as expected, return 0
    return (!expected_results);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
