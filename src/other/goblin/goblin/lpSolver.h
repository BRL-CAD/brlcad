
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Bernhard Schmidt and
//  Christian Fremuth-Paeger, January 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   lpSolver.h
/// \brief  #goblinLPSolver class interface

#ifndef _LP_SOLVER_H_
#define _LP_SOLVER_H_

#include "ilpWrapper.h"
#include "matrix.h"
#include "dictionary.h"


const double EPSILON=1e-10;
const double DELTA=1e-6;
const double TOLERANCE=1e-7;


/// \brief  Self-contained implementation of an LP Solver
///
///  LPs are considered to be in the form
///
///    min/max c^tx  under  L <= Ax <= U, l <= x <= u
///
///  Variable indexing:    0,1,..,#L()-1
///
///  Restriction indexing: 0,1,..,#K()+#L()-1
///
///  Restriction i is
///
///    LBound(i)     <= A_ix <= UBound(i)      for i <  #K()
///
///  and
///
///    LRange(i-K()) <=    x <= URange(i-K())  for i >= #K()

class goblinLPSolver : public virtual mipInstance
{
private:

    TRestr kAct;            // Number of active restrictions
    TRestr kMax;            // Maximum number of restrictions
    TRestr lAct;            // Number of active variables
    TRestr lMax;            // Maximum number of variables

    goblinHashTable<TIndex,TFloat>* coeff;
                            // Sparse matrix representation

    TFloat* cost;           // Cost coefficients
    TFloat* uBound;         // Upper bounds (Restrictions)
    TFloat* lBound;         // Lower bounds (Restrictions)
    TFloat* uRange;         // Upper bounds (Variables)
    TFloat* lRange;         // Lower bounds (Variables)
    TVarType* varType;      // Specify integer variables
    char** restrLabel;      // Restriction labels
    char** varLabel;        // Variable names

    mutable goblinDictionary<TVar>*   varIndex;   // Map variable labels to indices
    mutable goblinDictionary<TRestr>* restrIndex; // Map restriction labels to indeics

    TObjectSense dir;         // ObjectSense of optimality

    TFloat cCost;           // Constant for cost coefficients
    TFloat cUBound;         // Constant for upper bounds (Restrictions)
    TFloat cLBound;         // Constant for lower bounds (Restrictions)
    TFloat cURange;         // Constant for upper bounds (Variables)
    TFloat cLRange;         // Constant for lower bounds (Variables)
    TVarType cVarType;      // Specify integer variables

    TRestrType* restrType;  // Identify rows in basis
    TRestr*     index;      // Mapping variables to rows in basis
    TVar*       revIndex;   // Mapping rows in basis to variables
    mutable TFloat* x;      // Primal solution induced by current basis
    mutable TFloat* y;      // Dual solution induced by current basis

    mutable denseMatrix<TIndex,TFloat> *baseInv;   // Inverse of basis matrix
    mutable denseMatrix<TIndex,TFloat> *keptInv;   // Kept for basis evaluations

    mutable bool    baseInitial;    // Indicates the default basis (lower ranges)
    mutable bool    baseValid;      // Is the basis inverse up to date?
    mutable bool    dataValid;      // Are the solutions up to date?

public:

    goblinLPSolver(TRestr,TVar,TIndex,TObjectSense,
        goblinController & = goblinDefaultContext) throw();
    goblinLPSolver(const char* impFileName,
        goblinController &thisContext = goblinDefaultContext)
        throw(ERFile,ERParse);
    ~goblinLPSolver() throw();

    unsigned long   Allocated() const throw();
    unsigned long   Size() const throw();


    // *************************************************************** //
    //                      Instance Manipulation                      //
    // *************************************************************** //

    TVar    AddVar(TFloat,TFloat,TFloat,TVarType = VAR_FLOAT) throw(ERRejected);
    TRestr  AddRestr(TFloat,TFloat) throw(ERRejected);

    void    DeleteVar(TVar) throw(ERRange,ERRejected);
    void    DeleteRestr(TRestr) throw(ERRange);

    void    SetURange(TVar,TFloat) throw(ERRange);
    void    SetLRange(TVar,TFloat) throw(ERRange);
    void    SetUBound(TRestr,TFloat) throw(ERRange);
    void    SetLBound(TRestr,TFloat) throw(ERRange);
    void    SetCost(TVar,TFloat) throw(ERRange);
    void    SetVarType(TVar,TVarType) throw(ERRange);
    void    SetVarLabel(TVar,char*,TOwnership = OWNED_BY_SENDER)
                throw(ERRange,ERRejected);
    void    SetRestrLabel(TRestr,char*,TOwnership = OWNED_BY_SENDER)
                throw(ERRange,ERRejected);

    void    SetObjectSense(TObjectSense dd) throw();

    void    SetCoeff(TRestr,TVar,TFloat) throw(ERRange);

    void    SetRow(TRestr i,TVar len,TVar* index,double* val) throw(ERRange);
    // Sets row i of the coefficient matrix. len is the number
    // of nonzeros in the new row i. The coefficients are given by
    // index and val.

    void    SetColumn(TVar j,TRestr len,TRestr* index,double* val) throw(ERRange);
    // Sets columns j of the coefficient matrix. len is the
    // number of nonzeros in the new column j. The coefficients
    // are given by index and val.

    void    Resize(TRestr,TVar,TIndex) throw(ERRange);
    void    Strip() throw() {Resize(kAct,lAct,coeff->NZ());};


    // *************************************************************** //
    //                      Retrieval Operations                       //
    // *************************************************************** //

    /// \addtogroup objectDimensions
    /// @{

    TRestr  K() const throw();

    TVar  L() const throw();

    TIndex  NZ() const throw();

    /// @}

    TFloat  Cost(TVar) const throw(ERRange);
    TFloat  URange(TVar) const throw(ERRange);
    TFloat  LRange(TVar) const throw(ERRange);

    TFloat  UBound(TRestr) const throw(ERRange);
        // UBound(i) = upper bound of restriction i, i=0,...,kAct+lAct-1

    TFloat  LBound(TRestr) const throw(ERRange);
        // LBound(i) = lower bound of restriction i, i=0,...,kAct+lAct-1

    TVarType    VarType(TVar) const throw(ERRange);
    char*   VarLabel(TVar,TOwnership = OWNED_BY_RECEIVER) const throw(ERRange);
    char*   RestrLabel(TRestr,TOwnership = OWNED_BY_RECEIVER) const throw(ERRange);
    TVar    VarIndex(char*) const throw();
    TRestr  RestrIndex(char*) const throw();

    bool    CCost()    const throw() {return (cost==NULL);};
    bool    CURange()  const throw() {return (uRange==NULL);};
    bool    CLRange()  const throw() {return (lRange==NULL);};
    bool    CUBound()  const throw() {return (uBound==NULL);};
    bool    CLBound()  const throw() {return (lBound==NULL);};
    bool    CVarType() const throw() {return (varType==NULL);};

    TObjectSense  ObjectSense() const throw() {return dir;};

    TFloat  Coeff(TRestr,TVar) const throw(ERRange);

    TVar    GetRow(TRestr i,TVar* index,double* val) const throw(ERRange);
    // Gets Row i of the coefficient matrix and stores it in index
    // and val. val[j] is the coefficient of variable index[j] in row i. 
    // Returns the number of nonzeros of row i. index and val have to
    // be allocated before calling the method.

    TRestr  GetColumn(TVar j,TRestr* index,double* val) const throw(ERRange);
    // Gets Column j of the coefficient matrix and stores it in index
    // and val. val[i] is the coefficient of variable index[i] in row i. 
    // Returns the number of nonzeros of column j. index and val have to
    // be allocated before calling the method.


    // *************************************************************** //
    //                    Basic dependent information                  //
    // *************************************************************** //

    TRestrType  RestrType(TRestr) const throw(ERRange);
    TRestr  Index(TVar) const throw(ERRange);
    TVar    RowIndex(TRestr) const throw(ERRange) {return NoRestr;};
    TVar    RevIndex(TRestr) const throw(ERRange);

    void    SetRestrType(TRestr,TLowerUpper) throw(ERRange,ERRejected);
    void    SetIndex(TRestr,TVar,TLowerUpper) throw(ERRange,ERRejected);

    void    InitBasis() const throw();
    // Sets variable range restrictions as basis, all RestrType BASIC_LB

    bool    Initial() const throw() {return baseInitial;};

private:

    void    EvaluateBasis() const throw(ERRejected);
    void    DefaultBasisInverse() const throw(ERRejected);

    void    BasisUpdate(TRestr,TVar) const throw(ERRange,ERRejected);
    // Updates baseInv after a call of Pivot

    void    SolutionUpdate() const throw(ERRejected);
    // Updates primal and dual solution

    void    PrimallyFeasibleBasis() throw();
    // Modifies bounds!!! (have to be restored!)

    TVar    PricePrimal() throw(ERRejected);
    TRestr  QTestPrimal(TVar) throw(ERRejected);

    void    DuallyFeasibleBasis() throw();  
    // Modifies costs!!!  (have to be restored!)

    TRestr  PriceDual() throw(ERRejected);
    TVar    QTestDual(TRestr) throw(ERRejected);

public:

    TFloat  X(TVar) const throw(ERRange);
    TFloat  Y(TRestr,TLowerUpper) const throw(ERRange);

    TFloat  Tableau(TIndex,TIndex) const throw(ERRange,ERRejected);
    // Tableau(i,j)=A_j(A_B)^(-1)e_i

    TFloat  BaseInverse(TIndex,TIndex) const throw(ERRange,ERRejected);

    void    Pivot(TIndex,TIndex,TLowerUpper) throw(ERRange,ERRejected);


    // *************************************************************** //
    //                           Algorithms                            //
    // *************************************************************** //

    bool    StartPrimal() throw();
    TFloat  SolvePrimal() throw();

    bool    StartDual() throw();
    TFloat  SolveDual() throw();

};



//****************************************************************************//
//                   Entry function and package information                   //
//****************************************************************************//


/// \brief  Object factory for the #goblinLPSolver class

class nativeLPFactory : public virtual mipFactory
{
public:

    nativeLPFactory() throw();

    mipInstance*
        NewInstance(TRestr k,TVar l,TIndex nz,TObjectSense dir,
            goblinController& thisContext) const throw(ERRange);

    mipInstance*
        ReadInstance(const char* impFileName,goblinController& thisContext)
            const throw(ERRange);

    ~nativeLPFactory() throw();

    unsigned long Size() const throw();

    char*   Authors() const throw();
    int     MajorRelease() const throw();
    int     MinorRelease() const throw();
    char*   PatchLevel() const throw();
    char*   BuildDate() const throw();
    char*   License() const throw();

    TLPOrientation  Orientation() const throw();
};


#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Bernhard Schmidt and
//  Christian Fremuth-Paeger, January 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   lpSolver.h
/// \brief  #goblinLPSolver class interface

#ifndef _LP_SOLVER_H_
#define _LP_SOLVER_H_

#include "ilpWrapper.h"
#include "matrix.h"
#include "dictionary.h"


const double EPSILON=1e-10;
const double DELTA=1e-6;
const double TOLERANCE=1e-7;


/// \brief  Self-contained implementation of an LP Solver
///
///  LPs are considered to be in the form
///
///    min/max c^tx  under  L <= Ax <= U, l <= x <= u
///
///  Variable indexing:    0,1,..,#L()-1
///
///  Restriction indexing: 0,1,..,#K()+#L()-1
///
///  Restriction i is
///
///    LBound(i)     <= A_ix <= UBound(i)      for i <  #K()
///
///  and
///
///    LRange(i-K()) <=    x <= URange(i-K())  for i >= #K()

class goblinLPSolver : public virtual mipInstance
{
private:

    TRestr kAct;            // Number of active restrictions
    TRestr kMax;            // Maximum number of restrictions
    TRestr lAct;            // Number of active variables
    TRestr lMax;            // Maximum number of variables

    goblinHashTable<TIndex,TFloat>* coeff;
                            // Sparse matrix representation

    TFloat* cost;           // Cost coefficients
    TFloat* uBound;         // Upper bounds (Restrictions)
    TFloat* lBound;         // Lower bounds (Restrictions)
    TFloat* uRange;         // Upper bounds (Variables)
    TFloat* lRange;         // Lower bounds (Variables)
    TVarType* varType;      // Specify integer variables
    char** restrLabel;      // Restriction labels
    char** varLabel;        // Variable names

    mutable goblinDictionary<TVar>*   varIndex;   // Map variable labels to indices
    mutable goblinDictionary<TRestr>* restrIndex; // Map restriction labels to indeics

    TObjectSense dir;         // ObjectSense of optimality

    TFloat cCost;           // Constant for cost coefficients
    TFloat cUBound;         // Constant for upper bounds (Restrictions)
    TFloat cLBound;         // Constant for lower bounds (Restrictions)
    TFloat cURange;         // Constant for upper bounds (Variables)
    TFloat cLRange;         // Constant for lower bounds (Variables)
    TVarType cVarType;      // Specify integer variables

    TRestrType* restrType;  // Identify rows in basis
    TRestr*     index;      // Mapping variables to rows in basis
    TVar*       revIndex;   // Mapping rows in basis to variables
    mutable TFloat* x;      // Primal solution induced by current basis
    mutable TFloat* y;      // Dual solution induced by current basis

    mutable denseMatrix<TIndex,TFloat> *baseInv;   // Inverse of basis matrix
    mutable denseMatrix<TIndex,TFloat> *keptInv;   // Kept for basis evaluations

    mutable bool    baseInitial;    // Indicates the default basis (lower ranges)
    mutable bool    baseValid;      // Is the basis inverse up to date?
    mutable bool    dataValid;      // Are the solutions up to date?

public:

    goblinLPSolver(TRestr,TVar,TIndex,TObjectSense,
        goblinController & = goblinDefaultContext) throw();
    goblinLPSolver(const char* impFileName,
        goblinController &thisContext = goblinDefaultContext)
        throw(ERFile,ERParse);
    ~goblinLPSolver() throw();

    unsigned long   Allocated() const throw();
    unsigned long   Size() const throw();


    // *************************************************************** //
    //                      Instance Manipulation                      //
    // *************************************************************** //

    TVar    AddVar(TFloat,TFloat,TFloat,TVarType = VAR_FLOAT) throw(ERRejected);
    TRestr  AddRestr(TFloat,TFloat) throw(ERRejected);

    void    DeleteVar(TVar) throw(ERRange,ERRejected);
    void    DeleteRestr(TRestr) throw(ERRange);

    void    SetURange(TVar,TFloat) throw(ERRange);
    void    SetLRange(TVar,TFloat) throw(ERRange);
    void    SetUBound(TRestr,TFloat) throw(ERRange);
    void    SetLBound(TRestr,TFloat) throw(ERRange);
    void    SetCost(TVar,TFloat) throw(ERRange);
    void    SetVarType(TVar,TVarType) throw(ERRange);
    void    SetVarLabel(TVar,char*,TOwnership = OWNED_BY_SENDER)
                throw(ERRange,ERRejected);
    void    SetRestrLabel(TRestr,char*,TOwnership = OWNED_BY_SENDER)
                throw(ERRange,ERRejected);

    void    SetObjectSense(TObjectSense dd) throw();

    void    SetCoeff(TRestr,TVar,TFloat) throw(ERRange);

    void    SetRow(TRestr i,TVar len,TVar* index,double* val) throw(ERRange);
    // Sets row i of the coefficient matrix. len is the number
    // of nonzeros in the new row i. The coefficients are given by
    // index and val.

    void    SetColumn(TVar j,TRestr len,TRestr* index,double* val) throw(ERRange);
    // Sets columns j of the coefficient matrix. len is the
    // number of nonzeros in the new column j. The coefficients
    // are given by index and val.

    void    Resize(TRestr,TVar,TIndex) throw(ERRange);
    void    Strip() throw() {Resize(kAct,lAct,coeff->NZ());};


    // *************************************************************** //
    //                      Retrieval Operations                       //
    // *************************************************************** //

    /// \addtogroup objectDimensions
    /// @{

    TRestr  K() const throw();

    TVar  L() const throw();

    TIndex  NZ() const throw();

    /// @}

    TFloat  Cost(TVar) const throw(ERRange);
    TFloat  URange(TVar) const throw(ERRange);
    TFloat  LRange(TVar) const throw(ERRange);

    TFloat  UBound(TRestr) const throw(ERRange);
        // UBound(i) = upper bound of restriction i, i=0,...,kAct+lAct-1

    TFloat  LBound(TRestr) const throw(ERRange);
        // LBound(i) = lower bound of restriction i, i=0,...,kAct+lAct-1

    TVarType    VarType(TVar) const throw(ERRange);
    char*   VarLabel(TVar,TOwnership = OWNED_BY_RECEIVER) const throw(ERRange);
    char*   RestrLabel(TRestr,TOwnership = OWNED_BY_RECEIVER) const throw(ERRange);
    TVar    VarIndex(char*) const throw();
    TRestr  RestrIndex(char*) const throw();

    bool    CCost()    const throw() {return (cost==NULL);};
    bool    CURange()  const throw() {return (uRange==NULL);};
    bool    CLRange()  const throw() {return (lRange==NULL);};
    bool    CUBound()  const throw() {return (uBound==NULL);};
    bool    CLBound()  const throw() {return (lBound==NULL);};
    bool    CVarType() const throw() {return (varType==NULL);};

    TObjectSense  ObjectSense() const throw() {return dir;};

    TFloat  Coeff(TRestr,TVar) const throw(ERRange);

    TVar    GetRow(TRestr i,TVar* index,double* val) const throw(ERRange);
    // Gets Row i of the coefficient matrix and stores it in index
    // and val. val[j] is the coefficient of variable index[j] in row i. 
    // Returns the number of nonzeros of row i. index and val have to
    // be allocated before calling the method.

    TRestr  GetColumn(TVar j,TRestr* index,double* val) const throw(ERRange);
    // Gets Column j of the coefficient matrix and stores it in index
    // and val. val[i] is the coefficient of variable index[i] in row i. 
    // Returns the number of nonzeros of column j. index and val have to
    // be allocated before calling the method.


    // *************************************************************** //
    //                    Basic dependent information                  //
    // *************************************************************** //

    TRestrType  RestrType(TRestr) const throw(ERRange);
    TRestr  Index(TVar) const throw(ERRange);
    TVar    RowIndex(TRestr) const throw(ERRange) {return NoRestr;};
    TVar    RevIndex(TRestr) const throw(ERRange);

    void    SetRestrType(TRestr,TLowerUpper) throw(ERRange,ERRejected);
    void    SetIndex(TRestr,TVar,TLowerUpper) throw(ERRange,ERRejected);

    void    InitBasis() const throw();
    // Sets variable range restrictions as basis, all RestrType BASIC_LB

    bool    Initial() const throw() {return baseInitial;};

private:

    void    EvaluateBasis() const throw(ERRejected);
    void    DefaultBasisInverse() const throw(ERRejected);

    void    BasisUpdate(TRestr,TVar) const throw(ERRange,ERRejected);
    // Updates baseInv after a call of Pivot

    void    SolutionUpdate() const throw(ERRejected);
    // Updates primal and dual solution

    void    PrimallyFeasibleBasis() throw();
    // Modifies bounds!!! (have to be restored!)

    TVar    PricePrimal() throw(ERRejected);
    TRestr  QTestPrimal(TVar) throw(ERRejected);

    void    DuallyFeasibleBasis() throw();  
    // Modifies costs!!!  (have to be restored!)

    TRestr  PriceDual() throw(ERRejected);
    TVar    QTestDual(TRestr) throw(ERRejected);

public:

    TFloat  X(TVar) const throw(ERRange);
    TFloat  Y(TRestr,TLowerUpper) const throw(ERRange);

    TFloat  Tableau(TIndex,TIndex) const throw(ERRange,ERRejected);
    // Tableau(i,j)=A_j(A_B)^(-1)e_i

    TFloat  BaseInverse(TIndex,TIndex) const throw(ERRange,ERRejected);

    void    Pivot(TIndex,TIndex,TLowerUpper) throw(ERRange,ERRejected);


    // *************************************************************** //
    //                           Algorithms                            //
    // *************************************************************** //

    bool    StartPrimal() throw();
    TFloat  SolvePrimal() throw();

    bool    StartDual() throw();
    TFloat  SolveDual() throw();

};



//****************************************************************************//
//                   Entry function and package information                   //
//****************************************************************************//


/// \brief  Object factory for the #goblinLPSolver class

class nativeLPFactory : public virtual mipFactory
{
public:

    nativeLPFactory() throw();

    mipInstance*
        NewInstance(TRestr k,TVar l,TIndex nz,TObjectSense dir,
            goblinController& thisContext) const throw(ERRange);

    mipInstance*
        ReadInstance(const char* impFileName,goblinController& thisContext)
            const throw(ERRange);

    ~nativeLPFactory() throw();

    unsigned long Size() const throw();

    char*   Authors() const throw();
    int     MajorRelease() const throw();
    int     MinorRelease() const throw();
    char*   PatchLevel() const throw();
    char*   BuildDate() const throw();
    char*   License() const throw();

    TLPOrientation  Orientation() const throw();
};


#endif
