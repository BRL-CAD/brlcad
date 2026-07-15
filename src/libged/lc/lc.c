/*                         L C . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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
#include "bu/cmdschema.h"
#include "bu/sort.h"
#include "bu/ptbl.h"


struct region_record
{
    int ignore;
    const char *region_id;
    const char *material_id;
    const char *los;
    const char *aircode;
    const char *obj_name;
    const char *obj_parent;
};


struct lc_file_option {
    const char *path;
    int count;
};

struct lc_args {
    int duplicates;
    int mismatched;
    int unique_codes;
    int skip_subtracted;
    int descending;
    int sort0;
    int sort1;
    int sort2;
    int sort3;
    int sort4;
    int sort5;
    struct lc_file_option output_file;
};


static int
lc_file_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    if (!arg || !arg[0]) {
	if (msg)
	    bu_vls_printf(msg, "output file name is required");
	return -1;
    }
    if (storage) {
	struct lc_file_option *file = (struct lc_file_option *)storage;
	file->path = arg;
	file->count++;
    }
    return 0;
}


static const struct bu_cmd_option lc_schema_options[] = {
    BU_CMD_COUNTING_FLAG("d", NULL, struct lc_args, duplicates,
	"List duplicate region codes"),
    BU_CMD_COUNTING_FLAG("m", NULL, struct lc_args, mismatched,
	"List duplicate region IDs with mismatched codes"),
    BU_CMD_COUNTING_FLAG("s", NULL, struct lc_args, unique_codes,
	"List duplicate region IDs with distinct code records"),
    BU_CMD_COUNTING_FLAG("r", NULL, struct lc_args, skip_subtracted,
	"Skip regions subtracted from a parent region"),
    BU_CMD_COUNTING_FLAG("z", NULL, struct lc_args, descending,
	"Sort descending"),
    BU_CMD_COUNTING_FLAG("0", NULL, struct lc_args, sort0,
	"Keep stored order"),
    BU_CMD_COUNTING_FLAG("1", NULL, struct lc_args, sort1,
	"Sort by region ID"),
    BU_CMD_COUNTING_FLAG("2", NULL, struct lc_args, sort2,
	"Sort by material ID"),
    BU_CMD_COUNTING_FLAG("3", NULL, struct lc_args, sort3,
	"Sort by LOS"),
    BU_CMD_COUNTING_FLAG("4", NULL, struct lc_args, sort4,
	"Sort by air code"),
    BU_CMD_COUNTING_FLAG("5", NULL, struct lc_args, sort5,
	"Sort by region name"),
    BU_CMD_CUSTOM("f", NULL, struct lc_args, output_file, lc_file_parse,
	"file", "Write output to a new file"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand lc_schema_operands[] = {
    BU_CMD_OPERAND("group", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Group or combination to list", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static int lc_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result);
static const struct bu_cmd_schema lc_cmd_schema = {
    "lc", "List region codes in a group", lc_schema_options, lc_schema_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(lc_schema_validate, NULL)
};


static int
lc_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    struct lc_args args = {0};
    int sort_count;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    /* A generic incomplete result can legitimately be caused by a missing
     * option argument.  Keep that result intact rather than turning it into
     * a validator failure just because there is not enough argv to populate
     * the temporary record. */
    if (bu_cmd_schema_parse(&flat, &args, NULL, (int)argc, argv) < 0)
	return 0;

    sort_count = args.sort0 + args.sort1 + args.sort2 + args.sort3 +
	args.sort4 + args.sort5;
    if (args.output_file.count > 1 || sort_count > 1) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_INVALID;
	result->token_start = cursor_arg < argc ? cursor_arg : argc;
	result->token_end = result->token_start;
	result->expected = BU_CMD_EXPECT_NONE;
	result->completion_type = BU_CMD_VALUE_FLAG;
	result->hint = args.output_file.count > 1 ?
	    "-f may be specified only once" : "only one sort-column option may be specified";
    }
    return 0;
}


static void
lc_usage(struct ged *gedp)
{
    char *option_help = bu_cmd_schema_describe(&lc_cmd_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: lc [-d|-m|-s] [-r] [-z] [-0|-1|-2|-3|-4|-5] [-f file] group");
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", option_help);
	bu_free(option_help, "lc native option help");
    }
}

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
    if (cmp)
	return cmp;
    cmp = bu_strcmp(r1->material_id, r2->material_id);
    if (cmp)
	return cmp;
    cmp = bu_strcmp(r1->los, r2->los);
    if (cmp)
	return cmp;
    cmp = bu_strcmp(r1->aircode, r2->aircode);
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
	    goto continue_run;
	case 4:
	    temp1=atoi(r1->aircode);
	    temp2=atoi(r2->aircode);
continue_run:
	    if ( temp1 > temp2 )
		return 1;
	    if ( temp1 == temp2 )
		return 0;
	    return -1;
	case 5:
	    return bu_strcmp(r1->obj_name, r2->obj_name);
	case 6:
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
    struct lc_args args = {0};
    int operand_index;
    int operand_count;
    const char **operands = NULL;
    const char *file_name = NULL;
    int file_name_flag_cnt = 0;
    int sort_column = 1;
    int find_mismatched = 0;
    int sort_column_flag_cnt = 0;
    int find_duplicates_flag = 0;
    int skip_special_duplicates_flag = 0;
    int skip_subtracted_regions_flag = 0;
    int descending_sort_flag = 0;

    int orig_argc;
    const char **orig_argv;

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
    size_t aircode_len_max = 3;
    size_t obj_len_max = 6;

    /* For the output at the end */
    size_t start, end, incr;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	lc_usage(gedp);
	return GED_HELP;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    operand_index = bu_cmd_schema_parse(&lc_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	lc_usage(gedp);
	return BRLCAD_ERROR;
    }
    orig_argc = argc;
    orig_argv = argv;
    operand_count = argc - 1 - operand_index;
    operands = argv + 1 + operand_index;

    file_name = args.output_file.path;
    file_name_flag_cnt = args.output_file.count;
    sort_column_flag_cnt = args.sort0 + args.sort1 + args.sort2 + args.sort3 +
	args.sort4 + args.sort5;
    if (args.sort0) sort_column = 0;
    if (args.sort1) sort_column = 1;
    if (args.sort2) sort_column = 2;
    if (args.sort3) sort_column = 3;
    if (args.sort4) sort_column = 4;
    if (args.sort5) sort_column = 5;
    find_mismatched = args.mismatched > 0;
    skip_special_duplicates_flag = args.unique_codes > 0;
    find_duplicates_flag = args.duplicates > 0 || skip_special_duplicates_flag;
    skip_subtracted_regions_flag = args.skip_subtracted > 0;
    descending_sort_flag = args.descending > 0;

    /* Attempt to recreate the exact error messages from the original lc.tcl */

    if (file_name_flag_cnt > 1) {
	bu_vls_printf(gedp->ged_result_str, "Error: '-f' used more than once.\n");
	error_cnt++;
    }

    if (sort_column_flag_cnt > 1) {
	bu_vls_printf(gedp->ged_result_str, "Error: Sort column defined more than once.\n");
	error_cnt++;
    }


    if (operand_count > 1) {
	bu_vls_printf(gedp->ged_result_str, "Error: More than one group name and/or file name was specified.\n");
	error_cnt++;
    } else if (operand_count < 1) {
	if (file_name_flag_cnt && !file_name) {
	    bu_vls_printf(gedp->ged_result_str, "Error: Group name and file name not specified\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Error: Group name not specified.\n");
	}
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

    if (error_cnt > 0) {
	if (outfile)
	    fclose(outfile);
	return BRLCAD_ERROR;
    }

    /* Update references once before we start all of this - db_search
     * needs nref to be current to work correctly. */
    db_update_nref(gedp->dbip);

    group_name = operands[0];

    /* The 7 is for the "-name" and '\0' */
    plan = (char *) bu_malloc(sizeof(char) * (strlen(group_name) + 7), "ged_lc_core");
    sprintf(plan, "-name %s", group_name);
    matches = db_search(&results1, DB_SEARCH_TREE, plan, 0, NULL, gedp->dbip, NULL, NULL, NULL);
    if (matches < 1) {
	bu_vls_printf(gedp->ged_result_str, "Error: Group '%s' does not exist.\n", group_name);
	return BRLCAD_ERROR;
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
    matches = db_search(&results2, DB_SEARCH_TREE, plan, (int)root.fp_len, root.fp_names, gedp->dbip, NULL, NULL, NULL);
    bu_free(path, "ged_lc_core");
    if (matches < 1) { return BRLCAD_ERROR; }
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
	regions[j].aircode = get_attr(&avs, "aircode");
	V_MAX(aircode_len_max, strlen(regions[j].aircode));
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
		if (jm == (int)BU_PTBL_LEN(&results2) ||
		    bu_strcmp(regions[im].region_id, regions[jm].region_id) ||
		    !bu_strcmp(regions[im].region_id, "--")) {
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
		if (bu_strcmp(regions[im].material_id, regions[jm].material_id))
		    mismatch++;
		if (bu_strcmp(regions[im].los, regions[jm].los))
		    mismatch++;
		if (bu_strcmp(regions[im].aircode, regions[jm].aircode))
		    mismatch++;
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
			ssd += BU_STR_EQUAL(regions[im].aircode, regions[jm].aircode);
			if (ssd != 4) {
			    /* Found match, skip criteria not satisfied - set ignore flags */
			    regions[im].ignore = 0;
			    regions[jm].ignore = 0;
			}
		    } else if (BU_STR_EQUAL(regions[im].region_id, "--") &&
			      !BU_STR_EQUAL(regions[im].aircode, regions[jm].aircode)) {
			 /* edge case:
			  * aircodes usually do not have region_id
			  *     if region_id are matching empty and aircodes do not match - this is not a match
			  *
			  * ie do nothing
			  */
		    } else {
			/* Found match - set ignore flags */
			regions[im].ignore = 0;
			regions[jm].ignore = 0;
		    }
		} else if (BU_STR_EQUAL(regions[im].aircode, regions[jm].aircode) &&
			  !BU_STR_EQUAL(regions[im].aircode, "--")) {
		    /* Found matching aircodes - set ignore flags */
		    regions[im].ignore = 0;
		    regions[jm].ignore = 0;
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
	    return BRLCAD_ERROR;
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

    bu_vls_printf(output, "List length: %zu\n", BU_PTBL_LEN(&results2) - ignored_cnt);
    bu_vls_printf(output, "%-*s %-*s %-*s %-*s %-*s %s\n",
		  (int)region_id_len_max + 1, "ID",
		  (int)material_id_len_max + 1, "MAT",
		  (int)los_len_max, "LOS",
		  (int)aircode_len_max, "AIR",
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
	bu_vls_printf(output, "%-*s %-*s %-*s %-*s %-*s %s\n",
		      (int)region_id_len_max + 1, regions[i].region_id,
		      (int)material_id_len_max + 1, regions[i].material_id,
		      (int)los_len_max, regions[i].los,
		      (int)aircode_len_max, regions[i].aircode,
		      (int)obj_len_max, regions[i].obj_name,
		      regions[i].obj_parent);
    }
    bu_vls_printf(gedp->ged_result_str, "Done.");

    if (file_name) {
	bu_vls_fwrite(outfile, output);
	fclose(outfile);
    }

    bu_free(regions, "ged_lc_core");

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_LC_COMMANDS(X, XID) \
    X(lc, ged_lc_core, GED_CMD_DEFAULT, &lc_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_LC_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_lc", 1, GED_LC_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
