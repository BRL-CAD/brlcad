/*                 B U _ O P T _ P A R S E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <sys/stat.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"


#include "tclap/CmdLine.h"
#include "bu_opt_parse_private.h"
#include "bu_opt_parse.h"

using namespace std;
using namespace TCLAP;

/* using ideas from Cliff and Sean... */
/* local funcs */

/* not yet ready
void handle_MultiArg(bu_arg_vars *a, CmdLine &cmd);
void handle_MultiSwitchArg(bu_arg_vars *a, CmdLine &cmd);
void handle_UnlabeledMultiArg(bu_arg_vars *a, CmdLine &cmd);
void handle_ValueArg(bu_arg_vars *a, CmdLine &cmd);
*/

void handle_SwitchArg(bu_arg_vars *a, CmdLine &cmd);
void handle_UnlabeledValueArg(bu_arg_vars *a, CmdLine &cmd);

/**
 * construct all for TCLAP handling, ensure input args get proper values for the caller
 */
extern "C"
int
bu_opt_parse(bu_arg_vars *args[], int argc, char **argv)
{
  //if (argc < 2)
  //bu_exit(EXIT_FAILURE, "ERROR: too few args in %s\n", argv[0]);

  int retval = BU_ARG_PARSE_SUCCESS;
  try {

    // form the command line
    //
    // note help (-h and --help) and version (-v and --version) are
    // automatic
    TCLAP::CmdLine cmd("<usage or purpose>", ' ',
                       "[BRL_CAD_VERSION]"); // help and version are automatic
    // use our subclassed stdout
    BRLCAD_StdOutput brlstdout;
    cmd.setOutput(&brlstdout);
    // proceed normally ...

    int i = 0;
    while (args[i]) {
      // handle this arg and fill in the values
      // map inputs to TCLAP:
      bu_arg_vars *a = args[i];
      switch (a->arg_type) {
          case BU_ARG_SwitchArg:
            handle_SwitchArg(a, cmd);
            break;
          case BU_ARG_UnlabeledValueArg:
            handle_UnlabeledValueArg(a, cmd);
            break;
/* not yet ready
          case BU_ARG_MultiArg:
            handle_MultiArg(a, cmd);
            break;
          case BU_ARG_MultiSwitchArg:
            handle_MultiSwitchArg(a, cmd);
            break;
          case BU_ARG_UnlabeledMultiArg:
            handle_UnlabeledMultiArg(a, cmd);
            break;
          case BU_ARG_ValueArg:
            handle_ValueArg(a, cmd);
            break;
*/
          default:
             // error
            break;
      }
      // next arg
      ++i;

    }

    // parse the args
    cmd.parse(argc, argv);

  } catch (TCLAP::ArgException &e) { // catch any exceptions

    cerr << "error: " << e.error() << " for arg " << e.argId() << endl;
    return BU_ARG_PARSE_ERR;
  }

  return retval;

} // bu_opt_parse


void
handle_SwitchArg(bu_arg_vars *a, CmdLine &cmd)
{
  SwitchArg A(a->flag, a->name, a->desc, a->val.i);
  cmd.add(A);
}

void
handle_UnlabeledValueArg(bu_arg_vars *a, CmdLine &cmd)
{

  // this is a templated type
  bu_arg_value_t val_type = a->val_type;
  string type_desc;
  switch (val_type) {
      case BU_ARG_BOOL: {
        bool val = a->val.i;
        type_desc = "bool";
        UnlabeledValueArg<bool> A(a->name, a->desc, a->req, val, type_desc);
        cmd.add(A);
      }
        break;
      case BU_ARG_CHAR: {
        char val = a->val.c;
        type_desc = "char";
        UnlabeledValueArg<char> A(a->name, a->desc, a->req, val, type_desc);
        cmd.add(A);
      }
        break;
      case BU_ARG_DOUBLE: {
        double val = a->val.d;
        type_desc = "double";
        UnlabeledValueArg<double> A(a->name, a->desc, a->req, val, type_desc);
        cmd.add(A);
      }
        break;
      case BU_ARG_FLOAT: {
        type_desc = "float";
        float val = a->val.f;
        UnlabeledValueArg<float> A(a->name, a->desc, a->req, val, type_desc);
        cmd.add(A);
      }
        break;
      case BU_ARG_INT: {
        int val = a->val.i;
        type_desc = "int";
        UnlabeledValueArg<int> A(a->name, a->desc, a->req, val, type_desc);
        cmd.add(A);
      }
        break;
      case BU_ARG_STRING: {
        string val = a->val.s;
        type_desc = "string";
        UnlabeledValueArg<string> A(a->name, a->desc, a->req, val, type_desc);
        cmd.add(A);
      }
        break;
      default: {
        string val = a->val.s;
        type_desc = "string";
        UnlabeledValueArg<string> A(a->name, a->desc, a->req, val, type_desc);
        cmd.add(A);
      }
        break;
  }
}

/* not yet ready
void
handle_MultiArg(bu_arg_vars *a, CmdLine &cmd)
{
  MultiArg *A = new MultiArg();
}

void
handle_MultiSwitchArg(bu_arg_vars *a, CmdLine &cmd)
{
  MultiSwitchArg *A = new MultiSwitchArg();
}

void
handle_UnlabeledMultiArg(bu_arg_vars *a, CmdLine &cmd)
{
  UnlabeledMultiArg *A = new UnlabeledMultiArg();
}

void
handle_ValueArg(bu_arg_vars *a, CmdLine &cmd)
{
  ValueArg *A = new ValueArg();
}
*/
