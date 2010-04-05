
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2002
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   matrix.h
/// \brief  Class interfaces for matrix objects

#ifndef _MATRIX_H_
#define _MATRIX_H_

#include "managedObject.h"
#include "hashTable.h"


/// \brief  Base class for all matrix objects

template <class TItem,class TCoeff>
class goblinMatrix : public virtual managedObject
{
protected:

    TItem      k;      // Number of rows
    TItem      l;      // Number of columns
    bool       transp; // Switch between matrix and its transposed

public:

    goblinMatrix(TItem,TItem) throw();
    ~goblinMatrix() throw();

    unsigned long   Allocated() const throw() {return 0;};

    char*   Display() const throw();

    TItem   K() const throw() {if (transp) return l; else return k;};
    TItem   L() const throw() {if (transp) return k; else return l;};

    void    Transpose() throw() {transp = (transp==false);};

    virtual void    SetCoeff(TItem,TItem,TCoeff) throw(ERRange) = 0;
    virtual TCoeff  Coeff(TItem,TItem) const throw(ERRange) = 0;

    void    Add(goblinMatrix&) throw(ERRange);
    void    Sum(goblinMatrix&,goblinMatrix&) throw(ERRange);
    void    Product(goblinMatrix&,goblinMatrix&) throw(ERRange);

    void    GaussElim(goblinMatrix&,TFloat=0) throw(ERRange,ERRejected);

};


/// \brief  Full coefficient matrix representation

template <class TItem,class TCoeff>
class denseMatrix : public virtual goblinMatrix<TItem,TCoeff>
{
private:

    TCoeff* coeff;

public:

    denseMatrix(TItem,TItem,
        goblinController & = goblinDefaultContext) throw();
    denseMatrix(goblinMatrix<TItem,TCoeff> &) throw();
    ~denseMatrix() throw();

    unsigned long   Allocated() const throw();
    unsigned long   Size() const throw();

    void    SetCoeff(TItem,TItem,TCoeff) throw(ERRange);
    TCoeff  Coeff(TItem,TItem) const throw(ERRange);

};


/// \brief  Matrices represented by a hash table for the non-zero coefficients

template <class TItem,class TCoeff>
class sparseMatrix : public virtual goblinMatrix<TItem,TCoeff>
{
private:

    goblinHashTable<TItem,TCoeff>* coeff;

public:

    sparseMatrix(TItem,TItem,TItem,
        goblinController & = goblinDefaultContext) throw();
    sparseMatrix(goblinMatrix<TItem,TCoeff> &) throw();
    ~sparseMatrix() throw();

    unsigned long   Allocated() throw() {return 0;};
    unsigned long   Size() throw();

    void    SetCoeff(TItem,TItem,TCoeff) throw(ERRange);
    TCoeff  Coeff(TItem,TItem) const throw(ERRange);

};


#endif
