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

#ifndef __GECODE_FLATZINC_AST_HH__
#define __GECODE_FLATZINC_AST_HH__

#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>

/**
 * \namespace Gecode::FlatZinc::AST
 * \brief Abstract syntax trees for the %FlatZinc interpreter
 */

namespace Gecode { namespace FlatZinc { namespace AST {
  
  class Call;
  class Array;
  class Atom;
  class SetLit;
  
  /// %Exception signaling type error
  class GECODE_VTABLE_EXPORT TypeError {
  private:
    std::string _what;
  public:
    TypeError() : _what("") {}
    TypeError(std::string what) : _what(what) {}
    std::string what(void) const { return _what; }
  };
  
  /**
   * \brief A node in a %FlatZinc abstract syntax tree
   */
  class GECODE_VTABLE_EXPORT Node {
  public:
    /// Destructor
    virtual ~Node(void);
    
    /// Append \a n to an array node
    void append(Node* n);

    /// Test if node has atom with \a id
    bool hasAtom(const std::string& id);
    /// Test if node is int, if yes set \a i to the value
    bool isInt(int& i);
    /// Test if node is float, if yes set \a d to the value
    bool isFloat(double& i);
    /// Test if node is function call with \a id
    bool isCall(const std::string& id);
    /// Return function call
    Call* getCall(void);
    /// Test if node is function call or array containing function call \a id
    bool hasCall(const std::string& id);
    /// Return function call \a id
    Call* getCall(const std::string& id);
    /// Cast this node to an array node
    Array* getArray(void);
    /// Cast this node to an Atom node
    Atom* getAtom(void);
    /// Return name of variable represented by this node
    std::string getVarName(void);
    /// Cast this node to an integer variable node
    int getIntVar(void);
    /// Cast this node to a Boolean variable node
    int getBoolVar(void);
    /// Cast this node to a Float variable node
    int getFloatVar(void);
    /// Cast this node to a set variable node
    int getSetVar(void);
    
    /// Cast this node to an integer node
    int getInt(void);
    /// Cast this node to a Boolean node
    bool getBool(void);
    /// Cast this node to a Float node
    double getFloat(void);
    /// Cast this node to a set literal node
    SetLit *getSet(void);
    
    /// Cast this node to a string node
    std::string getString(void);
    
    /// Test if node is an integer variable node
    bool isIntVar(void);
    /// Test if node is a Boolean variable node
    bool isBoolVar(void);
    /// Test if node is a set variable node
    bool isSetVar(void);
    /// Test if node is a float variable node
    bool isFloatVar(void);
    /// Test if node is an integer node
    bool isInt(void);
    /// Test if node is a float node
    bool isFloat(void);
    /// Test if node is a Boolean node
    bool isBool(void);
    /// Test if node is a string node
    bool isString(void);
    /// Test if node is an array node
    bool isArray(void);
    /// Test if node is a set literal node
    bool isSet(void);
    /// Test if node is an atom node
    bool isAtom(void);
    
    /// Output string representation
    virtual void print(std::ostream&) = 0;
  };

  /// Boolean literal node
  class GECODE_VTABLE_EXPORT BoolLit : public Node {
  public:
    bool b;
    BoolLit(bool b0) : b(b0) {}
    virtual void print(std::ostream& os) {
      os << "b(" << (b ? "true" : "false") << ")";
    }
  };
  /// Integer literal node
  class GECODE_VTABLE_EXPORT IntLit : public Node {
  public:
    int i;
    IntLit(int i0) : i(i0) {}
    virtual void print(std::ostream& os) {
      os << "i("<<i<<")";
    }
  };
  /// Float literal node
  class GECODE_VTABLE_EXPORT FloatLit : public Node {
  public:
    double d;
    FloatLit(double d0) : d(d0) {}
    virtual void print(std::ostream& os) {
      os << "f("<<d<<")";
    }
  };
  /// %Set literal node
  class GECODE_VTABLE_EXPORT SetLit : public Node {
  public:
    bool interval;
    int min; int max;
    std::vector<int> s;
    SetLit(void) {}
    SetLit(int min0, int max0) : interval(true), min(min0), max(max0) {}
    SetLit(const std::vector<int>& s0) : interval(false), s(s0) {}
    bool empty(void) const {
      return ( (interval && min>max) || (!interval && s.size() == 0));
    }
    virtual void print(std::ostream& os) {
      os << "s()";
    }
  };
  
  /// Variable node base class
  class GECODE_VTABLE_EXPORT Var : public Node {
  public:
    int i; //< Index
    std::string n; //< Name
    /// Constructor
    Var(int i0, const std::string& n0) : i(i0), n(n0) {}
  };
  /// Boolean variable node
  class GECODE_VTABLE_EXPORT BoolVar : public Var {
  public:
    /// Constructor
    BoolVar(int i0, const std::string& n0="") : Var(i0,n0) {}
    virtual void print(std::ostream& os) {
      os << "xb("<<i<<")";
    }
  };
  /// Integer variable node
  class GECODE_VTABLE_EXPORT IntVar : public Var {
  public:
    IntVar(int i0, const std::string& n0="") : Var(i0,n0) {}
    virtual void print(std::ostream& os) {
      os << "xi("<<i<<")";
    }
  };
  /// Float variable node
  class GECODE_VTABLE_EXPORT FloatVar : public Var {
  public:
    FloatVar(int i0, const std::string& n0="") : Var(i0,n0) {}
    virtual void print(std::ostream& os) {
      os << "xf("<<i<<")";
    }
  };
  /// %Set variable node
  class GECODE_VTABLE_EXPORT SetVar : public Var {
  public:
    SetVar(int i0, const std::string& n0="") : Var(i0,n0) {}
    virtual void print(std::ostream& os) {
      os << "xs("<<i<<")";
    }
  };
  
  /// %Array node
  class GECODE_VTABLE_EXPORT Array : public Node {
  public:
    std::vector<Node*> a;
    Array(const std::vector<Node*>& a0)
    : a(a0) {}
    Array(Node* n)
    : a(1) { a[0] = n; }
    Array(int n=0) : a(n) {}
    virtual void print(std::ostream& os) {
      os << "[";
      for (unsigned int i=0; i<a.size(); i++) {
        a[i]->print(os);
        if (i<a.size()-1)
          os << ", ";
      }
      os << "]";
    }
    ~Array(void) {
      for (int i=a.size(); i--;)
        delete a[i];
    }
  };

  /// %Node representing a function call
  class GECODE_VTABLE_EXPORT Call : public Node {
  public:
    std::string id;
    Node* args;
    Call(const std::string& id0, Node* args0)
    : id(id0), args(args0) {}
    ~Call(void) { delete args; }
    virtual void print(std::ostream& os) {
      os << id << "("; args->print(os); os << ")";
    }
    Array* getArgs(unsigned int n) {
      Array *a = args->getArray();
      if (a->a.size() != n)
        throw TypeError("arity mismatch");
      return a;
    }
  };

  /// %Node representing an array access
  class GECODE_VTABLE_EXPORT ArrayAccess : public Node {
  public:
    Node* a;
    Node* idx;
    ArrayAccess(Node* a0, Node* idx0)
    : a(a0), idx(idx0) {}
    ~ArrayAccess(void) { delete a; delete idx; }
    virtual void print(std::ostream& os) {
      a->print(os);
      os << "[";
      idx->print(os);
      os << "]";
    }
  };

  /// %Node representing an atom
  class GECODE_VTABLE_EXPORT Atom : public Node {
  public:
    std::string id;
    Atom(const std::string& id0) : id(id0) {}
    virtual void print(std::ostream& os) {
      os << id;
    }
  };

  /// %String node
  class GECODE_VTABLE_EXPORT String : public Node {
  public:
    std::string s;
    String(const std::string& s0) : s(s0) {}
    virtual void print(std::ostream& os) {
      os << "s(\"" << s << "\")";
    }
  };
  
  inline
  Node::~Node(void) {}
  
  inline void
  Node::append(Node* newNode) {
    Array* a = dynamic_cast<Array*>(this);
    if (!a)
      throw TypeError("array expected");
    a->a.push_back(newNode);
  }

  inline bool
  Node::hasAtom(const std::string& id) {
    if (Array* a = dynamic_cast<Array*>(this)) {
      for (int i=a->a.size(); i--;)
        if (Atom* at = dynamic_cast<Atom*>(a->a[i]))
          if (at->id == id)
            return true;
    } else if (Atom* a = dynamic_cast<Atom*>(this)) {
      return a->id == id;
    }
    return false;
  }

  inline bool
  Node::isCall(const std::string& id) {
    if (Call* a = dynamic_cast<Call*>(this)) {
      if (a->id == id)
        return true;
    }
    return false;
  }

  inline Call*
  Node::getCall(void) {
    if (Call* a = dynamic_cast<Call*>(this))
      return a;
    throw TypeError("call expected");
  }

  inline bool
  Node::hasCall(const std::string& id) {
    if (Array* a = dynamic_cast<Array*>(this)) {
      for (int i=a->a.size(); i--;)
        if (Call* at = dynamic_cast<Call*>(a->a[i]))
          if (at->id == id) {
            return true;
          }
    } else if (Call* a = dynamic_cast<Call*>(this)) {
      return a->id == id;
    }
    return false;
  }

  inline bool
  Node::isInt(int& i) {
    if (IntLit* il = dynamic_cast<IntLit*>(this)) {
      i = il->i;
      return true;
    }
    return false;
  }

  inline bool
  Node::isFloat(double& d) {
    if (FloatLit* fl = dynamic_cast<FloatLit*>(this)) {
      d = fl->d;
      return true;
    }
    return false;
  }

  inline Call*
  Node::getCall(const std::string& id) {
    if (Array* a = dynamic_cast<Array*>(this)) {
      for (int i=a->a.size(); i--;)
        if (Call* at = dynamic_cast<Call*>(a->a[i]))
          if (at->id == id)
            return at;
    } else if (Call* a = dynamic_cast<Call*>(this)) {
      if (a->id == id)
        return a;
    }
    throw TypeError("call expected");
  }
  
  inline Array*
  Node::getArray(void) {
    if (Array* a = dynamic_cast<Array*>(this))
      return a;
    throw TypeError("array expected");
  }

  inline Atom*
  Node::getAtom(void) {
    if (Atom* a = dynamic_cast<Atom*>(this))
      return a;
    throw TypeError("atom expected");
  }
  
  inline std::string
  Node::getVarName(void) {
    if (Var* a = dynamic_cast<Var*>(this))
      return a->n;
    throw TypeError("variable expected");
  }
  inline int
  Node::getIntVar(void) {
    if (IntVar* a = dynamic_cast<IntVar*>(this))
      return a->i;
    throw TypeError("integer variable expected");
  }
  inline int
  Node::getBoolVar(void) {
    if (BoolVar* a = dynamic_cast<BoolVar*>(this))
      return a->i;
    throw TypeError("bool variable expected");
  }
  inline int
  Node::getFloatVar(void) {
    if (FloatVar* a = dynamic_cast<FloatVar*>(this))
      return a->i;
    throw TypeError("integer variable expected");
  }
  inline int
  Node::getSetVar(void) {
    if (SetVar* a = dynamic_cast<SetVar*>(this))
      return a->i;
    throw TypeError("set variable expected");
  }
  inline int
  Node::getInt(void) {
    if (IntLit* a = dynamic_cast<IntLit*>(this))
      return a->i;
    throw TypeError("integer literal expected");
  }
  inline bool
  Node::getBool(void) {
    if (BoolLit* a = dynamic_cast<BoolLit*>(this))
      return a->b;
    throw TypeError("bool literal expected");
  }
  inline double
  Node::getFloat(void) {
    if (FloatLit* a = dynamic_cast<FloatLit*>(this))
      return a->d;
    throw TypeError("float literal expected");
  }
  inline SetLit*
  Node::getSet(void) {
    if (SetLit* a = dynamic_cast<SetLit*>(this))
      return a;
    throw TypeError("set literal expected");
  }
  inline std::string
  Node::getString(void) {
    if (String* a = dynamic_cast<String*>(this))
      return a->s;
    throw TypeError("string literal expected");
  }
  inline bool
  Node::isIntVar(void) {
    return (dynamic_cast<IntVar*>(this) != NULL);
  }
  inline bool
  Node::isBoolVar(void) {
    return (dynamic_cast<BoolVar*>(this) != NULL);
  }
  inline bool
  Node::isSetVar(void) {
    return (dynamic_cast<SetVar*>(this) != NULL);
  }
  inline bool
  Node::isFloatVar(void) {
    return (dynamic_cast<FloatVar*>(this) != NULL);
  }
  inline bool
  Node::isInt(void) {
    return (dynamic_cast<IntLit*>(this) != NULL);
  }
  inline bool
  Node::isBool(void) {
    return (dynamic_cast<BoolLit*>(this) != NULL);
  }
  inline bool
  Node::isFloat(void) {
    return (dynamic_cast<FloatLit*>(this) != NULL);
  }
  inline bool
  Node::isSet(void) {
    return (dynamic_cast<SetLit*>(this) != NULL);
  }
  inline bool
  Node::isString(void) {
    return (dynamic_cast<String*>(this) != NULL);
  }
  inline bool
  Node::isArray(void) {
    return (dynamic_cast<Array*>(this) != NULL);
  }
  inline bool
  Node::isAtom(void) {
    return (dynamic_cast<Atom*>(this) != NULL);
  }

  inline Node*
  extractSingleton(Node* n) {
    if (Array* a = dynamic_cast<Array*>(n)) {
      if (a->a.size() == 1) {
        Node *ret = a->a[0];
        a->a[0] = NULL;
        delete a;
        return ret;
      }
    }
    return n;
  }

}}}

#endif

// STATISTICS: flatzinc-any
