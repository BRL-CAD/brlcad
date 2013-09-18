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

#include <cstdlib>
#include <sys/stat.h>
#include <cstring> // for strdup
#include "bio.h"

/* #include "bu.h" */
#include "vmath.h"
#include "bn.h"

#include "bu_arg_parse_private.h" /* includes bu_arg_parse.h which includes bu.h */

#include <vector>

using namespace std;
using namespace TCLAP;

/* using ideas from Cliff and Sean... */
/* local funcs */
bu_arg_vars *bu_arg_init();

/* not yet ready
Arg *handle_MultiArg(bu_arg_vars *a, CmdLine &cmd);
Arg *handle_MultiSwitchArg(bu_arg_vars *a, CmdLine &cmd);
Arg *handle_UnlabeledMultiArg(bu_arg_vars *a, CmdLine &cmd);
Arg *handle_ValueArg(bu_arg_vars *a, CmdLine &cmd);
*/

Arg *handle_SwitchArg(bu_arg_vars *a, CmdLine &cmd);
Arg *handle_UnlabeledValueArg(bu_arg_vars *a, CmdLine &cmd);

/* not yet ready
void extract_MultiArg_data(bu_arg_vars *a, Arg *A);
void extract_MultiSwitchArg_data(bu_arg_vars *a, Arg *A);
void extract_UnlabeledMultiArg_data(bu_arg_vars *a, Arg *A);
void extract_ValueArg_data(bu_arg_vars *a, Arg *A);
*/

void extract_SwitchArg_data(bu_arg_vars *a, Arg *A);
void extract_UnlabeledValueArg_data(bu_arg_vars *a, Arg *A);


/**
 * need vec for deleting Arg pointers when done
 */
static vector<Arg*> Arg_pointers;

/**
 * get a value for an arg
 */
long
bu_arg_get_bool_value(bu_arg_vars *arg)
{
  return (arg->val.u.l ? 1 : 0);
}

long
bu_arg_get_long_value(bu_arg_vars *arg)
{
  return arg->val.u.l;
}

double
bu_arg_get_double_value(bu_arg_vars *arg)
{
  return arg->val.u.d;
}

const char *
bu_arg_get_string_value(bu_arg_vars *arg)
{
  return arg->val.u.s.vls_str;
}

/**
 * general contructor for bu_arg_vars
 */
bu_arg_vars *
bu_arg_init()
{
  // get the mem for the struct
  bu_arg_vars *arg;
  BU_ALLOC(arg, bu_arg_vars);

  // fill in bu_vls_t pointers
  BU_VLS_INIT(&(arg->flag));
  BU_VLS_INIT(&(arg->name));
  BU_VLS_INIT(&(arg->desc));
  BU_VLS_INIT(&(arg->val.u.s));

  // return to caller
  return arg;
}

/**
 * C constructor for SwitchArg
 */
extern "C"
bu_arg_vars *
bu_arg_SwitchArg(const char *flag,
                 const char *name,
                 const char *desc,
                 const char *def_val)
{
  bu_arg_vars *arg = bu_arg_init();

  // fill in the values
  arg->arg_type = BU_ARG_SwitchArg;

  bu_vls_sprintf(&(arg->flag), flag);
  bu_vls_sprintf(&(arg->name), name);
  bu_vls_sprintf(&(arg->desc), desc);

  arg->val.typ = BU_ARG_BOOL;
  arg->val.u.l = (long)bu_str_true(def_val);

  return arg;
}

/**
 * C constructor for UnlabeledValueArg
 */
extern "C"
bu_arg_vars *
bu_arg_UnlabeledValueArg(const char *name,
                         const char *desc,
                         const char *def_val,
                         const bu_arg_req_t required,
                         const bu_arg_valtype_t val_typ)
{
  bu_arg_vars *arg = bu_arg_init();

  // fill in the values
  arg->arg_type = BU_ARG_UnlabeledValueArg;

  bu_vls_sprintf(&(arg->name), name);
  bu_vls_sprintf(&(arg->desc), desc);

  arg->req = required;

  switch (val_typ) {
      case BU_ARG_BOOL:
        arg->val.u.l = (long)bu_str_true(def_val);
        break;
      case BU_ARG_DOUBLE:
        arg->val.u.d = strtod(def_val, NULL);
        break;
      case BU_ARG_LONG:
        arg->val.u.l = strtol(def_val, NULL, 10);
        break;
      case BU_ARG_STRING:
        bu_vls_sprintf(&(arg->val.u.s), def_val);
        break;
      default:
        // error
        break;
  }

  return arg;
}

/**
 * construct all for TCLAP handling, ensure input args get proper values for the caller
 */
extern "C"
int
bu_arg_parse(bu_ptbl_t *ptbl_args, int argc, char * const argv[])
{
  BU_CK_PTBL(ptbl_args);

  // unpack the table into a C++ container for convenience
  vector<bu_arg_vars*> args;

  bu_arg_vars **ap;
  for (BU_PTBL_FOR(ap, (bu_arg_vars**), ptbl_args)) {
    bu_arg_vars *a = *ap;
    args.push_back(a);
  }

  // need a local collection to extract TCLAP data after a successful
  // parse
  vector<Arg*> tclap_results;

  int retval = BU_ARG_PARSE_SUCCESS;

  try {

    // form the command line
    // note help (-h and --help) and version (-v and --version) are
    // automatic
    TCLAP::CmdLine cmd("<usage or purpose>", ' ',
                       "[BRL_CAD_VERSION]"); // help and version are automatic

    // use our subclassed stdout
    BRLCAD_StdOutput brlstdout;
    cmd.setOutput(&brlstdout);

    // proceed normally ...

    for (unsigned i = 0; i < args.size(); ++i) {
      // handle this arg and fill in the values
      // map inputs to TCLAP:
      Arg *A         = 0;
      bu_arg_vars *a = args[i];
      switch (a->arg_type) {
          case BU_ARG_SwitchArg:
            A = handle_SwitchArg(a, cmd);
            break;
          case BU_ARG_UnlabeledValueArg:
            A = handle_UnlabeledValueArg(a, cmd);
            break;
/* not yet ready
          case BU_ARG_MultiArg:
            A = handle_MultiArg(a, cmd);
            break;
          case BU_ARG_MultiSwitchArg:
            A = handle_MultiSwitchArg(a, cmd);
            break;
          case BU_ARG_UnlabeledMultiArg:
            A = handle_UnlabeledMultiArg(a, cmd);
            break;
          case BU_ARG_ValueArg:
            A = handle_ValueArg(a, cmd);
            break;
*/
          default:
             // error
            break;
      }

      // save results for later
      tclap_results.push_back(A);
    }

    // parse the args
    cmd.parse(argc, argv);

    for (unsigned j = 0; j < tclap_results.size(); ++j) {
      Arg *A         = tclap_results[j];
      bu_arg_vars *a = args[j];
      switch (a->arg_type) {
          case BU_ARG_SwitchArg:
            extract_SwitchArg_data(a, A);
            break;
          case BU_ARG_UnlabeledValueArg:
            extract_UnlabeledValueArg_data(a, A);
            break;
/* not yet ready
          case BU_ARG_MultiArg:
            extract_MultiArg_data(a, cmd);
            break;
          case BU_ARG_MultiSwitchArg:
            extract_MultiSwitchArg_data(a, A);
            break;
          case BU_ARG_UnlabeledMultiArg:
            extract_UnlabeledMultiArg_data(a, A);
            break;
          case BU_ARG_ValueArg:
            extract_ValueArg_data(a, A);
            break;
*/
          default:
             // error
            break;
      }
    }

  } catch (TCLAP::ArgException &e) {
    // catch and handle any exceptions

    cerr << "error: " << e.error() << " for arg " << e.argId() << endl;

    // cleanup
    if (!Arg_pointers.empty())
      for (unsigned i = 0; i < Arg_pointers.size(); ++i) {
        delete Arg_pointers[i];
        Arg_pointers[i] = 0;
      }

    return BU_ARG_PARSE_ERR;
  }

  // successful parse, clean up heap memory used
  if (!Arg_pointers.empty())
    for (unsigned i = 0; i < Arg_pointers.size(); ++i) {
      delete Arg_pointers[i];
      Arg_pointers[i] = 0;
    }

  return retval;

} // bu_opt_parse

Arg *
handle_SwitchArg(bu_arg_vars *a, CmdLine &cmd)
{
  SwitchArg *A = new SwitchArg(a->flag.vls_str, a->name.vls_str, a->desc.vls_str, a->val.u.l);

  Arg_pointers.push_back(A);
  cmd.add(A);

  return A;
}

Arg *
handle_UnlabeledValueArg(bu_arg_vars *a, CmdLine &cmd)
{

  // this is a templated type
  bu_arg_valtype_t val_type = a->val.typ;
  string type_desc;
  Arg *A = 0;
  switch (val_type) {
      case BU_ARG_BOOL: {
        bool val = (a->val.u.l ? true : false);
        type_desc = "bool";
        A = new UnlabeledValueArg<bool>(a->name.vls_str, a->desc.vls_str, a->req, val, type_desc);
      }
        break;
      case BU_ARG_DOUBLE: {
        double val = a->val.u.d;
        type_desc = "double";
        A = new UnlabeledValueArg<double>(a->name.vls_str, a->desc.vls_str, a->req, val, type_desc);
      }
        break;
      case BU_ARG_LONG: {
        long val = a->val.u.l;
        type_desc = "long";
        A = new UnlabeledValueArg<long>(a->name.vls_str, a->desc.vls_str, a->req, val, type_desc);
      }
        break;
      case BU_ARG_STRING: {
        string val = (a->val.u.s.vls_str ? a->val.u.s.vls_str : "");
        type_desc = "string";
        A = new UnlabeledValueArg<string>(a->name.vls_str, a->desc.vls_str, a->req, val, type_desc);
      }
        break;
      default: {
        string val = (a->val.u.s.vls_str ? a->val.u.s.vls_str : "");
        type_desc = "string";
        A = new UnlabeledValueArg<string>(a->name.vls_str, a->desc.vls_str, a->req, val, type_desc);
      }
        break;
  }
  cmd.add(A);
  Arg_pointers.push_back(A);
  return A;
}

/* not yet ready
Arg *
handle_MultiArg(bu_arg_vars *a, CmdLine &cmd)
{
  MultiArg *A = new MultiArg();

  Arg_pointers.push_back(A);
  return A;
}

Arg *
handle_MultiSwitchArg(bu_arg_vars *a, CmdLine &cmd)
{
  MultiSwitchArg *A = new MultiSwitchArg();

  Arg_pointers.push_back(A);
  return A;
}

Arg *
handle_UnlabeledMultiArg(bu_arg_vars *a, CmdLine &cmd)
{
  UnlabeledMultiArg *A = new UnlabeledMultiArg();

  Arg_pointers.push_back(A);
  return A;
}

Arg *
handle_ValueArg(bu_arg_vars *a, CmdLine &cmd)
{
  ValueArg *A = new ValueArg();

  Arg_pointers.push_back(A);
  return A;
}
*/

void
extract_SwitchArg_data(bu_arg_vars *a, Arg *A)
{
  SwitchArg *B = dynamic_cast<SwitchArg*>(A);
  if (a->val.typ == BU_ARG_BOOL) {
    bool val = B->getValue();
    a->val.u.l = (val ? 1 : 0);
   }
  else {
    // it's a user error if this happens
    BU_ASSERT("tried to use non-existent bool type");
  }
}

void
extract_UnlabeledValueArg_data(bu_arg_vars *a, Arg *A)
{
  // this is a templated type
  bu_arg_valtype_t val_type = a->val.typ;
  switch (val_type) {
      case BU_ARG_BOOL: {
        UnlabeledValueArg<bool> *B = dynamic_cast<UnlabeledValueArg<bool> *>(A);
        bool val = B->getValue();
        a->val.u.l = (val ? 1 : 0);
      }
        break;
      case BU_ARG_DOUBLE: {
        UnlabeledValueArg<double> *B = dynamic_cast<UnlabeledValueArg<double> *>(A);
        a->val.u.d = B->getValue();
      }
        break;
      case BU_ARG_LONG: {
        UnlabeledValueArg<long> *B = dynamic_cast<UnlabeledValueArg<long> *>(A);
        a->val.u.l = B->getValue();
      }
        break;
      case BU_ARG_STRING: {
        UnlabeledValueArg<string> *B = dynamic_cast<UnlabeledValueArg<string> *>(A);
        // a->val.u.s is a bu_vsl_t
        bu_vls_sprintf(&(a->val.u.s), B->getValue().c_str());
      }
        break;
      default:
        // error
        break;
  }
}

/* not yet ready
void
extract_MultiArg_data(bu_arg_vars *a, Arg *A)
{
  MultiArg *B = dynamic_cast<MultiArg*>(A);
}


void
extract_MultiSwitchArg_data(bu_arg_vars *a, Arg *A)
{
  MultiSwitchArg *B = dynamic_cast<MultiSwitchArg*>(A);
}

void
extract_UnlabeledMultiArg_data(bu_arg_vars *a, Arg *A)
{
  UnlabeledMultiArg *B = dynamic_cast<UnlabeledMultiArg*>(A);
}

void
extract_ValueArg_data(bu_arg_vars *a, Arg *A)
{
  ValueArg *B = dynamic_cast<ValueArg*>(A);
}

*/

extern "C" void
bu_arg_vls_free(bu_arg_vars *arg)
{
  // frees memory used by bu_vls_t vars in the arg
  if (arg->flag.vls_str)
    BU_FREE(arg->flag.vls_str, char);
  if (arg->name.vls_str)
    BU_FREE(arg->name.vls_str, char);
  if (arg->desc.vls_str)
    BU_FREE(arg->desc.vls_str, char);
  if (arg->val.u.s.vls_str)
    BU_FREE(arg->val.u.s.vls_str, char);
}

extern "C" void
bu_arg_free(bu_ptbl_t *args)
{
  BU_CK_PTBL(args);
  /* pointer to access args table; */
  bu_arg_vars **ap;
  for (BU_PTBL_FOR(ap, (bu_arg_vars **), args)) {
    bu_arg_vars *a = *ap;

    // free all heap mem in a bu_arg_vars
    bu_arg_vls_free(a);

    // free a
    if (a)
      BU_FREE(a, bu_arg_vars);
  }

  // now free the ptbl memory
  BU_FREE(args->buffer, long*);
}
