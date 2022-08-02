/*                     T E S T _ D B I O . C
 * BRL-CAD
 *
 * Copyright (c) 2019-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file test_dbio.c
 *
 * Test performances of the database I/O routines
 *
 */

#include "brlcad.h"


void
print_elapsed(const char *label, int64_t elapsed)
{
    bu_log("%s elapsed: %.6f\n", label, (double)elapsed / 1000000.0);
}


int
main(int ac, char *av[])
{
    const char *gfile = "speed.g";
    struct rt_wdb *wdbfp;
    struct db_i *dbip;
    size_t iterations = 10;
    int64_t t, t2;

    bu_setprogname(av[0]);

    if (ac < 2 || ac > 3) {
	bu_exit(0, "Usage: %s {file.g} [iterations]\n", av[0]);
    }
    if (ac > 1) {
	gfile = av[1];
	if (bu_file_exists(gfile, NULL)) {
	    bu_exit(1, "ERROR: %s is in the way.  Delete or move.\n", gfile);
	}
    }
    if (ac > 2) {
	iterations = (size_t)strtol(av[2], NULL, 0);
    }

    {
	t = bu_gettime();
	for (size_t i = 0; i < iterations; i++) {
	    wdbfp = wdb_fopen(gfile);
	    wdb_close(wdbfp);
	}
	wdbfp = wdb_fopen(gfile);
	print_elapsed("wdb_fopen", bu_gettime() - t);

	if (!wdbfp) {
	    bu_exit(2, "ERROR: failed to open %s\n", gfile);
	}
    }

    {
	t = bu_gettime();
	for (size_t i = 0; i < iterations; i++) {
	    mk_id(wdbfp, "title");
	}
	print_elapsed("mk_id", bu_gettime() - t);
    }

    {
	struct wmember comblist;
	BU_LIST_INIT(&comblist.l);

	for (size_t i = 0; i < 5; i++) {
	    struct bu_vls sph = BU_VLS_INIT_ZERO;
	    static int num = 0;
	    vect_t pos = VINIT_ZERO;
	    fastf_t radius = bn_randmt() * 999999999999;
	    bu_vls_printf(&sph, "%012d.sph", num++);
	    t = bu_gettime();
	    for (size_t i = 0; i < iterations; i++)
		mk_sph(wdbfp, bu_vls_cstr(&sph), pos, radius);
	    print_elapsed("mk_sph", bu_gettime() - t);

	    t2 = bu_gettime();
	    for (size_t i = 0; i < iterations; i++)
		mk_addmember(bu_vls_cstr(&sph), &comblist.l, NULL, WMOP_UNION);
	    print_elapsed("mk_addmember", bu_gettime() - t2);
	}

	{
	    struct bu_vls comb = BU_VLS_INIT_ZERO;
	    static fastf_t count = 0;
	    bu_vls_printf(&comb, "%012lf.comb", count++);
	    t = bu_gettime();
	    /* FIXME: kaboom */
	    mk_lcomb(wdbfp, bu_vls_cstr(&comb), &comblist, 0, NULL, NULL, NULL, 0);
	    print_elapsed("mk_lcomb", bu_gettime() - t);
	}
    }

    {
	t = bu_gettime();
	for (size_t i = 0; i < iterations; i++) {
	    wdb_close(wdbfp);
	}
	print_elapsed("wdb_close", bu_gettime() - t);
    }

    {
	t = bu_gettime();
	for (size_t i = 0; i < iterations; i++) {
	    dbip = db_open(gfile, DB_OPEN_READONLY);
	    db_close(dbip);
	}
	dbip = db_open(gfile, DB_OPEN_READONLY);
	print_elapsed("db_open", bu_gettime() - t);

	if (!dbip) {
	    bu_exit(2, "ERROR: Unable to open database [%s]\n", gfile);
	}
    }

    {
	t = bu_gettime();
	for (size_t i = 0; i < iterations; i++) {
	    db_dirbuild(dbip);
	}
	print_elapsed("db_dirbuild", bu_gettime() - t);
    }

    {
	t = bu_gettime();
	for (size_t i = 0; i < iterations; i++) {
	    db_close(dbip);
	}
	print_elapsed("db_close", bu_gettime() - t);
    }

    return 0;
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
