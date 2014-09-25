/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2007
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

#ifndef __FLATZINC_PARSER_HH__
#define __FLATZINC_PARSER_HH__

#include <gecode/flatzinc.hh>

// This is a workaround for a bug in flex that only shows up
// with the Microsoft C++ compiler
#if defined(_MSC_VER)
#define YY_NO_UNISTD_H
#ifdef __cplusplus
extern "C" int isatty(int);
#endif
#endif

// The Microsoft C++ compiler marks certain functions as deprecated,
// so let's take the alternative definitions
#if defined(_MSC_VER)
#define strdup _strdup
#define fileno _fileno
#endif

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>

#include <gecode/flatzinc/option.hh>
#include <gecode/flatzinc/varspec.hh>
#include <gecode/flatzinc/conexpr.hh>
#include <gecode/flatzinc/ast.hh>
#include <gecode/flatzinc/parser.tab.hh>
#include <gecode/flatzinc/symboltable.hh>

namespace Gecode { namespace FlatZinc {

  typedef std::pair<std::string,Option<std::vector<int>* > > intvartype;

  class VarSpec;
  typedef std::pair<std::string, VarSpec*> varspec;

  /// Strict weak ordering for output items
  class OutputOrder {
  public:
    /// Return if \a x is less than \a y, based on first component
    bool operator ()(const std::pair<std::string,AST::Node*>& x,
                     const std::pair<std::string,AST::Node*>& y) {
      return x.first < y.first;
    }
  };

  /// Types of symbols
  enum SymbolType {
    ST_INTVAR,        //< Integer variable
    ST_BOOLVAR,       //< Boolean variable
    ST_FLOATVAR,      //< Float variable
    ST_SETVAR,        //< Set variable
    ST_INTVARARRAY,   //< Integer variable array
    ST_BOOLVARARRAY,  //< Boolean variable array
    ST_SETVARARRAY,   //< Set variable array
    ST_FLOATVARARRAY, //< Float variable array
    ST_INTVALARRAY,   //< Integer array
    ST_BOOLVALARRAY,  //< Boolean array
    ST_SETVALARRAY,   //< Set array
    ST_FLOATVALARRAY, //< Float array
    ST_INT,           //< Integer
    ST_BOOL,          //< Boolean
    ST_SET,           //< Set
    ST_FLOAT          //< Float
  };

  /// Entries in the symbol table
  class SymbolEntry {
  public:
    SymbolType t; //< Type of entry
    int i;        //< Value of entry or array start index
    /// Default constructor
    SymbolEntry(void) {}
    /// Constructor
    SymbolEntry(SymbolType t0, int i0) : t(t0), i(i0) {}
  };

  /// Construct integer variable entry
  forceinline SymbolEntry se_iv(int i) {
    return SymbolEntry(ST_INTVAR, i);
  }
  /// Construct Boolean variable entry
  forceinline SymbolEntry se_bv(int i) {
    return SymbolEntry(ST_BOOLVAR, i);
  }
  /// Construct float variable entry
  forceinline SymbolEntry se_fv(int i) {
    return SymbolEntry(ST_FLOATVAR, i);
  }
  /// Construct set variable entry
  forceinline SymbolEntry se_sv(int i) {
    return SymbolEntry(ST_SETVAR, i);
  }

  /// Construct integer variable array entry
  forceinline SymbolEntry se_iva(int i) {
    return SymbolEntry(ST_INTVARARRAY, i);
  }
  /// Construct Boolean variable array entry
  forceinline SymbolEntry se_bva(int i) {
    return SymbolEntry(ST_BOOLVARARRAY, i);
  }
  /// Construct float variable array entry
  forceinline SymbolEntry se_fva(int i) {
    return SymbolEntry(ST_FLOATVARARRAY, i);
  }
  /// Construct set variable array entry
  forceinline SymbolEntry se_sva(int i) {
    return SymbolEntry(ST_SETVARARRAY, i);
  }

  /// Construct integer entry
  forceinline SymbolEntry se_i(int i) {
    return SymbolEntry(ST_INT, i);
  }
  /// Construct Boolean entry
  forceinline SymbolEntry se_b(bool b) {
    return SymbolEntry(ST_BOOL, b);
  }
  /// Construct set entry
  forceinline SymbolEntry se_s(int i) {
    return SymbolEntry(ST_SET, i);
  }
  /// Construct float entry
  forceinline SymbolEntry se_f(int i) {
    return SymbolEntry(ST_FLOAT, i);
  }

  /// Construct integer array entry
  forceinline SymbolEntry se_ia(int i) {
    return SymbolEntry(ST_INTVALARRAY, i);
  }
  /// Construct Boolean array entry
  forceinline SymbolEntry se_ba(int i) {
    return SymbolEntry(ST_BOOLVALARRAY, i);
  }
  /// Construct set array entry
  forceinline SymbolEntry se_sa(int i) {
    return SymbolEntry(ST_SETVALARRAY, i);
  }
  /// Construct float array entry
  forceinline SymbolEntry se_fa(int i) {
    return SymbolEntry(ST_FLOATVALARRAY, i);
  }

  /// %State of the %FlatZinc parser
  class ParserState {
  public:
    ParserState(const std::string& b, std::ostream& err0,
                Gecode::FlatZinc::FlatZincSpace* fg0)
    : buf(b.c_str()), pos(0), length(b.size()), fg(fg0),
      hadError(false), err(err0) {}

    ParserState(char* buf0, int length0, std::ostream& err0,
                Gecode::FlatZinc::FlatZincSpace* fg0)
    : buf(buf0), pos(0), length(length0), fg(fg0),
      hadError(false), err(err0) {}
  
    void* yyscanner;
    const char* buf;
    unsigned int pos, length;
    Gecode::FlatZinc::FlatZincSpace* fg;
    std::vector<std::pair<std::string,AST::Node*> > _output;

    SymbolTable<SymbolEntry> symbols;

    std::vector<varspec> intvars;
    std::vector<varspec> boolvars;
    std::vector<varspec> setvars;
    std::vector<varspec> floatvars;
    std::vector<int> arrays;
    std::vector<AST::SetLit> setvals;
    std::vector<double> floatvals;
    std::vector<ConExpr*> constraints;

    std::vector<ConExpr*> domainConstraints;

    bool hadError;
    std::ostream& err;
  
    int fillBuffer(char* lexBuf, unsigned int lexBufSize) {
      if (pos >= length)
        return 0;
      int num = std::min(length - pos, lexBufSize);
      memcpy(lexBuf,buf+pos,num);
      pos += num;
      return num;    
    }

    void output(std::string x, AST::Node* n) {
      _output.push_back(std::pair<std::string,AST::Node*>(x,n));
    }
    
    AST::Array* getOutput(void) {
      OutputOrder oo;
      std::sort(_output.begin(),_output.end(),oo);
      AST::Array* a = new AST::Array();
      for (unsigned int i=0; i<_output.size(); i++) {
        a->a.push_back(new AST::String(_output[i].first+" = "));
        if (_output[i].second->isArray()) {
          AST::Array* oa = _output[i].second->getArray();
          for (unsigned int j=0; j<oa->a.size(); j++) {
            a->a.push_back(oa->a[j]);
            oa->a[j] = NULL;
          }
          delete _output[i].second;
        } else {
          a->a.push_back(_output[i].second);
        }
        a->a.push_back(new AST::String(";\n"));
      }
      return a;
    }
  
  };

}}

#endif

// STATISTICS: flatzinc-any
