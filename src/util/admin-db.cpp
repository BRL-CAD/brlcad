#include "common.h"

#include <string>
#include <map>
#include <stdlib.h>
#include <sys/stat.h>
#include "bio.h"

#include "bu_arg_parse.h" // includes bu.h
#include "raytrace.h"

using namespace std;

static const char usage[] = "Example: admin-db [...options] db.g [db2.g]\n";

/* purpose: manage various aspects of a BRL-CAD database
 *
 * description: Enables the user to perform various tasks on a BRL-CAD
 * database (DB) file including reporting statistics and shortening
 * the DB file by removing unused free space.  The original DB file
 * is opened read-only and is not modified in any way.
 *
 * The only option at the moment is to compress the input DB by removing
 * unused free space which accumulates during the construction of a BRL-CAD model.
 * The shortened copy of the input DB is written to a new file which may be named
 * by the user, otherwise the new file is name "<input file name>.compressed".
 *
 * As an example of how much a DB can grow during construction, creating a model
 * consisting of 1,000 spheres and killing all of them, and doing that 10 times,
 * creates a 45 Mb file.  Then converting it to ASCII and back reduces
 * the file size to 250 Kb.  (Note the use of g2asc and asc2g may allow mathematical
 * errors to creep in so their use is not recommended for production models.)
 */

string get_dli_type_name(const int typ);
string get_minor_type_name(const int typ);
string get_major_type_name(const int typ);

int
main(int argc, char** argv)
{
    // db pointers
    FILE *in = NULL;
    FILE *out = NULL;
    int res = 0; // for function returns

    struct stat sb;
    const char DBSUF[] = ".compressed";

    // vars expected from cmd line parsing
    int arg_err            = 0;
    int has_force          = 0;
    int has_help           = 0;
    int has_compress       = 0;
    char db_fname[BU_ARG_PARSE_BUFSZ]  = {0};
    char db2_fname[BU_ARG_PARSE_BUFSZ] = {0};

    // FIXME: this '-?' arg doesn't work correctly due to some TCLAPisms
    static bu_arg_switch_t h_arg;
    // define a force option to allow user to shoot himself in the foot
    static bu_arg_switch_t f_arg;
    // define a compress option
    static bu_arg_switch_t c_arg;

    // input file names
    static bu_arg_unlabeled_value_t db_arg;
    // the output file name (optional)
    static bu_arg_unlabeled_value_t db2_arg;

    // place the arg pointers in an array (note the array is of
    // type void* to hold the heterogeneous arg type structs)
    static void *args[] = {
        &h_arg, &f_arg, &c_arg,
        &db_arg, &db2_arg,
        NULL
    };

    BU_ARG_SWITCH_INIT(
        h_arg,
        "?",
        "short-help",
        "Same as '-h' or '--help'"
    );

    BU_ARG_SWITCH_INIT(
        f_arg,
        "f",
        "force",
        "Allow overwriting existing files."
    );

    BU_ARG_SWITCH_INIT(
        c_arg,
        "c",
        "compress",
        "Create a copy with no free space."
    );

    // input file name
    BU_ARG_UNLABELED_VALUE_INIT(
        db_arg,
        "",
        "DB_infile",
        "DB input file name",
        BU_ARG_REQUIRED,
        BU_ARG_STRING,
        ""
    );

    // the output file name (optional)
    BU_ARG_UNLABELED_VALUE_INIT(
        db2_arg,
        "",
        "DB_outfile",
        "DB output file name",
        BU_ARG_OPTIONAL,
        BU_ARG_STRING,
        ""
    );

    // parse the args
    arg_err = bu_arg_parse(args, argc, argv);

    if (arg_err == BU_ARG_PARSE_ERR) {
        // the TCLAP exception handler has fired with its own message
        // so need no message here
        bu_exit(EXIT_SUCCESS, NULL);
    }

    // Get the value parsed by each arg.
    has_force    = bu_arg_get_bool(&f_arg);
    has_help     = bu_arg_get_bool(&h_arg);
    has_compress = bu_arg_get_bool(&c_arg);
    bu_arg_get_string(&db_arg, db_fname);
    bu_arg_get_string(&db2_arg, db2_fname);

    // take appropriate action...

    // note this exit is SUCCESS because it is expected
    // behavior--important for good auto-man-page handling
    if (has_help) {
        bu_exit(EXIT_SUCCESS, usage);
    }

    if (has_compress) {
        if (!db2_fname[0]) {
            bu_strlcpy(db2_fname, db_fname, BU_ARG_PARSE_BUFSZ);
            bu_strlcat(db2_fname, DBSUF, BU_ARG_PARSE_BUFSZ);
        }
    }

    // open the db file read-only
    // TCLAP doesn't check for existing files (FIXME: add to TCLAP)
    if (stat(db_fname, &sb)) {
        bu_exit(EXIT_FAILURE, "non-existent input file '%s'\n", db_fname);
    }
    in = fopen(db_fname, "r");
    if (!in) {
        perror(db_fname);
        bu_exit(EXIT_FAILURE, "ERROR: input file open failure\n");
    }

    if (has_compress) {
        // TCLAP doesn't check for confusion in file names
        if (BU_STR_EQUAL(db_fname, db2_fname)) {
            bu_exit(EXIT_FAILURE, "overwriting an input file\n");
        }
        // check for existing file
        if (!stat(db2_fname, &sb)) {
            if (has_force) {
                printf("WARNING: overwriting an existing file...\n");
                bu_file_delete(db2_fname);
            } else {
                bu_exit(EXIT_FAILURE, "overwriting an existing file (use the '-f' option to continue)\n");
            }
        }
        out = fopen(db2_fname, "w");
        if (!out) {
            perror(db2_fname);
            bu_exit(EXIT_FAILURE, "ERROR: output file open failure\n");
        }
    }

    // database analysis ========================
    // a struct to hold a raw db object (see db5.h)
    struct db5_raw_internal r;
    // track dli, major, and minor types
    map<int,int> dli_count;
    map<int,int> major_count;
    map<int,int> minor_count;

    int nobj = 0;
    int fobj = 0;
    int nsiz = 0;
    int free_bytes = 0;
    int named_obj = 0;

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
            string s = get_dli_type_name(typ);
            int len = static_cast<int>(s.size());
            if (len > nsiz)
                nsiz = len;
        }

        // named object?
        bool has_name = static_cast<bool>(r.h_name_present);
        string name("(none)");
        if (has_name) {
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
            string s = get_major_type_name(typ);
            int len = static_cast<int>(s.size());
            if (len > nsiz)
                nsiz = len;
        }
        if (minor_count.find(m) != minor_count.end())
            ++minor_count[m];
        else {
            minor_count[m] = 1;
            string s = get_minor_type_name(typ);
            int len = static_cast<int>(s.size());
            if (len > nsiz)
                nsiz = len;
        }
        printf("Object DLI type/major/minor type: %3d/%3d/%3d name: %s\n",
               typ, M, m, name.c_str());

        if (typ == 2) {
            // free space, count bytes (object counted above)
            free_bytes += static_cast<int>(r.object_length);
            ++fobj;
        } else if (has_compress) {
            // write the object to the output file
            size_t nw = fwrite(r.buf, 1, r.object_length, out);
            if (nw != r.object_length)
                bu_bomb("nw != r.object_length");
        }

        // free the heap stuff
        if (r.buf) {
            bu_free(r.buf, "r.buf");
        }
    }

    printf("Found %d objects:\n", nobj);
    printf("  free space: %6d\n", fobj);
    printf("  named     : %6d\n", named_obj);
    printf("  other     : %6d\n", nobj - fobj - named_obj);
    printf("Object DLI types (the main category: defined in H Flags) :\n");
    for (map<int,int>::iterator i = dli_count.begin(); i != dli_count.end(); ++i) {
        // get name of dli type
        string tname = get_dli_type_name(i->first);
        int tlen = static_cast<int>(tname.size());
        int bsiz = nsiz - tlen + 3;
        printf("  %3d (%-s)%-*.*s: %6d\n",
               i->first,
               tname.c_str(),
               bsiz, bsiz, " ",
               i->second);
    }
    printf("Object major types:\n");
    for (map<int,int>::iterator i = major_count.begin(); i != major_count.end(); ++i) {
        // get name of major type
        string tname = get_major_type_name(i->first);
        int tlen = static_cast<int>(tname.size());
        int bsiz = nsiz - tlen + 3;
        printf("  %3d (%-s)%-*.*s: %6d\n",
               i->first,
               tname.c_str(),
               bsiz, bsiz, " ",
               i->second);
    }
    printf("Object minor types:\n");
    for (map<int,int>::iterator i = minor_count.begin(); i != minor_count.end(); ++i) {
        // get name of minor type
        string tname = get_minor_type_name(i->first);
        int tlen = static_cast<int>(tname.size());
        int bsiz = nsiz - tlen + 3;
        printf("  %3d (%-s)%-*.*s: %6d\n",
               i->first,
               tname.c_str(),
               bsiz, bsiz, " ",
               i->second);
    }

    if (fobj) {
        const int mb = 1024*1000;
        printf("Free space: %d bytes\n", free_bytes);
        if (free_bytes > mb)
            printf("          (%d Mb)\n", free_bytes/mb);
    }
    printf("Note res = %d (-1 = EOF)\n", res);

    if (has_compress)
        printf("See compressed file '%s'.\n", db2_fname);

    return 0;

} // main

string
get_dli_type_name(const int typ)
{
    // list from db5.h:
    switch(typ) {
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

string
get_major_type_name(const int typ)
{
    // list from db5.h:
    switch(typ) {
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

string
get_minor_type_name(const int typ)
{
    // list from db5.h:
    switch(typ) {
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

