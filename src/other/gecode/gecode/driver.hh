/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2009
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
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

#ifndef __GECODE_DRIVER_HH__
#define __GECODE_DRIVER_HH__

#include <gecode/minimodel.hh>
#include <gecode/search.hh>
#ifdef GECODE_HAS_GIST
#include <gecode/gist.hh>
#endif

/*
 * Configure linking
 *
 */
#if !defined(GECODE_STATIC_LIBS) && \
    (defined(__CYGWIN__) || defined(__MINGW32__) || defined(_MSC_VER))

#ifdef GECODE_BUILD_DRIVER
#define GECODE_DRIVER_EXPORT __declspec( dllexport )
#else
#define GECODE_DRIVER_EXPORT __declspec( dllimport )
#endif

#else

#ifdef GECODE_GCC_HAS_CLASS_VISIBILITY
#define GECODE_DRIVER_EXPORT __attribute__ ((visibility("default")))
#else
#define GECODE_DRIVER_EXPORT
#endif

#endif

// Configure auto-linking
#ifndef GECODE_BUILD_DRIVER
#define GECODE_LIBRARY_NAME "Driver"
#include <gecode/support/auto-link.hpp>
#endif

/**
 * \namespace Gecode::Driver
 * \brief Script commandline driver
 *
 * The Gecode::Driver namespace contains support for passing common
 * commandline options and for scripts that use the commandline
 * options.
 *
 */

namespace Gecode {


  /**
   * \defgroup TaskDriverCmd Commandline options for running scripts
   * \ingroup TaskDriver
   */

  /**
   * \brief Different modes for executing scripts
   * \ingroup TaskDriverCmd
   */
  enum ScriptMode {
    SM_SOLUTION, ///< Print solution and some statistics
    SM_TIME,     ///< Measure average runtime
    SM_STAT,     ///< Print statistics for script
    SM_GIST      ///< Run script in Gist
  };

  /**
   * \brief Different modes for restart-based search
   * \ingroup TaskDriverCmd
   */
  enum RestartMode {
    RM_NONE,     ///< No restarts
    RM_CONSTANT, ///< Restart with constant sequence
    RM_LINEAR,   ///< Restart with linear sequence
    RM_LUBY,     ///< Restart with Luby sequence
    RM_GEOMETRIC ///< Restart with geometric sequence
  };

  class BaseOptions;

  namespace Driver {
    /**
     * \brief Base class for options
     *
     */
    class GECODE_DRIVER_EXPORT BaseOption {
      friend class Gecode::BaseOptions;
    protected:
      const char* opt;  ///< String for option (including hyphen)
      const char* exp;  ///< Short explanation
      BaseOption* next; ///< Next option
      /// Check for option and return its argument
      char* argument(int argc, char* argv[]) const;
    public:
      /// Initialize for option \a o and explanation \a e
      BaseOption(const char* o, const char* e);
      /// Parse option at first position and return number of parsed arguments
      virtual int parse(int argc, char* argv[]) = 0;
      /// Print help text
      virtual void help(void) = 0;
      /// Destructor
      virtual ~BaseOption(void);
      /// Create heap-allocated copy of string \a s
      static char* strdup(const char* s);
      /// Delete heap-allocated copy of string \a s
      static void strdel(const char* s);
    };
    
    /**
     * \brief String-valued option
     *
     */
    class GECODE_DRIVER_EXPORT StringValueOption : public BaseOption {
    protected:
      const char* cur; ///< Current value
    public:
      /// Initialize for option \a o and explanation \a e and default value \a v
      StringValueOption(const char* o, const char* e, const char* v=NULL);
      /// Set default value to \a v
      void value(const char* v);
      /// Return current option value
      const char* value(void) const;
      /// Parse option at first position and return number of parsed arguments
      virtual int parse(int argc, char* argv[]);
      /// Print help text
      virtual void help(void);
      /// Destructor
      virtual ~StringValueOption(void);
    };
  

    /**
     * \brief String-valued option (integer value defined by strings)
     *
     */
    class GECODE_DRIVER_EXPORT StringOption : public BaseOption {
    protected:
      /// Option value
      class Value {
      public:
        int         val;  ///< Value for an option value
        const char* opt;  ///< String for option value
        const char* help; ///< Optional help text
        Value*      next; ///< Next option value
      };
      int    cur; ///< Current value
      Value* fst; ///< First option value
      Value* lst; ///< Last option value
    public:
      /// Initialize for option \a o and explanation \a e and default value \a v
      StringOption(const char* o, const char* e, int v=0);
      /// Set default value to \a v
      void value(int v);
      /// Return current option value
      int value(void) const;
      /// Add option value for value \a v, string \a o, and help text \a h
      void add(int v, const char* o, const char* h = NULL);
      /// Parse option at first position and return number of parsed arguments
      virtual int parse(int argc, char* argv[]);
      /// Print help text
      virtual void help(void);
      /// Destructor
      virtual ~StringOption(void);
    };
  

    /**
     * \brief Integer option
     *
     */
    class GECODE_DRIVER_EXPORT IntOption : public BaseOption {
    protected:
      int cur; ///< Current value
    public:
      /// Initialize for option \a o and explanation \a e and default value \a v
      IntOption(const char* o, const char* e, int v=0);
      /// Set default value to \a v
      void value(int v);
      /// Return current option value
      int value(void) const;
      /// Parse option at first position and return number of parsed arguments
      virtual int parse(int argc, char* argv[]);
      /// Print help text
      virtual void help(void);
    };

    /**
     * \brief Unsigned integer option
     *
     */
    class GECODE_DRIVER_EXPORT UnsignedIntOption : public BaseOption {
    protected:
      unsigned int cur; ///< Current value
    public:
      /// Initialize for option \a o and explanation \a e and default value \a v
      UnsignedIntOption(const char* o, const char* e, unsigned int v=0);
      /// Set default value to \a v
      void value(unsigned int v);
      /// Return current option value
      unsigned int value(void) const;
      /// Parse option at first position and return number of parsed arguments
      virtual int parse(int argc, char* argv[]);
      /// Print help text
      virtual void help(void);
    };

    /**
     * \brief Double option
     *
     */
    class GECODE_DRIVER_EXPORT DoubleOption : public BaseOption {
    protected:
      double cur; ///< Current value
    public:
      /// Initialize for option \a o and explanation \a e and default value \a v
      DoubleOption(const char* o, const char* e, double v=0);
      /// Set default value to \a v
      void value(double v);
      /// Return current option value
      double value(void) const;
      /// Parse option at first position and return number of parsed arguments
      virtual int parse(int argc, char* argv[]);
      /// Print help text
      virtual void help(void);
    };

    /**
     * \brief Boolean option
     *
     */
    class GECODE_DRIVER_EXPORT BoolOption : public BaseOption {
    protected:
      bool cur; ///< Current value
    public:
      /// Initialize for option \a o and explanation \a e and default value \a v
      BoolOption(const char* o, const char* e, bool v=false);
      /// Set default value to \a v
      void value(bool v);
      /// Return current option value
      bool value(void) const;
      /// Parse option at first position and return number of parsed arguments
      virtual int parse(int argc, char* argv[]);
      /// Print help text
      virtual void help(void);
    };

  }
  
  /**
   * \brief Base class for script options
   * \ingroup TaskDriverCmd
   */
  class GECODE_DRIVER_EXPORT BaseOptions {
  protected:
    Driver::BaseOption* fst;   ///< First registered option
    Driver::BaseOption* lst;   ///< Last registered option
    const char* _name; ///< Script name
  public:
    /// Initialize options for script with name \a s
    BaseOptions(const char* s);
    /// Print help text
    virtual void help(void);

    /// Add new option \a o
    void add(Driver::BaseOption& o);
    /**
     * \brief Parse options from arguments \a argv (number is \a argc)
     *
     * The options are parsed from position one onwards until no more options
     * are detected. After parsing, the parsed arguments have been removed.
     *
     */
    void parse(int& argc, char* argv[]);
    
    /// Return name of script
    const char* name(void) const;
    /// Set name of script
    void name(const char*);

    /// Destructor
    virtual ~BaseOptions(void);
  };
  
  /**
   * \brief %Options for scripts
   * \ingroup TaskDriverCmd
   */
  class GECODE_DRIVER_EXPORT Options : public BaseOptions {
  protected:    
    /// \name Model options
    //@{
    Driver::StringOption _model;       ///< General model options
    Driver::StringOption _symmetry;    ///< General symmetry options
    Driver::StringOption _propagation; ///< Propagation options
    Driver::StringOption _icl;         ///< Integer consistency level
    Driver::StringOption _branching;   ///< Branching options
    Driver::DoubleOption _decay;       ///< Decay option
    //@}
    
    /// \name Search options
    //@{
    Driver::StringOption      _search;        ///< Search options
    Driver::UnsignedIntOption _solutions;     ///< How many solutions
    Driver::DoubleOption      _threads;       ///< How many threads to use
    Driver::UnsignedIntOption _c_d;           ///< Copy recomputation distance
    Driver::UnsignedIntOption _a_d;           ///< Adaptive recomputation distance
    Driver::UnsignedIntOption _node;          ///< Cutoff for number of nodes
    Driver::UnsignedIntOption _fail;          ///< Cutoff for number of failures
    Driver::UnsignedIntOption _time;          ///< Cutoff for time
    Driver::StringOption      _restart;       ///< Restart method option
    Driver::DoubleOption      _r_base;        ///< Restart base
    Driver::UnsignedIntOption _r_scale;       ///< Restart scale factor
    Driver::BoolOption        _nogoods;       ///< Whether to use no-goods
    Driver::UnsignedIntOption _nogoods_limit; ///< Limit for no-good extraction
    Driver::BoolOption        _interrupt;     ///< Whether to catch SIGINT
    //@}
    
    /// \name Execution options
    //@{
    Driver::StringOption      _mode;       ///< Script mode to run
    Driver::UnsignedIntOption _samples;    ///< How many samples
    Driver::UnsignedIntOption _iterations; ///< How many iterations per sample
    Driver::BoolOption        _print_last; ///< Print only last solution found
    Driver::StringValueOption _out_file;   ///< Where to print solutions
    Driver::StringValueOption _log_file;   ///< Where to print statistics
    //@}

  public:
    /// Initialize options for script with name \a s
    Options(const char* s);
    
    /// \name Model options
    //@{
    /// Set default model value
    void model(int v);
    /// Add model option value for value \a v, string \a o, and help \a h
    void model(int v, const char* o, const char* h = NULL);
    /// Return model value
    int model(void) const;
    
    /// Set default symmetry value
    void symmetry(int v);
    /// Add symmetry option value for value \a v, string \a o, and help \a h
    void symmetry(int v, const char* o, const char* h = NULL);
    /// Return symmetry value
    int symmetry(void) const;
    
    /// Set default propagation value
    void propagation(int v);
    /// Add propagation option value for value \a v, string \a o, and help \a h
    void propagation(int v, const char* o, const char* h = NULL);
    /// Return propagation value
    int propagation(void) const;
    
    /// Set default integer consistency level
    void icl(IntConLevel i);
    /// Return integer consistency level
    IntConLevel icl(void) const;
    
    /// Set default branching value
    void branching(int v);
    /// Add branching option value for value \a v, string \a o, and help \a h
    void branching(int v, const char* o, const char* h = NULL);
    /// Return branching value
    int branching(void) const;

    /// Set default decay factor
    void decay(double d);
    /// Return decay factor
    double decay(void) const;
    //@}
    
    /// \name Search options
    //@{
    /// Set default search value
    void search(int v);
    /// Add search option value for value \a v, string \a o, and help \a h
    void search(int v, const char* o, const char* h = NULL);
    /// Return search value
    int search(void) const;
    
    /// Set default number of solutions to search for
    void solutions(unsigned int n);
    /// Return number of solutions to search for
    unsigned int solutions(void) const;

    /// Set number of parallel threads
    void threads(double n);
    /// Return number of parallel threads
    double threads(void) const;
    
    /// Set default copy recomputation distance
    void c_d(unsigned int d);
    /// Return copy recomputation distance
    unsigned int c_d(void) const;
    
    /// Set default adaptive recomputation distance
    void a_d(unsigned int d);
    /// Return adaptive recomputation distance
    unsigned int a_d(void) const;
    
    /// Set default node cutoff
    void node(unsigned int n);
    /// Return node cutoff
    unsigned int node(void) const;
    
    /// Set default failure cutoff
    void fail(unsigned int n);
    /// Return failure cutoff
    unsigned int fail(void) const;
    
    /// Set default time cutoff
    void time(unsigned int t);
    /// Return time cutoff
    unsigned int time(void) const;
    
    /// Set default restart mode
    void restart(RestartMode r);
    /// Return restart mode
    RestartMode restart(void) const;
    
    /// Set default restart base
    void restart_base(double base);
    /// Return restart base
    double restart_base(void) const;
    
    /// Set default restart scale factor
    void restart_scale(unsigned int scale);
    /// Return restart scale factor
    unsigned int restart_scale(void) const;
    
    /// Set default nogoods posting behavior
    void nogoods(bool b);
    /// Return whether nogoods are used
    bool nogoods(void) const;
    
    /// Set default nogoods depth limit
    void nogoods_limit(unsigned int l);
    /// Return depth limit for nogoods
    unsigned int nogoods_limit(void) const;
    
    /// Set default interrupt behavior
    void interrupt(bool b);
    /// Return interrupt behavior
    bool interrupt(void) const;
    //@}

    /// \name Execution options
    //@{
    /// Set default mode
    void mode(ScriptMode em);
    /// Return mode
    ScriptMode mode(void) const;
    
    /// Set default number of samples
    void samples(unsigned int s);
    /// Return number of samples
    unsigned int samples(void) const;

    /// Set default number of iterations
    void iterations(unsigned int i);
    /// Return number of iterations
    unsigned int iterations(void) const;
    
    /// Set whether to print only last solution found
    void print_last(bool p);
    /// Return whether to print only last solution found
    bool print_last(void) const;

    /// Set default output file name for solutions
    void out_file(const char* f);
    /// Get file name for solutions
    const char* out_file(void) const;

    /// Set default output file name for Gecode stats
    void log_file(const char* f);
    /// Get file name for Gecode stats
    const char* log_file(void) const;
    //@}

#ifdef GECODE_HAS_GIST
    /// Helper class storing Gist inspectors
    class _I {
    private:
      /// The double click inspectors
      Support::DynamicArray<Gist::Inspector*,Heap> _click;
      /// Number of double click inspectors
      unsigned int n_click;
      /// The solution inspectors
      Support::DynamicArray<Gist::Inspector*,Heap> _solution;
      /// Number of solution inspectors
      unsigned int n_solution;
      /// The move inspectors
      Support::DynamicArray<Gist::Inspector*,Heap> _move;
      /// Number of move inspectors
      unsigned int n_move;
      /// The comparators
      Support::DynamicArray<Gist::Comparator*,Heap> _compare;
      /// Number of comparators
      unsigned int n_compare;
    public:
      /// Constructor
      _I(void);
      /// Add inspector that reacts on node double clicks
      void click(Gist::Inspector* i);
      /// Add inspector that reacts on each new solution that is found
      void solution(Gist::Inspector* i);
      /// Add inspector that reacts on each move of the cursor
      void move(Gist::Inspector* i);
      /// Add comparator
      void compare(Gist::Comparator* i);
      
      /// Return click inspector number \a i, or NULL if it does not exist
      Gist::Inspector* click(unsigned int i) const;
      /// Return solution inspector number \a i, or NULL if it does not exist
      Gist::Inspector* solution(unsigned int i) const;
      /// Return move inspector number \a i, or NULL if it does not exist
      Gist::Inspector* move(unsigned int i) const;
      /// Return comparator number \a i, or NULL if it does not exist
      Gist::Comparator* compare(unsigned int i) const;
    } inspect;
#endif
  };

  /**
   * \brief %Options for scripts with additional size parameter
   * \ingroup TaskDriverCmd
   */
  class GECODE_DRIVER_EXPORT SizeOptions : public Options {
  protected:
    unsigned int _size; ///< Size value
  public:
    /// Initialize options for script with name \a s
    SizeOptions(const char* s);
    /// Print help text
    virtual void help(void);
    /// Parse options from arguments \a argv (number is \a argc)
    void parse(int& argc, char* argv[]);
    
    /// Set default size
    void size(unsigned int s);
    /// Return size
    unsigned int size(void) const;
  };

  /**
   * \brief %Options for scripts with additional instance parameter
   * \ingroup TaskDriverCmd
   */
  class GECODE_DRIVER_EXPORT InstanceOptions : public Options {
  protected:
    const char* _inst; ///< Instance string
  public:
    /// Initialize options for script with name \a s
    InstanceOptions(const char* s);
    /// Print help text
    virtual void help(void);
    /// Parse options from arguments \a argv (number is \a argc)
    void parse(int& argc, char* argv[]);
    
    /// Set default instance name
    void instance(const char* s);
    /// Return instance name
    const char* instance(void) const;
    /// Destructor
    ~InstanceOptions(void);
  };

}

#include <gecode/driver/options.hpp>

namespace Gecode {

  namespace Driver {
    /**
     * \brief Parametric base-class for scripts
     *
     * All scripts must inherit from this class
     *  - adds printing and comparison for Gist to scripts
     *  - run allows to execute scripts
     */
    template<class BaseSpace>
    class ScriptBase : public BaseSpace {
    public:
      /// Default constructor
      ScriptBase(void) {}
      /// Constructor used for cloning
      ScriptBase(bool share, ScriptBase& e) 
        : BaseSpace(share,e) {}
      /// Print a solution to \a os
      virtual void print(std::ostream& os) const { (void) os; }
      /// Compare with \a s
      virtual void compare(const Space&, std::ostream& os) const {
        (void) os;
      }
      /// Choose output stream according to \a name
      static std::ostream& select_ostream(const char* name, std::ofstream& ofs);
      /** Run script with search engine \a Engine and options \a opt
       *
       * In the solution and stat modes, search can be aborted by sending
       * SIGINT to the process (i.e., pressing Ctrl-C on the command
       * line).
       *
       * In case \a s is different from NULL, the search engine uses
       * \a s as root of the search tree.
       */
      template<class Script, template<class> class Engine, class Options>
      static void run(const Options& opt, Script* s=NULL);
    private:
      template<class Script, template<class> class Engine, class Options,
               template<template<class> class,class> class Meta>
      static void runMeta(const Options& opt, Script* s);
      /// Catch wrong definitions of copy constructor
      explicit ScriptBase(ScriptBase& e);
    };
  }
  
  /**
   * \defgroup TaskDriverScript Script classes
   * \ingroup TaskDriver
   */

  /**
   * \brief Base-class for scripts
   * \ingroup TaskDriverScript
   */
  typedef Driver::ScriptBase<Space> Script;
  /**
   * \brief Base-class for scripts for finding solution of lowest integer cost
   * \ingroup TaskDriverScript
   */
  typedef Driver::ScriptBase<MinimizeSpace> MinimizeScript;
  /**
   * \brief Base-class for scripts for finding solution of highest integer cost
   * \ingroup TaskDriverScript
   */
  typedef Driver::ScriptBase<MaximizeSpace> MaximizeScript;
  /**
   * \brief Base-class for scripts for finding solution of lowest integer cost
   * \ingroup TaskDriverScript
   */
  typedef Driver::ScriptBase<IntMinimizeSpace> IntMinimizeScript;
  /**
   * \brief Base-class for scripts for finding solution of highest integer cost
   * \ingroup TaskDriverScript
   */
  typedef Driver::ScriptBase<IntMaximizeSpace> IntMaximizeScript;

#ifdef GECODE_HAS_FLOAT_VARS 

  /**
   * \brief Base-class for scripts for finding solution of lowest float cost
   * \ingroup TaskDriverScript
   */
  typedef Driver::ScriptBase<FloatMinimizeSpace> FloatMinimizeScript;
  /**
   * \brief Base-class for scripts for finding solution of highest float cost
   * \ingroup TaskDriverScript
   */
  typedef Driver::ScriptBase<FloatMaximizeSpace> FloatMaximizeScript;

#endif

}

#include <gecode/driver/script.hpp>

#endif

// STATISTICS: driver-any
