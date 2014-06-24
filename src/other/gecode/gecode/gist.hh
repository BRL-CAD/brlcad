/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2006
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __GECODE_GIST_HH__
#define __GECODE_GIST_HH__

#include <gecode/kernel.hh>
#include <gecode/search.hh>
#include <gecode/int.hh>
#ifdef GECODE_HAS_SET_VARS
#include <gecode/set.hh>
#endif
#ifdef GECODE_HAS_FLOAT_VARS
#include <gecode/float.hh>
#endif

/*
 * Configure linking
 *
 */

#if !defined(GIST_STATIC_LIBS) && \
    (defined(__CYGWIN__) || defined(__MINGW32__) || defined(_MSC_VER))

#ifdef GECODE_BUILD_GIST
#define GECODE_GIST_EXPORT __declspec( dllexport )
#else
#define GECODE_GIST_EXPORT __declspec( dllimport )
#endif

#else

#ifdef GECODE_GCC_HAS_CLASS_VISIBILITY
#define GECODE_GIST_EXPORT __attribute__ ((visibility("default")))
#else
#define GECODE_GIST_EXPORT
#endif

#endif

// Configure auto-linking
#ifndef GECODE_BUILD_GIST
#define GECODE_LIBRARY_NAME "Gist"
#include <gecode/support/auto-link.hpp>
#endif

#include <string>
#include <sstream>

namespace Gecode {

  /**
   * \namespace Gecode::Gist
   * \brief The %Gecode Interactive %Search Tool
   *
   * The Gecode::Gist namespace contains the %Gecode Interactive %Search Tool,
   * a Qt-based graphical search engine.
   *
   */
  namespace Gist {

    /** \brief Abstract base class for inspectors
     *
     * An inspector provides a virtual method that is called
     * when a node in the search tree is inspected (e.g. by
     * double-clicking)
     *
     * \ingroup TaskGist
     */
    class GECODE_GIST_EXPORT Inspector {
    public:
      /// Call-back function
      virtual void inspect(const Space& s) = 0;
      /// Name of the inspector
      virtual std::string name(void);
      /// Clean up when Gist exits
      virtual void finalize(void);
      /// Destructor
      virtual ~Inspector(void);
    };

    /** \brief Abstract base class for comparators
     *
     * A comparator provides a virtual method that is called
     * when a node in the search tree is compared to another
     * node.
     *
     * \ingroup TaskGist
     */
    class GECODE_GIST_EXPORT Comparator {
    public:
      //@{
      ///\name Comparator interface
      
      /// Call-back function
      virtual void compare(const Space& s0, const Space& s1) = 0;
      /// Name of the comparator
      virtual std::string name(void);
      /// Clean up when Gist exits
      virtual void finalize(void);
      /// Destructor
      virtual ~Comparator(void);

      //@}

      //@{
      ///\name Helper methods
      
      /// Return string representation of difference between arrays \a x and \a y, which are called \a x_n
      template<class Var>
      static std::string compare(std::string x_n, const VarArgArray<Var>& x,
        const VarArgArray<Var>& y);
      /// Return string representation of difference between \a x and \a y, which are called \a x_n
      static std::string compare(std::string x_n, IntVar x, IntVar y);
      /// Return string representation of difference between \a x and \a y, which are called \a x_n
      static std::string compare(std::string x_n, BoolVar x, BoolVar y);
#ifdef GECODE_HAS_SET_VARS
      /// Return string representation of difference between \a x and \a y, which are called \a x_n
      static std::string compare(std::string x_n, SetVar x, SetVar y);
#endif
#ifdef GECODE_HAS_FLOAT_VARS
      /// Return string representation of difference between \a x and \a y, which are called \a x_n
      static std::string compare(std::string x_n, FloatVar x, FloatVar y);
#endif
      //@}
    };
        
    class TextOutputI;
    
    /// An window for simple text output
    class GECODE_GIST_EXPORT TextOutput {
    private:
      /// The implementation object
      TextOutputI *t;
      /// The name of the window
      std::string n;
    protected:
      /// Initialize the implementation object
      void init(void);
      /// Get the stream that is used to output text
      std::ostream& getStream(void);
      /// Flush stream
      void flush(void);
      /// Add html text \a s to the output
      void addHtml(const char* s);
    public:
      /// Constructor
      TextOutput(const std::string& name);
      /// Clean up when Gist exits
      void finalize(void);
      /// Destructor
      virtual ~TextOutput(void);
      /// Name of the inspector
      virtual std::string name(void);
    };
    
    /// An inspector for printing simple text output
    template<class S>
    class Print : public TextOutput, public Inspector {
    public:
      /// Constructor
      Print(const std::string& name);
      /// Use the print method of the template class S to print a space
      virtual void inspect(const Space& node);
      /// Return name
      virtual std::string name(void);
      /// Clean up when Gist exits
      virtual void finalize(void);
    };

    /**
     * \brief A simple comparator
     *
     * This class serves two purposes. First, it provides static methods
     * that compare two variables or two arrays of variables and return
     * a string representation of the differences.
     * Second, it implements a Comparator that uses the compare method of
     * the script to output the differences between two spaces.
     *
     */
    template<class S>
    class VarComparator : public TextOutput, public Comparator {
    public:
      /// Constructor
      VarComparator(std::string name);
      /// Compare \a s0 to \a s1
      virtual void compare(const Space& s0, const Space& s1);
      /// Return name
      virtual std::string name(void);
      /// Finalize when Gist exits
      virtual void finalize(void);
    };
    
    /// A branching that stops exploration
    GECODE_GIST_EXPORT
    void stopBranch(Space& home);
    
    /**
     * \brief %Options for %Gist
     *
     * Note that the \a d and \a stop fields of the Search::Options class
     * are not used by Gist.
     *
     */
    class Options : public Search::Options {
    public:
      /// Helper class storing inspectors
      class _I {
      private:
        Support::DynamicArray<Inspector*,Heap> _click;
        unsigned int n_click;
        Support::DynamicArray<Inspector*,Heap> _solution;
        unsigned int n_solution;
        Support::DynamicArray<Inspector*,Heap> _move;
        unsigned int n_move;
        Support::DynamicArray<Comparator*,Heap> _compare;
        unsigned int n_compare;
      public:
        /// Constructor
        _I(void);
        /// Add inspector that reacts on node double clicks
        void click(Inspector* i);
        /// Add inspector that reacts on each new solution that is found
        void solution(Inspector* i);
        /// Add inspector that reacts on each move of the cursor
        void move(Inspector* i);
        /// Add comparator
        void compare(Comparator* c);
        
        /// Return click inspector number \a i, or NULL if it does not exist
        Inspector* click(unsigned int i) const;
        /// Return solution inspector number \a i, or NULL if it does not exist
        Inspector* solution(unsigned int i) const;
        /// Return move inspector number \a i, or NULL if it does not exist
        Inspector* move(unsigned int i) const;
        /// Return comparator number \a i, or NULL if it does not exist
        Comparator* compare(unsigned int i) const;
      } inspect;
      /// Default options
      GECODE_GIST_EXPORT static const Options def;
      /// Initialize with default values
      Options(void);
    };

    
    /// Create a new stand-alone Gist for \a root using \a bab
    GECODE_GIST_EXPORT int 
    explore(Space* root, bool bab, const Options& opt);
    
    /**
     * \brief Create a new stand-alone Gist for \a root
     * \ingroup TaskGist
     */
    int 
    dfs(Space* root, const Gist::Options& opt = Gist::Options::def);
    
    /**
     * \brief Create a new stand-alone Gist for branch-and-bound search of \a root
     * \ingroup TaskGist
     */
    int 
    bab(Space* root, const Gist::Options& opt = Gist::Options::def);
    
  }	

}

#include <gecode/gist/gist.hpp>

#endif

// STATISTICS: gist-any
