/*                         L C . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file libged/lc.c
 *
 * The lc command.
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "ged.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bu/file.h"
#include "bu/getopt.h"
#include "bu/sort.h"
#include "bu/ptbl.h"


struct region_record
{
    int ignore;
    const char *region_id;
    const char *material_id;
    const char *los;
    const char *obj_name;
    const char *obj_parent;
};

/**
 * Sorts the region lists such that identical entries are next to each
 * other; used to remove duplicates.
 */
static int
cmp_regions(const void *a, const void *b, void *UNUSED(arg))
{
    struct region_record *r1 = (struct region_record *)a;
    struct region_record *r2 = (struct region_record *)b;
    int cmp;

    cmp = bu_strcmp(r1->region_id, r2->region_id);
    if (cmp) { return cmp; }
    cmp = bu_strcmp(r1->material_id, r2->material_id);
    if (cmp) { return cmp; }
    cmp = bu_strcmp(r1->los, r2->los);
    return cmp;
}


/**
 * Sorts the region list according to the sort parameter (1 - 5) passed in.
 */
static int
sort_regions(const void *a, const void *b, void *arg)
{
    struct region_record *r1 = (struct region_record *)a;
    struct region_record *r2 = (struct region_record *)b;
    int *sort_type = (int *)arg;
    int temp1,temp2;

    switch (*sort_type) {
	case 1:
	    temp1=atoi(r1->region_id);
	    temp2=atoi(r2->region_id);
	    goto continue_run;
	case 2:
	    temp1=atoi(r1->material_id);
	    temp2=atoi(r2->material_id);
	    goto continue_run;
	case 3:
	    temp1=atoi(r1->los);
	    temp2=atoi(r2->los);
continue_run:
	    if ( temp1 > temp2 )
		return 1;
	    if ( temp1 == temp2 )
		return 0;
	    return -1;
	case 4:
	    return bu_strcmp(r1->obj_name, r2->obj_name);
	case 5:
	    return bu_strcmp(r1->obj_parent, r2->obj_parent);
    }
    /* This should never be executed */
    return 0;
}

static void
print_cmd_args(struct bu_vls *output, int argc, const char *argv[])
{
    int i;
    bu_vls_printf(output, "Command args: '");
    for (i = 0; i < argc; i++) {
	bu_vls_printf(output, "%s", argv[i]);
	if (i < argc - 1) {
	    bu_vls_printf(output, " ");
	} else {
	    bu_vls_printf(output, "'\n");
	}
    }
}

/**
 * Get an attribute from a bu_avs, like bu_avs_get, but return "--" if
 * the item is missing, instead of NULL
 */
static const char *
get_attr(const struct bu_attribute_value_set *avp, const char *attribute)
{
    const char *value = bu_avs_get(avp, attribute);
    return value ? value : "--";
}


int
ged_lc_core(struct ged *gedp, int argc, const char *argv[])
{
    char *file_name = NULL;
    int file_name_flag_cnt = 0;
    int sort_column = 1;
    int find_mismatched = 0;
    int sort_column_flag_cnt = 0;
    int find_duplicates_flag = 0;
    int skip_special_duplicates_flag = 0;
    int skip_subtracted_regions_flag = 0;
    int descending_sort_flag = 0;
    int unrecognized_flag_cnt = 0;

    int orig_argc;
    const char **orig_argv;

    static const char *usage = "[-d|-s] [-r] [-z] [-0|-1|-2|-3|-4|-5] [-f {FileName}] {GroupName}";

    int c;
    int error_cnt = 0;

    FILE *outfile = NULL;
    struct bu_vls *output;

    const char *group_name;

    size_t i,j;
    struct bu_ptbl results1 = BU_PTBL_INIT_ZERO;
    struct bu_ptbl results2 = BU_PTBL_INIT_ZERO;
    char *path;
    char *plan;
    struct db_full_path root;
    int matches;
    struct region_record *regions;
    size_t ignored_cnt = 0;

    /* The defaults are the minimum widths to accommodate column headers */
    size_t region_id_len_max = 2;
    size_t material_id_len_max = 3;
    size_t los_len_max = 3;
    size_t obj_len_max = 6;

    /* For the output at the end */
    size_t start, end, incr;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage);
	return GED_HELP;
    }

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    bu_optind = 1; /* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "dmsrz012345f:")) != -1) {
	switch (c) {
	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
		sort_column_flag_cnt++;
		sort_column = c - '0';
		break;
	    case 'f':
		file_name_flag_cnt++;
		file_name = bu_optarg;
		break;
	    case 'm':
		find_mismatched = 1;
		break;
	    case 's':
		skip_special_duplicates_flag = 1;
		/* FALLTHROUGH */
	    case 'd':
		find_duplicates_flag = 1;
		break;
	    case 'r':
		skip_subtracted_regions_flag = 1;
		break;
	    case 'z':
		descending_sort_flag = 1;
		break;
	    default:
		unrecognized_flag_cnt++;
	}
    }
    orig_argc = argc;
    orig_argv = argv;
    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    /* Attempt to recreate the exact error messages from the original lc.tcl */

    if (file_name_flag_cnt > 1) {
	bu_vls_printf(gedp->ged_result_str, "Error: '-f' used more than once.\n");
	error_cnt++;
    }

    if (sort_column_flag_cnt > 1) {
	bu_vls_printf(gedp->ged_result_str, "Error: Sort column defined more than once.\n");
	error_cnt++;
    }

    if (file_name_flag_cnt + argc + unrecognized_flag_cnt > 3) {
	bu_vls_printf(gedp->ged_result_str, "Error: More than one group name and/or file name was specified.\n");
	error_cnt++;
    } else if (argc < 2) {
	if (file_name_flag_cnt && !file_name) {
	    bu_vls_printf(gedp->ged_result_str, "Error: Group name and file name not specified\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Error: Group name not specified.\n");
	}
	error_cnt++;
    } else if (argc + unrecognized_flag_cnt > 2) {
	bu_vls_printf(gedp->ged_result_str, "Error: More than one group name was specified.\n");
	error_cnt++;
    } else if (file_name_flag_cnt && !file_name) {
	bu_vls_printf(gedp->ged_result_str, "Error: File name not specified.\n");
	error_cnt++;
    }

    if (file_name) {
	char *norm_name;
	norm_name = bu_file_realpath(file_name, NULL);
	if (file_name[0] == '-') {
	    bu_vls_printf(gedp->ged_result_str, "Error: File name can not start with '-'.\n");
	    error_cnt++;
	} else if (bu_file_exists(file_name, NULL)) {
	    bu_vls_printf(gedp->ged_result_str, "Error: Output file %s already exists.\n",norm_name);
	    error_cnt++;
	} else {
	    outfile = fopen(file_name, "w");
	    if (!outfile) {
		bu_vls_printf(gedp->ged_result_str, "Error: %d\n", errno);
		error_cnt++;
	    }
	}
	bu_vls_printf(gedp->ged_result_str, "Output filename: %s\n", norm_name);
	bu_free(norm_name, "ged_lc_core");
	output = bu_vls_vlsinit();
    } else {
	output = gedp->ged_result_str;
    }

    if (error_cnt > 0) { return GED_ERROR; }

    /* Update references once before we start all of this - db_search
     * needs nref to be current to work correctly. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    group_name = argv[1];

    /* The 7 is for the "-name" and '\0' */
    plan = (char *) bu_malloc(sizeof(char) * (strlen(group_name) + 7), "ged_lc_core");
    sprintf(plan, "-name %s", group_name);
    matches = db_search(&results1, DB_SEARCH_TREE, plan, 0, NULL, gedp->dbip, NULL);
    if (matches < 1) {
	bu_vls_printf(gedp->ged_result_str, "Error: Group '%s' does not exist.\n", group_name);
	return GED_ERROR;
    }
    bu_free(plan, "ged_lc_core");
    db_search_free(&results1);

    if (skip_subtracted_regions_flag) {
	plan = "-type region ! -bool -";
    } else {
	plan = "-type region";
    }
    path = (char *) bu_malloc(sizeof(char) * (strlen(group_name) + 2), "ged_lc_core");
    sprintf(path, "/%s", group_name);
    db_string_to_path(&root, gedp->dbip, path);
    matches = db_search(&results2, DB_SEARCH_TREE, plan, root.fp_len, root.fp_names, gedp->dbip, NULL);
    bu_free(path, "ged_lc_core");
    if (matches < 1) { return GED_ERROR; }
    regions = (struct region_record *) bu_malloc(sizeof(struct region_record) * BU_PTBL_LEN(&results2), "ged_lc_core");
    for (i = 0; i < BU_PTBL_LEN(&results2); i++) {
	struct db_full_path *entry = (struct db_full_path *)BU_PTBL_GET(&results2, i);
	struct directory *dp_curr_dir = DB_FULL_PATH_CUR_DIR(entry);
	struct bu_attribute_value_set avs;

	j = BU_PTBL_LEN(&results2) - i - 1;
	regions[j].ignore = 0;

	bu_avs_init_empty(&avs);
	db5_get_attributes(gedp->dbip, &avs, dp_curr_dir);

	regions[j].region_id = get_attr(&avs, "region_id");
	V_MAX(region_id_len_max, strlen(regions[j].region_id));
	regions[j].material_id = get_attr(&avs, "material_id");
	V_MAX(material_id_len_max, strlen(regions[j].material_id));
	regions[j].los = get_attr(&avs, "los");
	V_MAX(los_len_max, strlen(regions[j].los));
	regions[j].obj_name = dp_curr_dir->d_namep;
	V_MAX(obj_len_max, strlen(regions[j].obj_name));

	if (entry->fp_len > 1) {
	    struct directory *dp_parent = DB_FULL_PATH_GET(entry, entry->fp_len - 2);
	    regions[j].obj_parent = dp_parent->d_namep;
	} else {
	    regions[j].obj_parent = "--";
	}
    }

    if (find_mismatched) {
	int im = 0;
	bu_sort((void *) regions, BU_PTBL_LEN(&results2), sizeof(struct region_record), cmp_regions, NULL);

	while (im < (int)BU_PTBL_LEN(&results2)) {
	    int found_all_matches = 0;
	    int jm = im + 1;
	    int mismatch = 0;
	    while (!found_all_matches) {
		if (jm == (int)BU_PTBL_LEN(&results2) || bu_strcmp(regions[im].region_id, regions[jm].region_id)) {
		    /* Found all matches - set ignore flags */
		    int km = 0;
		    int ignored = (mismatch) ? 0 : 1;
		    found_all_matches = 1;
		    for (km = im; km < jm; km++) {
			regions[km].ignore = ignored;
			if (ignored) ignored_cnt++;
		    }
		    im = jm;
		    continue;
		}
		if (bu_strcmp(regions[im].material_id, regions[jm].material_id)) mismatch++;
		if (bu_strcmp(regions[im].los, regions[jm].los)) mismatch++;
		jm++;
	    }
	}
	goto print_results;
    }

    if (find_duplicates_flag) {
	int im = 0;
	ignored_cnt = 0;
	bu_sort((void *) regions, BU_PTBL_LEN(&results2), sizeof(struct region_record), cmp_regions, NULL);

	for(im = 0; im < (int)BU_PTBL_LEN(&results2); im++) {
	    regions[im].ignore = 1;
	}

	im = 0;
	while (im < (int)BU_PTBL_LEN(&results2)) {
	    int jm = im + 1;
	    while (jm < (int)BU_PTBL_LEN(&results2)) {
		if (BU_STR_EQUAL(regions[im].region_id, regions[jm].region_id)) {
		    if (skip_special_duplicates_flag) {
			int ssd = 0;
			ssd += BU_STR_EQUAL(regions[im].obj_parent, regions[jm].obj_parent);
			ssd += BU_STR_EQUAL(regions[im].material_id, regions[jm].material_id);
			ssd += BU_STR_EQUAL(regions[im].los, regions[jm].los);
			if (ssd != 3) {
			    /* Found match, skip criteria not satisfied - set ignore flags */
			    regions[im].ignore = 0;
			    regions[jm].ignore = 0;
			}
		    } else {
			/* Found match - set ignore flags */
			regions[im].ignore = 0;
			regions[jm].ignore = 0;
		    }
		}
		jm++;
	    }
	    im++;
	}

	for(im = 0; im < (int)BU_PTBL_LEN(&results2); im++) {
	    if (regions[im].ignore) ignored_cnt++;
	}

	if (ignored_cnt == BU_PTBL_LEN(&results2)) {
	    if (file_name) {
		print_cmd_args(output, orig_argc, orig_argv);
		bu_vls_printf(output, "No duplicate region_id\n");
		bu_vls_fwrite(outfile, output);
		fclose(outfile);
	    }
	    bu_vls_printf(gedp->ged_result_str, "No duplicate region_id\n");
	    bu_vls_printf(gedp->ged_result_str, "Done.");
	    bu_free(regions, "ged_lc_core");
	    return GED_ERROR;
	} else {
	    goto print_results;
	}
    }

print_results:
    if (sort_column > 0) {
	bu_sort((void *) regions, BU_PTBL_LEN(&results2), sizeof(struct region_record), sort_regions, (void *)&sort_column);
    }

    if (file_name) {
	print_cmd_args(output, orig_argc, orig_argv);
    }

    bu_vls_printf(output, "List length: %lu\n", BU_PTBL_LEN(&results2) - ignored_cnt);
    bu_vls_printf(output, "%-*s %-*s %-*s %-*s %s\n",
		  (int)region_id_len_max + 1, "ID",
		  (int)material_id_len_max + 1, "MAT",
		  (int)los_len_max, "LOS",
		  (int)obj_len_max,  "REGION",
		  "PARENT");
    end = BU_PTBL_LEN(&results2);
    if (descending_sort_flag) {
	start = end - 1; end = -1; incr = -1;
    } else {
	start = 0; incr = 1;
    }
    for (i = start; i != end; i += incr) {
	if (regions[i].ignore) { continue; }
	bu_vls_printf(output, "%-*s %-*s %-*s %-*s %s\n",
		      (int)region_id_len_max + 1, regions[i].region_id,
		      (int)material_id_len_max + 1, regions[i].material_id,
		      (int)los_len_max, regions[i].los,
		      (int)obj_len_max, regions[i].obj_name,
		      regions[i].obj_parent);
    }
    bu_vls_printf(gedp->ged_result_str, "Done.");

    if (file_name) {
	bu_vls_fwrite(outfile, output);
	fclose(outfile);
    }

    bu_free(regions, "ged_lc_core");

    return GED_OK;
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl lc_cmd_impl = {
    "lc",
    ged_lc_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd lc_cmd = { &lc_cmd_impl };
const struct ged_cmd *lc_cmds[] = { &lc_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  lc_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
