/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2010
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

#ifndef __GECODE_FLATZINC_PLUGIN_HH__
#define __GECODE_FLATZINC_PLUGIN_HH__

#ifdef GECODE_HAS_QT
#include <QtCore>
#include <QPluginLoader>
#include <QLibrary>

namespace Gecode { namespace FlatZinc {
  /**
   * \brief Interface for branch plugins
   *
   * This interface has to be implemented as a Qt Plugin. It then
   * provides custom branch methods that can be loaded at runtime
   * from a FlatZinc file.
   *
   * In order to use a custom branching, add an annotation to the
   * FlatZinc solve item like the following:
   *
   * ::gecode_search(mybranch(some,arguments,x,y,z))
   *
   * This will load the plugin called mybranch and invoke its branch
   * method with the arguments encoded in an AST.
   *
   * The plugin mybranch must reside in a file called mybranch.dll
   * on Windows, libmybranch.dylib on Mac OS and libmybranch.so on
   * Linux systems.
   *
   * An example plugin can be found in the Gecode source tree under
   * gecode/flatzinc/exampleplugin.
   *
   */
  class GECODE_VTABLE_EXPORT BranchPlugin {
  public:
    /// The custom branch method that this plugin provides
    virtual void branch(Gecode::FlatZinc::FlatZincSpace& s,
                        Gecode::FlatZinc::AST::Call* c) = 0;
  };
}}
Q_DECLARE_INTERFACE(Gecode::FlatZinc::BranchPlugin,
  "org.gecode.FlatZinc.BranchPlugin/1.0");
#endif

#endif

// STATISTICS: flatzinc-any
