/*                      S C A D _ R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file scad_read.c
 *
 * libgcv import filter for OpenSCAD (.scad) files.
 *
 * OpenSCAD is a declarative Constructive Solid Geometry language.  This
 * filter parses and evaluates the .scad program (see scad_parse.c /
 * scad_eval.c) and writes the resulting CSG model into a BRL-CAD .g
 * database using native primitives and boolean combinations (see
 * scad_geom.c).  Documented fidelity losses are catalogued in TODO.scad.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bio.h"

#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "gcv/api.h"
#include "raytrace.h"
#include "wdb.h"

#include "scad.h"


/* ------------------------------------------------------------------ */
/* options                                                            */
/* ------------------------------------------------------------------ */

#define SCAD_OPT_COUNT 6		/* number of BU_OPT entries below */

static void
scad_read_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    struct scad_read_options *o;

    BU_ALLOC(o, struct scad_read_options);
    o->units = NULL;		/* NULL -> "mm" at use */
    o->fn = 0;
    o->fa = 0.0;		/* 0 -> honor file / OpenSCAD default (12) */
    o->fs = 0.0;		/* 0 -> honor file / OpenSCAD default (2)  */
    o->facetize = 0;
    o->facet_max = 12;
    *dest_options_data = o;

    *options_desc = (struct bu_opt_desc *)bu_calloc(SCAD_OPT_COUNT + 1,
						     sizeof(struct bu_opt_desc), "scad opts");

    BU_OPT((*options_desc)[0], NULL, "units", "unit",
	   bu_opt_str, &o->units, "units of the input model (default mm)");
    BU_OPT((*options_desc)[1], NULL, "fn", "n",
	   bu_opt_int, &o->fn, "fallback $fn when the file leaves it unset (0=auto)");
    BU_OPT((*options_desc)[2], NULL, "fa", "deg",
	   bu_opt_fastf_t, &o->fa, "fallback $fa minimum fragment angle (default 12)");
    BU_OPT((*options_desc)[3], NULL, "fs", "len",
	   bu_opt_fastf_t, &o->fs, "fallback $fs minimum fragment size (default 2)");
    BU_OPT((*options_desc)[4], NULL, "facetize", "",
	   bu_opt_bool, &o->facetize, "emit curved primitives as faceted BoT meshes");
    BU_OPT((*options_desc)[5], NULL, "facet-max", "n",
	   bu_opt_int, &o->facet_max,
	   "largest $fn kept as a faceted n-gon prism (default 12; above this, cylinders become smooth tgc)");
    BU_OPT_NULL((*options_desc)[6]);
}

static void
scad_read_free_opts(void *options_data)
{
    /* units is a non-owning pointer into caller argv (bu_opt_str) */
    bu_free(options_data, "scad opts");
}


/* ------------------------------------------------------------------ */
/* recognition                                                        */
/* ------------------------------------------------------------------ */

static int
scad_can_read(const char *data)
{
    FILE *fp;
    char buf[4096];
    size_t nread;
    static const char *sig[] = {
	"module ", "function ", "cube(", "cube (", "sphere(", "sphere (",
	"cylinder(", "cylinder (", "polyhedron(", "linear_extrude",
	"rotate_extrude", "difference(", "difference (", "union(",
	"intersection(", "translate(", "translate (", "$fn", NULL
    };
    int i;

    if (!data) return 0;
    fp = fopen(data, "rb");
    if (!fp) return 0;
    nread = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    if (nread == 0) return 0;
    buf[nread] = '\0';

    for (i = 0; sig[i]; i++)
	if (strstr(buf, sig[i]))
	    return 1;
    return 0;
}


/* ------------------------------------------------------------------ */
/* read entry                                                         */
/* ------------------------------------------------------------------ */

static int
scad_read(struct gcv_context *context, const struct gcv_opts *gcv_options,
	  const void *options_data, const char *source_path)
{
    struct scad_state st;
    struct scad_read_options *opts = (struct scad_read_options *)options_data;
    struct bu_vls dir = BU_VLS_INIT_ZERO;
    struct bu_vls title = BU_VLS_INIT_ZERO;
    char *text = NULL;
    struct scad_stmt **prog = NULL;
    size_t nprog = 0;
    struct scad_geom *emit_root;
    int ok = 0;

    memset(&st, 0, sizeof(st));
    st.gcv_options = gcv_options;
    st.opts = opts;
    st.input_file = source_path;
    bu_vls_init(&st.msg);

    st.wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    if (!st.wdbp) {
	bu_log("scad: cannot open output database\n");
	goto done;
    }

    text = scad_read_file(source_path);
    if (!text) {
	bu_log("scad: cannot read input file (%s)\n", source_path);
	goto done;
    }

    if (bu_path_component(&dir, source_path, BU_PATH_DIRNAME))
	st.base_dir = bu_strdup(bu_vls_cstr(&dir));

    bu_path_component(&title, source_path, BU_PATH_BASENAME);

    mk_id_units(st.wdbp, bu_vls_cstr(&title),
		(opts && opts->units) ? opts->units : "mm");

    prog = scad_parse(&st, text, source_path, &nprog);
    if (!prog) {
	bu_log("scad: parsing failed for %s\n", source_path);
	goto done;
    }

    if (scad_run(&st, prog, nprog) < 0)
	bu_log("scad: evaluation aborted; partial geometry may be produced\n");

    emit_root = st.root_only ? st.root_only : st.root;
    ok = scad_emit(&st, emit_root);

    if (ok) {
	struct directory *dp = db_lookup(context->dbip, "all", LOOKUP_QUIET);
	if (dp != RT_DIR_NULL) {
	    db5_update_attribute("all", "importer", "gcv-scad", context->dbip);
	    db5_update_attribute("all", "source_format", "openscad", context->dbip);
	}
    }

    if (st.dropped)
	bu_log("scad: %d construct(s) were unsupported and were skipped or approximated.\n",
	       st.dropped);
    if (!ok)
	bu_log("scad: warning, no geometry was created from %s\n", source_path);

done:
    if (st.root) scad_geom_free(st.root);
    if (prog) scad_stmts_free(prog, nprog);
    if (text) bu_free(text, "scad slurp");
    if (st.base_dir) bu_free(st.base_dir, "scad base dir");
    bu_vls_free(&st.msg);
    bu_vls_free(&dir);
    bu_vls_free(&title);
    return ok ? 1 : 0;
}


/* ------------------------------------------------------------------ */
/* plugin registration                                                */
/* ------------------------------------------------------------------ */

static const struct gcv_filter gcv_conv_scad_read = {
    "OpenSCAD Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_OPENSCAD, scad_can_read,
    scad_read_create_opts, scad_read_free_opts, scad_read
};

static const struct gcv_filter gcv_conv_scad_read_auto = {
    "OpenSCAD Reader (auto)", GCV_FILTER_READ, BU_MIME_MODEL_AUTO, scad_can_read,
    scad_read_create_opts, scad_read_free_opts, scad_read
};

static const struct gcv_filter * const filters[] = {
    &gcv_conv_scad_read, &gcv_conv_scad_read_auto, NULL
};

const struct gcv_plugin gcv_plugin_info_s = { filters };

COMPILER_DLLEXPORT const struct gcv_plugin *
gcv_plugin_info(void){ return &gcv_plugin_info_s; }

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
