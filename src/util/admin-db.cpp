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
    if (!stat(db_fname, &sb)) {
        bu_exit(EXIT_FAILURE, "non-existent input file\n");
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
    while ((res = db5_get_raw_internal_fp(&r, in)) != 0) {
      ++nobj;

      /* get major and minor type */
      unsigned char M = r.major_type;
      unsigned char m = r.minor_type;
      printf("Object major/minor type: %8x/%8x\n", M, m);
    }

    printf("Found %d objects, %d of which are free\n", nobj, fobj);


    return 0;

}
