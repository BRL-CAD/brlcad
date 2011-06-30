/*                        M A K E _ P N T S . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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
 */
/** @file libged/make_pnts.c
 *
 * The "make_pnts" command makes a point-cloud from a points data file.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "rtgeom.h"
#include "wdb.h"

#include "./ged_private.h"

#define INSERT_COORDINATE_INTO_STRUCTURE(_structure_type, _control_variable, _variable_to_insert) \
    switch (_control_variable) {					\
	case 'x':							\
	    ((struct _structure_type *)point)->v[X] = _variable_to_insert; \
	    break;							\
	case 'y':							\
	    ((struct _structure_type *)point)->v[Y] = _variable_to_insert; \
	    break;							\
	case 'z':							\
	    ((struct _structure_type *)point)->v[Z] = _variable_to_insert; \
	    break;							\
    }

#define INSERT_COLOR_INTO_STRUCTURE(_structure_type, _control_variable, _variable_to_insert) \
    switch (_control_variable) {					\
	case 'r':							\
	    ((struct _structure_type *)point)->c.buc_magic = BU_COLOR_MAGIC; \
	    ((struct _structure_type *)point)->c.buc_rgb[0] = _variable_to_insert; \
	    break;							\
	case 'g':							\
	    ((struct _structure_type *)point)->c.buc_rgb[1] = _variable_to_insert; \
	    break;							\
	case 'b':							\
	    ((struct _structure_type *)point)->c.buc_rgb[2] = _variable_to_insert; \
	    break;							\
    }

#define INSERT_SCALE_INTO_STRUCTURE(_structure_type, _control_variable, _variable_to_insert) \
    switch (_control_variable) {					\
	case 's':							\
	    ((struct _structure_type *)point)->s = _variable_to_insert; \
	    break;							\
    }

#define INSERT_NORMAL_INTO_STRUCTURE(_structure_type, _control_variable, _variable_to_insert) \
    switch (_control_variable) {					\
	case 'i':							\
	    ((struct _structure_type *)point)->n[X] = _variable_to_insert; \
	    break;							\
	case 'j':							\
	    ((struct _structure_type *)point)->n[Y] = _variable_to_insert; \
	    break;							\
	case 'k':							\
	    ((struct _structure_type *)point)->n[Z] = _variable_to_insert; \
	    break;							\
    }

static char *p_make_pnts[] = {
    "Enter point-cloud name: ",
    "Enter point file path and name: ",
    "Enter file data format (xyzrgbsijk?): ",
    "Enter file data units (um|mm|cm|m|km|in|ft|yd|mi)\nor conversion factor from file data units to millimeters: ",
    "Enter default point size: "
};


/*
 * Character compare function used by qsort function.
 */
int
compare_char(const char *a, const char *b)
{
    return (int)(*a - *b);
}


/*
 * Validate 'point file data format string', determine and output the
 * point-cloud type. A valid null terminated string is expected as
 * input.  The function returns GED_ERROR if the format string is
 * invalid or if null pointers were passed to the function.
 */
int
str2type(const char *format_string, rt_pnt_type *pnt_type, struct bu_vls *ged_result_str)
{
    struct bu_vls str;
    char *temp_string = (char *)NULL;
    size_t idx = 0;
    size_t format_string_length = 0;
    int ret = GED_OK;

    bu_vls_init(&str);

    if ((format_string == (char *)NULL) || (pnt_type == (rt_pnt_type *)NULL)) {
	bu_vls_printf(ged_result_str, "NULL pointer(s) passed to function 'str2type'.\n");
	ret = GED_ERROR;
    } else {
	format_string_length = strlen(format_string);

	/* remove any '?' from format string before testing for point-cloud type */
	for (idx = 0 ; idx < format_string_length ; idx++) {
	    if (format_string[idx] != '?') {
		bu_vls_putc(&str, format_string[idx]);
	    }
	}

	bu_vls_trimspace(&str);

	temp_string = bu_vls_addr(&str);
	qsort(temp_string, strlen(temp_string), sizeof(char), (int (*)(const void *a, const void *b))compare_char);

	if (BU_STR_EQUAL(temp_string, "xyz")) {
	    *pnt_type = RT_PNT_TYPE_PNT;
	} else if (BU_STR_EQUAL(temp_string, "bgrxyz")) {
	    *pnt_type = RT_PNT_TYPE_COL;
	} else if (BU_STR_EQUAL(temp_string, "sxyz")) {
	    *pnt_type = RT_PNT_TYPE_SCA;
	} else if (BU_STR_EQUAL(temp_string, "ijkxyz")) {
	    *pnt_type = RT_PNT_TYPE_NRM;
	} else if (BU_STR_EQUAL(temp_string, "bgrsxyz")) {
	    *pnt_type = RT_PNT_TYPE_COL_SCA;
	} else if (BU_STR_EQUAL(temp_string, "bgijkrxyz")) {
	    *pnt_type = RT_PNT_TYPE_COL_NRM;
	} else if (BU_STR_EQUAL(temp_string, "ijksxyz")) {
	    *pnt_type = RT_PNT_TYPE_SCA_NRM;
	} else if (BU_STR_EQUAL(temp_string, "bgijkrsxyz")) {
	    *pnt_type = RT_PNT_TYPE_COL_SCA_NRM;
	} else {
	    bu_vls_printf(ged_result_str, "Invalid format string '%s'", format_string);
	    ret = GED_ERROR;
	}
    }

    bu_vls_free(&str);

    return ret;
}


/*
 * Validate points data file unit string and output conversion factor
 * to millimeters. If string is not a standard units identifier, the
 * function assumes a custom conversion factor was specified. A valid
 * null terminated string is expected as input. The function returns
 * GED_ERROR if the unit string is invalid or if null pointers were
 * passed to the function.
 */
int
str2mm(const char *units_string, double *conv_factor, struct bu_vls *ged_result_str)
{
    struct bu_vls str;
    double tmp_value = 0.0;
    char *endp = (char *)NULL;
    int ret = GED_OK;

    bu_vls_init(&str);

    if ((units_string == (char *)NULL) || (conv_factor == (double *)NULL)) {
	bu_vls_printf(ged_result_str, "NULL pointer(s) passed to function 'str2mm'.\n");
	ret = GED_ERROR;
    } else {
	bu_vls_strcat(&str, units_string);
	bu_vls_trimspace(&str);

	tmp_value = strtod(bu_vls_addr(&str), &endp);
	if ((endp != bu_vls_addr(&str)) && (*endp == '\0')) {
	    /* convert to double success */
	    *conv_factor = tmp_value;
	} else if ((tmp_value = bu_mm_value(bu_vls_addr(&str))) > 0.0) {
	    *conv_factor = tmp_value;
	} else {
	    bu_vls_printf(ged_result_str, "Invalid units string '%s'\n", units_string);
	    ret = GED_ERROR;
	}
    }

    bu_vls_free(&str);

    return ret;
}


void
report_import_error_location(unsigned long int num_doubles_read, unsigned int num_doubles_per_point,
			     unsigned long int start_offset_of_current_double, char field, struct bu_vls *ged_result_str)
{
    /* The num_doubles_read is the number of doubles successfully read. This error
     * report is for the double where the error occurred, i.e. the next double.
     */
    unsigned long int point_number =  ceil((num_doubles_read + 1) / (double)num_doubles_per_point);

    bu_vls_printf(ged_result_str, "Failed reading point %lu field '%c' at file byte %lu.\n",
		  point_number, field, start_offset_of_current_double);
}


/*
 * 'make_pnts' command for importing point-cloud data into a 'pnts'
 * primitive.
 *
 * Input values:
 * argv[1] object name
 * argv[2] filename with path
 * argv[3] point data file format string
 * argv[4] point data file units string or conversion factor to millimeters
 * argv[5] default size of each point
 *
 */
int
ged_make_pnts(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal internal;

    rt_pnt_type type;
    double local2base;
    unsigned long numPoints = 0;
    struct rt_pnts_internal *pnts;

    double defaultSize = 0.0;
    void *headPoint = NULL;

    FILE *fp;

    int temp_string_index = 0; /* index into temp_string, set to first character in temp_string, i.e. the start */
    unsigned long int num_doubles_read = 0; /* counters of double read from file */

    int current_character_double = 0; /* flag indicating if current character read is part of a double or delimiter */
    int previous_character_double = 0; /* flag indicating if previously read character was part of a double or delimiter */

    unsigned long int num_characters_read_from_file = 0;  /* counter of number of characters read from file */
    unsigned long int start_offset_of_current_double = 0; /* character offset from start of file for current double */
    int found_double = 0; /* flag indicating if double encountered in file and needs to be processed */
    int found_eof = 0; /* flag indicating if end-of-file encountered when reading file */
    int done_processing_format_string = 0; /* flag indicating if loop processing format string should be exited */

    char *temp_char_ptr = (char *)NULL;

    int buf = 0; /* raw character read from file */
    double temp_double = 0.0;
    char temp_string[1024];
    int temp_string_size = 1024;  /* number of characters that can be stored in temp_string including null terminator character */
				  /* it is expected that the character representation of a double will never exceed this size string */
    char *endp = (char *)NULL;

    struct bu_vls format_string;
    size_t format_string_index = 0;

    unsigned int num_doubles_per_point = 0;

    void *point = NULL;

    char **prompt;

    static const char *usage = "point_cloud_name filename_with_path file_format file_data_units default_point_size";

    prompt = &p_make_pnts[0];

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 6) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* prompt for point-cloud name */
    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[0]);
	return GED_MORE;
    }

    GED_CHECK_EXISTS(gedp, argv[1], LOOKUP_QUIET, GED_ERROR);

    /* prompt for data file name with path */
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[1]);
	return GED_MORE;
    }

    /* prompt for data file format */
    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[2]);
	return GED_MORE;
    }

    /* Validate 'point file data format string' and return point-cloud type. */
    if (str2type(argv[3], &type, gedp->ged_result_str) == GED_ERROR) {
	return GED_ERROR;
    }

    /* prompt for data file units */
    if (argc < 5) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[3]);
	return GED_MORE;
    }

    /* Validate the unit string and return conversion factor to millimeters. */
    if (str2mm(argv[4], &local2base, gedp->ged_result_str) == GED_ERROR) {
	return GED_ERROR;
    }

    /* prompt for default point size */
    if (argc < 6) {
	bu_vls_printf(gedp->ged_result_str, "%s", prompt[4]);
	return GED_MORE;
    }

    defaultSize = strtod(argv[5], &endp);
    if ((endp != argv[5]) && (*endp == '\0')) {
	/* convert to double success */
	if (defaultSize < 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "Default point size '%lf' must be non-negative.\n", defaultSize);
	    return GED_ERROR;
	}
    } else {
	bu_vls_printf(gedp->ged_result_str, "Invalid default point size '%s'\n", argv[5]);
	return GED_ERROR;
    }

    bu_vls_init(&format_string);
    bu_vls_strcat(&format_string, argv[3]);
    bu_vls_trimspace(&format_string);

    /* init database structure */
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_PNTS;
    internal.idb_meth = &rt_functab[ID_PNTS];
    internal.idb_ptr = (genptr_t) bu_malloc(sizeof(struct rt_pnts_internal), "rt_pnts_internal");

    /* init internal structure */
    pnts = (struct rt_pnts_internal *) internal.idb_ptr;
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = defaultSize;
    pnts->type = type;
    pnts->count = numPoints;  /* set again later */
    pnts->point = NULL;

    /* empty list head */
    switch (type) {
	case RT_PNT_TYPE_PNT:
	    BU_GETSTRUCT(headPoint, pnt);
	    BU_LIST_INIT(&(((struct pnt *)headPoint)->l));
	    num_doubles_per_point = 3;
	    break;
	case RT_PNT_TYPE_COL:
	    BU_GETSTRUCT(headPoint, pnt_color);
	    BU_LIST_INIT(&(((struct pnt_color *)headPoint)->l));
	    num_doubles_per_point = 6;
	    break;
	case RT_PNT_TYPE_SCA:
	    BU_GETSTRUCT(headPoint, pnt_scale);
	    BU_LIST_INIT(&(((struct pnt_scale *)headPoint)->l));
	    num_doubles_per_point = 4;
	    break;
	case RT_PNT_TYPE_NRM:
	    BU_GETSTRUCT(headPoint, pnt_normal);
	    BU_LIST_INIT(&(((struct pnt_normal *)headPoint)->l));
	    num_doubles_per_point = 6;
	    break;
	case RT_PNT_TYPE_COL_SCA:
	    BU_GETSTRUCT(headPoint, pnt_color_scale);
	    BU_LIST_INIT(&(((struct pnt_color_scale *)headPoint)->l));
	    num_doubles_per_point = 7;
	    break;
	case RT_PNT_TYPE_COL_NRM:
	    BU_GETSTRUCT(headPoint, pnt_color_normal);
	    BU_LIST_INIT(&(((struct pnt_color_normal *)headPoint)->l));
	    num_doubles_per_point = 9;
	    break;
	case RT_PNT_TYPE_SCA_NRM:
	    BU_GETSTRUCT(headPoint, pnt_scale_normal);
	    BU_LIST_INIT(&(((struct pnt_scale_normal *)headPoint)->l));
	    num_doubles_per_point = 7;
	    break;
	case RT_PNT_TYPE_COL_SCA_NRM:
	    BU_GETSTRUCT(headPoint, pnt_color_scale_normal);
	    BU_LIST_INIT(&(((struct pnt_color_scale_normal *)headPoint)->l));
	    num_doubles_per_point = 10;
	    break;
    }
    BU_ASSERT_PTR(headPoint, !=, NULL);
    pnts->point = headPoint;

    if ((fp=fopen(argv[2], "rb")) == NULL) {
	bu_vls_printf(gedp->ged_result_str, "Make '%s' failed. ", argv[1]);
	bu_vls_printf(gedp->ged_result_str, "Could not open file '%s'.\n", argv[2]);
	bu_vls_free(&format_string);
	rt_db_free_internal(&internal);
	return GED_ERROR;
    }

    while (!found_eof) {
	/* points_loop */
	/* allocate memory for single point structure for current point-cloud type */
	switch (type) {
	    case RT_PNT_TYPE_PNT:
		BU_GETSTRUCT(point, pnt);
		break;
	    case RT_PNT_TYPE_COL:
		BU_GETSTRUCT(point, pnt_color);
		break;
	    case RT_PNT_TYPE_SCA:
		BU_GETSTRUCT(point, pnt_scale);
		break;
	    case RT_PNT_TYPE_NRM:
		BU_GETSTRUCT(point, pnt_normal);
		break;
	    case RT_PNT_TYPE_COL_SCA:
		BU_GETSTRUCT(point, pnt_color_scale);
		break;
	    case RT_PNT_TYPE_COL_NRM:
		BU_GETSTRUCT(point, pnt_color_normal);
		break;
	    case RT_PNT_TYPE_SCA_NRM:
		BU_GETSTRUCT(point, pnt_scale_normal);
		break;
	    case RT_PNT_TYPE_COL_SCA_NRM:
		BU_GETSTRUCT(point, pnt_color_scale_normal);
		break;
	}

	/* make sure we have something */
	BU_ASSERT_PTR(point, !=, NULL);

	while (!found_eof && !done_processing_format_string) {
	    /* format_string_loop */
	    char format = '\0';

	    while (!found_eof  && !found_double) {
		/* find_doubles_loop */
		format = bu_vls_addr(&format_string)[format_string_index];

		buf = fgetc(fp);

		num_characters_read_from_file++;

		if (feof(fp)) {
		    if (ferror(fp)) {
			perror("ERROR: Problem reading file, system error message");
			fclose(fp);
			bu_vls_printf(gedp->ged_result_str, "Make '%s' failed. ", argv[1]);
			bu_vls_printf(gedp->ged_result_str, "Unable to read file at byte '%lu'.\n", num_characters_read_from_file);
			bu_vls_free(&format_string);
			rt_db_free_internal(&internal);
			return GED_ERROR;
		    } else {
			found_eof = 1;
		    }
		}

		if (found_eof) {
		    fclose(fp);
		    current_character_double = 0;
		    if (num_doubles_read == 0) {
			bu_vls_printf(gedp->ged_result_str, "Make '%s' failed. ", argv[1]);
			bu_vls_printf(gedp->ged_result_str, "No data in file '%s'.\n", argv[2]);
			bu_vls_free(&format_string);
			rt_db_free_internal(&internal);
			return GED_ERROR;
		    }
		} else {
		    if ((temp_char_ptr = strchr("0123456789.+-eE", buf)) != NULL) {
			/* character read is part of a double */
			current_character_double = 1;
		    } else {
			current_character_double = 0;
		    }
		}

		if (previous_character_double && current_character_double) {
		    if (temp_string_index >= temp_string_size) {
			bu_vls_printf(gedp->ged_result_str, "Make '%s' failed. ", argv[1]);
			bu_vls_printf(gedp->ged_result_str, "String representing double too large, exceeds '%d' character limit. ", temp_string_size - 1);
			report_import_error_location(num_doubles_read, num_doubles_per_point, start_offset_of_current_double,
						     format, gedp->ged_result_str);
			bu_vls_free(&format_string);
			rt_db_free_internal(&internal);
			return GED_ERROR;
		    }
		    temp_string[temp_string_index] = (char)buf;
		    temp_string_index++;
		}

		if (previous_character_double && !current_character_double) {
		    if (temp_string_index >= temp_string_size) {
			bu_vls_printf(gedp->ged_result_str, "Make '%s' failed. ", argv[1]);
			bu_vls_printf(gedp->ged_result_str, "String representing double too large, exceeds '%d' character limit. ", temp_string_size - 1);
			report_import_error_location(num_doubles_read, num_doubles_per_point, start_offset_of_current_double,
						     format, gedp->ged_result_str);
			bu_vls_free(&format_string);
			rt_db_free_internal(&internal);
			return GED_ERROR;
		    }
		    temp_string[temp_string_index] = '\0';

		    /* do not convert string to double for format character '?' */
		    if (format != '?') {
			temp_double = strtod(temp_string, &endp);
			if (!((endp != temp_string) && (*endp == '\0'))) {
			    bu_vls_printf(gedp->ged_result_str, "Make '%s' failed. ", argv[1]);
			    bu_vls_printf(gedp->ged_result_str, "Unable to convert string '%s' to double. ", temp_string);
			    report_import_error_location(num_doubles_read, num_doubles_per_point, start_offset_of_current_double,
							 format, gedp->ged_result_str);
			    bu_vls_free(&format_string);
			    rt_db_free_internal(&internal);
			    return GED_ERROR;
			}
			num_doubles_read++;
		    } else {
			temp_double = 0.0;
		    }

		    temp_string_index = 0;
		    found_double = 1;
		    previous_character_double = current_character_double;
		}

		if (!previous_character_double && current_character_double) {
		    temp_string[temp_string_index] = (char)buf;
		    temp_string_index++;
		    start_offset_of_current_double = num_characters_read_from_file;
		    previous_character_double = current_character_double;
		}

	    } /* loop exits when eof encounted (and/or) double found */

	    if (found_double) {

		/* insert double into point structure for current point-cloud type */
		/* do not attempt to insert double into point structure for format character '?' */
		if (format != '?') {
		    switch (type) {
			case RT_PNT_TYPE_PNT:
			    INSERT_COORDINATE_INTO_STRUCTURE(pnt, format, (temp_double * local2base));
			    break;
			case RT_PNT_TYPE_COL:
			    INSERT_COORDINATE_INTO_STRUCTURE(pnt_color, format, (temp_double * local2base));
			    INSERT_COLOR_INTO_STRUCTURE(pnt_color, format, temp_double);
			    break;
			case RT_PNT_TYPE_SCA:
			    INSERT_COORDINATE_INTO_STRUCTURE(pnt_scale, format, (temp_double * local2base));
			    INSERT_SCALE_INTO_STRUCTURE(pnt_scale, format, (temp_double * local2base));
			    break;
			case RT_PNT_TYPE_NRM:
			    INSERT_COORDINATE_INTO_STRUCTURE(pnt_normal, format, (temp_double * local2base));
			    INSERT_NORMAL_INTO_STRUCTURE(pnt_normal, format, (temp_double * local2base));
			    break;
			case RT_PNT_TYPE_COL_SCA:
			    INSERT_COORDINATE_INTO_STRUCTURE(pnt_color_scale, format, (temp_double * local2base));
			    INSERT_COLOR_INTO_STRUCTURE(pnt_color_scale, format, temp_double);
			    INSERT_SCALE_INTO_STRUCTURE(pnt_color_scale, format, (temp_double * local2base));
			    break;
			case RT_PNT_TYPE_COL_NRM:
			    INSERT_COORDINATE_INTO_STRUCTURE(pnt_color_normal, format, (temp_double * local2base));
			    INSERT_COLOR_INTO_STRUCTURE(pnt_color_normal, format, temp_double);
			    INSERT_NORMAL_INTO_STRUCTURE(pnt_color_normal, format, (temp_double * local2base));
			    break;
			case RT_PNT_TYPE_SCA_NRM:
			    INSERT_COORDINATE_INTO_STRUCTURE(pnt_scale_normal, format, (temp_double * local2base));
			    INSERT_SCALE_INTO_STRUCTURE(pnt_scale_normal, format, (temp_double * local2base));
			    INSERT_NORMAL_INTO_STRUCTURE(pnt_scale_normal, format, (temp_double * local2base));
			    break;
			case RT_PNT_TYPE_COL_SCA_NRM:
			    INSERT_COORDINATE_INTO_STRUCTURE(pnt_color_scale_normal, format, (temp_double * local2base));
			    INSERT_COLOR_INTO_STRUCTURE(pnt_color_scale_normal, format, temp_double);
			    INSERT_SCALE_INTO_STRUCTURE(pnt_color_scale_normal, format, (temp_double * local2base));
			    INSERT_NORMAL_INTO_STRUCTURE(pnt_color_scale_normal, format, (temp_double * local2base));
			    break;
		    }
		}
		found_double = 0;  /* allows loop to continue */
		format_string_index++;
		if (format_string_index >= bu_vls_strlen(&format_string)) {
		    done_processing_format_string = 1;
		}
	    }

	} /* loop exits when eof encountered (and/or) all doubles for
	   * a single point are stored in point structure
	   */

	if (done_processing_format_string) {
	    /* push single point structure onto linked-list of points
	     * which makeup the point-cloud.
	     */
	    switch (type) {
		case RT_PNT_TYPE_PNT:
		    BU_LIST_PUSH(&(((struct pnt *)headPoint)->l), &((struct pnt *)point)->l);
		    break;
		case RT_PNT_TYPE_COL:
		    BU_LIST_PUSH(&(((struct pnt_color *)headPoint)->l), &((struct pnt_color *)point)->l);
		    break;
		case RT_PNT_TYPE_SCA:
		    BU_LIST_PUSH(&(((struct pnt_scale *)headPoint)->l), &((struct pnt_scale *)point)->l);
		    break;
		case RT_PNT_TYPE_NRM:
		    BU_LIST_PUSH(&(((struct pnt_normal *)headPoint)->l), &((struct pnt_normal *)point)->l);
		    break;
		case RT_PNT_TYPE_COL_SCA:
		    BU_LIST_PUSH(&(((struct pnt_color_scale *)headPoint)->l), &((struct pnt_color_scale *)point)->l);
		    break;
		case RT_PNT_TYPE_COL_NRM:
		    BU_LIST_PUSH(&(((struct pnt_color_normal *)headPoint)->l), &((struct pnt_color_normal *)point)->l);
		    break;
		case RT_PNT_TYPE_SCA_NRM:
		    BU_LIST_PUSH(&(((struct pnt_scale_normal *)headPoint)->l), &((struct pnt_scale_normal *)point)->l);
		    break;
		case RT_PNT_TYPE_COL_SCA_NRM:
		    BU_LIST_PUSH(&(((struct pnt_color_scale_normal *)headPoint)->l), &((struct pnt_color_scale_normal *)point)->l);
		    break;
	    }
	    numPoints++;
	    format_string_index = 0;
	    done_processing_format_string = 0;
	}

    } /* loop exits when eof encountered */

    if (num_doubles_read < num_doubles_per_point) {
	bu_vls_printf(gedp->ged_result_str, "Make '%s' failed. Number of values read inconsistent with point-cloud type ", argv[1]);
	bu_vls_printf(gedp->ged_result_str, "defined by format string '%V'. The number of values read must be an even ", format_string);
	bu_vls_printf(gedp->ged_result_str, "multiple of %d but read %lu values.\n", num_doubles_per_point, num_doubles_read);
	bu_vls_free(&format_string);
	rt_db_free_internal(&internal);
	return GED_ERROR;
    }

    if (num_doubles_read % num_doubles_per_point) {
	bu_vls_printf(gedp->ged_result_str, "Make '%s' failed. Number of values read inconsistent with point-cloud type ", argv[1]);
	bu_vls_printf(gedp->ged_result_str, "defined by format string '%V'. The number of values read must be an even ", format_string);
	bu_vls_printf(gedp->ged_result_str, "multiple of %d but read %lu values.\n", num_doubles_per_point, num_doubles_read);
	bu_vls_free(&format_string);
	rt_db_free_internal(&internal);
	return GED_ERROR;
    }

    pnts->count = numPoints;

    GED_DB_DIRADD(gedp, dp, argv[1], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&internal.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, dp, &internal, &rt_uniresource, GED_ERROR);

    bu_vls_free(&format_string);

    bu_vls_printf(gedp->ged_result_str, "Make '%s' success, %lu values read, %lu points imported.\n", argv[1], num_doubles_read, numPoints);

    return GED_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
