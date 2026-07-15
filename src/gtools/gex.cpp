/*                         G E X . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2026 United States Government as represented by
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
/** @file gex.cpp
 *
 * purpose: manage various aspects of a BRL-CAD database
 *
 * description: Enables the user to perform various tasks on a BRL-CAD
 * database (DB) file including reporting statistics and shortening
 * the DB file by removing unused free space.  The original DB file is
 * opened read-only and is not modified in any way.
 *
 * The only option at the moment is to compress the input DB by
 * removing unused free space which accumulates during the
 * construction of a BRL-CAD model.  The shortened copy of the input
 * DB is written to a new file which may be named by the user,
 * otherwise the new file is name "<input file name>.compressed".
 *
 * As an example of how much a DB can grow during construction,
 * creating a model consisting of 1, 000 spheres and killing all of
 * them, and doing that 10 times, creates a 45 Mb file.  Then
 * converting it to ASCII and back reduces the file size to 250 Kb.
 * (Note the use of g2asc and asc2g may allow mathematical errors to
 * creep in so their use is not recommended for production models.)
 *
 */

#include "common.h"

#include <cstdlib>
#include <ctype.h>
#include <string>
#include <map>
#include "bio.h"

#include "bu/app.h"
#include "bu/cmdschema.h"
#include "bu/str.h"
#include "raytrace.h"


std::string
get_dli_type_name(const int typ)
{
    // list from db5.h:
    switch (typ) {
        case 0:  return "APPLICATION_DATA_OBJECT";
            break;
        case 1:  return "HEADER_OBJECT";
            break;
        case 2:  return "FREE_STORAGE";
            break;
        default:  return "unknown";
            break;
    }
} // get_dli_type_name


std::string
get_major_type_name(const int typ)
{
    // list from db5.h:
    switch (typ) {
        case 0:  return "RESERVED";
            break;
        case 1:  return "BRLCAD";
            break;
        case 2:  return "ATTRIBUTE_ONLY";
            break;
        case 9:  return "BINARY_UNIF";
            break;
        case 10:  return "BINARY_MIME";
            break;
        default:  return "unknown";
            break;
    }
} // get_major_type_name


std::string
get_minor_type_name(const int typ)
{
    // list from db5.h:
    switch (typ) {
        case 0:  return "RESERVED";
            break;
        case 1:  return "TOR";
            break;
        case 2:  return "TGC";
            break;
        case 3:  return "ELL";
            break;
        case 4:  return "ARB8";
            break;
        case 5:  return "ARS";
            break;
        case 6:  return "HALF";
            break;
        case 7:  return "REC";
            break;
        case 8:  return "POLY";
            break;
        case 9:  return "BSPLINE";
            break;
        case 10:  return "SPH";
            break;
        case 11:  return "NMG";
            break;
        case 12:  return "EBM";
            break;
        case 13:  return "VOL";
            break;
        case 14:  return "ARBN";
            break;
        case 15:  return "PIPE";
            break;
        case 16:  return "PARTICLE";
            break;
        case 17:  return "RPC";
            break;
        case 18:  return "RHC";
            break;
        case 19:  return "EPA";
            break;
        case 20:  return "EHY";
            break;
        case 21:  return "ETO";
            break;
        case 22:  return "GRIP";
            break;
        case 23:  return "JOINT";
            break;
        case 24:  return "HF";
            break;
        case 25:  return "DSP";
            break;
        case 26:  return "SKETCH";
            break;
        case 27:  return "EXTRUDE";
            break;
        case 28:  return "SUBMODEL";
            break;
        case 29:  return "CLINE";
            break;
        case 30:  return "BOT";
            break;
        case 31:  return "COMBINATION";
            break;
        case 35:  return "SUPERELL";
            break;
        case 36:  return "METABALL";
            break;
        case 37:  return "BREP";
            break;
        case 38:  return "HYP";
            break;
        case 39:  return "CONSTRAINT";
            break;
        case 40:  return "REVOLVE";
            break;
        case 41:  return "ANNOTATION";
            break;
        case 42:  return "HRT";
            break;
        default:  return "unknown";
            break;
    }
} // get_minor_type_name


/**
 * hexdump() helper function for printing a human-readable view
 */
static void
print_representation(unsigned char *chars, size_t length)
{
    size_t i;

    for (i = 0; i < length; i++) {
	if (isascii(0x00FF & chars[i]) && (isprint(0x00FF & chars[i])))
	    printf("%c", chars[i]);
	else
	    printf(".");
    }
}


/**
 * Prints a canonical hexadecimal dump for a given block of bytes
 */
static void
hexdump(const unsigned char *from, const unsigned char *to)
{
    long i = 0L;
    long ii = 0L; // starting offset
    int	j = 0;
    int jj = 0;
    int k = 0;
    unsigned char c = 0;
    static const int PERLINE = 16;
    unsigned char chars[PERLINE] = {0};

    if (from > to)
	return;

    while (from < to) {
	c = *from++;

	if (! (i % PERLINE)) {
	    if (i) {
		j = 0;
		printf("  ");
		print_representation(chars, PERLINE);
	    }

	    printf("\n%8.8lx:", ii);
	}

	ii++;
	i++;
	printf(" %2.2x", (c & 0x00FF));
	chars[j++] = c;
    }

    k = (i % PERLINE);
    if (k) {
	k = PERLINE - k;

	for (jj = 0; jj < (3 * k); ++jj)
	    printf(" ");
    }

    printf("  ");
    print_representation(chars, PERLINE);
    printf("\n\n");
}


struct gex_args {
    int help;
    int force;
    int compress;
    const char *dump_object;
};

static const struct bu_cmd_option gex_options[] = {
    BU_CMD_FLAG("h", "help", struct gex_args, help, "Print help and exit"),
    BU_CMD_FLAG("f", "force", struct gex_args, force,
	"Allow overwriting an existing compressed output file"),
    BU_CMD_FLAG("c", "compress", struct gex_args, compress,
	"Create a copy with no free space"),
    BU_CMD_STRING("d", "dump", struct gex_args, dump_object, "object",
	"Hexdump a specific object"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand gex_operands[] = {
    BU_CMD_OPERAND("input_file", BU_CMD_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_CMD_OPERAND("output_file", BU_CMD_VALUE_FILE, 0, 1,
	"Optional compressed output database", NULL),
    BU_CMD_OPERAND_NULL
};

static const char *gex_compress_option[] = {"compress", NULL};
static const struct bu_cmd_constraint gex_constraints[] = {
    BU_CMD_CONSTRAINT_OPERANDS(BU_CMD_CONDITION_NO_OPTION_PRESENT,
	gex_compress_option, 1, 1,
	"an output file requires --compress"),
    BU_CMD_CONSTRAINT_NULL
};

static int
gex_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state != BU_CMD_VALIDATE_VALID)
	return ret;
    if (bu_cmd_schema_option_present(schema, argc, argv, "force") &&
	!bu_cmd_schema_option_present(schema, argc, argv, "compress")) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_INVALID;
	result->token_start = cursor_arg;
	result->token_end = cursor_arg;
	result->expected = BU_CMD_EXPECT_OPTION;
	result->completion_type = BU_CMD_VALUE_FLAG;
	result->hint = "--force requires --compress";
    }
    return 0;
}

static const struct bu_cmd_schema gex_schema = {
    "gex", "Examine, summarize, and compress a BRL-CAD database",
    gex_options, gex_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(gex_schema_validate, gex_constraints)
};

int
main(int argc, char** argv)
{
    static const char usage[] = "Usage: %s [options] input.g [output.g]\n";

    // db pointers
    FILE *in = NULL;
    FILE *out = NULL;
    int fd;
    int res = 0; // for function returns

    const char *argv0 = argv[0];
    const char DBSUF[] = ".compressed.g";
    struct bu_vls str = BU_VLS_INIT_ZERO;

    struct gex_args args = {0, 0, 0, NULL};
    struct bu_vls db_fname  = BU_VLS_INIT_ZERO;
    struct bu_vls db2_fname = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    /* Skip first arg */
    argv++; argc--;
    int help_requested = bu_cmd_schema_option_present(&gex_schema,
	(size_t)argc, (const char **)argv, "help");
    int operand_index = help_requested ?
	bu_cmd_schema_parse(&gex_schema, &args, &str, argc,
	    (const char **)argv) :
	bu_cmd_schema_parse_complete(&gex_schema, &args, &str, argc,
	    (const char **)argv);

    if (operand_index < 0) {
	bu_exit(EXIT_FAILURE, "%s", bu_vls_addr(&str));
    }
    bu_vls_free(&str);

    argc -= operand_index;
    argv += operand_index;

    if (args.help) {
	char *help = bu_cmd_schema_describe(&gex_schema);
	bu_log(usage, argv0);
	if (help) {
	    bu_log("Options:\n%s", help);
	    bu_free(help, "gex native help");
	}
	return EXIT_SUCCESS;
    }

    bu_vls_sprintf(&db_fname, "%s", argv[0]);
    argv++; argc--;

    /* output filename is optional */
    if (argc) {
	bu_vls_sprintf(&db2_fname, "%s", argv[0]);
	argv++; argc--;
    }

    if (args.compress && bu_vls_strlen(&db2_fname) == 0) {
	bu_vls_sprintf(&db2_fname, "%s%s", bu_vls_addr(&db_fname), DBSUF);
    }

    // open the db file read-only
    if (!bu_file_exists(bu_vls_cstr(&db_fname), &fd)) {
        bu_exit(EXIT_FAILURE, "ERROR: non-existent input file '%s'\n", bu_vls_addr(&db_fname));
    }
    in = fdopen(fd, "r");
    if (!in) {
        perror(bu_vls_addr(&db_fname));
        bu_exit(EXIT_FAILURE, "ERROR: input file [%s] open failure\n", bu_vls_addr(&db_fname));
    }

    if (args.compress) {
        if (BU_STR_EQUAL(bu_vls_addr(&db_fname), bu_vls_addr(&db2_fname))) {
            bu_exit(EXIT_FAILURE, "ERROR: output file cannot be same as input file\n");
        }
        // check for existing file
	if (bu_file_exists(bu_vls_cstr(&db2_fname), NULL)) {
	    if (args.force) {
                printf("WARNING: overwriting existing file [%s]...\n", bu_vls_addr(&db2_fname));
                bu_file_delete(bu_vls_addr(&db2_fname));
            } else {
                bu_exit(EXIT_FAILURE, "ERROR: output file [%s] already exists (use the '-f' option to overwrite)\n", bu_vls_addr(&db2_fname));
            }
        }
        out = fopen(bu_vls_addr(&db2_fname), "w");
        if (!out) {
            perror(bu_vls_addr(&db2_fname));
            bu_exit(EXIT_FAILURE, "ERROR: unable to open output file [%s]\n", bu_vls_addr(&db2_fname));
        }
    }

    // database analysis ========================
    // a struct to hold a raw db object (see db5.h)
    struct db5_raw_internal r;
    // track dli, major, and minor types
    std::map<int, int> dli_count;
    std::map<int, int> major_count;
    std::map<int, int> minor_count;

    int nobj = 0;
    int fobj = 0;
    int nsiz = 0;
    int free_bytes = 0;
    int named_obj = 0;
    int ncombs = 0;
    int nregs = 0;
    int wattrs = 0;

    // write an output header
    int fnsiz = strlen(bu_vls_addr(&db_fname));
    std::string eqs = "";
    for (int i = 0; i < fnsiz; ++i)
        eqs += '=';

    printf("=======================================%s=\n", eqs.c_str());
    printf(" Objects found in BRL-CAD V5 database: %s \n", bu_vls_addr(&db_fname));
    printf("=======================================%s=\n", eqs.c_str());
    printf("\n");

    // start reading the input file
    r.magic = DB5_RAW_INTERNAL_MAGIC;

    // see db5_io.c for the reading function:
    while ((res = db5_get_raw_internal_fp(&r, in)) == 0) {
        RT_CK_RIP(&r);
        ++nobj;

        // new primary type info in h flags (DLI) (see db5.h):
        // 0 = app data object; 1 = header object; 2 = free storage
        int typ = static_cast<int>(r.h_dli);

        // get major and minor type
        int M = static_cast<int>(r.major_type);
        int m = static_cast<int>(r.minor_type);
        if (dli_count.find(M) != dli_count.end())
            ++dli_count[typ];
        else {
            dli_count[typ] = 1;
            std::string s = get_dli_type_name(typ);
            int len = static_cast<int>(s.size());
            if (len > nsiz)
                nsiz = len;
        }

        // named object?
        std::string name("(none)");
        if (r.h_name_present != 0) {
            ++named_obj;
            size_t len = r.name.ext_nbytes;
            name = "";
            for (size_t i = 0; i < len; ++i)
                name += r.name.ext_buf[i];
        }
        if (major_count.find(M) != major_count.end())
            ++major_count[M];
        else {
            major_count[M] = 1;
            std::string s = get_major_type_name(typ);
            int len = static_cast<int>(s.size());
            if (len > nsiz)
                nsiz = len;
        }
        if (minor_count.find(m) != minor_count.end())
            ++minor_count[m];
        else {
            minor_count[m] = 1;
            std::string s = get_minor_type_name(typ);
            int len = static_cast<int>(s.size());
            if (len > nsiz)
                nsiz = len;
        }
        printf("Object DLI type/major/minor type: %3d/%3d/%3d name: %s\n",
               typ, M, m, name.c_str());
        // has attributes?
        if (r.a_present) {
            printf("  Has one or more attributes.\n");
            ++wattrs;
        }
        if (m == DB5_MINORTYPE_BRLCAD_COMBINATION) {
            ++ncombs;
            printf("  Is a combination");
            // is it a region?
            // extract attributes
            size_t len = r.attributes.ext_nbytes;
            std::string a= "";
            for (size_t i = 0; i < len; ++i) {
                if (r.attributes.ext_buf[i]) {
                    char c = tolower(r.attributes.ext_buf[i]);
                    if (a.empty() && c != 'r')
                        continue;
                    a += c;
                }
            }
            if (a.find("region") != std::string::npos) {
                printf(" (a region)");
                ++nregs;
            }
            printf(".\n");
        }
        // FIXME: (will have to do more decoding for that answer)

        if (typ == DB5HDR_HFLAGS_DLI_FREE_STORAGE) {
            // free space, count bytes (object counted above)
            free_bytes += static_cast<int>(r.object_length);
            ++fobj;
        } else if (args.compress) {
            // write the object to the output file
            size_t nw = fwrite(r.buf, 1, r.object_length, out);
            if (nw != r.object_length)
                bu_bomb("nw != r.object_length");
        }
	if (args.dump_object && BU_STR_EQUAL(args.dump_object, name.c_str())) {
	    printf("  Hex Dumping %zu bytes\n", r.object_length);
	    hexdump(r.buf, r.buf+r.object_length);
	}

        // free the heap stuff
        if (r.buf) {
            bu_free(r.buf, "r.buf");
        }
    }

    printf("\n");
    printf("==================================%s=\n", eqs.c_str());
    printf(" Summary for BRL-CAD V5 database: %s\n", bu_vls_addr(&db_fname));
    printf("==================================%s=\n", eqs.c_str());
    printf("\n");
    printf("Found %d objects:\n", nobj);
    printf("  free space: %6d\n", fobj);
    printf("  named     : %6d\n", named_obj);
    printf("  other     : %6d\n", nobj - fobj - named_obj);

    bool sepr(false);
    if (ncombs) {
        sepr = true;
        printf("\n");
        printf("%d objects are combinations (%d of which are regions).\n",
               ncombs, nregs);
    }
    if (wattrs) {
        if (!sepr) {
            printf("\n");
        }
        printf("%d objects have one or more attributes.\n",
               wattrs);
    }
    printf("\n");
    printf("Object DLI types (the main category: defined in H Flags):\n");
    for (std::map<int, int>::iterator i = dli_count.begin(); i != dli_count.end(); ++i) {
        // get name of dli type
        std::string tname = get_dli_type_name(i->first);
        int tlen = static_cast<int>(tname.size());
        int bsiz = nsiz - tlen + 3;
        printf("  %3d (%-s)%-*.*s: %6d\n",
               i->first,
               tname.c_str(),
               bsiz, bsiz, " ",
               i->second);
    }
    printf("Object major types:\n");
    for (std::map<int, int>::iterator i = major_count.begin(); i != major_count.end(); ++i) {
        // get name of major type
        std::string tname = get_major_type_name(i->first);
        int tlen = static_cast<int>(tname.size());
        int bsiz = nsiz - tlen + 3;
        printf("  %3d (%-s)%-*.*s: %6d\n",
               i->first,
               tname.c_str(),
               bsiz, bsiz, " ",
               i->second);
    }
    printf("Object minor types:\n");
    for (std::map<int, int>::iterator i = minor_count.begin(); i != minor_count.end(); ++i) {
        // get name of minor type
        std::string tname = get_minor_type_name(i->first);
        int tlen = static_cast<int>(tname.size());
        int bsiz = nsiz - tlen + 3;
        printf("  %3d (%-s)%-*.*s: %6d\n",
               i->first,
               tname.c_str(),
               bsiz, bsiz, " ",
               i->second);
    }
    printf("\n");

    if (fobj) {
        const double kb = 1024;
        const double mb = kb * 1000;
        printf("Free space: %d bytes", free_bytes);
        if (free_bytes > mb)
            printf(" (%.3f Mb)", free_bytes/mb);
        else if (free_bytes > kb)
            printf(" (%.3f Kb)", free_bytes/kb);
        printf("\n");
    }
    if (res == -1)
        printf("\nNote: file read ended normally at EOF.\n");
    else
        printf("\nNote: file read ended early with an error!\n");

    if (args.compress)
        printf("See compressed file '%s'.\n", bu_vls_addr(&db2_fname));

    return 0;

} // main

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
