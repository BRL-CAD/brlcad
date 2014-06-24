/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <gecode/driver.hh>

#include <iostream>
#include <iomanip>

#include <cstdlib>
#include <cstring>

namespace Gecode {

  namespace Driver {

    /*
     * Option baseclass
     *
     */
    char*
    BaseOption::strdup(const char* s) {
      if (s == NULL)
        return NULL;
      char* d = heap.alloc<char>(static_cast<unsigned long int>(strlen(s)+1));
      (void) strcpy(d,s);
      return d;
    }

    void
    BaseOption::strdel(const char* s) {
      if (s == NULL)
        return;
      heap.rfree(const_cast<char*>(s));
    }

    char*
    BaseOption::argument(int argc, char* argv[]) const {
      if ((argc < 2) || strcmp(argv[1],opt))
        return NULL;
      if (argc == 2) {
        std::cerr << "Missing argument for option \"" << opt << "\"" 
                  << std::endl;
        exit(EXIT_FAILURE);
      }
      return argv[2];
    }

    BaseOption::BaseOption(const char* o, const char* e)
      : opt(strdup(o)), exp(strdup(e)) {}

    BaseOption::~BaseOption(void) {
      strdel(opt);
      strdel(exp);
    }


    StringValueOption::StringValueOption(const char* o, const char* e, 
                                         const char* v)
      : BaseOption(o,e), cur(strdup(v)) {}
    void
    StringValueOption::value(const char* v) {
      strdel(cur);
      cur = strdup(v);
    }
    int
    StringValueOption::parse(int argc, char* argv[]) {
      if (char* a = argument(argc,argv)) {
        cur = strdup(a);
        return 2;
      }
      return 0;
    }
    void
    StringValueOption::help(void) {
      std::cerr << '\t' << opt << " (string) default: " 
                << ((cur == NULL) ? "NONE" : cur) << std::endl
                << "\t\t" << exp << std::endl;
    }
    StringValueOption::~StringValueOption(void) {
      strdel(cur);
    }
  


    void
    StringOption::add(int v, const char* o, const char* h) {
      Value* n = new Value;
      n->val  = v;
      n->opt  = strdup(o);
      n->help = strdup(h);
      n->next = NULL;
      if (fst == NULL) {
        fst = n;
      } else {
        lst->next = n;
      }
      lst = n;
    }
    int
    StringOption::parse(int argc, char* argv[]) {
      if (char* a = argument(argc,argv)) {
        for (Value* v = fst; v != NULL; v = v->next)
          if (!strcmp(a,v->opt)) {
            cur = v->val;
            return 2;
          }
        std::cerr << "Wrong argument \"" << a
                  << "\" for option \"" << opt << "\""
                  << std::endl;
        exit(EXIT_FAILURE);
      }
      return 0;
    }
    void
    StringOption::help(void) {
      if (fst == NULL)
        return;
      std::cerr << '\t' << opt << " (";
      const char* d = NULL;
      for (Value* v = fst; v != NULL; v = v->next) {
        std::cerr << v->opt << ((v->next != NULL) ? ", " : "");
        if (v->val == cur)
          d = v->opt;
      }
      std::cerr << ")";
      if (d != NULL)
        std::cerr << " default: " << d;
      std::cerr << std::endl << "\t\t" << exp << std::endl;
      for (Value* v = fst; v != NULL; v = v->next)
        if (v->help != NULL)
          std::cerr << "\t\t  " << v->opt << ": " << v->help << std::endl;
    }
    
    StringOption::~StringOption(void) {
      Value* v = fst;
      while (v != NULL) {
        strdel(v->opt);
        strdel(v->help);
        Value* n = v->next;
        delete v;
        v = n;
      }
    }
    
    
    int
    IntOption::parse(int argc, char* argv[]) {
      if (char* a = argument(argc,argv)) {
        cur = atoi(a);
        return 2;
      }
      return 0;
    }
    
    void
    IntOption::help(void) {
      std::cerr << '\t' << opt << " (int) default: " << cur << std::endl
                << "\t\t" << exp << std::endl;
    }
  

    int
    UnsignedIntOption::parse(int argc, char* argv[]) {
      if (char* a = argument(argc,argv)) {
        cur = static_cast<unsigned int>(atoi(a));
        return 2;
      }
      return 0;
    }
    
    void
    UnsignedIntOption::help(void) {
      std::cerr << '\t' << opt << " (unsigned int) default: " 
                << cur << std::endl
                << "\t\t" << exp << std::endl;
    }
  

    int
    DoubleOption::parse(int argc, char* argv[]) {
      if (char* a = argument(argc,argv)) {
        cur = atof(a);
        return 2;
      }
      return 0;
    }
    
    void
    DoubleOption::help(void) {
      using namespace std;
      cerr << '\t' << opt << " (double) default: " << cur << endl
           << "\t\t" << exp << endl;
    }

    int
    BoolOption::parse(int argc, char* argv[]) {
      if ((argc < 2) || strcmp(argv[1],opt))
        return 0;
      if (argc == 2) {
        // Option without argument
        cur = true;
        return 1;
      } else if (!strcmp(argv[2],"true") || !strcmp(argv[2],"1")) {
        cur = true;
        return 2;
      } else if (!strcmp(argv[2],"false") || !strcmp(argv[2],"0")) {
        cur = false;
        return 2;
      } else {
        // Option without argument
        cur = true;
        return 1;
      }
      return 0;
    }

    void 
    BoolOption::help(void) {
      using namespace std;
      cerr << '\t' << opt << " (optional: false, 0, true, 1) default: " 
           << (cur ? "true" : "false") << endl 
           << "\t\t" << exp << endl;
    }

  
  }

  void
  BaseOptions::add(Driver::BaseOption& o) {
    o.next = NULL;
    if (fst == NULL) {
      fst=&o;
    } else {
      lst->next=&o;
    }
    lst=&o;
  }
  BaseOptions::BaseOptions(const char* n)
    : fst(NULL), lst(NULL), 
      _name(Driver::BaseOption::strdup(n)) {}

  void
  BaseOptions::name(const char* n) {
    Driver::BaseOption::strdel(_name);
    _name = Driver::BaseOption::strdup(n);
  }

  void
  BaseOptions::help(void) {
    std::cerr << "Gecode configuration information:" << std::endl
              << " - Version: " << GECODE_VERSION << std::endl
              << " - Variable types: ";
#ifdef GECODE_HAS_INT_VARS
    std::cerr << "BoolVar IntVar ";
#endif
#ifdef GECODE_HAS_SET_VARS
    std::cerr << "SetVar ";
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    std::cerr << "FloatVar "
              << std::endl
              << " - Trigonometric and transcendental float constraints: ";
#ifdef GECODE_HAS_MPFR
    std::cerr  << "enabled";
#else
    std::cerr << "disabled";
#endif
#endif
    std::cerr << std::endl;
    std::cerr << " - Thread support: ";
#ifdef GECODE_HAS_THREADS
    if (Support::Thread::npu() == 1)
      std::cerr << "enabled (1 processing unit)";
    else
      std::cerr << "enabled (" << Support::Thread::npu() 
                << " processing units)";
#else
    std::cerr << "disabled";
#endif
    std::cerr << std::endl
              << " - Gist support: ";
#ifdef GECODE_HAS_GIST
    std::cerr << "enabled";
#else
    std::cerr << "disabled";
#endif
    std::cerr << std::endl << std::endl
              << "Options for " << name() << ":" << std::endl
              << "\t-help, --help, -?" << std::endl
              << "\t\tprint this help message" << std::endl;
    for (Driver::BaseOption* o = fst; o != NULL; o = o->next)
      o->help();
  }

  void
  BaseOptions::parse(int& argc, char* argv[]) {
    int c = argc;
    char** v = argv;
  next:
    for (Driver::BaseOption* o = fst; o != NULL; o = o->next)
      if (int a = o->parse(c,v)) {
        c -= a; v += a;
        goto next;
      }
    if (c >= 2) {
      if (!strcmp(v[1],"-help") || !strcmp(v[1],"--help") ||
          !strcmp(v[1],"-?")) {
        help();
        exit(EXIT_SUCCESS);
      }
    }
    // Copy remaining arguments
    argc = c;
    for (int i=1; i<argc; i++)
      argv[i] = v[i];
    return;
  }
  
  BaseOptions::~BaseOptions(void) {
    Driver::BaseOption::strdel(_name);
  }


  Options::Options(const char* n)
    : BaseOptions(n),
      
      _model("-model","model variants"),
      _symmetry("-symmetry","symmetry variants"),
      _propagation("-propagation","propagation variants"),
      _icl("-icl","integer consistency level",ICL_DEF),
      _branching("-branching","branching variants"),
      _decay("-decay","decay factor",1.0),
      
      _search("-search","search engine variants"),
      _solutions("-solutions","number of solutions (0 = all)",1),
      _threads("-threads","number of threads (0 = #processing units)",
               Search::Config::threads),
      _c_d("-c-d","recomputation commit distance",Search::Config::c_d),
      _a_d("-a-d","recomputation adaptation distance",Search::Config::a_d),
      _node("-node","node cutoff (0 = none, solution mode)"),
      _fail("-fail","failure cutoff (0 = none, solution mode)"),
      _time("-time","time (in ms) cutoff (0 = none, solution mode)"),
      _restart("-restart","restart sequence type",RM_NONE),
      _r_base("-restart-base","base for geometric restart sequence",1.5),
      _r_scale("-restart-scale","scale factor for restart sequence",250),
      _nogoods("-nogoods","whether to use no-goods from restarts",false),
      _nogoods_limit("-nogoods-limit","depth limit for no-good extraction",
                     Search::Config::nogoods_limit),
      _interrupt("-interrupt","whether to catch Ctrl-C (true) or not (false)",
                 true),
      
      _mode("-mode","how to execute script",SM_SOLUTION),
      _samples("-samples","how many samples (time mode)",1),
      _iterations("-iterations","iterations per sample (time mode)",1),
      _print_last("-print-last",
                  "whether to only print the last solution (solution mode)",
                  false),
      _out_file("-file-sol", "where to print solutions "
                "(supports stdout, stdlog, stderr)","stdout"),
      _log_file("-file-stat", "where to print statistics "
                "(supports stdout, stdlog, stderr)","stdout")
  {
    
    _icl.add(ICL_DEF, "def"); _icl.add(ICL_VAL, "val");
    _icl.add(ICL_BND, "bnd"); _icl.add(ICL_DOM, "dom");
    
    _mode.add(SM_SOLUTION, "solution");
    _mode.add(SM_TIME, "time");
    _mode.add(SM_STAT, "stat");
    _mode.add(SM_GIST, "gist");
    
    _restart.add(RM_NONE,"none");
    _restart.add(RM_CONSTANT,"constant");
    _restart.add(RM_LINEAR,"linear");
    _restart.add(RM_LUBY,"luby");
    _restart.add(RM_GEOMETRIC,"geometric");
    
    add(_model); add(_symmetry); add(_propagation); add(_icl); 
    add(_branching); add(_decay);
    add(_search); add(_solutions); add(_threads); add(_c_d); add(_a_d);
    add(_node); add(_fail); add(_time); add(_interrupt);
    add(_restart); add(_r_base); add(_r_scale); 
    add(_nogoods); add(_nogoods_limit);
    add(_mode); add(_iterations); add(_samples); add(_print_last);
    add(_out_file); add(_log_file);
  }

  
  SizeOptions::SizeOptions(const char* e)
    : Options(e), _size(0) {}
  
  void
  SizeOptions::help(void) {
    Options::help();
    std::cerr << "\t(unsigned int) default: " << size() << std::endl
              << "\t\twhich version/size for script" << std::endl;
  }

  void
  SizeOptions::parse(int& argc, char* argv[]) {
    Options::parse(argc,argv);
    if (argc < 2)
      return;
    size(static_cast<unsigned int>(atoi(argv[1])));
  }



  InstanceOptions::InstanceOptions(const char* e)
    : Options(e), _inst(NULL) {}
  
  void
  InstanceOptions::instance(const char* s) {
    Driver::BaseOption::strdel(_inst);
    _inst = Driver::BaseOption::strdup(s);
  }

  void
  InstanceOptions::help(void) {
    Options::help();
    std::cerr << "\t(string) default: " << instance() << std::endl
              << "\t\twhich instance for script" << std::endl;
  }

  void
  InstanceOptions::parse(int& argc, char* argv[]) {
    Options::parse(argc,argv);
    if (argc < 2)
      return;
    instance(argv[1]);
  }

  InstanceOptions::~InstanceOptions(void) {
    Driver::BaseOption::strdel(_inst);
  }

}

// STATISTICS: driver-any
