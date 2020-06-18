/*                         G - D O T . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2020 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/*
 * Exports BRL-CAD geometry to the DOT language format.
 *
 * The resulting .dot files can be rendered to an image using a
 * variety of applications such as:
 *
 * -- Graphviz, http://www.graphviz.org
 * -- Tulip, http://tulip.labri.fr/
 * -- OmniGraffle (Mac), http://www.omnigroup.com/products/omnigraffle/
 *
 */

#include "common.h"

/* system headers */
#include <string.h>

/* interface headers */
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/path.h"
#include "bu/str.h"
#include "brlcad_version.h"
#include "raytrace.h"
#include "ged.h"


const char *usage = "[-o output.dot] input.g [object1 ...]\n";

struct output {
    FILE *outfp;
    struct bu_ptbl groups;
    struct bu_ptbl regions;
    struct bu_ptbl combinations;
    struct bu_ptbl primitives;
};


static long
gdot_hash(uint8_t *key, size_t len)
{
    long hash = 5381;
    size_t i;

    if (!key) return hash;

    for (i = 0; i < len; i++) {
	hash = ((hash << 5) + hash) + key[i]; /* hash * 33 + c */
    }

    return hash;
}


static void
dot_comb(struct db_i *dbip, struct directory *dp, void *out)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    struct output *o = (struct output *)out;

    if (!o->outfp)
	return;

    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_log("ERROR: Database read error, skipping %s\n", dp->d_namep);
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb->region_flag) {
	long hash = gdot_hash((uint8_t *)dp->d_namep, strlen(dp->d_namep));
	if (bu_ptbl_ins_unique(&(o->regions), (long *)hash) == -1) {
	    fprintf(o->outfp, "\t\"%s\" [ color=blue shape=box3d ];\n", dp->d_namep);
	}
    } else {
	long hash = gdot_hash((uint8_t *)dp->d_namep, strlen(dp->d_namep));
	if (bu_ptbl_ins_unique(&(o->groups), (long *)hash) == -1) {
	    fprintf(o->outfp, "\t\"%s\" [ color=green ];\n", dp->d_namep);
	}
    }

    /* FIXME: this is yet-another copy of the commonly-used code that
     * gets a list of comb members.  needs to return tabular data.
     */
    if (comb->tree) {
	size_t i;
	size_t node_count = 0;
	size_t actual_count = 0;
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	struct rt_tree_array *rt_tree_array = NULL;

	if (db_ck_v4gift_tree(comb->tree) < 0) {
	    db_non_union_push(comb->tree, &rt_uniresource);
	    if (db_ck_v4gift_tree(comb->tree) < 0) {
		bu_log("INTERNAL_ERROR: Cannot flatten tree of [%s] for listing", dp->d_namep);
		return;
	    }
	}

	node_count = db_tree_nleaves(comb->tree);
	if (node_count > 0) {
	    rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");
	    actual_count = (struct rt_tree_array *)db_flatten_tree(rt_tree_array, comb->tree, OP_UNION, 1, &rt_uniresource) - rt_tree_array;
	    BU_ASSERT(actual_count == node_count);
	    comb->tree = TREE_NULL;
	} else {
	    actual_count = 0;
	    rt_tree_array = NULL;
	}

	for (i = 0; i < actual_count; i++) {
	    char op;

	    switch (rt_tree_array[i].tl_op) {
		case OP_UNION:
		    op = DB_OP_UNION;
		    break;
		case OP_INTERSECT:
		    op = DB_OP_INTERSECT;
		    break;
		case OP_SUBTRACT:
		    op = DB_OP_SUBTRACT;
		    break;
		default:
		    op = '?';
		    break;
	    }

	    fprintf(o->outfp, "\t\"%s\" -> \"%s\" [ label=\"%c\" ];\n", dp->d_namep, rt_tree_array[i].tl_tree->tr_l.tl_name, op);
	    db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
	}
	bu_vls_free(&vls);

	if (rt_tree_array)
	    bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
    }

    rt_db_free_internal(&intern);
}


static void
dot_leaf(struct db_i *UNUSED(dbip), struct directory *dp, void *out)
{
    struct output *o = (struct output *)out;
    unsigned long hash;

    if (!o->outfp)
	return;

    hash = gdot_hash((uint8_t *)dp->d_namep, strlen(dp->d_namep));
    if (bu_ptbl_ins_unique(&(o->primitives), (long *)hash) == -1) {
	fprintf(o->outfp, "\t\"%s\" [ color=red shape=box rank=min ];\n", dp->d_namep);
    }

    /* TODO: add { rank=same; prim1 prim2 ... } */
}


static void
dot_header(FILE *outfp, const char *label)
{
    struct bu_vls vp = BU_VLS_INIT_ZERO;

    if (!outfp)
	bu_exit(1, "INTERNAL ERROR: unable to write out .dot header\n");

    fprintf(outfp, "\n"
	    "/*\n"
	    " * BRL-CAD version %s DOT export\n"
	    " */\n", brlcad_version());

    fprintf(outfp, "\ndigraph \"BRL-CAD\" {\n");

    fprintf(outfp, "\tlabel=%s\n", bu_vls_encode(&vp, label));
    fprintf(outfp, "\tgraph [ rankdir=LR ];\n");
    fprintf(outfp, "\tnode [ style=filled ];\n");
    fprintf(outfp, "\tnode [ shape=box ];\n"); /* try Mrecord */
}


static void
dot_footer(FILE *outfp)
{
    if (!outfp)
	bu_exit(1, "INTERNAL ERROR: unable to write out .dot footer\n");

    fprintf(outfp, "}\n\n");
}


static void
help(const char *progname)
{
    bu_log("Usage: %s %s", progname, usage);
    bu_log("\n\t-o output.dot   (optional) name of output Graphviz .dot file");
    bu_log("\n\tinput.g         name of input BRL-CAD .g database");
    bu_log("\n\tobject1 ...     (optional) name of object(s) to export from .g file\n");
}


int
main(int ac, char *av[])
{
    int c;

    const char *argv0 = av[0];
    const int argc0 = ac;

    char *output = NULL;
    char *input = NULL;
    FILE *out = stdout;
    FILE *in = stdin;

    char **objs = NULL;
    struct ged *gp = NULL;

    /* tracks which objects are already output */
    struct output o = {NULL, BU_PTBL_INIT_ZERO, BU_PTBL_INIT_ZERO, BU_PTBL_INIT_ZERO, BU_PTBL_INIT_ZERO};

    bu_setprogname(av[0]);

    while ((c = bu_getopt(ac, av, "o:h?")) != -1) {
	switch (c) {
	    case 'o':
		output = bu_strdup(bu_optarg);
		break;
	    default:
		help(argv0);
		bu_exit(0, NULL);
		break;
	}
    }
    ac -= bu_optind;
    av += bu_optind;

    /* there should at least be a db filename remaining */
    if (ac < 1) {
	help(argv0);
	if (argc0 > 1) {
	    bu_exit(2, "ERROR: input geometry database not specified\n");
	} else {
	    bu_exit(1, NULL);
	}
    } else {
	input = bu_strdup(av[0]);
    }

    /* verify input */
    if (input && !(input[0] == '-' && input[1] == '\0')) {
	if (bu_file_exists(input, NULL)) {
	    bu_log("Reading input from [%s]\n", input);
	    in = fopen(input, "r");
	    if (!in) {
		perror(argv0);
		bu_exit(3, "ERROR: input file [%s] could not be opened for reading\n", input);
	    }
	} else {
	    bu_exit(4, "ERROR: input file [%s] does not exist\n", input);
	}
    } else {
#define MAX_BUFFER 1024
	char buffer[MAX_BUFFER] = {0};
	char filename[MAXPATHLEN] = {0};
	FILE *temp = NULL;
	size_t ret = 0;
	size_t n = 0;

	bu_log("Reading from standard input\n");

	/* save stdin to temp file since librt needs to seek */

	temp = bu_temp_file(filename, MAXPATHLEN);
	input = bu_strdup(filename);

	while (feof(stdin) == 0) {
	    n = fread(buffer, 1, MAX_BUFFER, stdin);
	    if (n > 0) {
		ret = fwrite(buffer, 1, n, temp);
		if (ret != n) {
		    bu_exit(5, "ERROR: problem encountered reading from standard input\n");
		}
	    }
	}
	fflush(temp);
	fclose(temp);
    }

    /* verify output */
    if (output && !(output[0] == '-' && output[1] == '\0')) {
	if (bu_file_exists(output, NULL)) {
	    bu_log("WARNING: %s already exists\n", output);
	    bu_log("Appending output to [%s]\n", output);
	} else {
	    bu_log("Writing output to [%s]\n", output);
	}
	out = fopen(output, "a");
	if (!out) {
	    perror(argv0);
	    bu_exit(2, "ERROR: Unable to open output file [%s] for writing\n", output);
	}
    } else {
	bu_log("Writing to standard output\n");
    }

    gp = ged_open("db", input, 1);
    if (!gp) {
	bu_exit(8, "ERROR: Unable to open [%s] for reading objects\n", input);
    }

    /* write out header */
    {
	struct bu_vls vp = BU_VLS_INIT_ZERO;
	const char *title[2] = {"title", NULL};

	ged_title(gp, 1, title);
	bu_vls_printf(&vp, "%s\\n", bu_vls_addr(gp->ged_result_str));
	if (!(av[0][0] == '-' && av[0][1] == '\0')) {
	    char base[MAXPATHLEN] = {0};
	    bu_path_basename(av[0], base);
	    bu_vls_printf(&vp, "%s ", base);
	}
	bu_vls_printf(&vp, "BRL-CAD Geometry Database");
	dot_header(out, bu_vls_addr(&vp));

	bu_vls_free(&vp);
    }

    /* anything else is assumed to be an object, get a list */
    if (ac > 1) {

	/* specified objects */

	objs = bu_argv_dup(ac - 1, (const char **)(av + 1));
    } else {
	char **topobjs;
	const char *tops[3] = {"tops", "-n", NULL};

	/* all top-level objects */

	ged_tops(gp, 2, tops);

	topobjs = (char **)bu_calloc(1, bu_vls_strlen(gp->ged_result_str), "alloc topobjs");
	c = (int)bu_argv_from_string(topobjs, bu_vls_strlen(gp->ged_result_str), bu_vls_addr(gp->ged_result_str));
	objs = bu_argv_dup(c, (const char **)topobjs);
	bu_free(topobjs, "free topobjs");
    }

    /* write out each object */
    c = 0;
    o.outfp = out;
    while (objs[c]) {
	struct directory *dp = NULL;

	dp = db_lookup(gp->ged_wdbp->dbip, objs[c], 1);
	if (dp) {
	    bu_log("Exporting object [%s]\n", objs[c]);
	    db_functree(gp->ged_wdbp->dbip, dp, dot_comb, dot_leaf, NULL, &o);
	} else {
	    bu_log("ERROR: Unable to locate [%s] within input database, skipping.\n", objs[c]);
	}

	c++;
    }

    /* write out footer */
    dot_footer(out);

    /* clean up */

    bu_ptbl_free(&o.primitives);
    bu_ptbl_free(&o.combinations);
    bu_ptbl_free(&o.regions);
    bu_ptbl_free(&o.groups);

    ged_close(gp);
    if (gp)
	BU_PUT(gp, struct ged);

    bu_argv_free(c, objs);

    if (input)
	bu_free(input, "free input");

    if (output)
	bu_free(output, "free output");

    if (in != stdin)
	fclose(in);

    if (out != stdout)
	fclose(stdout);

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
