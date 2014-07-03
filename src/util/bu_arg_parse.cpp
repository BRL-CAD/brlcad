/*                 B U _ O P T _ P A R S E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
#include <string>
#include <sstream>
#include <fstream>

using namespace std;
using namespace TCLAP;

// local funcs
static bu_arg_t _get_arg_type(void *arg);

// for static data handling
static void _insert_bool(void *addr, const bool b);
static void _insert_long(void *addr, const long l);
static void _insert_double(void *addr, const double d);
static void _insert_string(void *addr, const std::string& s);

static void _extract_SwitchArg_data(bu_arg_switch_t *a, Arg *A);
static void _extract_UnlabeledValueArg_data(bu_arg_unlabeled_value_t *a, Arg *A);

static Arg *_handle_UnlabeledValueArg(bu_arg_unlabeled_value_t *a, CmdLine &cmd);
static Arg *_handle_SwitchArg(bu_arg_switch_t *a, CmdLine &cmd);

// not yet ready
//static Arg *_handle_MultiArg(bu_arg_vars *a, CmdLine &cmd);
//static Arg *_handle_MultiSwitchArg(bu_arg_vars *a, CmdLine &cmd);
//static Arg *_handle_UnlabeledMultiArg(bu_arg_vars *a, CmdLine &cmd);
//static Arg *_handle_ValueArg(bu_arg_vars *a, CmdLine &cmd);
//static void _extract_MultiArg_data(bu_arg_vars *a, Arg *A);
//static void _extract_MultiSwitchArg_data(bu_arg_vars *a, Arg *A);
//static void _extract_UnlabeledMultiArg_data(bu_arg_vars *a, Arg *A);
//static void _extract_ValueArg_data(bu_arg_vars *a, Arg *A);

/**
 * need vec for deleting Arg pointers when done
 */
static vector<Arg*> _Arg_pointers;

Arg *
_handle_SwitchArg(bu_arg_switch_t *a, CmdLine &cmd)
{
  SwitchArg *A = new SwitchArg(a->flag, a->name, a->desc);

  _Arg_pointers.push_back(A);
  cmd.add(A);

  return A;
}

Arg *
_handle_UnlabeledValueArg(bu_arg_unlabeled_value_t *a, CmdLine &cmd)
{

  // this is a templated type
  bu_arg_valtype_t val_type = a->val_typ;
  string type_desc;
  Arg *A = 0;
  switch (val_type) {
      case BU_ARG_BOOL: {
        bool val = bu_str_true(a->def_val);
        type_desc = "bool";
        A = new UnlabeledValueArg<bool>(a->name, a->desc, a->req, val, type_desc);
      }
        break;
      case BU_ARG_DOUBLE: {
        double val = strtod(a->def_val, NULL);
        type_desc = "double";
        A = new UnlabeledValueArg<double>(a->name, a->desc, a->req, val, type_desc);
      }
        break;
      case BU_ARG_LONG: {
        long val = strtol(a->def_val, NULL, 10);
        type_desc = "long";
        A = new UnlabeledValueArg<long>(a->name, a->desc, a->req, val, type_desc);
      }
        break;
      case BU_ARG_STRING: {
        string val = a->def_val;
        type_desc = "string";
        A = new UnlabeledValueArg<string>(a->name, a->desc, a->req, val, type_desc);
      }
        break;
      default: {
        string val = a->def_val;
        type_desc = "string";
        A = new UnlabeledValueArg<string>(a->name, a->desc, a->req, val, type_desc);
      }
        break;
  }
  cmd.add(A);
  _Arg_pointers.push_back(A);
  return A;
} // _handle_UnlabeledValueArg


/* not yet ready
Arg *
_handle_MultiArg(bu_arg_vars *a, CmdLine &cmd)
{
  MultiArg *A = new MultiArg();

  _Arg_pointers.push_back(A);
  return A;
}

Arg *
_handle_MultiSwitchArg(bu_arg_vars *a, CmdLine &cmd)
{
  MultiSwitchArg *A = new MultiSwitchArg();

  _Arg_pointers.push_back(A);
  return A;
}

Arg *
_handle_UnlabeledMultiArg(bu_arg_vars *a, CmdLine &cmd)
{
  UnlabeledMultiArg *A = new UnlabeledMultiArg();

  _Arg_pointers.push_back(A);
  return A;
}

Arg *
_handle_ValueArg(bu_arg_vars *a, CmdLine &cmd)
{
  ValueArg *A = new ValueArg();

  _Arg_pointers.push_back(A);
  return A;
}
*/


/* not yet ready
void
_extract_MultiArg_data(bu_arg_vars *a, Arg *A)
{
  MultiArg *B = dynamic_cast<MultiArg*>(A);
}


void
_extract_MultiSwitchArg_data(bu_arg_vars *a, Arg *A)
{
  MultiSwitchArg *B = dynamic_cast<MultiSwitchArg*>(A);
}

void
_extract_UnlabeledMultiArg_data(bu_arg_vars *a, Arg *A)
{
  UnlabeledMultiArg *B = dynamic_cast<UnlabeledMultiArg*>(A);
}

void
_extract_ValueArg_data(bu_arg_vars *a, Arg *A)
{
  ValueArg *B = dynamic_cast<ValueArg*>(A);
}

*/


extern "C" int
bu_arg_get_bool(void *arg)
{
  bu_arg_general_t *t = (bu_arg_general_t *)arg;
  return t->retval.u.l;
}

extern "C" long
bu_arg_get_long(void *arg)
{
  bu_arg_general_t *t = (bu_arg_general_t *)arg;
  return t->retval.u.l;
}

extern "C" double
bu_arg_get_double(void *arg)
{
  bu_arg_general_t *t = (bu_arg_general_t *)arg;
  return t->retval.u.d;
}

extern "C" void
bu_arg_get_string(void *arg, char buf[])
{
  bu_arg_general_t *t = (bu_arg_general_t *)arg;
  bu_strlcpy(buf, t->retval.buf, BU_ARG_PARSE_BUFSZ);
}

extern "C" int
bu_arg_parse(void *args[], int argc, char * const argv[])
{

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

    int i = 0;
    while (args[i]) {
      // handle this arg and fill in the values
      // map inputs to TCLAP:
      Arg *A         = 0;
      void *a = args[i];
      bu_arg_t arg_type = _get_arg_type(a);
      switch (arg_type) {
          case BU_ARG_SwitchArg:
            A = _handle_SwitchArg((bu_arg_switch_t *)a, cmd);
            break;
          case BU_ARG_UnlabeledValueArg:
            A = _handle_UnlabeledValueArg((bu_arg_unlabeled_value_t *)a, cmd);
            break;
// not yet ready
//          case BU_ARG_MultiArg:
//            A = _handle_MultiArg(a, cmd);
//            break;
//          case BU_ARG_MultiSwitchArg:
//            A = _handle_MultiSwitchArg(a, cmd);
//            break;
//          case BU_ARG_UnlabeledMultiArg:
//            A = _handle_UnlabeledMultiArg(a, cmd);
//            break;
//          case BU_ARG_ValueArg:
//            A = _handle_ValueArg(a, cmd);
//            break;

          default:
             // error
            BU_ASSERT(0 && "tried to use non-handled BU_ARG type\n");
            break;
      }

      // save results for later
      tclap_results.push_back(A);

      // next arg
      ++i;
    }

    // parse the args
    cmd.parse(argc, argv);

    for (unsigned j = 0; j < tclap_results.size(); ++j) {
      Arg *A         = tclap_results[j];
      void *a = args[j];
      bu_arg_t arg_type = _get_arg_type(a);
      switch (arg_type) {
          case BU_ARG_SwitchArg:
            _extract_SwitchArg_data((bu_arg_switch_t *)a, A);
            break;
          case BU_ARG_UnlabeledValueArg:
            _extract_UnlabeledValueArg_data((bu_arg_unlabeled_value_t *)a, A);
            break;
// not yet ready
//          case BU_ARG_MultiArg:
//            _extract_MultiArg_data(a, cmd);
//            break;
//          case BU_ARG_MultiSwitchArg:
//            _extract_MultiSwitchArg_data(a, A);
//            break;
//          case BU_ARG_UnlabeledMultiArg:
//            _extract_UnlabeledMultiArg_data(a, A);
//            break;
//          case BU_ARG_ValueArg:
//            _extract_ValueArg_data(a, A);
//            break;

          default:
             // error
            BU_ASSERT(0 && "tried to use non-existent BU_ARG type\n");
            break;
      }
    }

  } catch (TCLAP::ArgException &e) {
    // catch and handle any exceptions

    cerr << "error: " << e.error() << " for arg " << e.argId() << endl;

    return BU_ARG_PARSE_ERR;
  }

  return retval;

} // bu_arg_parse



void
_extract_SwitchArg_data(bu_arg_switch_t *a, Arg *A)
{
  SwitchArg *B = dynamic_cast<SwitchArg*>(A);
  bool val = B->getValue();
  _insert_bool(a, val);
}

void
_extract_UnlabeledValueArg_data(bu_arg_unlabeled_value_t *a, Arg *A)
{
  // this is a templated type
  bu_arg_valtype_t val_type = a->val_typ;
  switch (val_type) {
      case BU_ARG_BOOL: {
        UnlabeledValueArg<bool> *B = dynamic_cast<UnlabeledValueArg<bool> *>(A);
        bool val = B->getValue();
        _insert_bool(a, val);
      }
        break;
      case BU_ARG_DOUBLE: {
        UnlabeledValueArg<double> *B = dynamic_cast<UnlabeledValueArg<double> *>(A);
        double val = B->getValue();
        _insert_double(a, val);
      }
        break;
      case BU_ARG_LONG: {
        UnlabeledValueArg<long> *B = dynamic_cast<UnlabeledValueArg<long> *>(A);
        long val = B->getValue();
        _insert_long(a, val);
      }
        break;
      case BU_ARG_STRING: {
        UnlabeledValueArg<string> *B = dynamic_cast<UnlabeledValueArg<string> *>(A);
        string val = B->getValue();
        _insert_string(a, val);
      }
        break;
      default:
        // error
        BU_ASSERT(0 && "tried to use non-existent BU_ARG value type\n");
        break;
  }
} // _extract_UnlabeledValueArg_data


void
_insert_bool(void *addr, const bool b)
{
  bu_arg_general_t *t = (bu_arg_general_t *)addr;
  if (b)
    t->retval.u.l = 1L;
  else
    t->retval.u.l = 0L;
}

void
_insert_long(void *addr, const long l)
{
  bu_arg_general_t *t = (bu_arg_general_t *)addr;
  t->retval.u.l = l;
}

void
_insert_double(void *addr, const double d)
{
  bu_arg_general_t *t = (bu_arg_general_t *)addr;
  t->retval.u.d = d;
}

void
_insert_string(void *addr, const std::string& s)
{
  bu_arg_general_t *t = (bu_arg_general_t *)addr;
  bu_strlcpy(t->retval.buf, s.c_str(), BU_ARG_PARSE_BUFSZ);
}


bu_arg_t
_get_arg_type(void *arg)
{
  // this only works if arg is a struct with the correct MAGIC in the
  // first field and the type in the second field

  // cast to any one of the magical bu_arg type structs, but switch is
  // the simplest
  bu_arg_switch_t *a = (bu_arg_switch_t *)arg;
  BU_ASSERT(a->magic == BU_ARG_MAGIC);
  bu_arg_t arg_type = a->arg_type;
  return arg_type;
}
