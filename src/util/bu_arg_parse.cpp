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
#include <string>
#include <sstream>
#include <fstream>

using namespace std;
using namespace TCLAP;

// local vars
static bool _debug(false);
//static bool _debug(true);

// using ideas from Cliff and Sean...
// local funcs
static bu_arg_t _get_arg_type(void *arg);
static bu_arg_vars *bu_arg_init();
static std::string _get_fname(void *arg);
static void _write_bool(void *addr, const bool b);
static void _write_long(void *addr, const long l);
static void _write_double(void *addr, const double d);
static void _write_string(void *addr, const std::string& s);

// for static data handling
static void _insert_bool(void *addr, const bool b);
static void _insert_long(void *addr, const long l);
static void _insert_double(void *addr, const double d);
static void _insert_string(void *addr, const std::string& s);

static bool _read_bool(void *addr);
static long _read_long(void *addr);
static double _read_double(void *addr);
static std::string _read_string(void *addr);

/* not yet ready
static Arg *_handle_MultiArg(bu_arg_vars *a, CmdLine &cmd);
static Arg *_handle_MultiSwitchArg(bu_arg_vars *a, CmdLine &cmd);
static Arg *_handle_UnlabeledMultiArg(bu_arg_vars *a, CmdLine &cmd);
static Arg *_handle_ValueArg(bu_arg_vars *a, CmdLine &cmd);
*/

static Arg *_handle_SwitchArg(bu_arg_vars *a, CmdLine &cmd);
static Arg *_handle_UnlabeledValueArg(bu_arg_vars *a, CmdLine &cmd);

static Arg *_handle_SwitchArg2(bu_arg_switch_t *a, CmdLine &cmd);
static Arg *_handle_UnlabeledValueArg2(bu_arg_unlabeled_value_t *a, CmdLine &cmd);

/* not yet ready
static void _extract_MultiArg_data(bu_arg_vars *a, Arg *A);
static void _extract_MultiSwitchArg_data(bu_arg_vars *a, Arg *A);
static void _extract_UnlabeledMultiArg_data(bu_arg_vars *a, Arg *A);
static void _extract_ValueArg_data(bu_arg_vars *a, Arg *A);
*/

static void _extract_SwitchArg_data(bu_arg_vars *a, Arg *A);
static void _extract_UnlabeledValueArg_data(bu_arg_vars *a, Arg *A);

static void _extract_SwitchArg_data2(bu_arg_switch_t *a, Arg *A);
static void _extract_UnlabeledValueArg_data2(bu_arg_unlabeled_value_t *a, Arg *A);


/**
 * need vec for deleting Arg pointers when done
 */
static vector<Arg*> _Arg_pointers;

/**
 * get a value for an arg
 */
int
bu_arg_get_bool(bu_arg_vars *arg)
{
  return (arg->val.u.l ? 1 : 0);
}

long
bu_arg_get_long(bu_arg_vars *arg)
{
  return arg->val.u.l;
}

double
bu_arg_get_double(bu_arg_vars *arg)
{
  return arg->val.u.d;
}

const char *
bu_arg_get_string(bu_arg_vars *arg)
{
  return arg->val.u.s.vls_str;
}

/**
 * general constructor for bu_arg_vars
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
bu_arg_switch(const char *flag,
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
bu_arg_unlabeled_value(const char *name,
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
        BU_ASSERT(0 && "tried to use non-existent BU_ARG value type\n");
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
            A = _handle_SwitchArg(a, cmd);
            break;
          case BU_ARG_UnlabeledValueArg:
            A = _handle_UnlabeledValueArg(a, cmd);
            break;
/* not yet ready
          case BU_ARG_MultiArg:
            A = _handle_MultiArg(a, cmd);
            break;
          case BU_ARG_MultiSwitchArg:
            A = _handle_MultiSwitchArg(a, cmd);
            break;
          case BU_ARG_UnlabeledMultiArg:
            A = _handle_UnlabeledMultiArg(a, cmd);
            break;
          case BU_ARG_ValueArg:
            A = _handle_ValueArg(a, cmd);
            break;
*/
          default:
             // error
            BU_ASSERT(0 && "tried to use non-handled BU_ARG type\n");
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
            _extract_SwitchArg_data(a, A);
            break;
          case BU_ARG_UnlabeledValueArg:
            _extract_UnlabeledValueArg_data(a, A);
            break;
/* not yet ready
          case BU_ARG_MultiArg:
            _extract_MultiArg_data(a, cmd);
            break;
          case BU_ARG_MultiSwitchArg:
            _extract_MultiSwitchArg_data(a, A);
            break;
          case BU_ARG_UnlabeledMultiArg:
            _extract_UnlabeledMultiArg_data(a, A);
            break;
          case BU_ARG_ValueArg:
            _extract_ValueArg_data(a, A);
            break;
*/
          default:
             // error
            BU_ASSERT(0 && "tried to use non-existent BU_ARG type\n");
            break;
      }
    }

  } catch (TCLAP::ArgException &e) {
    // catch and handle any exceptions

    cerr << "error: " << e.error() << " for arg " << e.argId() << endl;

    // cleanup
    if (!_Arg_pointers.empty())
      for (unsigned i = 0; i < _Arg_pointers.size(); ++i) {
        delete _Arg_pointers[i];
        _Arg_pointers[i] = 0;
      }

    return BU_ARG_PARSE_ERR;
  }

  // successful parse, clean up heap memory used
  if (!_Arg_pointers.empty())
    for (unsigned i = 0; i < _Arg_pointers.size(); ++i) {
      delete _Arg_pointers[i];
      _Arg_pointers[i] = 0;
    }

  return retval;

} // bu_opt_parse

Arg *
_handle_SwitchArg(bu_arg_vars *a, CmdLine &cmd)
{
  SwitchArg *A = new SwitchArg(a->flag.vls_str, a->name.vls_str, a->desc.vls_str, a->val.u.l);

  _Arg_pointers.push_back(A);
  cmd.add(A);

  return A;
}

Arg *
_handle_SwitchArg2(bu_arg_switch_t *a, CmdLine &cmd)
{
  SwitchArg *A = new SwitchArg(a->flag, a->name, a->desc);

  _Arg_pointers.push_back(A);
  cmd.add(A);

  return A;
}

Arg *
_handle_SwitchArg4(bu_arg_switch_t *a, CmdLine &cmd)
{
  SwitchArg *A = new SwitchArg(a->flag, a->name, a->desc);

  _Arg_pointers.push_back(A);
  cmd.add(A);

  return A;
}

Arg *
_handle_UnlabeledValueArg(bu_arg_vars *a, CmdLine &cmd)
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
  _Arg_pointers.push_back(A);
  return A;
}

Arg *
_handle_UnlabeledValueArg2(bu_arg_unlabeled_value_t *a, CmdLine &cmd)
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
} // _handle_UnlabeledValueArg2

Arg *
_handle_UnlabeledValueArg4(bu_arg_unlabeled_value_t *a, CmdLine &cmd)
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
} // _handle_UnlabeledValueArg4


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

void
_extract_SwitchArg_data(bu_arg_vars *a, Arg *A)
{
  SwitchArg *B = dynamic_cast<SwitchArg*>(A);
  if (a->val.typ == BU_ARG_BOOL) {
    bool val = B->getValue();
    a->val.u.l = (val ? 1 : 0);
   }
  else {
    // error
    BU_ASSERT(0 && "tried to use non-existent BU_ARG bool type");
  }
}

void
_extract_UnlabeledValueArg_data(bu_arg_vars *a, Arg *A)
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
        BU_ASSERT(0 && "tried to use non-existent BU_ARG value type\n");
        break;
  }
}

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

extern "C" void
bu_arg_exit(const int status, const char *msg, bu_ptbl_t *args)
{
  bu_arg_free(args);
  bu_exit(status, msg);
}

// funcs below are for the static init API
// type 2 ================================
extern "C" int
bu_arg_get_bool2(void *arg)
{
  return _read_bool(arg);
}

extern "C" long
bu_arg_get_long2(void *arg)
{
  return _read_long(arg);
}

extern "C" double
bu_arg_get_double2(void *arg)
{
  return _read_double(arg);
}

extern "C" void
bu_arg_get_string2(void *arg, char buf[], const size_t buflen)
{
  string s = _read_string(arg);
  snprintf(buf, buflen, "%s", s.c_str());
}

extern "C" int
bu_arg_parse2(void *args[], int argc, char * const argv[])
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
            A = _handle_SwitchArg2((bu_arg_switch_t *)a, cmd);
            break;
          case BU_ARG_UnlabeledValueArg:
            A = _handle_UnlabeledValueArg2((bu_arg_unlabeled_value_t *)a, cmd);
            break;
/* not yet ready
          case BU_ARG_MultiArg:
            A = _handle_MultiArg(a, cmd);
            break;
          case BU_ARG_MultiSwitchArg:
            A = _handle_MultiSwitchArg(a, cmd);
            break;
          case BU_ARG_UnlabeledMultiArg:
            A = _handle_UnlabeledMultiArg(a, cmd);
            break;
          case BU_ARG_ValueArg:
            A = _handle_ValueArg(a, cmd);
            break;
*/
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
      void *a = args[i];
      bu_arg_t arg_type = _get_arg_type(a);
      switch (arg_type) {
          case BU_ARG_SwitchArg:
            _extract_SwitchArg_data2((bu_arg_switch_t *)a, A);
            break;
          case BU_ARG_UnlabeledValueArg:
            _extract_UnlabeledValueArg_data2((bu_arg_unlabeled_value_t *)a, A);
            break;
/* not yet ready
          case BU_ARG_MultiArg:
            _extract_MultiArg_data(a, cmd);
            break;
          case BU_ARG_MultiSwitchArg:
            _extract_MultiSwitchArg_data(a, A);
            break;
          case BU_ARG_UnlabeledMultiArg:
            _extract_UnlabeledMultiArg_data(a, A);
            break;
          case BU_ARG_ValueArg:
            _extract_ValueArg_data(a, A);
            break;
*/
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

} // bu_arg_parse2

// type 4 ================================
extern "C" int
bu_arg_get_bool4(void *arg)
{
  bu_arg_general_t *t = (bu_arg_general_t *)arg;
  return t->retval.u4.l;
}

extern "C" long
bu_arg_get_long4(void *arg)
{
  bu_arg_general_t *t = (bu_arg_general_t *)arg;
  return t->retval.u4.l;
}

extern "C" double
bu_arg_get_double4(void *arg)
{
  bu_arg_general_t *t = (bu_arg_general_t *)arg;
  return t->retval.u4.d;
}

extern "C" void
bu_arg_get_string4(void *arg, char buf[])
{
  bu_arg_general_t *t = (bu_arg_general_t *)arg;
  bu_strlcpy(buf, t->retval.buf, BU_ARG_PARSE_BUFSZ);
}

extern "C" int
bu_arg_parse4(void *args[], int argc, char * const argv[])
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
            A = _handle_SwitchArg4((bu_arg_switch_t *)a, cmd);
            break;
          case BU_ARG_UnlabeledValueArg:
            A = _handle_UnlabeledValueArg4((bu_arg_unlabeled_value_t *)a, cmd);
            break;
/* not yet ready
          case BU_ARG_MultiArg:
            A = _handle_MultiArg(a, cmd);
            break;
          case BU_ARG_MultiSwitchArg:
            A = _handle_MultiSwitchArg(a, cmd);
            break;
          case BU_ARG_UnlabeledMultiArg:
            A = _handle_UnlabeledMultiArg(a, cmd);
            break;
          case BU_ARG_ValueArg:
            A = _handle_ValueArg(a, cmd);
            break;
*/
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
      void *a = args[i];
      bu_arg_t arg_type = _get_arg_type(a);
      switch (arg_type) {
          case BU_ARG_SwitchArg:
            _extract_SwitchArg_data2((bu_arg_switch_t *)a, A);
            break;
          case BU_ARG_UnlabeledValueArg:
            _extract_UnlabeledValueArg_data2((bu_arg_unlabeled_value_t *)a, A);
            break;
/* not yet ready
          case BU_ARG_MultiArg:
            _extract_MultiArg_data(a, cmd);
            break;
          case BU_ARG_MultiSwitchArg:
            _extract_MultiSwitchArg_data(a, A);
            break;
          case BU_ARG_UnlabeledMultiArg:
            _extract_UnlabeledMultiArg_data(a, A);
            break;
          case BU_ARG_ValueArg:
            _extract_ValueArg_data(a, A);
            break;
*/
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

} // bu_arg_parse4

std::string
_get_fname(void *addr)
{
#ifdef HAVE_UNISTD_H
  // get file name from address and pid of parent process
  static pid_t pid = getppid();

  std::string num1, num2;
  std::stringstream str1, str2;

  str1 << addr;
  str1 >> num1;
  str2 << pid;
  str2 >> num2;

  return "tmp" + num1 + num2;
#else

  std::string num1;
  std::stringstream str1;

  str1 << addr;
  str1 >> num1;

  return "tmp" + num1;
#endif
}


//====== for static init ==========
void
_extract_SwitchArg_data2(bu_arg_switch_t *a, Arg *A)
{
  SwitchArg *B = dynamic_cast<SwitchArg*>(A);
  bool val = B->getValue();
  _write_bool(a, val);
}
void
_extract_SwitchArg_data4(bu_arg_switch_t *a, Arg *A)
{
  SwitchArg *B = dynamic_cast<SwitchArg*>(A);
  bool val = B->getValue();
  _insert_bool(a, val);
}

void
_extract_UnlabeledValueArg_data2(bu_arg_unlabeled_value_t *a, Arg *A)
{
  // this is a templated type
  bu_arg_valtype_t val_type = a->val_typ;
  switch (val_type) {
      case BU_ARG_BOOL: {
        UnlabeledValueArg<bool> *B = dynamic_cast<UnlabeledValueArg<bool> *>(A);
        bool val = B->getValue();
        _write_bool(a, val);
      }
        break;
      case BU_ARG_DOUBLE: {
        UnlabeledValueArg<double> *B = dynamic_cast<UnlabeledValueArg<double> *>(A);
        double val = B->getValue();
        _write_double(a, val);
      }
        break;
      case BU_ARG_LONG: {
        UnlabeledValueArg<long> *B = dynamic_cast<UnlabeledValueArg<long> *>(A);
        long val = B->getValue();
        _write_long(a, val);
      }
        break;
      case BU_ARG_STRING: {
        UnlabeledValueArg<string> *B = dynamic_cast<UnlabeledValueArg<string> *>(A);
        string val = B->getValue();
        _write_string(a, val);
      }
        break;
      default:
        // error
        BU_ASSERT(0 && "tried to use non-existent BU_ARG value type\n");
        break;
  }
} // _extract_UnlabeledValueArg_data2

void
_extract_UnlabeledValueArg_data4(bu_arg_unlabeled_value_t *a, Arg *A)
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
} // _extract_UnlabeledValueArg_data4

bool
_read_bool(void *addr)
{
  string fname = _get_fname(addr);
  // FIXME: need error checking (existence, good open, etc.)
  ifstream fi(fname.c_str());

  bool b;
  fi >> b;

  // normally we no longer need this file
  if (!_debug)
    bu_file_delete(fname.c_str());

  return b;
}

long
_read_long(void *addr)
{
  string fname = _get_fname(addr);
  // FIXME: need error checking (existence, good open, etc.)
  ifstream fi(fname.c_str());

  long l;
  fi >> l;

  // normally we no longer need this file
  if (!_debug)
    bu_file_delete(fname.c_str());

  return l;
}

double
_read_double(void *addr)
{
  string fname = _get_fname(addr);
  // FIXME: need error checking (existence, good open, etc.)
  ifstream fi(fname.c_str());

  double d;
  fi >> d;

  // normally we no longer need this file
  if (!_debug)
    bu_file_delete(fname.c_str());

  return d;
}

std::string
_read_string(void *addr)
{
  string fname = _get_fname(addr);
  // FIXME: need error checking (existence, good open, etc.)
  ifstream fi(fname.c_str());

  std::string s;
  fi >> s;

  // normally we no longer need this file
  if (!_debug)
    bu_file_delete(fname.c_str());

  return s;
}

void
_insert_bool(void *addr, const bool b)
{
  bu_arg_general_t *t = (bu_arg_general_t *)addr;
  if (b)
    t->retval.u4.l = 1L;
  else
    t->retval.u4.l = 0L;
}

void
_insert_long(void *addr, const long l)
{
  bu_arg_general_t *t = (bu_arg_general_t *)addr;
  t->retval.u4.l = l;
}

void
_insert_double(void *addr, const double d)
{
  bu_arg_general_t *t = (bu_arg_general_t *)addr;
  t->retval.u4.d = d;
}

void
_insert_string(void *addr, const std::string& s)
{
  bu_arg_general_t *t = (bu_arg_general_t *)addr;
  bu_strlcpy(t->retval.buf, s.c_str(), BU_ARG_PARSE_BUFSZ);
}

void
_write_bool(void *addr, const bool b)
{
  string fname = _get_fname(addr);
  // FIXME: need error checking (existence, good open, etc.)
  ofstream fo(fname.c_str());

  fo << b;
}

void
_write_long(void *addr, const long l)
{
  string fname = _get_fname(addr);
  // FIXME: need error checking (existence, good open, etc.)
  ofstream fo(fname.c_str());

  fo << l;
}

void
_write_double(void *addr, const double d)
{
  string fname = _get_fname(addr);
  // FIXME: need error checking (existence, good open, etc.)
  ofstream fo(fname.c_str());

  fo << d;
}

void
_write_string(void *addr, const std::string& s)
{
  string fname = _get_fname(addr);
  // FIXME: need error checking (existence, good open, etc.)
  ofstream fo(fname.c_str());

  fo << s;
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
