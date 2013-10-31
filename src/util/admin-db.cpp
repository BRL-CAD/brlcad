#include "common.h"

#include <stdlib.h>
#include <sys/stat.h>
#include "bio.h"

#include "bu_arg_parse.h" /* includes bu.h */
#include "raytrace.h"

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
 * the file size to 250 Kb.  (Note the use of g2asc and asc2g may allow mathmatical
 * errors to creep in so their use is not recommended for production models.)
 */


int
main(int argc, char** argv)
{
    /* db pointers */
    FILE *in = NULL;
    FILE *out = NULL;
    /* a struct to hold a raw db object (see db5.h) */
    struct db5_raw_internal r;
    int res = 0; /* for function returns */
    int nobj = 0;
    int fobj = 0;
    size_t free_bytes = 0;
    int M0m0  = 0;
    int M1m3  = 0;
    int M1m31 = 0;
    int M2m0  = 0;

    struct stat sb;
    const char DBSUF[] = ".compressed";

    /* vars expected from cmd line parsing */
    int arg_err            = 0;
    int has_force          = 0;
    int has_help           = 0;
    int has_compress       = 0;
    char db_fname[BU_ARG_PARSE_BUFSZ]  = {0};
    char db2_fname[BU_ARG_PARSE_BUFSZ] = {0};

    /* FIXME: this '-?' arg doesn't work correctly due to some TCLAPisms */
    static bu_arg_switch_t h_arg;
    /* define a force option to allow user to shoot himself in the foot */
    static bu_arg_switch_t f_arg;
    /* define a compress option */
    static bu_arg_switch_t c_arg;

    /* input file names */
    static bu_arg_unlabeled_value_t db_arg;
    /* the output file name (optional) */
    static bu_arg_unlabeled_value_t db2_arg;

    /* place the arg pointers in an array (note the array is of
     * type void* to hold the heterogeneous arg type structs)
     */
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

    /* input file name */
    BU_ARG_UNLABELED_VALUE_INIT(
      db_arg,
      "",
      "DB_infile",
      "DB input file name",
      BU_ARG_REQUIRED,
      BU_ARG_STRING,
      ""
      );

    /* the output file name (optional) */
    BU_ARG_UNLABELED_VALUE_INIT(
      db2_arg,
      "",
      "DB_outfile",
      "DB output file name",
      BU_ARG_OPTIONAL,
      BU_ARG_STRING,
      ""
      );

    /* parse the args */
    arg_err = bu_arg_parse(args, argc, argv);

    if (arg_err == BU_ARG_PARSE_ERR) {
        /* the TCLAP exception handler has fired with its own message
         * so need no message here
         */
        bu_exit(EXIT_SUCCESS, NULL);
    }

    /* Get the value parsed by each arg. */
    has_force    = bu_arg_get_bool(&f_arg);
    has_help     = bu_arg_get_bool(&h_arg);
    has_compress = bu_arg_get_bool(&c_arg);
    bu_arg_get_string(&db_arg, db_fname);
    bu_arg_get_string(&db2_arg, db2_fname);

    /* take appropriate action... */

    /* note this exit is SUCCESS because it is expected
     * behavior--important for good auto-man-page handling
     */
    if (has_help) {
      bu_exit(EXIT_SUCCESS, usage);
    }

    if (has_compress) {
      if (!db2_fname[0]) {
        bu_strlcpy(db2_fname, db_fname, BU_ARG_PARSE_BUFSZ);
        bu_strlcat(db2_fname, DBSUF, BU_ARG_PARSE_BUFSZ);
      }
    }

    /* open the db file read-only */
    /* TCLAP doesn't check for existing files (FIXME: add to TCLAP) */
    if (stat(db_fname, &sb)) {
        bu_exit(EXIT_FAILURE, "non-existent input file '%s'\n", db_fname);
    }
    in = fopen(db_fname, "r");
    if (!in) {
      perror(db_fname);
      bu_exit(EXIT_FAILURE, "ERROR: input file open failure\n");
    }

    if (has_compress) {
      /* TCLAP doesn't check for confusion in file names */
      if (BU_STR_EQUAL(db_fname, db2_fname)) {
        bu_exit(EXIT_FAILURE, "overwriting an input file\n");
      }
      /* check for existing file */
      if (!stat(db2_fname, &sb)) {
        if (has_force) {
          printf("WARNING: overwriting an existing file...\n");
          bu_file_delete(db2_fname);
        }
        else {
          bu_exit(EXIT_FAILURE, "overwriting an existing file (use the '-f' option to continue)\n");
        }
      }
      out = fopen(db2_fname, "w");
      if (!out) {
        perror(db2_fname);
        bu_exit(EXIT_FAILURE, "ERROR: output file open failure\n");
      }
    }

    /* start reading the input file */
    r.magic = DB5_RAW_INTERNAL_MAGIC;

    while ((res = db5_get_raw_internal_fp(&r, in)) == 0) {
      RT_CK_RIP(&r);
      ++nobj;

      /* get major and minor type */
      unsigned char M = r.major_type;
      unsigned char m = r.minor_type;
      printf("Object major/minor type: %3u/%3u\n", (unsigned)M, (unsigned)m);
      if (M == 0) {
        if (m == 0)
          ++M0m0;
        else
          bu_bomb("duh");
      }
      else if (M == 1) {
        if (m == 3)
          ++M1m3;
        else if (m == 31)
          ++M1m31;
        else
          bu_bomb("duh");
      }
      else if (M == 2) {
        if (m == 0)
          ++M2m0;
        else
          bu_bomb("duh");
      };

      if (M == 0 && m == 0) {
        /* free space, count object and bytes */
        ++fobj;
        free_bytes += r.object_length;
      }
      /* free the heap stuff */
      if (r.buf) {
        bu_free(r.buf, "r.buf");
      }
    }
    printf("Found %d objects, %d of which are free space\n", nobj, fobj);
    printf("Object types:\n");
    printf("  0/0  (RESERVED/RESERVED)       : %5d\n", M0m0);
    printf("  1/3  (BRLCAD/ELL)              : %5d\n", M1m3);
    printf("  1/31 (BRLCAD/COMBINATION)      : %5d\n", M1m31);
    printf("  2/0  (ATTRIBUTE_ONLY/RESERVED) : %5d\n", M2m0);
    if (fobj) {
        const int mb = 1024*1000;
        printf("  Free space: %d bytes (assumes 0/0 is free space)\n", (int)free_bytes);
        if ((int)free_bytes > mb)
          printf("  Free space: %d Mb \n", (int)free_bytes/mb);
    }
    printf("Note res = %d (-1 = EOF)\n", res);

    return 0;

}
