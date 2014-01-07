/*                       D S P _ A D D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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
/** @file util/dsp_add.c
 *
 * add 2 files of network unsigned shorts
 *
 * Options
 * h help
 */

#include "common.h"

#include <stdlib.h>
#include <sys/stat.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"

#include "tclap/CmdLine.h"

/* declarations to support use of TCLAP arg parsing */
static const char usage[] = "Usage: dsp_add dsp_1 dsp_2 > dsp_3\n";

/* purpose: combine two dsp files
 *
 * description: Combines two dsp files (which are binary files
 * comprised of network unsigned shorts).  The two files must be of
 * identical size.  The result, written to stdout, is a file where
 * each cell's height is the total of the heights of the same cell
 * in the input files.
 *
 * See the BRL-CAD wiki for a tutorial on using dsp's.
 *
 * see_also: dsp(5) asc2dsp(1) cv(1)
 *
 */

#define ADD_STYLE_INT 0
#define ADD_STYLE_FLOAT 1

static int style = ADD_STYLE_INT;

static void
swap_bytes(unsigned short *buf, unsigned long count)
{
    unsigned short *p;

    for (p = &buf[count-1]; p >= buf; p--)
	*p = ((*p << 8) & 0x0ff00) | (*p >> 8);
}


/*
 * Perform floating point addition and re-normalization of the data.
 */
static void
add_float(unsigned short *buf1, unsigned short *buf2, unsigned long count)
{
    unsigned short *p, *q, *e;
    double *dbuf, *d;
    double min, max, k;

    dbuf = (double *)bu_malloc(sizeof(double) * count, "buffer of double");

    min = MAX_FASTF;
    max = -MAX_FASTF;
    e = &buf1[count];

    /* add everything, keeping track of the min/max values found */
    for (d = dbuf, p = buf1, q = buf2; p < e; p++, q++, d++) {
	*d = *p + *q;
	if (*d > max) max = *d;
	if (*d < min) min = *d;
    }

    /* now we convert back to unsigned shorts in the range 1 .. 65535 */

    k = 65534.0 / (max - min);

    bu_log("min: %g scale: %g\n", min - k, k);

    for (d = dbuf, p = buf1, q = buf2; p < e; p++, q++, d++)
	*p = (unsigned short)  ((*d - min) * k) + 1;

    bu_free(dbuf, "buffer of double");
}


/*
 * Perform simple integer addition to the input streams.
 * Issue warning on overflow.
 *
 * Result:	buf1 contents modified
 */
static void
add_int(unsigned short *buf1, unsigned short *buf2, unsigned long count)
{
    int int_value;
    unsigned long i;
    unsigned short s;

    for (i = 0; i < count; i++) {
	int_value = buf1[i] + buf2[i];
	s = (unsigned short)int_value;

	if (s != int_value) {
	    bu_log("overflow (%d+%d) == %d at %lu\n",
		   buf1[i], buf2[i], int_value, i);
	}
	buf1[i] = s;
    }

}

using namespace std;
using namespace TCLAP;

// some customization of TCLAP classes
class BRLCAD_StdOutput : public StdOutput
{
  // example usage in main:
  //   CmdLine cmd("this is a message", ' ', "0.99" );
  //   // set the output
  //   BRLCAD_StdOutput brlstdout;
  //   cmd.setOutput(&brlstdout);
  //   // proceed normally ...

public:
  virtual void failure(CmdLineInterface& c, ArgException& e) {
    list<Arg*> args = c.getArgList(); // quieten compiler
    cerr << "Input error: " << endl
         << e.what() << endl;
    exit(1);
  }

  virtual void usage(CmdLineInterface& c) {
    cout << "Usage:" << endl;
    list<Arg*> args = c.getArgList();
    for (ArgListIterator it = args.begin(); it != args.end(); it++)
      cout << (*it)->longID()
           << "  (" << (*it)->getDescription() << ")" << endl;
  }

  virtual void version(CmdLineInterface& c) {
    list<Arg*> args = c.getArgList(); // quieten compiler
    ; // do not show version
    //cout << "my version message: 0.1" << endl;
  }
};

int
main(int ac, char *av[])
{
  /*  int next_arg; *//* <= not needed */
    FILE *in1 = NULL;
    FILE *in2 = NULL;
    unsigned short *buf1 = NULL;
    unsigned short *buf2 = NULL;
    size_t count = 0;
    int in_cookie, out_cookie;
    int conv;
    struct stat sb;
    size_t ret;

    try {

      // form the command line
      //
      // note help (-h and --help) and version (-v and --version) are
      // automatic
      TCLAP::CmdLine cmd(usage, ' ',
                         "[BRL_CAD_VERSION]"); // help and version are automatic
      // use our subclassed stdout
      BRLCAD_StdOutput brlstdout;
      cmd.setOutput(&brlstdout);
      // proceed normally ...


/*
      // we also want the '-?' option (note empty second arg for no
      // long option), last arg means option not required
      TCLAP::SwitchArg h_arg("?",    // short option char
                             "help",     // long option name, if any
                             "Displays usage information and exits.",  // short description string
                             cmd,    // add to 'cmd' object
                             false); // default value
*/
      // need two file names
      TCLAP::UnlabeledValueArg<string> dsp1_arg("dsp_file1", // name of object
                                                "first dsp input file name", // description
                                                true,      // arg is required
                                                "",        // default value
                                                "dsp_file1",  // type of arg value
                                                cmd);      // add to cmd object

      // need two file names
      TCLAP::UnlabeledValueArg<string> dsp2_arg("<dsp_file2>", // name of object
                                                "second dsp input file name", // description
                                                true,      // arg is required
                                                "",        // default value
                                                "dsp_file2",  // type of arg value
                                                cmd);      // add to cmd object

      // parse the args
      cmd.parse(ac, av);

      // Get the value parsed by each arg.
      //bool has_h = h_arg.getValue();
      const char *dsp1_fname = dsp1_arg.getValue().c_str();
      const char *dsp2_fname = dsp2_arg.getValue().c_str();

      // take appropriate action
      //if (has_h) {
      //bu_exit(1, usage);
      //}

      // open files
      in1 = fopen(dsp1_fname, "r");
      if (!in1) {
	perror(dsp1_fname);
	return EXIT_FAILURE;
      }

      if (fstat(fileno(in1), &sb)) {
        perror(dsp1_fname);
	fclose(in1);
	return EXIT_FAILURE;
      }

      count = sb.st_size;
      buf1 = (unsigned short *)bu_malloc((size_t)sb.st_size, "buf1");

      in2 = fopen(dsp2_fname, "r");
      if (!in2) {
	perror(dsp2_fname);
	fclose(in1);
	return EXIT_FAILURE;
      }

      if (fstat(fileno(in2), &sb)) {
	perror(dsp2_fname);
	fclose(in1);
	fclose(in2);
	return EXIT_FAILURE;
      }

      if ((size_t)sb.st_size != count) {
	fclose(in1);
	fclose(in2);
	bu_exit(EXIT_FAILURE, "**** ERROR **** file size mis-match\n");
      }

    } catch (TCLAP::ArgException &e) { // catch any exceptions

      cerr << "error: " << e.error() << " for arg " << e.argId() << endl;

    }

    if (ac < 2)
      bu_exit(1, usage);

    if (isatty(fileno(stdout)))
      bu_exit(1, "Must redirect standard output\n");

    /*
    next_arg = parse_args(ac, av);

    if (next_arg >= ac)
	print_usage("No files specified\n");

    */

    /* Open the files */
    /* see try block above */
    /*
    in1 = fopen(av[next_arg], "r");
    if (!in1) {
	perror(av[next_arg]);
	return EXIT_FAILURE;
    }

    if (fstat(fileno(in1), &sb)) {
	perror(av[next_arg]);
	fclose(in1);
	return EXIT_FAILURE;
    }

    count = sb.st_size;
    buf1 = (unsigned short *)bu_malloc((size_t)sb.st_size, "buf1");

    next_arg++;

    in2 = fopen(av[next_arg], "r");
    if (!in2) {
	perror(av[next_arg]);
	fclose(in1);
	return EXIT_FAILURE;
    }

    if (fstat(fileno(in2), &sb)) {
	perror(av[next_arg]);
	fclose(in1);
	fclose(in2);
	return EXIT_FAILURE;
    }

    if ((size_t)sb.st_size != count) {
	fclose(in1);
	fclose(in2);
	bu_exit(EXIT_FAILURE, "**** ERROR **** file size mis-match\n");
    }
    */

    buf2 = (unsigned short *)bu_malloc((size_t)sb.st_size, "buf2");

    count = count >> 1; /* convert count of char to count of short */

    /* Read the terrain data */
    ret = fread(buf1, sizeof(short), count, in1);
    if (ret < count)
	perror("fread");
    fclose(in1);

    ret = fread(buf2, sizeof(short), count, in2);
    if (ret < count)
	perror("fread");
    fclose(in2);

    /* Convert from network to host format */
    in_cookie = bu_cv_cookie("nus");
    out_cookie = bu_cv_cookie("hus");
    conv = (bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie));

    if (conv) {
	swap_bytes(buf1, count);
	swap_bytes(buf2, count);
    }

    /* add the two datasets together */
    switch (style) {
	case ADD_STYLE_FLOAT	: add_float(buf1, buf2, count); break;
	case ADD_STYLE_INT	: add_int(buf1, buf2, count); break;
	default			: bu_log("Error: Unknown add style\n");
	    break;
    }

    /* convert back to network format & write out */
    if (conv) {
	swap_bytes(buf1, count);
	swap_bytes(buf2, count);
    }

    if (fwrite(buf1, sizeof(short), count, stdout) != count) {
	bu_exit(EXIT_FAILURE, "Error writing data\n");
    }

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
