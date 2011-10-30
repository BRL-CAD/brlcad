/*                        D E M - G . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file conv/dem-g.c
 *
 * USGS ASCII DEM file to dsp primitive converter
 *
 * There are several global element arrays that contain information
 * about the structure of the DEM-G file, record types 'A', 'B' and
 * 'C'.  Data is loaded into these arrays during main().
 *
 * An 'element' is a 'data element' as defined in the DEM-G file
 * specification.
 *
 * A 'sub-element' is a field of data within an 'element'.
 *
 * An 'element' will contain at least one 'sub-element'.  The '?'
 * within these array descriptions can be replaced with 'a', 'b' or
 * 'c' to indicate a specific array which corresponds to a specific
 * record type.
 *
 * ARRAY DESCRIPTION: record_?_element_size[index1][index2][index3]
 * index1 = element number
 * index2 = sub-element number within element
 * index3 = value number
 * Three values are stored for each sub-element...
 *
 * when index3=1, value = start character of sub-element within DEM-G
 * file 1024 character A/B/C record.
 *
 * when index3=2, value = number of characters in sub-element.
 *
 * when index3=3, value = datatype of sub-element (1=alpha, 2=signed
 * long integer, 3=double precision float).
 *
 * ARRAY DESCRIPTION: record_?_sub_elements[index]
 * index = element number
 * value = number of sub-elements per element
 *
 * This information allows the code to loop through the contents of
 * the array 'record_?_element_size' since each element can contain a
 * different number of sub-elements.
 *
 * No values are stored with an index of zero within these arrays,
 * therefore the size of each dimension is increased by one to
 * accommodate this.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"

#include "bu.h"
#include "bn.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"
#include "db.h"


const char *progname ="dem-g";

#define DSP_MAX_RAW_ELEVATION 65535

const char record_type_names[4][24] = {
    "",
    "logical record type 'A'",
    "logical record type 'B'",
    "logical record type 'C'"
};

typedef enum _lrt {
    type_a=1,
    type_b=2,
    type_c=3
} logical_record_type;

typedef enum _sed {
    type_alpha=1,
    type_integer=2,
    type_double=3
} sub_element_datatype;


#define A_ROWS 32      /* DEM-G file type 'A' record, number of elements.                        */
#define A_COLS 16      /* DEM-G file type 'A' record, max number of sub-elements per element.    */
#define A_VALS 4       /* DEM-G file type 'A' record, number of values to store per sub-element. */
#define RECORD_TYPE 4  /* DEM-G file type, record type (1=A, 2=B, 3=C) */

int element_counts[4];
int sub_elements_required_list_counts[4];
int element_attributes[RECORD_TYPE][A_ROWS][A_COLS][A_VALS];
int sub_element_counts[RECORD_TYPE][A_ROWS];
int sub_elements_required_list[4][A_ROWS][3];
double conversion_factor_to_milimeters[4];

/**
 * The size of the largest string needed to contain the widest ascii
 * sub-element field as defined in the DEM-G file specification for
 * record types 'A', 'B' and 'C'.  To this size add '4' to account for
 * possibly appending 'E+00' and add '1' to account for the string
 * terminator character. This value should not change since it is
 * derived from the DEM-G file specification.
 */
#define MAX_STRING_LENGTH 45

/**
 * This type defines the structure which contains the input & output
 * parameters passed to & from the function 'read_element'.
 */
typedef struct {
    char *in_buffer;
    logical_record_type in_record_type;
    int in_is_b_header;
    int in_element_number;
    int in_sub_element_number;
    sub_element_datatype out_datatype;
    char out_alpha[MAX_STRING_LENGTH];
    long int out_integer;
    double out_double;
    int out_undefined;
} ResultStruct;


double
round_closest(double x)
{
    return (x > 0.0) ? floor(x + 0.5) : ceil (x - 0.5);
}


void
usage(void)
{
    bu_log("Usage: %s dem_file\n", progname);
}


int
flip_high_low_bytes(long int in_value, unsigned char *out_string)
{
    /* it is expected the out_string points to */
    /* a string of at least 3 characters */
    unsigned char highbyte = '\0';
    unsigned char lowbyte = '\0';
    int status = BRLCAD_ERROR;

    if ((in_value >= 0) && (in_value <= 65535)) {
	highbyte = (unsigned char)floor(in_value / 256);
	lowbyte = (unsigned char)(in_value - (highbyte * 256));
	out_string[0] = highbyte;
	out_string[1] = lowbyte;
	out_string[2] = '\0';
	status = BRLCAD_OK;
    } else {
	bu_log("ERROR: function flip_high_low_bytes input value '%ld' not within 0-65535.\n", in_value);
	out_string[0] = '\0';
	status = BRLCAD_ERROR;
    }
    return status;
}


int
output_elevation(long int in_value, FILE *fp)
{
    unsigned char buf[3] = "";
    int status = BRLCAD_ERROR;

    /* allow for clipping */
    if (in_value > DSP_MAX_RAW_ELEVATION) {
	in_value = DSP_MAX_RAW_ELEVATION;
    }

    if (flip_high_low_bytes(in_value, buf) == BRLCAD_OK) {
	if (fwrite(buf, 2, 1, fp) == 1) {
	    status = BRLCAD_OK;
	}
    }
    if (status == BRLCAD_ERROR) {
	bu_log("Within function output_elevation, error writing elevation to temp file.\n");
    }
    return status;
}


/*
 * ----------------------------------------------------------------------------
 * removes pre and post whitespace from a string
 * ----------------------------------------------------------------------------
 */
void remove_whitespace(char *input_string)
{
    char *idx = '\0';
    int idx2 = 0;
    int input_string_length = 0;
    char *firstp = '\0';
    char *lastp = '\0';
    int found_start = 0;
    int found_end = 0;
    int cleaned_string_length = 0;

    input_string_length = strlen(input_string);
    firstp = input_string;
    /* initially lastp points to null at end of input string */
    lastp = firstp + input_string_length;
    /* test for zero length input_string, if zero then do nothing */
    if (input_string_length == 0) {
	return;
    }

    /* find start character and set pointer firstp to this character */
    found_start = 0;
    idx = firstp;
    while ((found_start == 0) && (idx < lastp)) {
	if (isspace(idx[0]) == 0) {
	    /* execute if non-space found */
	    found_start = 1;
	    firstp = idx;
	}
	idx++;
    }
    /* if found_start is 0 then string must be all whitespace */
    if (found_start == 0) {
	/* set null to first character a do nothing more */
	input_string[0] = '\0';
	return;
    }

    /* If found_start is 1, check for trailing whitespace.  Find
     * last character and set pointer lastp to next character after,
     * i.e. where null would be.  There as at least one non-space
     * character in this string therefore will not need to deal with
     * an empty string condition in the loop looking for the string
     * end.
     */
    found_end = 0;
    idx = lastp - 1;
    while ((found_end == 0) && (idx >= firstp)) {
	if (isspace(idx[0]) == 0) {
	    /* execute if non-space found */
	    found_end = 1;
	    lastp = idx + 1;
	}
	idx--;
    }

    /* Test if characters in string need to be shifted left and then
     * do nothing more.
     */
    if (firstp > input_string) {
	/* Execute if need to shift left, this would happen only */
	/* if input_string contained pre whitspace. */
	cleaned_string_length = lastp - firstp;
	for (idx2 = 0; idx2 < cleaned_string_length; idx2++) {
	    input_string[idx2] = firstp[idx2];
	}
	input_string[cleaned_string_length] = '\0';
	return;
    }

    /* no need to shift left, set null to location of lastp */
    lastp[0] = '\0';
}


/**
 * retrieve the value from element, sub_element of record a
 */
int read_element(ResultStruct *io_struct)
{
    char tmp_str[MAX_STRING_LENGTH];
    int element = 0;
    int sub_element = 0;
    char *search_result_uppercase;
    char *search_result_lowercase;
    double tmp_dbl = 0;
    long tmp_long = 0;
    char *endp;
    char *buf = '\0';
    int status = BRLCAD_ERROR;
    logical_record_type record_type ;
    int start_character = 0;
    int field_width = 0;
    sub_element_datatype datatype;
    int is_b_header = 0;

    /* assign input structure values to local variables */
    buf = (*io_struct).in_buffer;
    record_type = (*io_struct).in_record_type;
    is_b_header = (*io_struct).in_is_b_header;
    element = (*io_struct).in_element_number;
    sub_element = (*io_struct).in_sub_element_number;

    /* set output fields to defaults */
    (*io_struct).out_datatype = type_alpha;
    (*io_struct).out_alpha[0] = '\0';
    (*io_struct).out_integer = 0;
    (*io_struct).out_double = 0;
    (*io_struct).out_undefined = 0;

    if (!((record_type == type_b) && (element == 6))) {
	/* when the value to be read is not an elevation */
	/* fyi element 6 in a B record type is an elevation */
	field_width = element_attributes[record_type][element][sub_element][2];
	start_character = element_attributes[record_type][element][sub_element][1];
	datatype = element_attributes[record_type][element][sub_element][3];
	(*io_struct).out_datatype = datatype;
    } else {
	/* when the value to be read is an elevation */
	/* must compute the start_character of the elevation value within the buffer */
	field_width = 6;
	datatype = type_integer;    /* sets datatype to integer */
	(*io_struct).out_datatype = datatype;
	if (is_b_header == 1) {
	    /* the buffer contains a B record header and elevation data */
	    start_character = 145 + ((sub_element - 1) * field_width);
	} else {
	    /* the buffer contains a B record with only elevation data */
	    start_character = 1 + ((sub_element - 1) * field_width);
	}
    }

    /* strncpy(tmp_str, &buf[element_attributes[record_type][element][sub_element][1]-1], element_attributes[record_type][element][sub_element][2]); */

    /* in bu_strlcpy must include in the 3rd parameter (i.e. count or
     * size) one extra character to account for string terminator.
     */

    bu_strlcpy(tmp_str, &buf[start_character - 1], field_width + 1);

    tmp_str[field_width] = '\0';

    /* removes pre & post whitespace, if all whitespace then returns
     * empty string
     */
    remove_whitespace(tmp_str);

    if (strlen(tmp_str) == 0) {
	/* data was all whitespace */
	(*io_struct).out_undefined = 1;
	return BRLCAD_OK;
    }

    /* sub_element contains non-whitespace */

    /*
     * --------------------------------------------------------------------
     * process strings
     * --------------------------------------------------------------------
     */
    if (datatype == type_alpha) {
	/* strcpy((*io_struct).out_alpha, tmp_str); */
	bu_strlcpy((*io_struct).out_alpha, tmp_str, strlen(tmp_str)+1);
	status = BRLCAD_OK;
    }

    /*
     * --------------------------------------------------------------------
     * process integers
     * --------------------------------------------------------------------
     */
    if (datatype == type_integer) {
	/* sub_element was defined as a integer */
	tmp_long = strtol(tmp_str, &endp, 10);
	if ((tmp_str != endp) && (*endp == '\0')) {
	    /* convert to integer success */
	    (*io_struct).out_integer = tmp_long;
	    status = BRLCAD_OK;
	} else {
	    /* convert to integer failed */
	    /* copy string which failed to convert to inetger to output structure */
	    strcpy((*io_struct).out_alpha, tmp_str);
	    status = BRLCAD_ERROR;
	}
    }

    /*
     * --------------------------------------------------------------------
     * process doubles
     * --------------------------------------------------------------------
     */
    if (datatype == type_double) {
	/* sub_element was defined as a double */
	if ((search_result_uppercase = strchr(tmp_str, 'D')) != NULL) {
	    /* uppercase 'D' found, replace with 'E' */
	    search_result_uppercase[0] = 'E';
	} else {
	    if ((search_result_lowercase = strchr(tmp_str, 'd')) != NULL) {
		/* lowercase 'd' found, replace with 'e' */
		search_result_lowercase[0] = 'e';
	    } else {
		if ((strchr(tmp_str, 'E') == NULL) && (strchr(tmp_str, 'e') == NULL)) {
		    /* if no uppercase 'E' and no lowercase 'e' then append 'E+00' */
		    /* required for function 'strtod' to convert string to double */
		    /* strcat(tmp_str, "E+00"); */
		    bu_strlcat(tmp_str, "E+00", sizeof(tmp_str));
		}
	    }
	}
	/* convert to double */
	tmp_dbl = strtod(tmp_str, &endp);
	if ((tmp_str != endp) && (*endp == '\0')) {
	    /* convert to double success */
	    (*io_struct).out_double = tmp_dbl;
	    status = BRLCAD_OK;
	} else {
	    /* convert to double failed */
	    /* copy string which failed to convert to double to output structure */
	    strcpy((*io_struct).out_alpha, tmp_str);
	    status = BRLCAD_ERROR;
	}
    }

    return status;
}


int
validate_dem_record(char *buf, int create_log, logical_record_type record_type)
{
    /* uses global arrays 'sub_element_counts', 'element_counts',
     * 'record_type_names' and 'sub_elements_required_list_counts'
     */
    ResultStruct my_out2;
    ResultStruct *my_out_ptr2;
    int status = BRLCAD_ERROR;
    int element_number = 0;
    int sub_element_number = 0;
    int idx = 0;

    my_out_ptr2 = &my_out2;

    /* Loop through all elements and sub_elements to be sure there are
     * no errors trying to read any of the data.
     */
    for (element_number = 1; element_number <= element_counts[record_type]; element_number++) {
	for (sub_element_number = 1; sub_element_number <= sub_element_counts[record_type][element_number]; sub_element_number++) {
	    (*my_out_ptr2).in_buffer = buf;
	    (*my_out_ptr2).in_record_type = record_type;
	    (*my_out_ptr2).in_element_number = element_number;
	    (*my_out_ptr2).in_sub_element_number = sub_element_number;
	    if (read_element(my_out_ptr2) == BRLCAD_ERROR) {
		if (create_log == 1) {
		    bu_log("Failed validation of %s element %i sub_element %i, error reading value.\n",
			   record_type_names[record_type], (*my_out_ptr2).in_element_number,
			   (*my_out_ptr2).in_sub_element_number);
		}
		return BRLCAD_ERROR;
	    }
	}
    }

    /* Loop through a list of element sub_elements that must be
     * defined.  No test for read failure within this loop since this
     * is expected to already have been tested.
     */
    for (idx = 1; idx <= sub_elements_required_list_counts[record_type]; idx++) {
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = sub_elements_required_list[record_type][idx][1];
	(*my_out_ptr2).in_sub_element_number = sub_elements_required_list[record_type][idx][2];
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_undefined == 1) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, value undefined.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number);
	    }
	    return BRLCAD_ERROR;
	}
    }

    if (record_type == type_a) {
	/* Test some of the defined sub_elements for expected values */
	/* Validate record 'A' element 8 sub_element 1 */
	/* Unit of measure for ground planimetric coordinates 0=radians, 1=feet, 2=meters, 3=arc-seconds */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 8;
	(*my_out_ptr2).in_sub_element_number = 1;
	status = read_element(my_out_ptr2);
	if (!(((*my_out_ptr2).out_integer >= 0) && ((*my_out_ptr2).out_integer <= 3))) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%ld', expected '0, 1, 2, 3'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_integer);
	    }
	    return BRLCAD_ERROR;
	}

	/* Test some of the defined sub_elements for expected values */
	/* Validate record 'A' element 9 sub_element 1 */
	/* Unit of measure for elevation coordinates 1=feet, 2=meters */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 9;
	(*my_out_ptr2).in_sub_element_number = 1;
	status = read_element(my_out_ptr2);
	if (!(((*my_out_ptr2).out_integer == 1) || ((*my_out_ptr2).out_integer == 2))) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%ld', expected '1, 2'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_integer);
	    }
	    return BRLCAD_ERROR;
	}

	/* Test some of the defined sub_elements for expected values */
	/* Validate record 'A' element 10 sub_element 1 */
	/* Number of sides in the polygon which defines the coverage of the DEM file */
	/* Only a value of '4' is supported by this program */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 10;
	(*my_out_ptr2).in_sub_element_number = 1;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_integer != 4) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%ld', expected '4'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_integer);
	    }
	    return BRLCAD_ERROR;
	}

	/* Test some of the defined sub_elements for expected values */
	/* Validate record 'A' element 12 sub_element 2 */
	/* Max elevation for DEM */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 12;
	(*my_out_ptr2).in_sub_element_number = 2;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_double < 0) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%g', expected >= '0'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_double);
	    }
	    return BRLCAD_ERROR;
	}

	/* test some of the defined sub_elements for expected values */
	/* Validate record 'A' element 15 sub_element 1 */
	/* DEM spatial resolution for X */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 15;
	(*my_out_ptr2).in_sub_element_number = 1;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_double <= 0) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%g', expected > '0'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_double);
	    }
	    return BRLCAD_ERROR;
	}

	/* test some of the defined sub_elements for expected values */
	/* Validate record 'A' element 15 sub_element 2 */
	/* DEM spatial resolution for Y */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 15;
	(*my_out_ptr2).in_sub_element_number = 2;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_double <= 0) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%g', expected > '0'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_double);
	    }
	    return BRLCAD_ERROR;
	}

	/* test some of the defined sub_elements for expected values */
	/* Validate record 'A' element 15 sub_element 3 */
	/* DEM spatial resolution for Z */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 15;
	(*my_out_ptr2).in_sub_element_number = 3;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_double <= 0) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%g', expected > '0'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_double);
	    }
	    return BRLCAD_ERROR;
	}

	/* Test some of the defined sub_elements for expected values */
	/* Validate record 'A' element 16 sub_element 1 */
	/* Number of rows in DEM file, set to 1 to indicate element 16 */
	/* sub_element 2 is number of columns in DEM file */
	/* Only a value of '1' is supported by this program */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 16;
	(*my_out_ptr2).in_sub_element_number = 1;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_integer != 1) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%ld', expected '1'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_integer);
	    }
	    return BRLCAD_ERROR;
	}

	/* Test some of the defined sub_elements for expected values */
	/* Validate record 'A' element 16 sub_element 2 */
	/* Number of columns in DEM file */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 16;
	(*my_out_ptr2).in_sub_element_number = 2;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_integer < 1) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%ld', expected >= '1'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_integer);
	    }
	    return BRLCAD_ERROR;
	}
    } /* endif when record_type == type_a */

    if (record_type == type_b) {
	/* Test some of the defined sub_elements for expected values */
	/* Validate record 'B' element 1 sub_element 1 */
	/* Row identification number of the profile, row number is normally set to 1 */
	/* Only a value of '1' is supported by this program */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 1;
	(*my_out_ptr2).in_sub_element_number = 1;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_integer != 1) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%ld', expected '1'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_integer);
	    }
	    return BRLCAD_ERROR;
	}

	/* Test some of the defined sub_elements for expected values */
	/* Validate record 'B' element 1 sub_element 2 */
	/* Column identification number of the profile */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 1;
	(*my_out_ptr2).in_sub_element_number = 2;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_integer < 1) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%ld', expected >= '1'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_integer);
	    }
	    return BRLCAD_ERROR;
	}

	/* Test some of the defined sub_elements for expected values */
	/* Validate record 'B' element 2 sub_element 1 */
	/* Number of elevations in profile */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 2;
	(*my_out_ptr2).in_sub_element_number = 1;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_integer < 1) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%ld', expected >= '1'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_integer);
	    }
	    return BRLCAD_ERROR;
	}

	/* Test some of the defined sub_elements for expected values */
	/* Validate record 'B' element 2 sub_element 2 */
	/* Number of elevations in profile, 1 indicates 1 column per 'B' record */
	/* Only a value of '1' is supported by this program */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 2;
	(*my_out_ptr2).in_sub_element_number = 2;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_integer != 1) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%ld', expected '1'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_integer);
	    }
	    return BRLCAD_ERROR;
	}

	/* Test some of the defined sub_elements for expected values */
	/* Validate record 'B' element 4 sub_element 1 */
	/* Elevation of local datum for profile */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 4;
	(*my_out_ptr2).in_sub_element_number = 1;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_double < 0) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%g', expected >= '0'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_double);
	    }
	    return BRLCAD_ERROR;
	}

	/* Test some of the defined sub_elements for expected values */
	/* Validate record 'B' element 5 sub_element 2 */
	/* Max elevation for profile */
	(*my_out_ptr2).in_buffer = buf;
	(*my_out_ptr2).in_record_type = record_type;
	(*my_out_ptr2).in_element_number = 5;
	(*my_out_ptr2).in_sub_element_number = 2;
	status = read_element(my_out_ptr2);
	if ((*my_out_ptr2).out_double < 0) {
	    if (create_log == 1) {
		bu_log("Failed validation of %s element %i sub_element %i, unexpected value, found '%g', expected >= '0'.\n",
		       record_type_names[record_type], (*my_out_ptr2).in_element_number,
		       (*my_out_ptr2).in_sub_element_number, (*my_out_ptr2).out_double);
	    }
	    return BRLCAD_ERROR;
	}
    } /* endif when record_type == type_b */

    status = BRLCAD_OK;
    return status;
}


/**
 * the output of this function is to decide if the user input scale
 * factor should be used or not and to warn the user of valid scale
 * factors which have side effects on the data
 */
int process_manual_scale_factor(
    double *in_raw_dem_2_raw_dsp_auto_scale_factor_ptr,
    double *in_raw_dem_2_raw_dsp_manual_scale_factor_ptr,
    double *in_derived_dem_max_raw_elevation_ptr,
    double *in_z_spatial_resolution_ptr,
    double *in_datum_elevation_in_curr_b_record_ptr)
{
    long int dem_max_raw_clipped_elevation = 0;
    double dem_max_real_clipped_elevation = 0;
    double raw_dem_2_raw_dsp_manual_scale_factor_lowerlimit = 0;
    double raw_dem_2_raw_dsp_manual_scale_factor_upperlimit = 0;


    /* test manual scale factor if out of valid range */
    raw_dem_2_raw_dsp_manual_scale_factor_lowerlimit = 65535 / 999999;
    raw_dem_2_raw_dsp_manual_scale_factor_upperlimit = 65535;
    if (!((*in_raw_dem_2_raw_dsp_manual_scale_factor_ptr >= raw_dem_2_raw_dsp_manual_scale_factor_lowerlimit) &&
	  (*in_raw_dem_2_raw_dsp_manual_scale_factor_ptr <= raw_dem_2_raw_dsp_manual_scale_factor_upperlimit))) {
	bu_log("Scale factor '%g' was entered. Scale factor must be between '%g' and '%g' inclusive.\n",
	       *in_raw_dem_2_raw_dsp_manual_scale_factor_ptr, raw_dem_2_raw_dsp_manual_scale_factor_lowerlimit,
	       raw_dem_2_raw_dsp_manual_scale_factor_upperlimit);
	return BRLCAD_ERROR;
    }

    if (EQUAL(*in_raw_dem_2_raw_dsp_manual_scale_factor_ptr, *in_raw_dem_2_raw_dsp_auto_scale_factor_ptr)) {
	/* manual scale factor = auto scale
	 * factor. derived_dem_max_raw_elevation is any value
	 * 0-999999
	 */
	bu_log("Entered scale factor '%g' matches the default computed scale factor.\n", *in_raw_dem_2_raw_dsp_manual_scale_factor_ptr);
    } else {
	if (*in_raw_dem_2_raw_dsp_manual_scale_factor_ptr > *in_raw_dem_2_raw_dsp_auto_scale_factor_ptr) {
	    /* clipping.  manual scale factor > auto scale factor
	     * derived_dem_max_raw_elevation is any value 0-999999
	     */
	    dem_max_raw_clipped_elevation = round_closest(65535 / *in_raw_dem_2_raw_dsp_manual_scale_factor_ptr);
	    dem_max_real_clipped_elevation =
		(dem_max_raw_clipped_elevation * *in_z_spatial_resolution_ptr) + *in_datum_elevation_in_curr_b_record_ptr;
	    bu_log("Scale factor '%g' was entered. Scale factors above '%g' cause clipping.\n",
		   *in_raw_dem_2_raw_dsp_manual_scale_factor_ptr, *in_raw_dem_2_raw_dsp_auto_scale_factor_ptr);
	    bu_log("Raw DEM elevations above '%ld' are clipped.\n", dem_max_raw_clipped_elevation);
	    /* real elevations are in milimeters, convert to meters before reporting value to user */
	    bu_log("Real DEM elevations above '%g' meters are clipped.\n",
		   (dem_max_real_clipped_elevation / conversion_factor_to_milimeters[2]));
	} else {
	    /* manual scale factor < auto scale factor */
	    if (*in_derived_dem_max_raw_elevation_ptr > 65535) {
		/* additional loss of resolution.  dem file contains
		 * raw elevations requiring more than a 2 byte integer
		 * to represent therefore an unavoidable amount of
		 * loss of resolution is required. in this case where
		 * manual scale factor < auto scale factor, this
		 * introduces an additional loss of resolution that is
		 * not necessary.
		 */
		bu_log("Scale factor '%g' was entered. Scale factors below '%g' cause additional loss of resolution.\n",
		       *in_raw_dem_2_raw_dsp_manual_scale_factor_ptr, *in_raw_dem_2_raw_dsp_auto_scale_factor_ptr);
	    } else {
		/* derived_dem_max_raw_elevation <= 65535.  dem file
		 * elevations can fit into a 2 byte integer, therefore
		 * no minimun loss of resolution is required to
		 * represent the dem elevations within the dsp format.
		 */
		if (*in_raw_dem_2_raw_dsp_manual_scale_factor_ptr < 1) {
		    /* forced loss of resolution.  manual scale factor
		     * < 1 (and) manual scale factor < auto scale
		     * factor
		     */
		    bu_log("Scale factor '%g' was entered. Scale factors below '1' cause loss of resolution.\n",
			   *in_raw_dem_2_raw_dsp_manual_scale_factor_ptr);
		} else {
		    /* no loss of resolution.  manual scale factor >=
		     * 1 (and) manual scale factor < auto scale factor
		     * under these conditions no loss of resolution
		     * will occur.
		     */
		    bu_log("Scale factor '%g' was entered. No loss of resolution will occur.\n",
			   *in_raw_dem_2_raw_dsp_manual_scale_factor_ptr);
		}
	    }
	}
    }
    return BRLCAD_OK;
}


/**
 * the output of this function is to decide if the user input 'dem max
 * raw elevation' should be used or not and to warn the user of valid
 * max elevations which have side effects on the data
 */
int process_manual_dem_max_raw_elevation(
    double *out_raw_dem_2_raw_dsp_scale_factor_ptr,
    double *UNUSED(in_raw_dem_2_raw_dsp_auto_scale_factor_ptr),
    long int *in_manual_dem_max_raw_elevation_ptr,
    double *in_derived_dem_max_raw_elevation_ptr,
    double *in_z_spatial_resolution_ptr,
    double *in_datum_elevation_in_curr_b_record_ptr)
{
    double dem_max_real_clipped_elevation = 0;
    long int manual_dem_max_raw_elevation_lowerlimit = 1;
    long int manual_dem_max_raw_elevation_upperlimit = 999999;

    /* compute raw_dem_2_raw_dsp_scale_factor based on the user entered */
    /* dem max raw elevation */
    /* test for zero to avoid divide by 0 math error */
    if (round_closest(*in_manual_dem_max_raw_elevation_ptr) > 0) {
	*out_raw_dem_2_raw_dsp_scale_factor_ptr = DSP_MAX_RAW_ELEVATION / (double)*in_manual_dem_max_raw_elevation_ptr;
    } else {
	*out_raw_dem_2_raw_dsp_scale_factor_ptr = 1;
    }

    /* test user input 'dem max raw elevation' if out of valid range */
    if (!((*in_manual_dem_max_raw_elevation_ptr >= manual_dem_max_raw_elevation_lowerlimit) &&
	  (*in_manual_dem_max_raw_elevation_ptr <= manual_dem_max_raw_elevation_upperlimit))) {
	bu_log("DEM max raw elevation '%ld' was entered. DEM max raw elevation must be between '%ld' and '%ld' inclusive.\n",
	       *in_manual_dem_max_raw_elevation_ptr, manual_dem_max_raw_elevation_lowerlimit, manual_dem_max_raw_elevation_upperlimit);
	return BRLCAD_ERROR;
    }

    if (EQUAL(*in_manual_dem_max_raw_elevation_ptr, *in_derived_dem_max_raw_elevation_ptr)) {
	/* user input 'dem max raw elevation' = 'derived dem max raw
	 * elevation'.  the derived raw dem elevation must be derived
	 * from the real elevation listed in the 'a' record.
	 * derived_dem_max_raw_elevation can be any value 0-999999
	 */
	bu_log("Entered DEM max raw elevation '%ld' matches actual DEM max raw elevation.\n", *in_manual_dem_max_raw_elevation_ptr);
    } else {
	if (*in_manual_dem_max_raw_elevation_ptr < *in_derived_dem_max_raw_elevation_ptr) {
	    /* clipping.  user input 'dem max raw elevation' <
	     * 'derived dem max raw elevation'
	     * derived_dem_max_raw_elevation is any value 0-999999
	     */
	    dem_max_real_clipped_elevation = (*in_manual_dem_max_raw_elevation_ptr * *in_z_spatial_resolution_ptr) +
		*in_datum_elevation_in_curr_b_record_ptr;
	    bu_log("DEM max raw elevation '%ld' was entered. Elevations below '%g' cause clipping.\n",
		   *in_manual_dem_max_raw_elevation_ptr, *in_derived_dem_max_raw_elevation_ptr);
	    bu_log("Raw DEM elevations above '%ld' are clipped.\n", *in_manual_dem_max_raw_elevation_ptr);
	    /* real elevations are in milimeters, convert to meters before reporting value to user */
	    bu_log("Real DEM elevations above '%g' meters are clipped.\n",
		   (dem_max_real_clipped_elevation / conversion_factor_to_milimeters[2]));
	} else {
	    /* user input 'dem max raw elevation' > 'derived dem max raw elevation' */
	    if (*in_derived_dem_max_raw_elevation_ptr > 65535) {
		/* additional loss of resolution.  dem file contains
		 * raw elevations requiring more than a 2 byte integer
		 * to represent therefore an unavoidable amount of
		 * loss of resolution is required. in this case where
		 * 'dem max raw elevation' > 'derived dem max raw
		 * elevation', this introduces an additional loss of
		 * resolution that is not necessary.
		 */
		bu_log("DEM max raw elevation '%ld' was entered. Elevations above '%g' cause additional loss of resolution.\n",
		       *in_manual_dem_max_raw_elevation_ptr, *in_derived_dem_max_raw_elevation_ptr);
	    } else {
		/* derived_dem_max_raw_elevation <= 65535 */
		/* dem file elevations can fit into a 2 byte integer, therefore no minimun loss of resolution */
		/* is required to represent the dem elevations within the dsp format. */
		if (*in_manual_dem_max_raw_elevation_ptr > 65535) {
		    /* forced loss of resolution */
		    /* user input 'dem max raw elevation' > 65535 (and) */
		    /* user input 'dem max raw elevation' > 'derived dem max raw elevation' */
		    bu_log("DEM max raw elevation '%ld' was entered. Raw elevations above 65535 cause loss of resolution.\n",
			   *in_manual_dem_max_raw_elevation_ptr);
		} else {
		    /* no loss of resolution */
		    /* user input 'dem max raw elevation' <= 65535 (and) */
		    /* user input 'dem max raw elevation' > 'derived dem max raw elevation' */
		    /* under these conditions no loss of resolution will occur. */
		    bu_log("DEM max raw elevation '%ld' was entered. No loss of resolution will occur.\n",
			   *in_manual_dem_max_raw_elevation_ptr);
		}
	    }
	}
    }
    return BRLCAD_OK;
}


/**
 * the output of this function is to decide if the user input 'dem max
 * real elevation' should be used or not and to warn the user of valid
 * max elevations which have side effects on the data.
 */
int process_manual_dem_max_real_elevation(
    double *out_raw_dem_2_raw_dsp_scale_factor_ptr,
    double *UNUSED(in_raw_dem_2_raw_dsp_auto_scale_factor_ptr),
    double *in_manual_dem_max_real_elevation_ptr,
    double *UNUSED(in_derived_dem_max_raw_elevation_ptr),
    double *in_z_spatial_resolution_ptr,
    double *in_datum_elevation_in_curr_b_record_ptr)
{
    double manual_dem_max_real_elevation_lowerlimit = 0;
    double manual_dem_max_real_elevation_upperlimit = 0;
    double dem_max_raw_elevation = 0;
    double adjusted_manual_dem_max_real_elevation = 0; /* value adjusted to multiple of z_spatial */
    /* resolution then add datum elevation */

    /* makes sure *in_manual_dem_max_real_elevation_ptr >= *in_datum_elevation_in_curr_b_record_ptr */
    manual_dem_max_real_elevation_lowerlimit = *in_z_spatial_resolution_ptr + *in_datum_elevation_in_curr_b_record_ptr;
    manual_dem_max_real_elevation_upperlimit = (999999 * *in_z_spatial_resolution_ptr) + *in_datum_elevation_in_curr_b_record_ptr;

    /* test user input 'dem max real elevation' if out of valid range */
    if (!((*in_manual_dem_max_real_elevation_ptr >= manual_dem_max_real_elevation_lowerlimit) &&
	  (*in_manual_dem_max_real_elevation_ptr <= manual_dem_max_real_elevation_upperlimit))) {
	/* real elevations are processed in the unit milimeters, convert to meters before reporting to user */
	bu_log("DEM max real elevation '%g' meters was entered.\n",
	       (*in_manual_dem_max_real_elevation_ptr / conversion_factor_to_milimeters[2]));
	bu_log("DEM max real elevation must be between '%g' meters and '%g' meters inclusive.\n",
	       (manual_dem_max_real_elevation_lowerlimit / conversion_factor_to_milimeters[2]),
	       (manual_dem_max_real_elevation_upperlimit / conversion_factor_to_milimeters[2]));
	return BRLCAD_ERROR;
    }

    /* manual_dem_max_real_elevation value adjusted to multiple of z_spatial resolution then add datum elevation */
    adjusted_manual_dem_max_real_elevation =
	(round_closest((*in_manual_dem_max_real_elevation_ptr - *in_datum_elevation_in_curr_b_record_ptr) / *in_z_spatial_resolution_ptr) *
	 *in_z_spatial_resolution_ptr) + *in_datum_elevation_in_curr_b_record_ptr;

    /* compute raw elevation that corresponds to the adjusted user input max real elevation */
    dem_max_raw_elevation = (adjusted_manual_dem_max_real_elevation - *in_datum_elevation_in_curr_b_record_ptr) / *in_z_spatial_resolution_ptr;

    /* report to user the actual max real elevation used if not the value the user entered */
    if (!EQUAL(adjusted_manual_dem_max_real_elevation, *in_manual_dem_max_real_elevation_ptr)) {
	/* real elevations are processed in the unit milimeters, convert to meters before reporting to user */
	bu_log("Using max real elevation '%g' meters instead of '%g' meters to allow correct scaling.\n",
	       (adjusted_manual_dem_max_real_elevation / conversion_factor_to_milimeters[2]),
	       (*in_manual_dem_max_real_elevation_ptr / conversion_factor_to_milimeters[2]));
    }

    /* compute raw_dem_2_raw_dsp_scale_factor based on the adjusted, user entered, dem max real elevation */
    /* test for zero to avoid divide by 0 math error */
    if (round_closest(dem_max_raw_elevation) > 0) {
	*out_raw_dem_2_raw_dsp_scale_factor_ptr = DSP_MAX_RAW_ELEVATION / (double)dem_max_raw_elevation;
    } else {
	*out_raw_dem_2_raw_dsp_scale_factor_ptr = 1;
    }
    return BRLCAD_OK;
}


int
read_dem(
    char *in_input_filename,                           /* dem input file path and file name */
    double *in_raw_dem_2_raw_dsp_manual_scale_factor,  /* user specified raw dem-to-dsp scale factor */
    long int *in_manual_dem_max_raw_elevation,         /* user specified max raw elevation */
    double *in_manual_dem_max_real_elevation,          /* user specified max real elevation in meters */
    char *in_temp_filename,                            /* temp file path and file name */
    long int *out_xdim,                                /* x dimension of dem (w cells) */
    long int *out_ydim,                                /* y dimension of dem (n cells) */
    double *out_dsp_elevation,                         /* datum elevation in milimeters (dsp V z coordinate) */
    double *out_x_cell_size,                           /* x scaling factor in milimeters */
    double *out_y_cell_size,                           /* y scaling factor in milimeters */
    double *out_unit_elevation)                        /* z scaling factor in milimeters */
{
    size_t ret;
    int status = BRLCAD_ERROR;
    FILE *fp;
    FILE *fp2;
    char buf[1024];
    long int curr_b_record = 0;
    long int indx2 = 0;
    long int indx3 = 0;
    long int indx4 = 0;
    long int indx5 = 0;
    long int indx6 = 0;
    long int tot_elevations_in_curr_b_record = 0;
    long int tot_elevations_in_previous_b_record = 0;
    long int curr_elevation = 0;
    long int additional_1024char_chunks = 0;
    long int number_of_previous_b_elevations = 0;
    long int number_of_last_elevations = 0;
    long int tot_elevations_in_prev_b_records = 0;
    long int elevation_number_in_curr_b_record = 0;
    long int elevation_number = 0;
    double datum_elevation_in_curr_b_record = 0;
    double datum_elevation_in_previous_b_record = 0;
    double elevation_max_in_a_record = 0;
    double raw_dem_2_raw_dsp_auto_scale_factor = 1;
    long int elevation_units = 0;
    long int ground_units = 0;
    double derived_dem_max_raw_elevation = 0;
    double raw_dem_2_raw_dsp_scale_factor = 0;

    ResultStruct my_out;
    ResultStruct *my_out_ptr;
    my_out_ptr = &my_out;

    /* set defaults for output variables */
    *out_xdim = 0;
    *out_ydim = 0;
    *out_dsp_elevation = 0;
    *out_x_cell_size = 0;
    *out_y_cell_size = 0;
    *out_unit_elevation = 0;

    if ((fp=fopen(in_input_filename, "r")) == NULL) {
	bu_log("Could not open '%s' for read.\n", in_input_filename);
	return BRLCAD_ERROR;
    }
    if ((fp2=fopen(in_temp_filename, "wb")) == NULL) {
	bu_log("Could not open '%s' for write.\n", in_temp_filename);
	fclose(fp);
	return BRLCAD_ERROR;
    }

    /* Reads 1st 1024 character block from dem-g file; this block
     * contains the record 'a' data.
     */
    ret = fread(buf, sizeof(buf[0]), sizeof(buf)/sizeof(buf[0]), fp);
    if (ret < sizeof(buf) / sizeof(buf[0])) {
	bu_log("Failed to read the 'A' record\n");
    }

    /* Validates all 'A' record sub_elements can be read and that all
     * required sub_elements contain values.
     */
    if (validate_dem_record(buf, 1, type_a) == BRLCAD_ERROR) {
	bu_log("The DEM file did not validate, failed on logical record type 'A'.\n");
	fclose(fp);
	fclose(fp2);
	return BRLCAD_ERROR;
    }

    /* read quantity of b type records from a record */
    /* this is also the x dimension of the dsp */
    (*my_out_ptr).in_buffer = buf;
    (*my_out_ptr).in_record_type = type_a;
    (*my_out_ptr).in_element_number = 16;
    (*my_out_ptr).in_sub_element_number = 2;
    status = read_element(my_out_ptr);
    *out_xdim = (*my_out_ptr).out_integer;
    bu_log("total b records in dem file: %ld\n", *out_xdim);

    /* Read elevation units from 'a' record */
    (*my_out_ptr).in_buffer = buf;
    (*my_out_ptr).in_record_type = type_a;
    (*my_out_ptr).in_element_number = 9;
    (*my_out_ptr).in_sub_element_number = 1;
    status = read_element(my_out_ptr);
    elevation_units = (*my_out_ptr).out_integer;
    if (elevation_units == 1) {
	bu_log("elevation units in feet\n");
    }
    if (elevation_units == 2) {
	bu_log("elevation units in meters\n");
    }

    /* Read ground units from 'a' record */
    (*my_out_ptr).in_buffer = buf;
    (*my_out_ptr).in_record_type = type_a;
    (*my_out_ptr).in_element_number = 8;
    (*my_out_ptr).in_sub_element_number = 1;
    status = read_element(my_out_ptr);
    ground_units = (*my_out_ptr).out_integer;
    if (ground_units == 0) {
	bu_log("ground units in radians, unit conversion assumes 1 arc-second = 30 meters\n");
    }
    if (ground_units == 1) {
	bu_log("ground units in feet\n");
    }
    if (ground_units == 2) {
	bu_log("ground units in meters\n");
    }
    if (ground_units == 3) {
	bu_log("ground units in arc-seconds, unit conversion assumes 1 arc-second = 30 meters\n");
    }

    /* Read x spatial resolution from 'a' record. The x spatial
     * resolution is the x cell size of the dsp primative.
     */
    (*my_out_ptr).in_buffer = buf;
    (*my_out_ptr).in_record_type = type_a;
    (*my_out_ptr).in_element_number = 15;
    (*my_out_ptr).in_sub_element_number = 1;
    status = read_element(my_out_ptr);
    *out_x_cell_size = (*my_out_ptr).out_double * conversion_factor_to_milimeters[ground_units];
    bu_log("dsp x cell size (mm): %g\n", *out_x_cell_size);

    /* Read y spatial resolution from 'a' record.  The y spatial
     * resolution is the y cell size of the dsp primative.
     */
    (*my_out_ptr).in_buffer = buf;
    (*my_out_ptr).in_record_type = type_a;
    (*my_out_ptr).in_element_number = 15;
    (*my_out_ptr).in_sub_element_number = 2;
    status = read_element(my_out_ptr);
    *out_y_cell_size = (*my_out_ptr).out_double * conversion_factor_to_milimeters[ground_units];
    bu_log("dsp y cell size (mm): %g\n", *out_y_cell_size);

    /* Read z spatial resolution from 'a' record.  The z spatial
     * resolution is the unit elevation of the dsp primative.
     */
    (*my_out_ptr).in_buffer = buf;
    (*my_out_ptr).in_record_type = type_a;
    (*my_out_ptr).in_element_number = 15;
    (*my_out_ptr).in_sub_element_number = 3;
    status = read_element(my_out_ptr);
    *out_unit_elevation = (*my_out_ptr).out_double * conversion_factor_to_milimeters[elevation_units];
    bu_log("dsp unit elevation (mm): %g\n", *out_unit_elevation);

    /* read elevation max from 'a' record.  this value is the true max
     * elevation adjusted for all factors and is reported in the 'a'
     * record.
     */
    (*my_out_ptr).in_buffer = buf;
    (*my_out_ptr).in_record_type = type_a;
    (*my_out_ptr).in_element_number = 12;
    (*my_out_ptr).in_sub_element_number = 2;
    status = read_element(my_out_ptr);
    elevation_max_in_a_record = (*my_out_ptr).out_double * conversion_factor_to_milimeters[elevation_units];
    bu_log("real world elevation max in dem file (mm): %g\n", elevation_max_in_a_record);

    /* set value to zero before start of tallying */
    tot_elevations_in_prev_b_records = 0;
    for (curr_b_record = 1; curr_b_record <= *out_xdim; curr_b_record++) {

	/* Reads 1024 character block from dem-g file */
	/* this block contains the record 'b' header and data. */
	ret = fread(buf, sizeof(buf[0]), sizeof(buf)/sizeof(buf[0]), fp);
	if (ret < sizeof(buf) / sizeof(buf[0])) {
	    bu_log("Failed to read the 'B' record\n");
	}

	/* Validates all 'B' record header sub_elements can be read
	 * and all required sub_elements contain values.
	 */
	if (validate_dem_record(buf, 1, type_b) == BRLCAD_ERROR) {
	    bu_log("The DEM file did not validate, failed on logical record type 'B' number '%ld'.\n", curr_b_record);
	    fclose(fp);
	    fclose(fp2);
	    return BRLCAD_ERROR;
	}

	/* Saves the number of elevations in the previous b record */
	tot_elevations_in_previous_b_record = tot_elevations_in_curr_b_record;

	/* Read total elevations in current 'b' record from the 'b'
	 * record header currently in the buffer.
	 */
	(*my_out_ptr).in_buffer = buf;
	(*my_out_ptr).in_record_type = type_b;
	(*my_out_ptr).in_element_number = 2;
	(*my_out_ptr).in_sub_element_number = 1;
	status = read_element(my_out_ptr);
	tot_elevations_in_curr_b_record = (*my_out_ptr).out_integer;

	/* Saves the datum elevation from the previous 'b' record */
	datum_elevation_in_previous_b_record = datum_elevation_in_curr_b_record;

	/* Read datum elevation in current 'b' record from the 'b'
	 * record header currently in the buffer.
	 */
	(*my_out_ptr).in_buffer = buf;
	(*my_out_ptr).in_record_type = type_b;
	(*my_out_ptr).in_element_number = 4;
	(*my_out_ptr).in_sub_element_number = 1;
	status = read_element(my_out_ptr);
	datum_elevation_in_curr_b_record = (*my_out_ptr).out_double * conversion_factor_to_milimeters[elevation_units];

	/* Enforces all 'b' records have the same datum elevation and
	 * each 'b' record has the same number of elevations.  The dem
	 * standard allows these values to be different in each 'b'
	 * record but this is not supported by this code.
	 */
	if (curr_b_record > 1) {
	    if (!EQUAL(datum_elevation_in_curr_b_record, datum_elevation_in_previous_b_record)) {
		bu_log("Datum elevation in 'B' record number '%ld' does not match previous 'B' record datum elevations.\n",
		       curr_b_record);
		bu_log("Datum elevation in current b record is: %g\n", datum_elevation_in_curr_b_record);
		bu_log("Datum elevation in previous b record is: %g\n", datum_elevation_in_previous_b_record);
		bu_log("This condition is unsupported, import can not continue.\n");
		fclose(fp);
		fclose(fp2);
		return BRLCAD_ERROR;
	    }
	    if (tot_elevations_in_curr_b_record != tot_elevations_in_previous_b_record) {
		bu_log("Number of elevations in 'B' record number '%ld' does not match previous 'B' record number of elevations.\n",
		       curr_b_record);
		bu_log("The number of elevations in the current b record is: %ld\n", tot_elevations_in_curr_b_record);
		bu_log("The number of elevations in the previous b record is: %ld\n", tot_elevations_in_previous_b_record);
		bu_log("This condition is unsupported, import can not continue.\n");
		fclose(fp);
		fclose(fp2);
		return BRLCAD_ERROR;
	    }
	}

	/* only perform these computations once for the dem file using
	 * only the values from header of the 1st b record it is
	 * assumed the input values will be the same for the remaining
	 * b records, but this should be verified.
	 */
	if (curr_b_record == 1) {
	    /* compute scaling factor to convert raw dem elevation values into raw dsp elevation values */
	    derived_dem_max_raw_elevation = (elevation_max_in_a_record - datum_elevation_in_curr_b_record) / *out_unit_elevation;
	    bu_log("derived_dem_max_raw_elevation: %g\n", derived_dem_max_raw_elevation);

	    /* Test for negative value of
	     * derived_dem_max_raw_elevation if a negative value
	     * occurs, exit import because a fatal inconsistency
	     * exists in the dem data. This test assumes that
	     * *out_unit_elevation is always > 0.
	     */
	    if (derived_dem_max_raw_elevation < 0) {
		bu_log("A fatal inconsistency occured in DEM data.\n");
		bu_log("'B' record datum elevation can not be greater than 'A' record max elevation.\n");
		bu_log("Error occured in 'B' record number '1'.\n");
		bu_log("'A' record max elevation is: %g\n", elevation_max_in_a_record);
		bu_log("'B' record datum elevation is: %g\n", datum_elevation_in_curr_b_record);
		bu_log("Import can not continue.\n");
		fclose(fp);
		fclose(fp2);
		return BRLCAD_ERROR;
	    }

	    /* test for zero to avoid divide by 0 math error */
	    if (!ZERO(derived_dem_max_raw_elevation)) {
		raw_dem_2_raw_dsp_auto_scale_factor = DSP_MAX_RAW_ELEVATION / (double)derived_dem_max_raw_elevation;
	    } else {
		raw_dem_2_raw_dsp_auto_scale_factor = 1;
	    }
	    /* set the scale factor to use the default auto_scale_factor */
	    /* the auto value should be used if user have not specified */
	    /* any custom values. */
	    raw_dem_2_raw_dsp_scale_factor = raw_dem_2_raw_dsp_auto_scale_factor;

	    /* Test to be sure only one user custom value was passed to */
	    /* this function. A non-zero value indicates a value was passed. */
	    /* More than one value defined will cause this function to abort. */
	    if (*in_raw_dem_2_raw_dsp_manual_scale_factor > 0) {
		if (*in_manual_dem_max_raw_elevation > 0) {
		    bu_log("Error occured in function 'read_dem', too many user values passed to this function.\n");
		    return BRLCAD_ERROR;
		}
		if (*in_manual_dem_max_real_elevation > 0) {
		    bu_log("Error occured in function 'read_dem', too many user values passed to this function.\n");
		    return BRLCAD_ERROR;
		}
	    } else {
		if (*in_manual_dem_max_raw_elevation > 0) {
		    if (*in_raw_dem_2_raw_dsp_manual_scale_factor > 0) {
			bu_log("Error occured in function 'read_dem', too many user values passed to this function.\n");
			return BRLCAD_ERROR;
		    }
		    if (*in_manual_dem_max_real_elevation > 0) {
			bu_log("Error occured in function 'read_dem', too many user values passed to this function.\n");
			return BRLCAD_ERROR;
		    }
		} else {
		    if (*in_manual_dem_max_real_elevation > 0) {
			if (*in_raw_dem_2_raw_dsp_manual_scale_factor > 0) {
			    bu_log("Error occured in function 'read_dem', too many user values passed to this function.\n");
			    return BRLCAD_ERROR;
			}
			if (*in_manual_dem_max_raw_elevation > 0) {
			    bu_log("Error occured in function 'read_dem', too many user values passed to this function.\n");
			    return BRLCAD_ERROR;
			}
		    }
		}
	    }

	    /* Test for user entered raw scale factor, if so, process value. */
	    /* Test if value is within a valid range and warn user if a chosen */
	    /* valid value will produce less than optimal results. */
	    if (*in_raw_dem_2_raw_dsp_manual_scale_factor > 0) {
		raw_dem_2_raw_dsp_scale_factor = *in_raw_dem_2_raw_dsp_manual_scale_factor;
		status = process_manual_scale_factor(&raw_dem_2_raw_dsp_auto_scale_factor,
						     in_raw_dem_2_raw_dsp_manual_scale_factor, &derived_dem_max_raw_elevation, out_unit_elevation,
						     &datum_elevation_in_curr_b_record);
		if (status == BRLCAD_ERROR) {
		    /* problem encountered processing custom value for scale factor, */
		    /* or value entered was not within the valid range. */
		    fclose(fp);
		    fclose(fp2);
		    return BRLCAD_ERROR;
		}
	    }

	    /* Test for user entered max raw elevation, if so, process value. */
	    /* Test if value is within a valid range and warn user if a chosen */
	    /* valid value will produce less than optimal results. */
	    /* The value 'raw_dem_2_raw_dsp_scale_factor' is output from this function. */
	    if (*in_manual_dem_max_raw_elevation > 0) {
		status = process_manual_dem_max_raw_elevation(
		    &raw_dem_2_raw_dsp_scale_factor,
		    &raw_dem_2_raw_dsp_auto_scale_factor,
		    in_manual_dem_max_raw_elevation,
		    &derived_dem_max_raw_elevation,
		    out_unit_elevation,
		    &datum_elevation_in_curr_b_record);
		if (status == BRLCAD_ERROR) {
		    /* problem encountered processing custom value for dem max raw elevation, */
		    /* or value entered was not within the valid range. */
		    fclose(fp);
		    fclose(fp2);
		    return BRLCAD_ERROR;
		}
	    }

	    /* Test for user entered max real elevation, if so, process value */
	    /* Test if value is within a valid range. */
	    /* The value 'raw_dem_2_raw_dsp_scale_factor' is output from this function */
	    if (*in_manual_dem_max_real_elevation > 0) {
		/* assumes user input max real elevation in meters, so convert to milimeters */
		*in_manual_dem_max_real_elevation = *in_manual_dem_max_real_elevation * conversion_factor_to_milimeters[2];
		status = process_manual_dem_max_real_elevation(
		    &raw_dem_2_raw_dsp_scale_factor,
		    &raw_dem_2_raw_dsp_auto_scale_factor,
		    in_manual_dem_max_real_elevation,
		    &derived_dem_max_raw_elevation,
		    out_unit_elevation,
		    &datum_elevation_in_curr_b_record);
		if (status == BRLCAD_ERROR) {
		    /* problem encountered processing custom value for dem max real elevation, */
		    /* or value entered was not within the valid range. */
		    fclose(fp);
		    fclose(fp2);
		    return BRLCAD_ERROR;
		}
	    }


	    /* compute dsp primative 'unit elevation' value */
	    *out_unit_elevation = *out_unit_elevation / raw_dem_2_raw_dsp_scale_factor;
	    bu_log("Computed dsp unit elevation, input this into brl-cad (mm): %g\n", *out_unit_elevation);

	    /* It is assumed all 'b' records will have the same number of elevations. */
	    /* This assumption is enforced elsewhere in this code. Therefore the number */
	    /* of elevations in the 1st 'b' record can be used as the y dimension of the dsp. */
	    *out_ydim = tot_elevations_in_curr_b_record;
	    bu_log("Number of elevations in each 'b' record, also the number of rows in dsp: %ld\n", *out_ydim);


	    /* It is assumed all 'b' records will have the same datum elevation. */
	    /* This assumption is enforced elsewhere in this code. Therefore the */
	    /* datum elevation in the 1st 'b' record can be used as the dsp elevation. */
	    /* CONVERT TO MM */
	    *out_dsp_elevation = datum_elevation_in_curr_b_record;


	} /* endif when curr_b_record == 1 */

	if (tot_elevations_in_curr_b_record > 146) {
	    /* process all 146 b_elevations from current 1024_char chunk */
	    for (indx3 = 1; indx3 <= 146; indx3++) {
		/* loop thru 146 elevations in b type buffer with b header */
		/* read total elevations in current b record from the */
		/* b record header currently in the buffer */
		(*my_out_ptr).in_buffer = buf;
		(*my_out_ptr).in_record_type = type_b;
		(*my_out_ptr).in_element_number = 6;
		(*my_out_ptr).in_sub_element_number = indx3;
		(*my_out_ptr).in_is_b_header = 1;
		status = read_element(my_out_ptr);
		curr_elevation = (*my_out_ptr).out_integer;

		elevation_number_in_curr_b_record = indx3;
		elevation_number = tot_elevations_in_prev_b_records + elevation_number_in_curr_b_record;

		if (curr_elevation < 0) {
		    bu_log("WARNING: Invalid elevation on 'b' record# '%ld', record elevation# '%ld', dem elevation# '%ld', raw elevation value '%ld'. Set elevation value to zero to compensate.\n", curr_b_record, elevation_number_in_curr_b_record, elevation_number, curr_elevation);
		    curr_elevation = 0;
		}

		if (output_elevation((long int)round_closest(curr_elevation * raw_dem_2_raw_dsp_scale_factor), fp2) == BRLCAD_ERROR) {
		    bu_log("Function 'output_elevation' failed on 'b' record# '%ld', record elevation# '%ld', dem elevation# '%ld', raw elevation value '%ld'.\n", curr_b_record, elevation_number_in_curr_b_record, elevation_number, curr_elevation);
		    fclose(fp);
		    fclose(fp2);
		    return BRLCAD_ERROR;
		}
	    }
	    additional_1024char_chunks = (long int)ceil((tot_elevations_in_curr_b_record - 146.0) / 170.0);

	    if (additional_1024char_chunks > 0) {
		for (indx2 = 1; indx2 < additional_1024char_chunks; indx2++) {
		    /* no equal used in condition here because don't want to process last chunk in */
		    /* since the last chunk is a partial chunk */

		    ret = fread(buf, sizeof(buf[0]), sizeof(buf)/sizeof(buf[0]), fp);
		    if (ret < sizeof(buf) / sizeof(buf[0])) {
			bu_log("Failed to read elevation chunk\n");
		    }

		    for (indx5 = 1; indx5 <= 170; indx5++) {
			/* loop thru 170 elevations in b type buffer with no b header */
			/* read total elevations in current b record from the */
			/* b record header currently in the buffer */
			(*my_out_ptr).in_buffer = buf;
			(*my_out_ptr).in_record_type = type_b;
			(*my_out_ptr).in_element_number = 6;
			(*my_out_ptr).in_sub_element_number = indx5;
			(*my_out_ptr).in_is_b_header = 0;
			status = read_element(my_out_ptr);
			curr_elevation = (*my_out_ptr).out_integer;

			elevation_number_in_curr_b_record = (146 + ((indx2-1)*170)) + indx5;
			elevation_number = tot_elevations_in_prev_b_records + elevation_number_in_curr_b_record;

			if (curr_elevation < 0) {
			    bu_log("WARNING: Invalid elevation on 'b' record# '%ld', record elevation# '%ld', dem elevation# '%ld', raw elevation value '%ld'. Set elevation value to zero to compensate.\n", curr_b_record, elevation_number_in_curr_b_record, elevation_number, curr_elevation);
			    curr_elevation = 0;
			}

			if (output_elevation((long int)round_closest(curr_elevation * raw_dem_2_raw_dsp_scale_factor), fp2) == BRLCAD_ERROR) {
			    bu_log("Function 'output_elevation' failed on 'b' record# '%ld', record elevation# '%ld', dem elevation# '%ld', raw elevation value '%ld'.\n", curr_b_record, elevation_number_in_curr_b_record, elevation_number, curr_elevation);
			    fclose(fp);
			    fclose(fp2);
			    return BRLCAD_ERROR;
			}
		    }
		}
		/* process last elevation chunk of b record */
		/* determine # elevations in last 1024 char_chunk */
		number_of_previous_b_elevations = 146 + ((additional_1024char_chunks - 1)*170);
		number_of_last_elevations = tot_elevations_in_curr_b_record - number_of_previous_b_elevations;

		ret = fread(buf, sizeof(buf[0]), sizeof(buf)/sizeof(buf[0]), fp);
		if (ret < sizeof(buf) / sizeof(buf[0])) {
		    bu_log("Failed to read input chunk\n");
		}

		for (indx6 = 1; indx6 <= number_of_last_elevations; indx6++) {
		    /* loop thru remaining elevations in b type buffer with no b header */
		    /* b record header currently in the buffer */
		    (*my_out_ptr).in_buffer = buf;
		    (*my_out_ptr).in_record_type = type_b;
		    (*my_out_ptr).in_element_number = 6;
		    (*my_out_ptr).in_sub_element_number = indx6;
		    (*my_out_ptr).in_is_b_header = 0;
		    status = read_element(my_out_ptr);
		    curr_elevation = (*my_out_ptr).out_integer;

		    elevation_number_in_curr_b_record = number_of_previous_b_elevations + indx6;
		    elevation_number = tot_elevations_in_prev_b_records + elevation_number_in_curr_b_record;

		    if (curr_elevation < 0) {
			bu_log("WARNING: Invalid elevation on 'b' record# '%ld', record elevation# '%ld', dem elevation# '%ld', raw elevation value '%ld'. Set elevation value to zero to compensate.\n", curr_b_record, elevation_number_in_curr_b_record, elevation_number, curr_elevation);
			curr_elevation = 0;
		    }

		    if (output_elevation((long int)round_closest(curr_elevation * raw_dem_2_raw_dsp_scale_factor), fp2) == BRLCAD_ERROR) {
			bu_log("Function 'output_elevation' failed on 'b' record# '%ld', record elevation# '%ld', dem elevation# '%ld', raw elevation value '%ld'.\n", curr_b_record, elevation_number_in_curr_b_record, elevation_number, curr_elevation);
			fclose(fp);
			fclose(fp2);
			return BRLCAD_ERROR;
		    }
		}
	    }
	} else {
	    /* number of total elevations in the b record was less than 146 */
	    for (indx4 = 1; indx4 <= tot_elevations_in_curr_b_record; indx4++) {
		/* loop thru elevations in b type buffer with b header */
		/* b record header currently in the buffer */
		(*my_out_ptr).in_buffer = buf;
		(*my_out_ptr).in_record_type = type_b;
		(*my_out_ptr).in_element_number = 6;
		(*my_out_ptr).in_sub_element_number = indx4;
		(*my_out_ptr).in_is_b_header = 1;
		status = read_element(my_out_ptr);
		curr_elevation = (*my_out_ptr).out_integer;

		elevation_number_in_curr_b_record = indx4;
		elevation_number = tot_elevations_in_prev_b_records + elevation_number_in_curr_b_record;

		if (curr_elevation < 0) {
		    bu_log("WARNING: Invalid elevation on 'b' record# '%ld', record elevation# '%ld', dem elevation# '%ld', raw elevation value '%ld'. Set elevation value to zero to compensate.\n", curr_b_record, elevation_number_in_curr_b_record, elevation_number, curr_elevation);
		    curr_elevation = 0;
		}

		if (output_elevation((long int)round_closest(curr_elevation * raw_dem_2_raw_dsp_scale_factor), fp2) == BRLCAD_ERROR) {
		    bu_log("Function 'output_elevation' failed on 'b' record# '%ld', record elevation# '%ld', dem elevation# '%ld', raw elevation value '%ld'.\n", curr_b_record, elevation_number_in_curr_b_record, elevation_number, curr_elevation);
		    fclose(fp);
		    fclose(fp2);
		    return BRLCAD_ERROR;
		}
	    }
	}
	tot_elevations_in_prev_b_records = tot_elevations_in_prev_b_records + tot_elevations_in_curr_b_record;
    } /* outer for loop using curr_b_record */


    fclose(fp);
    fclose(fp2);

    return status;
}

/* convert 'load-by-column to load-by-row' */
int
convert_load_order(
    char *in_temp_filename,        /* temp file path and file name */
    char *in_dsp_output_filename,  /* dsp output file path and file name */
    long int *in_xdim,      /* x dimension of dem (w cells) */
    long int *in_ydim)      /* y dimension of dem (n cells) */
{
    FILE *fp3;
    FILE *fp4;
    long int offset = 0;
    long int column = 0;
    unsigned short int *buf3 = NULL;
    unsigned short int buf4 = 0;
    size_t ret;

    buf3 = bu_calloc(*in_ydim, sizeof(unsigned short int), "buf3");

    if ((fp4=fopen(in_dsp_output_filename, "wb")) == NULL) {
	bu_log("Could not open '%s' for write.\n", in_dsp_output_filename);
	bu_free(buf3, "buf3");
	return BRLCAD_ERROR;
    }
    for (offset = 0; offset <= *in_ydim-1; offset++) {
	if ((fp3=fopen(in_temp_filename, "rb")) == NULL) {
	    bu_log("Could not open '%s' for read.\n", in_temp_filename);
	    fclose(fp4);
	    bu_free(buf3, "buf3");
	    return BRLCAD_ERROR;
	}
	for (column = 1; column <= *in_xdim; column++) {
	    ret = fread(buf3, sizeof(unsigned short int), sizeof(unsigned short int) * (*in_ydim)/sizeof(unsigned short int), fp3);
	    if (ret < sizeof(unsigned short int) * (*in_ydim)/sizeof(unsigned short int)) {
		bu_log("Failed to read input chunk\n");
	    }
	    buf4 = *(buf3 + offset);
	    ret = fwrite(&buf4, sizeof(buf4), 1, fp4);
	    if (ret < 1) {
		bu_log("Failed to write to output file\n");
	    }
	}
	fclose(fp3);
    }
    fclose(fp4);
    bu_free(buf3, "buf3");

    return BRLCAD_OK;
}

int
create_model(
    char *in_dsp_output_filename,    /* dsp output file path and file name */
    char *in_model_output_filename,  /* model output file path and file name */
    long int *in_xdim,               /* x dimension of dem (w cells) */
    long int *in_ydim,               /* y dimension of dem (n cells) */
    double *in_dsp_elevation,        /* datum elevation in milimeters (dsp V z coordinate) */
    double *in_x_cell_size,          /* x scaling factor in milimeters */
    double *in_y_cell_size,          /* y scaling factor in milimeters */
    double *in_unit_elevation)       /* z scaling factor in milimeters */
{
    struct rt_wdb *db_fp;
    fastf_t dsp_mat[ELEMENTS_PER_MAT];

    if ((db_fp = wdb_fopen(in_model_output_filename)) == NULL) {
	perror(in_model_output_filename);
	return BRLCAD_ERROR;
    }

    mk_id(db_fp, "My Database"); /* create the database header record */

    /* all units in the database file are stored in millimeters.  This
     * constrains the arguments to the mk_* routines to also be in
     * millimeters
     */

    /*
     * create dsp
     */

    dsp_mat[0] = *in_x_cell_size;      /* X scale factor in mm */
    dsp_mat[1] = 0;
    dsp_mat[2] = 0;
    dsp_mat[3] = 0;                    /* X coordinate of dsp V in mm */

    dsp_mat[4] = 0;
    dsp_mat[5] = *in_y_cell_size;      /* Y scale factor in mm */
    dsp_mat[6] = 0;
    dsp_mat[7] = 0;                    /* Y coordinate of dsp V in mm */

    dsp_mat[8] = 0;
    dsp_mat[9] = 0;
    dsp_mat[10] = *in_unit_elevation;  /* unit elevation in mm, Z scale factor in mm */
    dsp_mat[11] = *in_dsp_elevation ;  /* Z coordinate of dsp V in mm */

    dsp_mat[12] = 0;
    dsp_mat[13] = 0;
    dsp_mat[14] = 0;
    dsp_mat[15] = 1;

    mk_dsp(db_fp, "dsp.s", in_dsp_output_filename, *in_xdim, *in_ydim, dsp_mat);

    wdb_close(db_fp);

    return BRLCAD_OK;
}


int
main(int ac, char *av[])
{
    double raw_dem_2_raw_dsp_manual_scale_factor = 0;  /* user specified raw dem-to-dsp scale factor */
    long int manual_dem_max_raw_elevation = 0;         /* user specified max raw elevation */
    double manual_dem_max_real_elevation = 0;          /* user specified max real elevation in meters */
    long int xdim = 0;                                 /* x dimension of dem (w cells) */
    long int ydim = 0;                                 /* y dimension of dem (n cells) */
    double dsp_elevation = 0;                          /* datum elevation in milimeters (dsp V z coordinate) */
    double x_cell_size = 0;                            /* x scaling factor in milimeters */
    double y_cell_size = 0;                            /* y scaling factor in milimeters */
    double unit_elevation = 0;                         /* z scaling factor in milimeters */
    int string_length = 0;

    char *temp_filename;           /* temp file path and file name */
    char *input_filename;          /* dem input file path and file name */
    char *dsp_output_filename;     /* dsp output file path and file name */
    char *model_output_filename;   /* model output file path and file name */

    /*
     * element_counts[]
     * element_counts[record_type] = number of elements in each record type
     * record_type=1 are 'a' type records
     * record_type=2 are 'b' type records
     * record_type=3 are 'c' type records
     */
    element_counts[type_a] = 31;
    element_counts[type_b] = 5;
    element_counts[type_c] = 6;

    /*
     * element_attributes[][][][]
     * element_attributes[record_type][element_number][sub_element_number][1] = start byte of sub_element within 1024 character buffer
     * element_attributes[record_type][element_number][sub_element_number][2] = sub_element ascii character width
     * element_attributes[record_type][element_number][sub_element_number][3] = sub_element data_type
     * record_type=1 are 'a' type records
     * record_type=2 are 'b' type records
     * record_type=3 are 'c' type records
     * data_type=1 alpha
     * data_type=2 integer
     * data_type=3 double
     */
    element_attributes[type_a][1][1][1] = 1;
    element_attributes[type_a][1][1][2] = 40;
    element_attributes[type_a][1][1][3] = 1;
    element_attributes[type_a][1][2][1] = 41;
    element_attributes[type_a][1][2][2] = 40;
    element_attributes[type_a][1][2][3] = 1;
    element_attributes[type_a][1][3][1] = 81;
    element_attributes[type_a][1][3][2] = 29;
    element_attributes[type_a][1][3][3] = 1;
    element_attributes[type_a][1][4][1] = 110;
    element_attributes[type_a][1][4][2] = 4;
    element_attributes[type_a][1][4][3] = 2;
    element_attributes[type_a][1][5][1] = 114;
    element_attributes[type_a][1][5][2] = 2;
    element_attributes[type_a][1][5][3] = 2;
    element_attributes[type_a][1][6][1] = 116;
    element_attributes[type_a][1][6][2] = 7;
    element_attributes[type_a][1][6][3] = 3;
    element_attributes[type_a][1][7][1] = 123;
    element_attributes[type_a][1][7][2] = 4;
    element_attributes[type_a][1][7][3] = 2;
    element_attributes[type_a][1][8][1] = 127;
    element_attributes[type_a][1][8][2] = 2;
    element_attributes[type_a][1][8][3] = 2;
    element_attributes[type_a][1][9][1] = 129;
    element_attributes[type_a][1][9][2] = 7;
    element_attributes[type_a][1][9][3] = 3;
    element_attributes[type_a][1][10][1] = 136;
    element_attributes[type_a][1][10][2] = 1;
    element_attributes[type_a][1][10][3] = 1;
    element_attributes[type_a][1][11][1] = 137;
    element_attributes[type_a][1][11][2] = 1;
    element_attributes[type_a][1][11][3] = 1;
    element_attributes[type_a][1][12][1] = 138;
    element_attributes[type_a][1][12][2] = 3;
    element_attributes[type_a][1][12][3] = 1;
    element_attributes[type_a][2][1][1] = 141;
    element_attributes[type_a][2][1][2] = 4;
    element_attributes[type_a][2][1][3] = 1;
    element_attributes[type_a][3][1][1] = 145;
    element_attributes[type_a][3][1][2] = 6;
    element_attributes[type_a][3][1][3] = 2;
    element_attributes[type_a][4][1][1] = 151;
    element_attributes[type_a][4][1][2] = 6;
    element_attributes[type_a][4][1][3] = 2;
    element_attributes[type_a][5][1][1] = 157;
    element_attributes[type_a][5][1][2] = 6;
    element_attributes[type_a][5][1][3] = 2;
    element_attributes[type_a][6][1][1] = 163;
    element_attributes[type_a][6][1][2] = 6;
    element_attributes[type_a][6][1][3] = 2;
    element_attributes[type_a][7][1][1] = 169;
    element_attributes[type_a][7][1][2] = 24;
    element_attributes[type_a][7][1][3] = 3;
    element_attributes[type_a][7][2][1] = 193;
    element_attributes[type_a][7][2][2] = 24;
    element_attributes[type_a][7][2][3] = 3;
    element_attributes[type_a][7][3][1] = 217;
    element_attributes[type_a][7][3][2] = 24;
    element_attributes[type_a][7][3][3] = 3;
    element_attributes[type_a][7][4][1] = 241;
    element_attributes[type_a][7][4][2] = 24;
    element_attributes[type_a][7][4][3] = 3;
    element_attributes[type_a][7][5][1] = 265;
    element_attributes[type_a][7][5][2] = 24;
    element_attributes[type_a][7][5][3] = 3;
    element_attributes[type_a][7][6][1] = 289;
    element_attributes[type_a][7][6][2] = 24;
    element_attributes[type_a][7][6][3] = 3;
    element_attributes[type_a][7][7][1] = 313;
    element_attributes[type_a][7][7][2] = 24;
    element_attributes[type_a][7][7][3] = 3;
    element_attributes[type_a][7][8][1] = 337;
    element_attributes[type_a][7][8][2] = 24;
    element_attributes[type_a][7][8][3] = 3;
    element_attributes[type_a][7][9][1] = 361;
    element_attributes[type_a][7][9][2] = 24;
    element_attributes[type_a][7][9][3] = 3;
    element_attributes[type_a][7][10][1] = 385;
    element_attributes[type_a][7][10][2] = 24;
    element_attributes[type_a][7][10][3] = 3;
    element_attributes[type_a][7][11][1] = 409;
    element_attributes[type_a][7][11][2] = 24;
    element_attributes[type_a][7][11][3] = 3;
    element_attributes[type_a][7][12][1] = 433;
    element_attributes[type_a][7][12][2] = 24;
    element_attributes[type_a][7][12][3] = 3;
    element_attributes[type_a][7][13][1] = 457;
    element_attributes[type_a][7][13][2] = 24;
    element_attributes[type_a][7][13][3] = 3;
    element_attributes[type_a][7][14][1] = 481;
    element_attributes[type_a][7][14][2] = 24;
    element_attributes[type_a][7][14][3] = 3;
    element_attributes[type_a][7][15][1] = 505;
    element_attributes[type_a][7][15][2] = 24;
    element_attributes[type_a][7][15][3] = 3;
    element_attributes[type_a][8][1][1] = 529;
    element_attributes[type_a][8][1][2] = 6;
    element_attributes[type_a][8][1][3] = 2;
    element_attributes[type_a][9][1][1] = 535;
    element_attributes[type_a][9][1][2] = 6;
    element_attributes[type_a][9][1][3] = 2;
    element_attributes[type_a][10][1][1] = 541;
    element_attributes[type_a][10][1][2] = 6;
    element_attributes[type_a][10][1][3] = 2;
    element_attributes[type_a][11][1][1] = 547;
    element_attributes[type_a][11][1][2] = 24;
    element_attributes[type_a][11][1][3] = 3;
    element_attributes[type_a][11][2][1] = 571;
    element_attributes[type_a][11][2][2] = 24;
    element_attributes[type_a][11][2][3] = 3;
    element_attributes[type_a][11][3][1] = 595;
    element_attributes[type_a][11][3][2] = 24;
    element_attributes[type_a][11][3][3] = 3;
    element_attributes[type_a][11][4][1] = 619;
    element_attributes[type_a][11][4][2] = 24;
    element_attributes[type_a][11][4][3] = 3;
    element_attributes[type_a][11][5][1] = 643;
    element_attributes[type_a][11][5][2] = 24;
    element_attributes[type_a][11][5][3] = 3;
    element_attributes[type_a][11][6][1] = 667;
    element_attributes[type_a][11][6][2] = 24;
    element_attributes[type_a][11][6][3] = 3;
    element_attributes[type_a][11][7][1] = 691;
    element_attributes[type_a][11][7][2] = 24;
    element_attributes[type_a][11][7][3] = 3;
    element_attributes[type_a][11][8][1] = 715;
    element_attributes[type_a][11][8][2] = 24;
    element_attributes[type_a][11][8][3] = 3;
    element_attributes[type_a][12][1][1] = 739;
    element_attributes[type_a][12][1][2] = 24;
    element_attributes[type_a][12][1][3] = 3;
    element_attributes[type_a][12][2][1] = 763;
    element_attributes[type_a][12][2][2] = 24;
    element_attributes[type_a][12][2][3] = 3;
    element_attributes[type_a][13][1][1] = 787;
    element_attributes[type_a][13][1][2] = 24;
    element_attributes[type_a][13][1][3] = 3;
    element_attributes[type_a][14][1][1] = 811;
    element_attributes[type_a][14][1][2] = 6;
    element_attributes[type_a][14][1][3] = 2;
    element_attributes[type_a][15][1][1] = 817;
    element_attributes[type_a][15][1][2] = 12;
    element_attributes[type_a][15][1][3] = 3;
    element_attributes[type_a][15][2][1] = 829;
    element_attributes[type_a][15][2][2] = 12;
    element_attributes[type_a][15][2][3] = 3;
    element_attributes[type_a][15][3][1] = 841;
    element_attributes[type_a][15][3][2] = 12;
    element_attributes[type_a][15][3][3] = 3;
    element_attributes[type_a][16][1][1] = 853;
    element_attributes[type_a][16][1][2] = 6;
    element_attributes[type_a][16][1][3] = 2;
    element_attributes[type_a][16][2][1] = 859;
    element_attributes[type_a][16][2][2] = 6;
    element_attributes[type_a][16][2][3] = 2;
    element_attributes[type_a][17][1][1] = 865;
    element_attributes[type_a][17][1][2] = 5;
    element_attributes[type_a][17][1][3] = 2;
    element_attributes[type_a][18][1][1] = 870;
    element_attributes[type_a][18][1][2] = 1;
    element_attributes[type_a][18][1][3] = 2;
    element_attributes[type_a][19][1][1] = 871;
    element_attributes[type_a][19][1][2] = 5;
    element_attributes[type_a][19][1][3] = 2;
    element_attributes[type_a][20][1][1] = 876;
    element_attributes[type_a][20][1][2] = 1;
    element_attributes[type_a][20][1][3] = 2;
    element_attributes[type_a][21][1][1] = 877;
    element_attributes[type_a][21][1][2] = 4;
    element_attributes[type_a][21][1][3] = 2;
    element_attributes[type_a][22][1][1] = 881;
    element_attributes[type_a][22][1][2] = 4;
    element_attributes[type_a][22][1][3] = 2;
    element_attributes[type_a][23][1][1] = 885;
    element_attributes[type_a][23][1][2] = 1;
    element_attributes[type_a][23][1][3] = 1;
    element_attributes[type_a][24][1][1] = 886;
    element_attributes[type_a][24][1][2] = 1;
    element_attributes[type_a][24][1][3] = 2;
    element_attributes[type_a][25][1][1] = 887;
    element_attributes[type_a][25][1][2] = 2;
    element_attributes[type_a][25][1][3] = 2;
    element_attributes[type_a][26][1][1] = 889;
    element_attributes[type_a][26][1][2] = 2;
    element_attributes[type_a][26][1][3] = 2;
    element_attributes[type_a][27][1][1] = 891;
    element_attributes[type_a][27][1][2] = 2;
    element_attributes[type_a][27][1][3] = 2;
    element_attributes[type_a][28][1][1] = 893;
    element_attributes[type_a][28][1][2] = 4;
    element_attributes[type_a][28][1][3] = 2;
    element_attributes[type_a][29][1][1] = 897;
    element_attributes[type_a][29][1][2] = 4;
    element_attributes[type_a][29][1][3] = 2;
    element_attributes[type_a][30][1][1] = 901;
    element_attributes[type_a][30][1][2] = 2;
    element_attributes[type_a][30][1][3] = 2;
    element_attributes[type_a][30][2][1] = 903;
    element_attributes[type_a][30][2][2] = 2;
    element_attributes[type_a][30][2][3] = 2;
    element_attributes[type_a][30][3][1] = 905;
    element_attributes[type_a][30][3][2] = 2;
    element_attributes[type_a][30][3][3] = 2;
    element_attributes[type_a][30][4][1] = 907;
    element_attributes[type_a][30][4][2] = 2;
    element_attributes[type_a][30][4][3] = 2;
    element_attributes[type_a][31][1][1] = 909;
    element_attributes[type_a][31][1][2] = 7;
    element_attributes[type_a][31][1][3] = 3;
    element_attributes[type_b][1][1][1] = 1;
    element_attributes[type_b][1][1][2] = 6;
    element_attributes[type_b][1][1][3] = 2;
    element_attributes[type_b][1][2][1] = 7;
    element_attributes[type_b][1][2][2] = 6;
    element_attributes[type_b][1][2][3] = 2;
    element_attributes[type_b][2][1][1] = 13;
    element_attributes[type_b][2][1][2] = 6;
    element_attributes[type_b][2][1][3] = 2;
    element_attributes[type_b][2][2][1] = 19;
    element_attributes[type_b][2][2][2] = 6;
    element_attributes[type_b][2][2][3] = 2;
    element_attributes[type_b][3][1][1] = 25;
    element_attributes[type_b][3][1][2] = 24;
    element_attributes[type_b][3][1][3] = 3;
    element_attributes[type_b][3][2][1] = 49;
    element_attributes[type_b][3][2][2] = 24;
    element_attributes[type_b][3][2][3] = 3;
    element_attributes[type_b][4][1][1] = 73;
    element_attributes[type_b][4][1][2] = 24;
    element_attributes[type_b][4][1][3] = 3;
    element_attributes[type_b][5][1][1] = 97;
    element_attributes[type_b][5][1][2] = 24;
    element_attributes[type_b][5][1][3] = 3;
    element_attributes[type_b][5][2][1] = 121;
    element_attributes[type_b][5][2][2] = 24;
    element_attributes[type_b][5][2][3] = 3;
    element_attributes[type_c][1][1][1] = 1;
    element_attributes[type_c][1][1][2] = 6;
    element_attributes[type_c][1][1][3] = 2;
    element_attributes[type_c][2][1][1] = 7;
    element_attributes[type_c][2][1][2] = 6;
    element_attributes[type_c][2][1][3] = 2;
    element_attributes[type_c][2][2][1] = 13;
    element_attributes[type_c][2][2][2] = 6;
    element_attributes[type_c][2][2][3] = 2;
    element_attributes[type_c][2][3][1] = 19;
    element_attributes[type_c][2][3][2] = 6;
    element_attributes[type_c][2][3][3] = 2;
    element_attributes[type_c][3][1][1] = 25;
    element_attributes[type_c][3][1][2] = 6;
    element_attributes[type_c][3][1][3] = 2;
    element_attributes[type_c][4][1][1] = 31;
    element_attributes[type_c][4][1][2] = 6;
    element_attributes[type_c][4][1][3] = 2;
    element_attributes[type_c][5][1][1] = 37;
    element_attributes[type_c][5][1][2] = 6;
    element_attributes[type_c][5][1][3] = 2;
    element_attributes[type_c][5][2][1] = 43;
    element_attributes[type_c][5][2][2] = 6;
    element_attributes[type_c][5][2][3] = 2;
    element_attributes[type_c][5][3][1] = 49;
    element_attributes[type_c][5][3][2] = 6;
    element_attributes[type_c][5][3][3] = 2;
    element_attributes[type_c][6][1][1] = 55;
    element_attributes[type_c][6][1][2] = 6;
    element_attributes[type_c][6][1][3] = 2;

    /*
     * sub_element_counts[][]
     * sub_element_counts[record_type][element_number] = number of sub_elements within element
     * record_type=1 are 'a' type records
     * record_type=2 are 'b' type records
     * record_type=3 are 'c' type records
     */
    sub_element_counts[type_a][1] = 12;
    sub_element_counts[type_a][2] = 1;
    sub_element_counts[type_a][3] = 1;
    sub_element_counts[type_a][4] = 1;
    sub_element_counts[type_a][5] = 1;
    sub_element_counts[type_a][6] = 1;
    sub_element_counts[type_a][7] = 15;
    sub_element_counts[type_a][8] = 1;
    sub_element_counts[type_a][9] = 1;
    sub_element_counts[type_a][10] = 1;
    sub_element_counts[type_a][11] = 8;
    sub_element_counts[type_a][12] = 2;
    sub_element_counts[type_a][13] = 1;
    sub_element_counts[type_a][14] = 1;
    sub_element_counts[type_a][15] = 3;
    sub_element_counts[type_a][16] = 2;
    sub_element_counts[type_a][17] = 1;
    sub_element_counts[type_a][18] = 1;
    sub_element_counts[type_a][19] = 1;
    sub_element_counts[type_a][20] = 1;
    sub_element_counts[type_a][21] = 1;
    sub_element_counts[type_a][22] = 1;
    sub_element_counts[type_a][23] = 1;
    sub_element_counts[type_a][24] = 1;
    sub_element_counts[type_a][25] = 1;
    sub_element_counts[type_a][26] = 1;
    sub_element_counts[type_a][27] = 1;
    sub_element_counts[type_a][28] = 1;
    sub_element_counts[type_a][29] = 1;
    sub_element_counts[type_a][30] = 4;
    sub_element_counts[type_a][31] = 1;
    sub_element_counts[type_b][1] = 2;
    sub_element_counts[type_b][2] = 2;
    sub_element_counts[type_b][3] = 2;
    sub_element_counts[type_b][4] = 1;
    sub_element_counts[type_b][5] = 2;
    sub_element_counts[type_c][1] = 1;
    sub_element_counts[type_c][2] = 3;
    sub_element_counts[type_c][3] = 1;
    sub_element_counts[type_c][4] = 1;
    sub_element_counts[type_c][5] = 3;
    sub_element_counts[type_c][6] = 1;


    /*
     * sub_elements_required_list_counts[]
     * sub_elements_required_list_counts[record_type] = number of required sub_elements for each record type
     * record_type=1 are 'a' type records
     * record_type=2 are 'b' type records
     * record_type=3 are 'c' type records
     */
    sub_elements_required_list_counts[type_a] = 9;
    sub_elements_required_list_counts[type_b] = 6;
    sub_elements_required_list_counts[type_c] = 0;


    /*
     * sub_elements_required_list[][][]
     * sub_elements_required_list[record_type][index][1] = element_number
     * sub_elements_required_list[record_type][index][2] = sub_element_number that must contain a value
     * record_type=1 are 'a' type records
     * record_type=2 are 'b' type records
     * record_type=3 are 'c' type records
     * index = arbitrary number from 1 to the number of entries in the list
     */
    sub_elements_required_list[type_a][1][1] = 8;
    sub_elements_required_list[type_a][1][2] = 1;
    sub_elements_required_list[type_a][2][1] = 9;
    sub_elements_required_list[type_a][2][2] = 1;
    sub_elements_required_list[type_a][3][1] = 10;
    sub_elements_required_list[type_a][3][2] = 1;
    sub_elements_required_list[type_a][4][1] = 12;
    sub_elements_required_list[type_a][4][2] = 2;
    sub_elements_required_list[type_a][5][1] = 15;
    sub_elements_required_list[type_a][5][2] = 1;
    sub_elements_required_list[type_a][6][1] = 15;
    sub_elements_required_list[type_a][6][2] = 2;
    sub_elements_required_list[type_a][7][1] = 15;
    sub_elements_required_list[type_a][7][2] = 3;
    sub_elements_required_list[type_a][8][1] = 16;
    sub_elements_required_list[type_a][8][2] = 1;
    sub_elements_required_list[type_a][9][1] = 16;
    sub_elements_required_list[type_a][9][2] = 2;
    sub_elements_required_list[type_b][1][1] = 1;
    sub_elements_required_list[type_b][1][2] = 1;
    sub_elements_required_list[type_b][2][1] = 1;
    sub_elements_required_list[type_b][2][2] = 2;
    sub_elements_required_list[type_b][3][1] = 2;
    sub_elements_required_list[type_b][3][2] = 1;
    sub_elements_required_list[type_b][4][1] = 2;
    sub_elements_required_list[type_b][4][2] = 2;
    sub_elements_required_list[type_b][5][1] = 4;
    sub_elements_required_list[type_b][5][2] = 1;
    sub_elements_required_list[type_b][6][1] = 5;
    sub_elements_required_list[type_b][6][2] = 2;

    /*
     * conversion_factor_to_milimeters[]
     * 0=radians, 1=feet, 2=meters, 3=arc-seconds
     * 0, 3 used for ground units, assumes 1 arc-second = 30 meters
     * 1, 2 used for ground units and elevation units
     */
    conversion_factor_to_milimeters[0] = 6187944187.412890655;
    conversion_factor_to_milimeters[1] = 304.8;
    conversion_factor_to_milimeters[2] = 1000.0;
    conversion_factor_to_milimeters[3] = 30000.0;

    progname = *av;

    if (ac < 2) {
	usage();
	bu_exit(BRLCAD_ERROR, "Exiting.\n");
    }

    remove_whitespace(av[1]);
    string_length = strlen(av[1]) + 5;

    temp_filename = bu_calloc(1, string_length, "temp_filename");
    input_filename = bu_calloc(1, string_length, "input_filename");
    dsp_output_filename = bu_calloc(1, string_length, "dsp_output_filename");
    model_output_filename = bu_calloc(1, string_length, "model_output_filename");

    strcpy(input_filename, av[1]);
    strcpy(temp_filename, input_filename);
    strcat(temp_filename, ".tmp");
    strcpy(dsp_output_filename, input_filename);
    strcat(dsp_output_filename, ".dsp");
    strcpy(model_output_filename, input_filename);
    strcat(model_output_filename, ".g");

    bu_log("input_filename '%s'\n", input_filename);
    bu_log("temp_filename '%s'\n", temp_filename);
    bu_log("dsp_output_filename '%s'\n", dsp_output_filename);
    bu_log("model_output_filename '%s'\n", model_output_filename);

    raw_dem_2_raw_dsp_manual_scale_factor = 0;
    manual_dem_max_raw_elevation = 0;
    manual_dem_max_real_elevation = 0;

    if (read_dem(
	    /* input variables */
	    input_filename,
	    &raw_dem_2_raw_dsp_manual_scale_factor,
	    &manual_dem_max_raw_elevation,
	    &manual_dem_max_real_elevation,
	    temp_filename,
	    /* output variables */
	    &xdim,
	    &ydim,
	    &dsp_elevation,
	    &x_cell_size,
	    &y_cell_size,
	    &unit_elevation) == BRLCAD_ERROR) {
	    bu_free(input_filename, "input_filename");
	    bu_free(temp_filename, "temp_filename");
	    bu_free(dsp_output_filename, "dsp_output_filename");
	    bu_free(model_output_filename, "model_output_filename");
	bu_exit(BRLCAD_ERROR, "Error occured within function 'read_dem'. Import can not continue.\n");
    }

    if (convert_load_order(temp_filename, dsp_output_filename, &xdim, &ydim) == BRLCAD_ERROR) {
	    bu_free(input_filename, "input_filename");
	    bu_free(temp_filename, "temp_filename");
	    bu_free(dsp_output_filename, "dsp_output_filename");
	    bu_free(model_output_filename, "model_output_filename");
	bu_exit(BRLCAD_ERROR, "Error occured within function 'convert_load_order'. Import can not continue.\n");
    }

    if (create_model(
	    dsp_output_filename,
	    model_output_filename,
	    &xdim,
	    &ydim,
	    &dsp_elevation,
	    &x_cell_size,
	    &y_cell_size,
	    &unit_elevation) == BRLCAD_ERROR) {
	    bu_free(input_filename, "input_filename");
	    bu_free(temp_filename, "temp_filename");
	    bu_free(dsp_output_filename, "dsp_output_filename");
	    bu_free(model_output_filename, "model_output_filename");
	bu_exit(BRLCAD_ERROR, "Error occured within function 'create_model'. Model creation can not continue.\n");
    }
    bu_free(input_filename, "input_filename");
    bu_free(temp_filename, "temp_filename");
    bu_free(dsp_output_filename, "dsp_output_filename");
    bu_free(model_output_filename, "model_output_filename");

    temp_filename = NULL;
    input_filename = NULL;
    dsp_output_filename = NULL;
    model_output_filename = NULL;

    return 0;
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
