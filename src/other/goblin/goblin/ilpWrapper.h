
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2002
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   ilpWrapper.h
/// \brief  #mipInstance class interface

#ifndef _ILP_WRAPPER_H_
#define _ILP_WRAPPER_H_

#include "managedObject.h"
#include "fileImport.h"
#include "fileExport.h"


/// \brief  General interface class for LP and MIP instances

class mipInstance : public managedObject
{
private:

    unsigned        bufferLength;
    char*           labelBuffer;

protected:

    TFloat*         varValue;
    TVar            numVars;

public:

    mipInstance(goblinController&) throw();
    virtual ~mipInstance() throw();

    unsigned long   Allocated() const throw();

    bool            IsMixedILP() const;


    // *************************************************************** //
    //                             File IO                             //
    // *************************************************************** //

    enum TDisplayOpt {
        DISPLAY_PROBLEM = 0,
        DISPLAY_OBJECTIVE = 1,
        DISPLAY_RESTRICTIONS = 2,
        DISPLAY_BOUNDS = 4,
        DISPLAY_INTEGERS = 8,
        DISPLAY_FIXED = 16,
        DISPLAY_PRIMAL = 32,
        DISPLAY_DUAL = 64,
        DISPLAY_SLACKS = 128,
        DISPLAY_BASIS = 256,
        DISPLAY_TABLEAU = 512,
        DISPLAY_INVERSE = 1024,
        DISPLAY_VALUES = 2048
    };

    enum TLPFormat {
        MPS_FORMAT = 0,
        LP_FORMAT = 1,
        MPS_CPLEX = 2,
        BAS_CPLEX = 3,
        BAS_GOBLIN = 4,
        GOB_FORMAT = 5
    };


    /// \addtogroup groupObjectExport
    /// @{

    void    Write(const char*,TOption = 0)
                const throw(ERFile,ERRejected);
    void    Write(const char*,TLPFormat,TOption = 0)
                const throw(ERFile,ERRejected);
    void    WriteLPNaive(const char*,TDisplayOpt = DISPLAY_PROBLEM)
                const throw(ERFile,ERRejected);
    void    WriteMPSFile(const char*,TLPFormat = MPS_CPLEX)
                const throw(ERFile,ERRejected);
    void    WriteMPSFile(ofstream&,TLPFormat = MPS_CPLEX)
                const throw(ERFile,ERRejected);
    void    WriteBASFile(const char*,TLPFormat = BAS_CPLEX)
                const throw(ERFile,ERRejected);
    void    WriteBASFile(ofstream&,TLPFormat = BAS_CPLEX)
                const throw(ERFile,ERRejected);
    void    WriteVarValues(goblinExport*) const throw();
    char*   Display() const throw(ERFile,ERRejected);

    /// @}


    /// \addtogroup groupObjectImport
    /// @{

private:

    TVar    ReadRowLabel(char*) throw();
    TRestr  ReadColLabel(char*,bool) throw();

public:

    void    ReadMPSFile(const char*) throw(ERParse,ERRejected);
    void    ReadMPSFile(ifstream&) throw(ERParse,ERRejected);
    void    ReadBASFile(const char*) throw(ERParse,ERRejected);
    void    ReadBASFile(ifstream&) throw(ERParse,ERRejected);
    void    ReadVarValues(goblinImport*,TVar) throw(ERParse);

    /// @}

    void    NoSuchVar(char*,TVar) const throw(ERRange);
    void    NoSuchRestr(char*,TRestr) const throw(ERRange);


    // *************************************************************** //
    //                      Instance Manipulation                      //
    // *************************************************************** //

    enum TVarType
        {VAR_FLOAT=0,VAR_INT=1,VAR_CANCELED=2};

    virtual TVar    AddVar(TFloat,TFloat,TFloat,TVarType = VAR_FLOAT)
                        throw(ERRejected) = 0;
    virtual TRestr  AddRestr(TFloat,TFloat) throw(ERRejected) = 0;

    virtual void    DeleteVar(TVar) throw(ERRange,ERRejected) = 0;
    virtual void    DeleteRestr(TRestr) throw(ERRange) = 0;

    virtual void    SetURange(TVar,TFloat) throw(ERRange) = 0;
    virtual void    SetLRange(TVar,TFloat) throw(ERRange) = 0;
    virtual void    SetUBound(TRestr,TFloat) throw(ERRange) = 0;
    virtual void    SetLBound(TRestr,TFloat) throw(ERRange) = 0;
    virtual void    SetCost(TVar,TFloat) throw(ERRange) = 0;
    virtual void    SetVarType(TVar,TVarType) throw(ERRange) = 0;

    virtual void    SetVarLabel(TVar,char*,TOwnership = OWNED_BY_SENDER)
                        throw(ERRange,ERRejected) = 0;
    virtual void    SetRestrLabel(TRestr,char*,TOwnership = OWNED_BY_SENDER)
                        throw(ERRange,ERRejected) = 0;

    virtual void    SetObjectSense(TObjectSense) throw() = 0;

    virtual void    SetCoeff(TRestr,TVar,TFloat) throw(ERRange) = 0;

    virtual void    SetRow(TRestr i,TVar len,TVar* index,double* val)
                        throw(ERRange) = 0;
    // Sets row i of the coefficient matrix. len is the number
    // of nonzeros in the new row i. The coefficients are given by
    // index and val.

    virtual void    SetColumn(TVar j,TRestr len,TRestr* index,double* val)
                    throw(ERRange) = 0;
    // Sets columns j of the coefficient matrix. len is the
    // number of nonzeros in the new column j. The coefficients
    // are given by index and val.

    virtual void    SetVarVisibility(TVar,bool) throw(ERRange);
    virtual void    SetRestrVisibility(TRestr,bool) throw(ERRange);

    virtual void    Resize(TRestr,TVar,TIndex) throw(ERRange) = 0;
    virtual void    Strip() throw() = 0;

    virtual void    SetVarValue(TVar,TFloat) throw(ERRange);
    virtual void    InitVarValues(TFloat = InfFloat) throw();
    virtual void    ReleaseVarValues() throw();

    // *************************************************************** //
    //                      Retrieval Operations                       //
    // *************************************************************** //

    /// \addtogroup objectDimensions
    /// @{

    virtual TRestr  K() const throw() = 0;
    virtual TVar L() const throw() = 0;
    virtual TIndex  NZ() const throw() = 0;

    /// @}

    virtual TFloat  Cost(TVar) const throw(ERRange) = 0;
    virtual TFloat  URange(TVar) const throw(ERRange) = 0;
    virtual TFloat  LRange(TVar) const throw(ERRange) = 0;
    virtual TFloat  UBound(TRestr) const throw(ERRange) = 0;
    virtual TFloat  LBound(TRestr) const throw(ERRange) = 0;

    virtual TVarType    VarType(TVar) const throw(ERRange) = 0;

    virtual char*   VarLabel(TVar,TOwnership = OWNED_BY_RECEIVER) const throw(ERRange);
    virtual char*   RestrLabel(TRestr,TOwnership = OWNED_BY_RECEIVER) const throw(ERRange);
    char*           UnifiedLabel(TRestr,TOwnership = OWNED_BY_RECEIVER) const throw(ERRange);
    virtual TVar    VarIndex(char*) const throw();
    virtual TRestr  RestrIndex(char*) const throw();

    virtual TObjectSense  ObjectSense() const throw() = 0;

    virtual TFloat  Coeff(TRestr,TVar) const throw(ERRange) = 0;

    virtual TVar    GetRow(TRestr i,TVar* index,double* val) const throw(ERRange) = 0;
    // Gets Row i of the coefficient matrix and stores it in index
    // and val. val[j] is the coefficient of variable index[j] in row i.
    // Returns the number of nonzeros of row i. index and val have to
    // be allocated before calling the method.

    virtual TRestr  GetColumn(TVar j,TRestr* index,double* val) const throw(ERRange) = 0;
    // Gets Column j of the coefficient matrix and stores it in index
    // and val. val[i] is the coefficient of variable index[i] in row i.
    // Returns the number of nonzeros of column j. index and val have to
    // be allocated before calling the method.

    virtual bool    VarVisibility(TVar) const throw(ERRange);
    virtual bool    RestrVisibility(TRestr) const throw(ERRange);

    virtual TFloat  VarValue(TVar) const throw(ERRange);


    // *************************************************************** //
    //                    Basis related information                    //
    // *************************************************************** //

    enum TRestrType     {BASIC_LB=0,BASIC_UB=1,NON_BASIC=2,RESTR_CANCELED=3};

    virtual TRestrType  RestrType(TRestr) const throw(ERRange) = 0;
    virtual TRestr  Index(TVar) const throw(ERRange) = 0;
    virtual TRestr  RowIndex(TRestr) const throw(ERRange) = 0;
    virtual TVar    RevIndex(TRestr) const throw(ERRange) = 0;

    enum TLowerUpper    {LOWER=0,UPPER=1};

    virtual void    SetRestrType(TRestr,TLowerUpper) throw(ERRange,ERRejected) = 0;
    virtual void    SetIndex(TRestr,TVar,TLowerUpper) throw(ERRange,ERRejected) = 0;

protected:

    mutable TVar            pivotColumn;
    mutable TRestr          pivotRow;
    mutable TLowerUpper     pivotDir;

    virtual void    InitBasis() const throw() = 0;

public:

    TVar            PivotColumn() const throw();
    TRestr          PivotRow() const throw();
    TLowerUpper     PivotDirection() const throw();

    virtual void    ResetBasis() throw();
    virtual bool    Initial() const throw() = 0;

    virtual TFloat  X(TVar) const throw(ERRange) = 0;
    virtual TFloat  Y(TRestr,TLowerUpper) const throw(ERRange) = 0;

    virtual TFloat  Tableau(TIndex,TIndex) const throw(ERRange,ERRejected) = 0;
    virtual TFloat  BaseInverse(TIndex,TIndex) const throw(ERRange,ERRejected) = 0;

    virtual void    Pivot(TIndex,TIndex,TLowerUpper) throw(ERRange,ERRejected) = 0;

    virtual TFloat  ObjVal() const throw();
    virtual TFloat  Slack(TRestr,TLowerUpper) const throw(ERRange);
    // Slack(i,LB)=A_ix-L_i bzw. x_i-l_i;
    // Slack(i,UB)=U_i-A_ix bzw. u_i-x_i;

    virtual bool    PrimalFeasible(TFloat epsilon = 0.01) throw();
    virtual bool    DualFeasible(TFloat epsilon = 0.01) throw();


    // *************************************************************** //
    //                           Algorithms                            //
    // *************************************************************** //

    enum TSimplexMethod {SIMPLEX_AUTO=0,SIMPLEX_PRIMAL=1,SIMPLEX_DUAL=2};
    enum TStartBasis    {START_AUTO=0,START_LRANGE=1,START_CURRENT=2};
    enum TPricing       {VAR_PRICING=0,FIRST_FIT=1};
    enum TQTest         {EXACT=0,MAX_ABS=1};

    virtual TFloat  SolvePrimal() throw() = 0;      // Phase II
    virtual TFloat  SolveDual() throw() = 0;        // Phase II
    virtual bool    StartPrimal() throw() = 0;      // Phase I
    virtual bool    StartDual() throw() = 0;        // Phase I

    virtual TVar    PricePrimal() throw(ERRejected) = 0;
    virtual TRestr  QTestPrimal(TVar) throw(ERRejected) = 0;
    virtual TRestr  PriceDual() throw(ERRejected) = 0;
    virtual TVar    QTestDual(TRestr) throw(ERRejected) = 0;

    virtual TFloat  SolveLP() throw(ERRejected);
    virtual TFloat  SolveMIP() throw(ERRejected);

    virtual void    AddCuttingPlane() throw(ERRejected);


    // *************************************************************** //
    //                          LP composition                         //
    // *************************************************************** //

    mipInstance*   Clone() throw(ERRejected);
    mipInstance*   DualForm() throw(ERRejected);
    mipInstance*   StandardForm() throw(ERRejected);
    mipInstance*   CanonicalForm() throw(ERRejected);
    void                FlipObjectSense() throw();


    /// \addtogroup groupTextDisplay
    /// @{

    /// \brief  Export a mip object to file, in a class dependent ASCII format
    ///
    /// \param fileName  The destination file name
    /// \param format    A join of TDisplayOpt options
    void  ExportToAscii(const char* fileName,TOption format=0) const throw(ERFile);

    /// @}

};


inline TVar mipInstance::PivotColumn() const throw()
{
    return pivotColumn;
}


inline TRestr mipInstance::PivotRow() const throw()
{
    return pivotRow;
}


inline mipInstance::TLowerUpper mipInstance::PivotDirection() const throw()
{
    return pivotDir;
}



//****************************************************************************//
//                   Entry function and package information                   //
//****************************************************************************//


/// \brief  Base class for factories of #mipInstance objects

class mipFactory : public virtual managedObject
{
public:

    mipFactory() throw();

    virtual ~mipFactory() throw();

    virtual mipInstance*
        NewInstance(TRestr,TVar,TIndex,TObjectSense,goblinController&)
            const throw(ERRange) = 0;

    virtual mipInstance*
        ReadInstance(const char*,goblinController&) const throw(ERRange) = 0;

    virtual char*   Authors() const throw() = 0;
    virtual int     MajorRelease() const throw() = 0;
    virtual int     MinorRelease() const throw() = 0;
    virtual char*   PatchLevel() const throw() = 0;
    virtual char*   BuildDate() const throw() = 0;
    virtual char*   License() const throw() = 0;

    enum TLPOrientation {ROW_ORIENTED = 0,COLUMN_ORIENTED = 1};

    virtual TLPOrientation  Orientation() const throw() = 0;

};


#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2002
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   ilpWrapper.h
/// \brief  #mipInstance class interface

#ifndef _ILP_WRAPPER_H_
#define _ILP_WRAPPER_H_

#include "managedObject.h"
#include "fileImport.h"
#include "fileExport.h"


/// \brief  General interface class for LP and MIP instances

class mipInstance : public managedObject
{
private:

    unsigned        bufferLength;
    char*           labelBuffer;

protected:

    TFloat*         varValue;
    TVar            numVars;

public:

    mipInstance(goblinController&) throw();
    virtual ~mipInstance() throw();

    unsigned long   Allocated() const throw();

    bool            IsMixedILP() const;


    // *************************************************************** //
    //                             File IO                             //
    // *************************************************************** //

    enum TDisplayOpt {
        DISPLAY_PROBLEM = 0,
        DISPLAY_OBJECTIVE = 1,
        DISPLAY_RESTRICTIONS = 2,
        DISPLAY_BOUNDS = 4,
        DISPLAY_INTEGERS = 8,
        DISPLAY_FIXED = 16,
        DISPLAY_PRIMAL = 32,
        DISPLAY_DUAL = 64,
        DISPLAY_SLACKS = 128,
        DISPLAY_BASIS = 256,
        DISPLAY_TABLEAU = 512,
        DISPLAY_INVERSE = 1024,
        DISPLAY_VALUES = 2048
    };

    enum TLPFormat {
        MPS_FORMAT = 0,
        LP_FORMAT = 1,
        MPS_CPLEX = 2,
        BAS_CPLEX = 3,
        BAS_GOBLIN = 4,
        GOB_FORMAT = 5
    };


    /// \addtogroup groupObjectExport
    /// @{

    void    Write(const char*,TOption = 0)
                const throw(ERFile,ERRejected);
    void    Write(const char*,TLPFormat,TOption = 0)
                const throw(ERFile,ERRejected);
    void    WriteLPNaive(const char*,TDisplayOpt = DISPLAY_PROBLEM)
                const throw(ERFile,ERRejected);
    void    WriteMPSFile(const char*,TLPFormat = MPS_CPLEX)
                const throw(ERFile,ERRejected);
    void    WriteMPSFile(ofstream&,TLPFormat = MPS_CPLEX)
                const throw(ERFile,ERRejected);
    void    WriteBASFile(const char*,TLPFormat = BAS_CPLEX)
                const throw(ERFile,ERRejected);
    void    WriteBASFile(ofstream&,TLPFormat = BAS_CPLEX)
                const throw(ERFile,ERRejected);
    void    WriteVarValues(goblinExport*) const throw();
    char*   Display() const throw(ERFile,ERRejected);

    /// @}


    /// \addtogroup groupObjectImport
    /// @{

private:

    TVar    ReadRowLabel(char*) throw();
    TRestr  ReadColLabel(char*,bool) throw();

public:

    void    ReadMPSFile(const char*) throw(ERParse,ERRejected);
    void    ReadMPSFile(ifstream&) throw(ERParse,ERRejected);
    void    ReadBASFile(const char*) throw(ERParse,ERRejected);
    void    ReadBASFile(ifstream&) throw(ERParse,ERRejected);
    void    ReadVarValues(goblinImport*,TVar) throw(ERParse);

    /// @}

    void    NoSuchVar(char*,TVar) const throw(ERRange);
    void    NoSuchRestr(char*,TRestr) const throw(ERRange);


    // *************************************************************** //
    //                      Instance Manipulation                      //
    // *************************************************************** //

    enum TVarType
        {VAR_FLOAT=0,VAR_INT=1,VAR_CANCELED=2};

    virtual TVar    AddVar(TFloat,TFloat,TFloat,TVarType = VAR_FLOAT)
                        throw(ERRejected) = 0;
    virtual TRestr  AddRestr(TFloat,TFloat) throw(ERRejected) = 0;

    virtual void    DeleteVar(TVar) throw(ERRange,ERRejected) = 0;
    virtual void    DeleteRestr(TRestr) throw(ERRange) = 0;

    virtual void    SetURange(TVar,TFloat) throw(ERRange) = 0;
    virtual void    SetLRange(TVar,TFloat) throw(ERRange) = 0;
    virtual void    SetUBound(TRestr,TFloat) throw(ERRange) = 0;
    virtual void    SetLBound(TRestr,TFloat) throw(ERRange) = 0;
    virtual void    SetCost(TVar,TFloat) throw(ERRange) = 0;
    virtual void    SetVarType(TVar,TVarType) throw(ERRange) = 0;

    virtual void    SetVarLabel(TVar,char*,TOwnership = OWNED_BY_SENDER)
                        throw(ERRange,ERRejected) = 0;
    virtual void    SetRestrLabel(TRestr,char*,TOwnership = OWNED_BY_SENDER)
                        throw(ERRange,ERRejected) = 0;

    virtual void    SetObjectSense(TObjectSense) throw() = 0;

    virtual void    SetCoeff(TRestr,TVar,TFloat) throw(ERRange) = 0;

    virtual void    SetRow(TRestr i,TVar len,TVar* index,double* val)
                        throw(ERRange) = 0;
    // Sets row i of the coefficient matrix. len is the number
    // of nonzeros in the new row i. The coefficients are given by
    // index and val.

    virtual void    SetColumn(TVar j,TRestr len,TRestr* index,double* val)
                    throw(ERRange) = 0;
    // Sets columns j of the coefficient matrix. len is the
    // number of nonzeros in the new column j. The coefficients
    // are given by index and val.

    virtual void    SetVarVisibility(TVar,bool) throw(ERRange);
    virtual void    SetRestrVisibility(TRestr,bool) throw(ERRange);

    virtual void    Resize(TRestr,TVar,TIndex) throw(ERRange) = 0;
    virtual void    Strip() throw() = 0;

    virtual void    SetVarValue(TVar,TFloat) throw(ERRange);
    virtual void    InitVarValues(TFloat = InfFloat) throw();
    virtual void    ReleaseVarValues() throw();

    // *************************************************************** //
    //                      Retrieval Operations                       //
    // *************************************************************** //

    /// \addtogroup objectDimensions
    /// @{

    virtual TRestr  K() const throw() = 0;
    virtual TVar L() const throw() = 0;
    virtual TIndex  NZ() const throw() = 0;

    /// @}

    virtual TFloat  Cost(TVar) const throw(ERRange) = 0;
    virtual TFloat  URange(TVar) const throw(ERRange) = 0;
    virtual TFloat  LRange(TVar) const throw(ERRange) = 0;
    virtual TFloat  UBound(TRestr) const throw(ERRange) = 0;
    virtual TFloat  LBound(TRestr) const throw(ERRange) = 0;

    virtual TVarType    VarType(TVar) const throw(ERRange) = 0;

    virtual char*   VarLabel(TVar,TOwnership = OWNED_BY_RECEIVER) const throw(ERRange);
    virtual char*   RestrLabel(TRestr,TOwnership = OWNED_BY_RECEIVER) const throw(ERRange);
    char*           UnifiedLabel(TRestr,TOwnership = OWNED_BY_RECEIVER) const throw(ERRange);
    virtual TVar    VarIndex(char*) const throw();
    virtual TRestr  RestrIndex(char*) const throw();

    virtual TObjectSense  ObjectSense() const throw() = 0;

    virtual TFloat  Coeff(TRestr,TVar) const throw(ERRange) = 0;

    virtual TVar    GetRow(TRestr i,TVar* index,double* val) const throw(ERRange) = 0;
    // Gets Row i of the coefficient matrix and stores it in index
    // and val. val[j] is the coefficient of variable index[j] in row i.
    // Returns the number of nonzeros of row i. index and val have to
    // be allocated before calling the method.

    virtual TRestr  GetColumn(TVar j,TRestr* index,double* val) const throw(ERRange) = 0;
    // Gets Column j of the coefficient matrix and stores it in index
    // and val. val[i] is the coefficient of variable index[i] in row i.
    // Returns the number of nonzeros of column j. index and val have to
    // be allocated before calling the method.

    virtual bool    VarVisibility(TVar) const throw(ERRange);
    virtual bool    RestrVisibility(TRestr) const throw(ERRange);

    virtual TFloat  VarValue(TVar) const throw(ERRange);


    // *************************************************************** //
    //                    Basis related information                    //
    // *************************************************************** //

    enum TRestrType     {BASIC_LB=0,BASIC_UB=1,NON_BASIC=2,RESTR_CANCELED=3};

    virtual TRestrType  RestrType(TRestr) const throw(ERRange) = 0;
    virtual TRestr  Index(TVar) const throw(ERRange) = 0;
    virtual TRestr  RowIndex(TRestr) const throw(ERRange) = 0;
    virtual TVar    RevIndex(TRestr) const throw(ERRange) = 0;

    enum TLowerUpper    {LOWER=0,UPPER=1};

    virtual void    SetRestrType(TRestr,TLowerUpper) throw(ERRange,ERRejected) = 0;
    virtual void    SetIndex(TRestr,TVar,TLowerUpper) throw(ERRange,ERRejected) = 0;

protected:

    mutable TVar            pivotColumn;
    mutable TRestr          pivotRow;
    mutable TLowerUpper     pivotDir;

    virtual void    InitBasis() const throw() = 0;

public:

    TVar            PivotColumn() const throw();
    TRestr          PivotRow() const throw();
    TLowerUpper     PivotDirection() const throw();

    virtual void    ResetBasis() throw();
    virtual bool    Initial() const throw() = 0;

    virtual TFloat  X(TVar) const throw(ERRange) = 0;
    virtual TFloat  Y(TRestr,TLowerUpper) const throw(ERRange) = 0;

    virtual TFloat  Tableau(TIndex,TIndex) const throw(ERRange,ERRejected) = 0;
    virtual TFloat  BaseInverse(TIndex,TIndex) const throw(ERRange,ERRejected) = 0;

    virtual void    Pivot(TIndex,TIndex,TLowerUpper) throw(ERRange,ERRejected) = 0;

    virtual TFloat  ObjVal() const throw();
    virtual TFloat  Slack(TRestr,TLowerUpper) const throw(ERRange);
    // Slack(i,LB)=A_ix-L_i bzw. x_i-l_i;
    // Slack(i,UB)=U_i-A_ix bzw. u_i-x_i;

    virtual bool    PrimalFeasible(TFloat epsilon = 0.01) throw();
    virtual bool    DualFeasible(TFloat epsilon = 0.01) throw();


    // *************************************************************** //
    //                           Algorithms                            //
    // *************************************************************** //

    enum TSimplexMethod {SIMPLEX_AUTO=0,SIMPLEX_PRIMAL=1,SIMPLEX_DUAL=2};
    enum TStartBasis    {START_AUTO=0,START_LRANGE=1,START_CURRENT=2};
    enum TPricing       {VAR_PRICING=0,FIRST_FIT=1};
    enum TQTest         {EXACT=0,MAX_ABS=1};

    virtual TFloat  SolvePrimal() throw() = 0;      // Phase II
    virtual TFloat  SolveDual() throw() = 0;        // Phase II
    virtual bool    StartPrimal() throw() = 0;      // Phase I
    virtual bool    StartDual() throw() = 0;        // Phase I

    virtual TVar    PricePrimal() throw(ERRejected) = 0;
    virtual TRestr  QTestPrimal(TVar) throw(ERRejected) = 0;
    virtual TRestr  PriceDual() throw(ERRejected) = 0;
    virtual TVar    QTestDual(TRestr) throw(ERRejected) = 0;

    virtual TFloat  SolveLP() throw(ERRejected);
    virtual TFloat  SolveMIP() throw(ERRejected);

    virtual void    AddCuttingPlane() throw(ERRejected);


    // *************************************************************** //
    //                          LP composition                         //
    // *************************************************************** //

    mipInstance*   Clone() throw(ERRejected);
    mipInstance*   DualForm() throw(ERRejected);
    mipInstance*   StandardForm() throw(ERRejected);
    mipInstance*   CanonicalForm() throw(ERRejected);
    void                FlipObjectSense() throw();


    /// \addtogroup groupTextDisplay
    /// @{

    /// \brief  Export a mip object to file, in a class dependent ASCII format
    ///
    /// \param fileName  The destination file name
    /// \param format    A join of TDisplayOpt options
    void  ExportToAscii(const char* fileName,TOption format=0) const throw(ERFile);

    /// @}

};


inline TVar mipInstance::PivotColumn() const throw()
{
    return pivotColumn;
}


inline TRestr mipInstance::PivotRow() const throw()
{
    return pivotRow;
}


inline mipInstance::TLowerUpper mipInstance::PivotDirection() const throw()
{
    return pivotDir;
}



//****************************************************************************//
//                   Entry function and package information                   //
//****************************************************************************//


/// \brief  Base class for factories of #mipInstance objects

class mipFactory : public virtual managedObject
{
public:

    mipFactory() throw();

    virtual ~mipFactory() throw();

    virtual mipInstance*
        NewInstance(TRestr,TVar,TIndex,TObjectSense,goblinController&)
            const throw(ERRange) = 0;

    virtual mipInstance*
        ReadInstance(const char*,goblinController&) const throw(ERRange) = 0;

    virtual char*   Authors() const throw() = 0;
    virtual int     MajorRelease() const throw() = 0;
    virtual int     MinorRelease() const throw() = 0;
    virtual char*   PatchLevel() const throw() = 0;
    virtual char*   BuildDate() const throw() = 0;
    virtual char*   License() const throw() = 0;

    enum TLPOrientation {ROW_ORIENTED = 0,COLUMN_ORIENTED = 1};

    virtual TLPOrientation  Orientation() const throw() = 0;

};


#endif
