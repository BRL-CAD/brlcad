
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2002
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   matrix.cpp
/// \brief  Implementations of matrix objects

#include "matrix.h"


template <class TItem,class TCoeff>
goblinMatrix<TItem,TCoeff>::goblinMatrix(TItem kk,TItem ll) throw()
{
    this->k = kk;
    this->l = ll;
    this->transp = false;

    LogEntry(LOG_MEM,"...Abstract matrix allocated");
}


template <class TItem,class TCoeff>
goblinMatrix<TItem,TCoeff>::~goblinMatrix() throw()
{
    LogEntry(LOG_MEM,"...Abstract matrix disallocated");
}


template <class TItem,class TCoeff>
char* goblinMatrix<TItem,TCoeff>::Display() const throw()
{
    LogEntry(MSG_TRACE,"Matrix");

    for (TItem i=0;i<k;i++)
    {
        LogEntry(MSG_TRACE2,"| ");

        for (TItem j=0;j<l;j++)
        {
            sprintf(this->CT.logBuffer,"%g ",Coeff(i,j));
            LogEntry(MSG_APPEND,this->CT.logBuffer);
        }

        LogEntry(MSG_APPEND,"|");
    }

    return NULL;
}


template <class TItem,class TCoeff>
void goblinMatrix<TItem,TCoeff>::Add(goblinMatrix& A)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (A.L()!=l || A.K()!=k)
        Error(ERR_RANGE,"Add","Incompatible matrix dimensions");

    #endif

    for (TItem i=0;i<k;i++)
    {
        for (TItem j=0;j<l;j++)
            SetCoeff(i,j,A.Coeff(i,j)+Coeff(i,j));
    }
}


template <class TItem,class TCoeff>
void goblinMatrix<TItem,TCoeff>::Sum(goblinMatrix& A,goblinMatrix& B)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (A.L()!=l || B.L()!=l || A.K()!=k || B.K()!=k)
        Error(ERR_RANGE,"Sum","Incompatible matrix dimensions");

    #endif

    for (TItem i=0;i<k;i++)
    {
        for (TItem j=0;j<l;j++)
            SetCoeff(i,j,A.Coeff(i,j)+B.Coeff(i,j));
    }
}


template <class TItem,class TCoeff>
void goblinMatrix<TItem,TCoeff>::Product(goblinMatrix& A,goblinMatrix& B)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (A.L()!=B.K() || A.K()!=k || B.L()!=l)
        Error(ERR_RANGE,"Product","Incompatible matrix dimensions");

    #endif

    for (TItem i=0;i<k;i++)
    {
        for (TItem j=0;j<l;j++)
        {
            TCoeff x = 0;
            for (TItem m=0;m<A.L();m++)
                x += A.Coeff(i,m)*B.Coeff(m,j);
            SetCoeff(i,j,x);
        }
    }
}


template <class TItem,class TCoeff>
void goblinMatrix<TItem,TCoeff>::GaussElim(goblinMatrix<TItem,TCoeff> &B,TFloat epsilon)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (k!=l) Error(ERR_RANGE,"GaussElim","Matrix must be square");

    if (B.K()!=l) Error(ERR_RANGE,"GaussElim","Incompatible matrix dimensions");

    #endif

    if (epsilon<=0) epsilon = this->CT.epsilon;

    LogEntry(LOG_METH,"Solving linear equation system...");

    // Computing triangular form

    TItem dim = k;
    TItem* pivotIndex = new TItem[dim];
    TItem* revPivotIndex = new TItem[dim];

    for (TItem i=0;i<dim;i++)
    {
        TFloat maxAbs = 0;
        TItem maxIndex = 0;

        for (TItem j=0;j<dim;j++)
            if (fabs(Coeff(i,j))>fabs(maxAbs))
            {
                maxAbs = Coeff(i,j);
                maxIndex = j;
            }

        revPivotIndex[maxIndex] = i;
        pivotIndex[i] = maxIndex;

        if (fabs(maxAbs)<this->CT.epsilon)
            Error(ERR_REJECTED,"GaussElim","Matrix is singular");

        for (TItem j=0;j<dim;j++)
            SetCoeff(i,j,Coeff(i,j)/maxAbs);

        for (TItem j=0;j<B.L();j++)
            B.SetCoeff(i,j,B.Coeff(i,j)/maxAbs);

        for (TItem i2=i+1;i2<dim;i2++)
        {
            TFloat thisFac = Coeff(i2,maxIndex);
            for (TItem j=0;j<dim;j++)
                SetCoeff(i2,j,Coeff(i2,j)-thisFac*Coeff(i,j));
            for (TItem j=0;j<B.L();j++)
                B.SetCoeff(i2,j,B.Coeff(i2,j)-thisFac*B.Coeff(i,j));
        }
    }

    // Computing diagonal form

    for (TItem i=dim-1;i>0;i--)
    {
        TItem column = NoIndex;

        for (TItem j=0;j<dim && column==NoIndex;j++)
            if (fabs(Coeff(i,j))>=epsilon)
                column = j;

        for (TItem i2=0;i2<i;i2++)
        {
            TFloat thisFac = Coeff(i2,column);
            for (TItem j=0;j<dim;j++)
                SetCoeff(i2,j,Coeff(i2,j)-thisFac*Coeff(i,j));
            for (TItem j=0;j<B.L();j++)
                B.SetCoeff(i2,j,B.Coeff(i2,j)-thisFac*B.Coeff(i,j));
        }
    }

    // Reorder rows

    for (TItem i=0;i<dim;i++)
    {
        TIndex column = revPivotIndex[i];

        if (i!=column)
        {
            TFloat swap = 0;

            for (TItem j=0;j<dim;j++)
            {
                swap = Coeff(i,j);
                SetCoeff(i,j,Coeff(column,j));
                SetCoeff(column,j,swap);
            }

            for (TItem j=0;j<B.L();j++)
            {
                swap = B.Coeff(i,j);
                B.SetCoeff(i,j,B.Coeff(column,j));
                B.SetCoeff(column,j,swap);
            }
        }

        revPivotIndex[pivotIndex[i]] = column;
        pivotIndex[revPivotIndex[i]] = pivotIndex[i];
    }

    delete[] pivotIndex;
    delete[] revPivotIndex;

    LogEntry(LOG_RES,"...Linear equation system solved");
}


template class goblinMatrix<TIndex,TFloat>;


template <class TItem,class TCoeff>
denseMatrix<TItem,TCoeff>::denseMatrix(TItem kk,TItem ll,goblinController &thisContext)
    throw() : managedObject(thisContext), goblinMatrix<TItem,TCoeff>(kk,ll)
{
    coeff = new TFloat[(this->k)*(this->l)];

    for (TItem i=0;i<(this->k)*(this->l);i++) coeff[i] = 0;

    this->LogEntry(LOG_MEM,"...Dense matrix allocated");
}


template <class TItem,class TCoeff>
denseMatrix<TItem,TCoeff>::denseMatrix(goblinMatrix<TItem,TCoeff> &A)
    throw() : managedObject(A.Context()), goblinMatrix<TItem,TCoeff>(A.K(),A.L())
{
    coeff = new TFloat[(this->k)*(this->l)];

    for (TItem i=0;i<this->k;i++)
    {
        for (TItem j=0;j<this->l;j++)
            coeff[(this->l)*i+j] = A.Coeff(i,j);
    }

    this->LogEntry(LOG_MEM,"...Dense matrix allocated");
}


template <class TItem,class TCoeff>
denseMatrix<TItem,TCoeff>::~denseMatrix() throw()
{
    delete[] coeff;

    this->LogEntry(LOG_MEM,"...Dense matrix disallocated");
}


template <class TItem,class TCoeff>
unsigned long denseMatrix<TItem,TCoeff>::Size() const throw()
{
    return
          sizeof(denseMatrix)
        + Allocated()
        + managedObject::Allocated()
        + goblinMatrix<TItem,TCoeff>::Allocated();
}


template <class TItem,class TCoeff>
unsigned long denseMatrix<TItem,TCoeff>::Allocated() const throw()
{
    return sizeof(TFloat)*(this->k)*(this->l);
}


template <class TItem,class TCoeff>
TCoeff denseMatrix<TItem,TCoeff>::Coeff(TItem i,TItem j)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (!this->transp && i>=this->k)  NoSuchIndex("Coeff",i);
    if (!this->transp && j>=this->l)  NoSuchIndex("Coeff",j);
    if (this->transp && i>=this->l)   NoSuchIndex("Coeff",i);
    if (this->transp && j>=this->k)   NoSuchIndex("Coeff",j);

    #endif

    if (this->transp) return coeff[(this->l)*j+i];
    else return coeff[(this->l)*i+j];
}


template <class TItem,class TCoeff>
void denseMatrix<TItem,TCoeff>::SetCoeff(TItem i,TItem j,TCoeff a)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (!this->transp && i>=this->k)  NoSuchIndex("Coeff",i);
    if (!this->transp && j>=this->l)  NoSuchIndex("Coeff",j);
    if (this->transp && i>=this->l)   NoSuchIndex("SetCoeff",i);
    if (this->transp && j>=this->k)   NoSuchIndex("SetCoeff",j);

    if (a>=InfFloat || a<=-InfFloat)
        this->Error(ERR_RANGE,"SetCoeff","Finite matrix coefficients required");

    #endif

    if (this->transp) coeff[(this->l)*j+i] = a;
    else coeff[(this->l)*i+j] = a;
}


template class denseMatrix<TIndex,TFloat>;


template <class TItem,class TCoeff>
sparseMatrix<TItem,TCoeff>::sparseMatrix(TItem kk,TItem ll,TItem size,
    goblinController &thisContext) throw() :
    managedObject(thisContext),
    goblinMatrix<TItem,TCoeff>(kk,ll)
{
    coeff = new goblinHashTable<TItem,TFloat>(kk*ll,size,0,this->CT);

    this->LogEntry(LOG_MEM,"...Sparse matrix allocated");
}


template <class TItem,class TCoeff>
sparseMatrix<TItem,TCoeff>::sparseMatrix(goblinMatrix<TItem,TCoeff> &A)
    throw() : managedObject(A.Context()), goblinMatrix<TItem,TCoeff>(A.K(),A.L())
{
    TItem size = 0;

    for (TItem i=0;i<this->k;i++)
    {
        for (TItem j=0;j<this->l;j++)
            if (A.Coeff(i,j)!=0) size++;
    }

    coeff = new goblinHashTable<TItem,TFloat>((this->k)*(this->l),size,0,this->CT);

    for (TItem i=0;i<this->k;i++)
    {
        for (TItem j=0;j<this->l;j++)
            coeff -> ChangeKey((this->l)*i+j,A.Coeff(i,j));
    }

    this->LogEntry(LOG_MEM,"...Sparse matrix allocated");
}


template <class TItem,class TCoeff>
sparseMatrix<TItem,TCoeff>::~sparseMatrix() throw()
{
    delete coeff;

    this->LogEntry(LOG_MEM,"...Sparse matrix disallocated");
}


template <class TItem,class TCoeff>
unsigned long sparseMatrix<TItem,TCoeff>::Size() throw()
{
    return
          sizeof(sparseMatrix)
        + managedObject::Allocated()
        + goblinMatrix<TItem,TCoeff>::Allocated();
}


template <class TItem,class TCoeff>
TCoeff sparseMatrix<TItem,TCoeff>::Coeff(TItem i,TItem j)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (!this->transp && i>=this->k)  NoSuchIndex("Coeff",i);
    if (!this->transp && j>=this->l)  NoSuchIndex("Coeff",j);
    if (this->transp && i>=this->l)   NoSuchIndex("Coeff",i);
    if (this->transp && j>=this->k)   NoSuchIndex("Coeff",j);

    #endif

    if (this->transp) return coeff->Key((this->l)*j+i);
    else return coeff->Key((this->l)*i+j);
}


template <class TItem,class TCoeff>
void sparseMatrix<TItem,TCoeff>::SetCoeff(TItem i,TItem j,TCoeff a)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (!this->transp && i>=this->k)  NoSuchIndex("SetCoeff",i);
    if (!this->transp && j>=this->l)  NoSuchIndex("SetCoeff",j);
    if (this->transp && i>=this->l)   NoSuchIndex("SetCoeff",i);
    if (this->transp && j>=this->k)   NoSuchIndex("SetCoeff",j);

    if (a>=InfFloat || a<=-InfFloat)
        this->Error(ERR_RANGE,"SetCoeff","Finite matrix coefficients required");

    #endif

    if (this->transp) coeff->ChangeKey((this->l)*j+i,a);
    else coeff->ChangeKey((this->l)*i+j,a);
}


template class sparseMatrix<TIndex,TFloat>;
