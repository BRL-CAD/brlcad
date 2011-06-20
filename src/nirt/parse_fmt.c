/*                     P A R S E _ F M T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file nirt/parse_fmt.c
 *
 * Parse the output formatter
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "vmath.h"
#include "raytrace.h"
#include "./nirt.h"
#include "./usrfmt.h"


/* The table of output values */
outval ValTab[] = {
    { 0, VTI_LITERAL, OIT_FNOUNIT, {0} },
    { "x_orig", VTI_X_ORIG, OIT_FLOAT, {0} },
    { "y_orig", VTI_Y_ORIG, OIT_FLOAT, {0} },
    { "z_orig", VTI_Z_ORIG, OIT_FLOAT, {0} },
    { "h", VTI_H, OIT_FLOAT, {0} },
    { "v", VTI_V, OIT_FLOAT, {0} },
    { "d_orig", VTI_D_ORIG, OIT_FLOAT, {0} },
    { "x_dir", VTI_X_DIR, OIT_FNOUNIT, {0} },
    { "y_dir", VTI_Y_DIR, OIT_FNOUNIT, {0} },
    { "z_dir", VTI_Z_DIR, OIT_FNOUNIT, {0} },
    { "a", VTI_A, OIT_FNOUNIT, {0} },
    { "e", VTI_E, OIT_FNOUNIT, {0} },
    { "x_in", VTI_X_IN, OIT_FLOAT, {0} },
    { "y_in", VTI_Y_IN, OIT_FLOAT, {0} },
    { "z_in", VTI_Z_IN, OIT_FLOAT, {0} },
    { "d_in", VTI_D_IN, OIT_FLOAT, {0} },
    { "x_out", VTI_X_OUT, OIT_FLOAT, {0} },
    { "y_out", VTI_Y_OUT, OIT_FLOAT, {0} },
    { "z_out", VTI_Z_OUT, OIT_FLOAT, {0} },
    { "d_out", VTI_D_OUT, OIT_FLOAT, {0} },
    { "los", VTI_LOS, OIT_FLOAT, {0} },
    { "scaled_los", VTI_SLOS, OIT_FLOAT, {0} },
    { "path_name", VTI_PATH_NAME, OIT_STRING, {0} },
    { "reg_name", VTI_REG_NAME, OIT_STRING, {0} },
    { "reg_id", VTI_REG_ID, OIT_INT, {0} },
    { "obliq_in", VTI_OBLIQ_IN, OIT_FNOUNIT, {0} },
    { "obliq_out", VTI_OBLIQ_OUT, OIT_FNOUNIT, {0} },
    { "nm_x_in", VTI_NM_X_IN, OIT_FNOUNIT, {0} },
    { "nm_y_in", VTI_NM_Y_IN, OIT_FNOUNIT, {0} },
    { "nm_z_in", VTI_NM_Z_IN, OIT_FNOUNIT, {0} },
    { "nm_d_in", VTI_NM_D_IN, OIT_FNOUNIT, {0} },
    { "nm_h_in", VTI_NM_H_IN, OIT_FNOUNIT, {0} },
    { "nm_v_in", VTI_NM_V_IN, OIT_FNOUNIT, {0} },
    { "nm_x_out", VTI_NM_X_OUT, OIT_FNOUNIT, {0} },
    { "nm_y_out", VTI_NM_Y_OUT, OIT_FNOUNIT, {0} },
    { "nm_z_out", VTI_NM_Z_OUT, OIT_FNOUNIT, {0} },
    { "nm_d_out", VTI_NM_D_OUT, OIT_FNOUNIT, {0} },
    { "nm_h_out", VTI_NM_H_OUT, OIT_FNOUNIT, {0} },
    { "nm_v_out", VTI_NM_V_OUT, OIT_FNOUNIT, {0} },
    { "ov_reg1_name", VTI_OV_REG1_NAME, OIT_STRING, {0} },
    { "ov_reg1_id", VTI_OV_REG1_ID, OIT_INT, {0} },
    { "ov_reg2_name", VTI_OV_REG2_NAME, OIT_STRING, {0} },
    { "ov_reg2_id", VTI_OV_REG2_ID, OIT_INT, {0} },
    { "ov_sol_in", VTI_OV_SOL_IN, OIT_STRING, {0} },
    { "ov_sol_out", VTI_OV_SOL_OUT, OIT_STRING, {0} },
    { "ov_los", VTI_OV_LOS, OIT_FLOAT, {0} },
    { "ov_x_in", VTI_OV_X_IN, OIT_FLOAT, {0} },
    { "ov_y_in", VTI_OV_Y_IN, OIT_FLOAT, {0} },
    { "ov_z_in", VTI_OV_Z_IN, OIT_FLOAT, {0} },
    { "ov_d_in", VTI_OV_D_IN, OIT_FLOAT, {0} },
    { "ov_x_out", VTI_OV_X_OUT, OIT_FLOAT, {0} },
    { "ov_y_out", VTI_OV_Y_OUT, OIT_FLOAT, {0} },
    { "ov_z_out", VTI_OV_Z_OUT, OIT_FLOAT, {0} },
    { "ov_d_out", VTI_OV_D_OUT, OIT_FLOAT, {0} },
    { "surf_num_in", VTI_SURF_NUM_IN, OIT_INT, {0} },
    { "surf_num_out", VTI_SURF_NUM_OUT, OIT_INT, {0} },
    { "claimant_count", VTI_CLAIMANT_COUNT, OIT_INT, {0} },
    { "claimant_list", VTI_CLAIMANT_LIST, OIT_STRING, {0} },
    { "claimant_listn", VTI_CLAIMANT_LISTN, OIT_STRING, {0} },
    { "attributes", VTI_ATTRIBUTES, OIT_STRING, {0} },
    { "x_gap_in", VTI_XPREV_OUT, OIT_FLOAT, {0} },
    { "y_gap_in", VTI_YPREV_OUT, OIT_FLOAT, {0} },
    { "z_gap_in", VTI_ZPREV_OUT, OIT_FLOAT, {0} },
    { "gap_los", VTI_GAP_LOS, OIT_FLOAT, {0} },
    { 0, 0, 0, {0} }
};

outitem *oi_list[FMT_NONE];
static char def_dest_string[] = "stdout";
char *dest_string = def_dest_string;
static char def_sf_name[] = DEF_SF_NAME;
char *sf_name = def_sf_name;		/* Name of state file */

FILE *outf = (FILE *)NULL;
const char *def_fmt[] = {
    "\"Origin (x y z) = (%.8f %.8f %.8f)  (h v d) = (%.4f %.4f %.4f)\nDirection (x y z) = (%.8f %.8f %.8f)  (az el) = (%.8f %.8f)\n\" x_orig y_orig z_orig h v d_orig x_dir y_dir z_dir a e",
    "\"    Region Name               Entry (x y z)              LOS  Obliq_in Attrib\n\"",
    "\"%-20s (%9.4f %9.4f %9.4f) %8.4f %8.4f %s\n\" reg_name x_in y_in z_in los obliq_in attributes",
    "\"\"",
    "\"You missed the target\n\"",
    "\"OVERLAP: '%s' and '%s' xyz_in=(%g %g %g) los=%g\n\" ov_reg1_name ov_reg2_name ov_x_in ov_y_in ov_z_in ov_los",
    "\"\""
};

void free_ospec(outitem *oil);

extern double base2local;
extern struct application ap;
extern char local_u_name[];
extern int overlap_claims;
extern char *ocname[];


void
format_output (const char* buffer, com_table* ctp, struct rt_i *UNUSED(rtip))
{
    const char* bp = buffer;	/* was + 1; */
    int fmt_type = FMT_NONE;
    int i;
    int use_defaults = 0;

    void parse_fmt(const char* uoutspec, int outcom_type);
    void show_ospec(outitem *oil);

    /* Handle no args, arg=='?', and obvious bad arg */
    if (*bp != '\0')
	++bp;
    while (isspace(*bp))
	++bp;
    switch (*bp) {
	case 'r':
	    fmt_type = FMT_RAY;
	    break;
	case 'h':
	    fmt_type = FMT_HEAD;
	    break;
	case 'p':
	    fmt_type = FMT_PART;
	    break;
	case 'f':
	    fmt_type = FMT_FOOT;
	    break;
	case 'm':
	    fmt_type = FMT_MISS;
	    break;
	case 'o':
	    fmt_type = FMT_OVLP;
	    break;
	case 'g':
	    fmt_type = FMT_GAP;
	    break;
	default:
	    --bp;
	    break;
    }
    while (isspace(*++bp))
	;

    switch (*bp) {
	case '\0':     /* display current output specs */
	    if (fmt_type == FMT_NONE)
		fprintf(stderr, "Error: No output-statement type specified\n");
	    else
		show_ospec(oi_list[fmt_type]);
	    return;
	case '"':
	    if (fmt_type == FMT_NONE) {
		fprintf(stderr, "Error: No output-statement type specified\n");
		return;
	    }
	    break;
	default:
	    if (strncmp(bp, "default", 7) == 0) {
		use_defaults = 1;
		break;
	    }
	    fprintf(stderr, "Error: Illegal format specifiation: '%s'\n", buffer);
	    /* fall through here */
	case '?':
	    com_usage(ctp);
	    return;
    }

    if (use_defaults) {
	if (fmt_type == FMT_NONE) {
	    for (i = 0; i < FMT_NONE; ++i) {
		parse_fmt(def_fmt[i], i);
	    }
	} else {
	    parse_fmt(def_fmt[fmt_type], fmt_type);
	}
    } else {
	parse_fmt(bp, fmt_type);
    }
}


/**
 * uoutspect is the user's output specification (format & args).
 * outcom_type is the type of output command
 */
void
parse_fmt(const char *uoutspec, int outcom_type)
{
    char *of;		/* Format for current output item */
    char *up;
    char *mycopy;	/* Solely for handing to bu_free() */
    char *uos;
    int nm_cs;		/* Number of conversion specifications */
    outitem *oil = OUTITEM_NULL;
    outitem *oip;
    outitem *prev_oip = OUTITEM_NULL;
    outval *vtp;

    mycopy = uos = bu_malloc(strlen(uoutspec)+1, "uos");
    bu_strlcpy(uos, uoutspec, strlen(uoutspec)+1);

    /* Break up the format specification into pieces,
     * one per conversion specification (and, hopefully)
     * one per argument.
     */
    if (*uos != '"') {
	fprintf(stderr,
		"parse_fmt sees first character `%c`.  Shouldn't happen.\n",
		*uos);
	bu_free(mycopy, "Copy of user's output spec");
	return;
    }
    ++uos;
    while (*uos != '"') {
	nm_cs = 0;
	/* Allocate storage for the next item in the output list */
	oip = (outitem *) bu_malloc(sizeof(outitem), "output item");
	oip->next = OUTITEM_NULL;

	for (up = uos; *uos != '"'; ++uos) {
	    if (*uos == '%') {
		if (*(uos + 1) == '%')
		    ++uos;
		else if (nm_cs == 1)
		    break;
		else /* nm_cs == 0 */
		    ++nm_cs;
	    }
	    if (*uos == '\\')
		if (*(uos + 1) == '"')
		    ++uos;
	}

	/* Allocate memory for and store the format.
	 * The code_nm field will be used at this point
	 * to record whether this format specification
	 * needs an output item or not (i.e. whether it
	 * contains 1 conversion spec vs. none)
	 */
	oip->format = bu_malloc(uos - up + 1, "format");
	of = oip->format;
	while (up != uos) {
	    if (*up == '\\') {
		switch (*(up + 1)) {
		    case 'n':
			*of++ = '\n';
			up += 2;
			break;
		    case '\042':
		    case '\047':
		    case '\134':
			*of++ = *(up + 1);
			up += 2;
			break;
		    default:
			*of++ = *up++;
			break;
		}
	    } else {
		*of++ = *up++;
	    }
	}
	*of = '\0';
	oip->code_nm = nm_cs;

	if (prev_oip == OUTITEM_NULL)
	    oil = oip;
	else
	    prev_oip->next = oip;
	prev_oip = oip;
    }

    /* Skip any garbage beyond the close quote */
    for (up = ++uos; (! isspace(*uos)) && (*uos != '\0'); ++uos)
	;

    if (up != uos) {
	*uos = '\0';
	fprintf(stderr,
		"Warning: suffix '%s' after format specification ignored\n",
		up);
    }

    /* Read in the list of objects to output */
    for (oip = oil; oip != OUTITEM_NULL; oip = oip->next) {
	if (oip->code_nm == 0)
	    continue;		/* outitem's format has no conversion spec */

	while (isspace(*uos))
	    ++uos;
	if (*uos == '\0') {
	    fprintf(stderr,
		    "Error: Fewer output items than conversion specs\n");
	    bu_free(mycopy, "Copy of user's output spec");
	    return;
	}
	for (up = uos; (! isspace(*uos)) && (*uos != '\0'); ++uos)
	    ;

	if (*uos != '\0')
	    *uos++ = '\0';

	oip->code_nm = 0;
	for (vtp = (outval *)(ValTab + 1); vtp->name; ++vtp) {
	    if (BU_STR_EQUAL(vtp->name, up)) {
		oip->code_nm = vtp->code_nm;
		break;
	    }
	}

	if (vtp->name == '\0') {
	    fprintf(stderr, "Error: Invalid output item '%s'\n", up);
	    bu_free(mycopy, "Copy of user's output spec");
	    return;
	}
    }

    while (isspace(*uos))
	++uos;

    if (*uos != '\0') {
	fprintf(stderr, "Error: More output items than conversion specs\n");
	fprintf(stderr, "Offending spec:\n\t%s\n", mycopy);
	bu_free(mycopy, "Copy of user's output spec");
	return;
    }

    check_conv_spec(oil);

    /* We are now satisfied that oil is a valid list of output items,
     * so it's time to install it as such
     */
    free_ospec(oi_list[outcom_type]);
    oi_list[outcom_type] = oil;

    bu_free(mycopy, "Copy of user's output spec");
}

void
default_ospec (void)
{
    int i;

    for (i = 0; i < FMT_NONE; ++i) {
	oi_list[i] = OUTITEM_NULL;
	parse_fmt(def_fmt[i], i);
    }
}


/**
 * oil is a list of output items
 */
void
show_ospec (outitem *oil)
{
    outitem *oip;		/* Pointer into list of output items */
    char *c;

    /* Display the format specification */
    printf("Format: \"");
    for (oip = oil; oip != OUTITEM_NULL; oip = oip->next) {
	for (c = oip->format; *c != '\0'; ++c) {
	    if (*c == '\n')
		printf("\\n");
	    else
		putchar(*c);
	}
    }
    printf("\"\n");

    /* Display the list of item names */
    printf("Item(s):");
    for (oip = oil; oip != OUTITEM_NULL; oip = oip->next) {
	if (ValTab[oip->code_nm].name)
	    printf(" %s", ValTab[oip->code_nm].name);
    }
    printf("\n");
}


void
report(int outcom_type)
{
    outitem *oip;

    if ((outcom_type < 0) || (outcom_type >= FMT_NONE)) {
	fprintf(stderr,
		"Illegal output-statement type: %d.  Shouldn't happen\n",
		outcom_type);
	return;
    }
    if (outf == (FILE *)NULL)
	outf = stdout;

    for (oip = oi_list[outcom_type]; oip != OUTITEM_NULL; oip = oip->next) {
	switch (ValTab[oip->code_nm].type) {
	    case OIT_INT:
		fprintf(outf, oip->format, ValTab[oip->code_nm].value.ival);
		break;
	    case OIT_FLOAT:
		fprintf(outf, oip->format,
			ValTab[oip->code_nm].value.fval * base2local);
		break;
	    case OIT_FNOUNIT:
		fprintf(outf, oip->format,
			ValTab[oip->code_nm].value.fval);
		break;
	    case OIT_STRING:
		fprintf(outf, oip->format, ValTab[oip->code_nm].value.sval);
		break;
	    default:
		fflush(stdout);
		fprintf(stderr, "Fatal: Invalid item type %d.  ",
			ValTab[oip->code_nm].type);
		fprintf(stderr, "This shouldn't happen\n");
		bu_exit (1, NULL);
	}
    }
    fflush(outf);
}


void
print_item (char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    char *bp = buffer;
    char *bp0;
    outval *vtp;


    /* Handle no args, arg=='?', and obvious bad arg */
    if (*bp != '\0')
	++bp;

    while (isspace(*bp))
	++bp;

    switch (*bp) {
	case '\0':
	case '?':
	    com_usage(ctp);
	    return;
    }

    /* Read in the list of objects to output */
    while (*bp != '\0') {
	while (isspace(*bp))
	    ++bp;

	for (bp0 = bp; (! isspace(*bp)) && (*bp != '\0'); ++bp)
	    ;

	if (*bp != '\0')
	    *bp++ = '\0';

	for (vtp = (outval *)(ValTab + 1); vtp->name; ++vtp) {
	    if (BU_STR_EQUAL(vtp->name, bp0)) {
		switch (vtp->type) {
		    case OIT_INT:
			printf("%d\n", vtp->value.ival);
			break;
		    case OIT_FLOAT:
			printf("%g\n", vtp->value.fval * base2local);
			break;
		    case OIT_FNOUNIT:
			printf("%g\n", vtp->value.fval);
			break;
		    case OIT_STRING:
			printf("'%s'\n", vtp->value.sval);
			break;
		    default:
			fflush(stdout);
			fprintf(stderr, "Fatal: Invalid item type %d.  ",
				ValTab[vtp->code_nm].type);
			fprintf(stderr, "This shouldn't happen\n");
			bu_exit (1, NULL);
		}
		break;
	    }
	}

	if (vtp->name == '\0') {
	    fprintf(stderr, "Error: Invalid output item '%s'\n", bp0);
	    return;
	}
    }
}

FILE *
fopenrc(void)
{
    char *rc_file_name;
    char *home;
    FILE *fPtr;
    size_t len;

    if ((fPtr = fopen(DEF_RCF_NAME, "rb")) == NULL) {
	if ((home = getenv("HOME")) != NULL) {
	    len = strlen(home) + strlen(DEF_RCF_NAME) + 2;
	    rc_file_name = bu_malloc(len, "rc_file_name");
	    snprintf(rc_file_name, len, "%s/%s", home, DEF_RCF_NAME);
	    fPtr = fopen(rc_file_name, "rb");
	}
    }
    return fPtr;
}

int
check_conv_spec(outitem *oip)
{
    char *cp;
    int oi_type;
    int warnings = 0;

    for (; oip != OUTITEM_NULL; oip = oip->next) {
	for (cp = oip->format; *cp != '\0'; ++cp) {
	    if (*cp == '%') {
		if (*(cp + 1) == '%') {
		    ++cp;
		} else {
		    ++cp;
		    break;
		}
	    }
	}

	if (*cp == '\0')		/* Added 8 Jun 90 */
	    continue;

	/* Skip optional crud */
	if (*cp == '-')
	    ++cp;
	while (isdigit(*cp))
	    ++cp;
	if (*cp == '.')
	    ++cp;
	while (isdigit(*cp))
	    ++cp;

	oi_type = ValTab[oip->code_nm].type;
	switch (*cp) {
	    case 'd':
	    case 'o':
	    case 'u':
	    case 'x':
		if (oi_type != OIT_INT) {
		    ++warnings;
		    fprintf(stderr,
			    "Warning: Conversion type '%%%c' specified",
			    *cp);
		    fprintf(stderr,
			    " for item %s, which is %s\n",
			    ValTab[oip->code_nm].name, oit_name(oi_type));
		}
		break;
	    case 'f':
	    case 'e':
	    case 'E':
	    case 'g':
	    case 'G':
		if ((oi_type != OIT_FLOAT) && (oi_type != OIT_FNOUNIT)) {
		    ++warnings;
		    fprintf(stderr,
			    "Warning: Conversion type '%%%c' specified",
			    *cp);
		    fprintf(stderr,
			    " for item %s, which is %s\n",
			    ValTab[oip->code_nm].name, oit_name(oi_type));
		}
		break;
	    case 's':
		if (oi_type != OIT_STRING) {
		    ++warnings;
		    fprintf(stderr,
			    "Warning: Conversion type '%%%c' specified",
			    *cp);
		    fprintf(stderr,
			    " for item %s, which is %s\n",
			    ValTab[oip->code_nm].name, oit_name(oi_type));
		}
		break;
	    case 'c':
		++warnings;
		fprintf(stderr,
			"Warning: Conversion type '%%%c' specified", *cp);
		fprintf(stderr,
			" for item %s, which is a %s\n",
			ValTab[oip->code_nm].name, oit_name(oi_type));
		break;
	    default:
		++warnings;
		fprintf(stderr,
			"Warning: Unknown conversion type '%%%c'\n", *cp);
		break;
	}
    }
    return warnings;
}


void
direct_output(const char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    size_t i = 0;      /* current position on the *buffer */
    size_t j = 0;      /* position of last non-whitespace char in the *buffer */
    FILE *newf;
    static char *new_dest;
    static FILE *(*openfunc)() = 0;

    while (isspace(*(buffer+i)))
	++i;

    if (*(buffer+i) == '\0') {
	/* display current destination */
#if defined(HAVE_POPEN) && !defined(STRICT_FLAGS)
	printf("destination = %s%s'\n",
	       (openfunc == popen) ? "'| " : "'", dest_string);
#endif
	return;
    }

    if (BU_STR_EQUAL(buffer + i, "?")) {
	com_usage(ctp);
	return;
    }

    if (BU_STR_EQUAL(buffer + i, "default")) {
	newf = stdout;
	new_dest = def_dest_string;
	openfunc = 0;
    } else {
	if (*(buffer + i) == '|') {
#if defined(HAVE_POPEN) && !defined(STRICT_FLAGS)
	    openfunc=popen;
	    ++i;
#else
	    fprintf(stderr, "Error, support for pipe output is disabled.  Try a redirect instead.\n");
	    return;
#endif
	} else {
	    openfunc=fopen;
	}
	/*Find last non-whitespace character*/
	j = strlen(buffer);
	while (isspace(*(buffer+j-1))) j--;

	new_dest = bu_malloc(strlen(buffer + i)+1, "new_dest");

	snprintf(new_dest, j-i+1, "%s", buffer + i);
	if (bu_file_exists(new_dest)) {
	    fprintf(stderr, "File %s already exists.\n", new_dest);
	    return;
	}
	if ((newf = (*openfunc)(new_dest, "w")) == NULL) {
#if defined(HAVE_POPEN) && !defined(STRICT_FLAGS)
	    fprintf(stderr, "Cannot open %s '%s'\n",
		    (openfunc == popen) ? "pipe" : "file", new_dest);
#endif
	    fprintf(stderr, "Destination remains = '%s'\n", dest_string);

	    bu_free(new_dest, "new(now old)dest");
	    return;
	}


    }

    /* Clean up from previous output destination */
    if (outf != (FILE *)NULL && outf != stdout)
	fclose(outf);

    if (dest_string != def_dest_string)
	bu_free(dest_string, "free dest_string");

    /* Establish the new destination */
    outf = newf;
    dest_string = new_dest;
}


void
state_file(const char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    int i = 0;      /* current position on the *buffer */
    static char *new_name;

    while (isspace(*(buffer+i)))
	++i;

    if (*(buffer+i) == '\0') {
	/* display current state name */
	printf("statefile = '%s'\n", sf_name);
	return;
    }

    if (BU_STR_EQUAL(buffer + i, "?")) {
	com_usage(ctp);
	return;
    }

    if (BU_STR_EQUAL(buffer + i, "default")) {
	new_name = def_sf_name;
    } else {
	new_name = bu_malloc(strlen(buffer + i)+1, "new_state_filename");
	snprintf(new_name, strlen(buffer+i), "%s", buffer + i);
    }

    /* Clean up from previous output destination */
    if (sf_name != def_sf_name)
	bu_free(sf_name, "new(now old)statefile");

    /* Establish the new destination */
    sf_name = new_name;
}


void
dump_state(const char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    char *c;
    static const char fmt_char[] = {'r', 'h', 'p', 'f', 'm', 'o', 'g'};
    FILE *sfPtr;
    int f;
    outitem *oip;		/* Pointer into list of output items */

    /* quellage */
    buffer = buffer;
    ctp = ctp;

    if ((sfPtr = fopen(sf_name, "wb")) == NULL) {
	fprintf(stderr, "Cannot open statefile '%s'\n", sf_name);
	return;
    }

    printf("Dumping NIRT state to file '%s'...", sf_name);
    fprintf(sfPtr, "%c file created by the dump command of nirt\n", CMT_CHAR);
    fprintf(sfPtr, "xyz %g %g %g\n", target(X), target(Y), target(Z));
    fprintf(sfPtr, "dir %g %g %g\n", direct(X), direct(Y), direct(Z));
    fprintf(sfPtr, "useair %d\n", ap.a_rt_i->useair);
    fprintf(sfPtr, "units %s\n", local_u_name);
    if (BU_STR_EQUAL(dest_string, "stdout"))
	fputs("dest default\n", sfPtr);
    else
	fprintf(sfPtr, "dest %s\n", dest_string);
    fprintf(sfPtr, "overlap_claims %s\n", ocname[overlap_claims]);

    for (f = 0; f < FMT_NONE; ++f) {
	fprintf(sfPtr, "fmt %c \"", fmt_char[f]);
	/* Display the conversion specifications */
	for (oip = oi_list[f]; oip != OUTITEM_NULL; oip = oip->next)
	    for (c = oip->format; *c != '\0'; ++c)
		if (*c == '\n')
		    fprintf(sfPtr, "\\n");
		else
		    fputc(*c, sfPtr);
	fprintf(sfPtr, "\"");

	/* Display the item name */
	for (oip = oi_list[f]; oip != OUTITEM_NULL; oip = oip->next)
	    if (ValTab[oip->code_nm].name)
		fprintf(sfPtr, " %s", ValTab[oip->code_nm].name);
	fprintf(sfPtr, "\n");
    }
    printf("\n");
    fclose(sfPtr);
}


void
load_state(char *buffer, com_table *ctp, struct rt_i *rtip)
{
    FILE *sfPtr;

    /* quellage */
    buffer = buffer;
    ctp = ctp;

    if ((sfPtr = fopen(sf_name, "rb")) == NULL) {
	fprintf(stderr, "Cannot open statefile '%s'\n", sf_name);
	return;
    }
    bu_log("Loading NIRT state from file '%s'...", sf_name);
    interact(READING_FILE, sfPtr, rtip);
    bu_log("\n");
    fclose(sfPtr);
}


/**
 * oil is a list of output items
 */
void
free_ospec (outitem *oil)
{
    outitem *next = oil;	/* Pointer to next output item */
    outitem *oip;		/* Pointer to output item to free */

    while (next != OUTITEM_NULL) {
	oip = next;
	next = oip->next;
	bu_free(oip->format, "outitem.format");
	if (oip != oil)
	    bu_free((char *) oip, "outitem");
    }
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
