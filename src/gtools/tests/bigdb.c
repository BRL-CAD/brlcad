/*                         B I G D B . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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
/** @file bigdb.c
 *
 * This program was written to test how well BRL-CAD's libraries deal
 * with large geometry databases.  In particular, it provides a
 * modicum of verification that the libraries correctly seek beyond
 * 2GB/4GB limits, particularly relevant for verifying 32-bit and
 * 64-bit build behavior.
 *
 * NOTE: It currently creates a large database by writing out a
 * massive 'title', which is typically a short string.  As such, care
 * is not taken when making (4x) copies of large multi-GB title
 * strings.  That means a 4GB .g file may utilize 16GB of memory
 * briefly during construction, a 6GB file may use 24GB, etc.
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>

#include "bu.h"
#include "raytrace.h"
#include "wdb.h"
#include "ged.h"


int
main(int ac, char *av[])
{
    char filename[MAXPATHLEN] = {0};
    size_t sz = 1024L * 1024L * 1024L;
    FILE *fp;
    struct rt_wdb *outfp;
    char *title = NULL;
    vect_t center = VINIT_ZERO;
    int failures = 0;

    bu_setprogname(av[0]);

    if (ac == 1 || ac > 2) {
	bu_log("Usage: %s {gigabytes}\n\n", av[0]);
	bu_log("This program creates a .g geometry database at least {gigabytes} in\n"
	       "size (e.g., specifying '3' would result in a 3+GB .g file).\n");
	return 1;
    } else if (ac == 2) {
	double gigs = atof(av[1]);
	if (gigs > 0)
	    sz *= gigs;
    }
    sz += 2; /* nudge */

    fp = bu_temp_file(filename, MAXPATHLEN);
    outfp = wdb_fopen(filename);

    /* bu_log("using %s temp file\n", filename); */

    /* make sure we don't overflow */
    BU_ASSERT((uint64_t)sz < (uint64_t)(SIZE_MAX/2));
    BU_ASSERT(sz > strlen("123......321")+1);

    /* intentionally using malloc so we can halt the test if this
     * system will let us allocate enough memory.
     *
     * unfortunately, librt currently re-allocates a title 9 times in
     * the process of writing it to disk, keeping what appears to be 5
     * of them in memory at the same time.
     */
#define MULTIPLIER 5
    title = (char *)malloc(sz * MULTIPLIER);
    if (!title) {
	bu_log("WARNING: unable to allocate %zu MB\n", (sz * MULTIPLIER) / (1024 * 1024));
	bu_exit(123, "Aborting test.\n");
    }
    free(title);

    title = (char *)bu_malloc(sz, "title");
    memset(title, ' ', sz);
    title[0] = '1';
    title[1] = '2';
    title[2] = '3';
    title[3] = '.';
    title[4] = '.';
    title[5] = '.';
    title[sz-7] = '.';
    title[sz-6] = '.';
    title[sz-5] = '.';
    title[sz-4] = '3';
    title[sz-3] = '2';
    title[sz-2] = '1';
    title[sz-1] = '\0';

    /* bu_log("setting title to %ld == %ld chars long (%c%c%c <> %c%c%c)\n", strlen(title), sz-1, title[0], title[1], title[2], title[sz-4], title[sz-3], title[sz-2]); */

#define ORIGIN_SPHERE "origin.sph"
    mk_id(outfp, title);
    mk_sph(outfp, ORIGIN_SPHERE, center, 1.0);

    wdb_close(outfp);
    fclose(fp);
    bu_free(title, "title");

    {
	/* validate */

	struct ged *gedp = ged_open("db", filename, 1);
	const char *tops_cmd[3] = {"tops", "-n", NULL};
	const char *title_cmd[2] = {"title", NULL};
	const char *id = NULL;
	struct bu_vls vp = BU_VLS_INIT_ZERO;
	char *base = bu_path_basename(av[0], NULL);
	size_t idlen = 0;
	int match = 0;

	ged_tops(gedp, 2, tops_cmd);

	bu_vls_trimspace(gedp->ged_result_str); /* trailing newline */
	printf("%s_tops=\"%s\"\n", base, bu_vls_cstr(gedp->ged_result_str));
	if (!BU_STR_EQUIV(bu_vls_cstr(gedp->ged_result_str), ORIGIN_SPHERE))
	    failures++;

	ged_title(gedp, 1, title_cmd);

	id = bu_vls_cstr(gedp->ged_result_str);
	idlen = bu_vls_strlen(gedp->ged_result_str);

	bu_vls_sprintf(&vp, "%s_idlen=%zd", base, idlen);
	printf("%s\n", bu_vls_cstr(&vp));

	bu_vls_sprintf(&vp, "%s_szlen=%zd", base, sz-1);
	printf("%s\n", bu_vls_cstr(&vp));

	printf("%s_lenmatch=%d\n", base, sz-1 == idlen);
	if (sz-1 != idlen)
	    failures++;

	match = (id[0] == id[idlen-1] && id[1] == id[idlen-2] && id[2] == id[idlen-3]) ? 1 : 0;
	printf("%s_strmatch=%d\n", base, match);
	if (!match)
	    failures++;

	bu_vls_free(&vp);
	ged_close(gedp);
	bu_free(base, "basename");
    }

    bu_file_delete(filename);

    return failures;
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
