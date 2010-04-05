
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written byChristian Fremuth-Paeger, March 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   lpBasis.cpp
/// \brief  Handling of base indices and tableaus, extracting solutions from the current basis

#include "lpSolver.h"
#include "fileImport.h"
#include "moduleGuard.h"


mipInstance::TRestrType goblinLPSolver::RestrType(TRestr i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchRestr("RestrType",i);

    #endif

    if (i>=kAct) i += (kMax-kAct);

    return restrType[i];
}


void goblinLPSolver::SetRestrType(TRestr i,TLowerUpper rt)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchRestr("RestrType",i);

    #endif

    if (i>=kAct) i += (kMax-kAct);

    #if defined(_FAILSAVE_)

    if (restrType[i]==RESTR_CANCELED || restrType[i]==NON_BASIC)
        Error(ERR_REJECTED,"SetRestrType","Restriction must be basic");

    #endif

    restrType[i] = TRestrType(rt);
}


TRestr goblinLPSolver::Index(TVar i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct)  NoSuchRestr("Index",i);

    #endif

    if (index[i]<kMax || index[i]==NoRestr)
        return index[i];
    else return index[i]-(kMax-kAct);
}


TVar goblinLPSolver::RevIndex(TRestr i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchRestr("RevIndex",i);

    #endif

    if (i>=kAct) i += (kMax-kAct);

    return revIndex[i];
}


void goblinLPSolver::SetIndex(TRestr i,TVar j,TLowerUpper rt)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchRestr("SetIndex",i);
    if (j>=lAct)  NoSuchVar("SetIndex",j);
    if (VarType(j)==VAR_CANCELED)
        Error(ERR_REJECTED,"SetIndex","Pivot variable is canceled");
    if (restrType[i]==RESTR_CANCELED)
        Error(ERR_REJECTED,"SetIndex","Pivot restriction is canceled");

    #endif

    if (i>=kAct) i += (kMax-kAct);

    TRestr i2 = index[j];
    TRestrType rt2 = NON_BASIC;

    if (i2!=i && i2!=NoRestr)
    {
        rt2 = restrType[i2];
        restrType[i2] = NON_BASIC;
        revIndex[i2] = NoVar;
    }

    TVar j2 = revIndex[i];

    if (j2!=j && j2!=NoVar)
    {
        if (i2!=i && i2!=NoRestr)
        {
            index[j2]     = i2;
            revIndex[i2]  = j2;
            restrType[i2] = rt2;
        }
        else index[j2] = NoRestr;
    }

    index[j]     = i;
    revIndex[i]  = j;
    restrType[i] = TRestrType(rt);

    baseInitial = false;
    baseValid = false;
    dataValid = false;
}


void goblinLPSolver::InitBasis()
    const throw()
{
    for (TIndex i=0;i<kAct;i++)
    {
        revIndex[i]  = NoVar;
        restrType[i] = NON_BASIC;
    }

    for (TIndex i=0;i<lAct;i++)
    {
        index[i]          = i+kMax;
        revIndex[i+kMax]  = i;
        restrType[i+kAct] = BASIC_LB;
    }

    baseInitial = true;
    baseValid = false;
    dataValid = false;

    pivotColumn = NoVar;
    pivotRow = NoRestr;
}


void goblinLPSolver::DefaultBasisInverse()
    const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!baseInitial)
        Error(ERR_REJECTED,"DefaultBasisInverse","Basis is not initial");

    #endif

    if (baseInv==NULL)
    {
        baseInv = new denseMatrix<TIndex,TFloat>(lAct,lAct,CT);
        keptInv = new denseMatrix<TIndex,TFloat>(lAct,lAct,CT);
        x = new TFloat[lAct];
        y = new TFloat[kAct+lAct];
    }

    // Compute basis inverse matrix

    if (!baseValid)
    {
        for (TVar i=0;i<lAct;i++)
            for (TVar j=0;j<lAct;j++)
                if (i==j) baseInv->SetCoeff(i,j,1);
                else baseInv->SetCoeff(i,j,0);
    }

    // Compute basic solution

    for (TVar i=0;i<lAct;i++)
    {
        if(LBound(kAct+i)==-InfFloat && UBound(kAct+i)==InfFloat) x[i]=0;
        else
        {
            if(RestrType(kAct+i)==BASIC_LB)
                x[i]=LBound(kAct+i);
            else if(RestrType(kAct+i)==BASIC_UB)
                x[i]=UBound(kAct+i);
        }
    }

    //Compute dual basic solution

    for (TVar i=0;i<kAct;i++) y[i]=0;
    for (TVar i=kAct;i<kAct+lAct;i++) y[i]=Cost(i-kAct);  

    baseValid = true;
    dataValid = true;
}


void goblinLPSolver::EvaluateBasis()
    const throw(ERRejected)
{
    if (dataValid) return;

    if (baseInitial)
    {
        DefaultBasisInverse();
        return;
    }

    moduleGuard M(ModLpPivoting,*this);

    if (baseInv==NULL)
    {
        baseInv = new denseMatrix<TIndex,TFloat>(lAct,lAct,CT);
        keptInv = new denseMatrix<TIndex,TFloat>(lAct,lAct,CT);
        x = new TFloat[lAct];
        y = new TFloat[kAct+lAct];
        baseValid = false;
    }

    if (!baseValid)
    {
        // Compute basis inverse matrix

        for (TIndex i=0;i<lAct;i++)
        {
            // Find next restriction in basis

            TIndex iNext = Index(i);

            if (iNext==NoRestr)
                Error(ERR_REJECTED,"EvaluateBasis","Incomplete basis information");

            // Copy matrix coefficients

            for (TIndex j=0;j<lAct;j++) 
            {
                TFloat thisCoeff = 0;

                if (iNext<kAct) thisCoeff = Coeff(iNext,j);
                else
                {
                    if (j==iNext-kAct) thisCoeff = 1;
                    else thisCoeff = 0;
                }

                keptInv -> SetCoeff(i,j,thisCoeff);

                if (i==j) baseInv->SetCoeff(i,j,1);
                else baseInv->SetCoeff(i,j,0);
            }
        }

        keptInv -> GaussElim(*baseInv);

        baseValid = true;

        M.Shutdown(LOG_METH2,"...Basis information is evaluated");
    }

    SolutionUpdate();
}


void goblinLPSolver::Pivot(TIndex j,TIndex i,TLowerUpper rt)
    throw(ERRange,ERRejected)
{
    // entering: i with type rt, leaving: j

    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchRestr("Pivot",i);
    if (j>=kAct+lAct)  NoSuchRestr("Pivot",j);
    if (RestrType(j)==RESTR_CANCELED)
        Error(ERR_REJECTED,"Pivot","Leaving row is canceled");
    if (RestrType(i)==RESTR_CANCELED)
        Error(ERR_REJECTED,"Pivot","Entering row is canceled");
    if (RestrType(i)!=NON_BASIC && i!=j)
        Error(ERR_REJECTED,"Pivot","Row is already in basis");
    if (RestrType(j)==NON_BASIC)
        Error(ERR_REJECTED,"Pivot","Leaving row is non-basic");

    #endif

    pivotRow = i;
    pivotColumn = RevIndex(j); 

    SetIndex(i,RevIndex(j),rt);

    if (!baseInv) EvaluateBasis();
    else BasisUpdate(pivotRow,pivotColumn);
}


void goblinLPSolver::BasisUpdate(TRestr i,TVar j)
    const throw(ERRange,ERRejected)
{
    // entering: i with type rt, leaving: Index(j)

    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchRestr("BasisUpdate",i);
    if (j>=lAct)  NoSuchVar("BasisUpdate",j);
    if (!baseInv)
        Error(ERR_REJECTED,"BasisUpdate","No initial basis inverse");

    #endif

    moduleGuard M(ModLpPivoting,*this);

    // SetIndex() has cleared this flag but we want access to the old tableau

    baseValid = true;

    // Update basis inverse

    TFloat pivotElt = Tableau(Index(j),i);

    if (fabs(pivotElt)<EPSILON)
        Error(ERR_REJECTED,"BasisUpdate","Pivot element too small");

    TFloat* pivotCand = new TFloat[lAct];
    for (TVar r=0;r<lAct;r++)
        pivotCand[r] = Tableau(Index(r),i);

    for (TVar r=0;r<j;r++)
        for(TVar s=0;s<lAct;s++)
            baseInv->SetCoeff(s,r,
                baseInv->Coeff(s,r)-pivotCand[r]*baseInv->Coeff(s,j)/pivotElt);

    for (TVar r=j+1;r<lAct;r++)
        for(TVar s=0;s<lAct;s++)
            baseInv->SetCoeff(s,r,
                baseInv->Coeff(s,r)-pivotCand[r]*baseInv->Coeff(s,j)/pivotElt);

    for (TVar s=0;s<lAct;s++)
        baseInv->SetCoeff(s,j, baseInv->Coeff(s,j)/pivotElt);

    delete[] pivotCand;

    M.Shutdown(LOG_METH2,"...Basis inverse is updated");
}


void goblinLPSolver::SolutionUpdate()
    const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!baseInv)
        Error(ERR_REJECTED,"SolutionUpdate","Base inverse does not exist");
    if (!baseValid)
        Error(ERR_REJECTED,"SolutionUpdate","Base inverse is not up to date");

    #endif

    moduleGuard M(ModLpPivoting,*this);

    // Compute primal solution

    for (TIndex i=0;i<lAct;i++)
    {
        x[i] = 0;

        for (TIndex j=0;j<lAct;j++)
        {
            //If basic restriction corresponds to free variable, then RHS=0

            TFloat RHS = 0;
            TRestr k=Index(j);

            if (RestrType(k)==BASIC_UB && UBound(k)<InfFloat)
                RHS = UBound(k);
            else if(LBound(k)> -InfFloat)
                RHS = LBound(k);

            x[i] += baseInv->Coeff(i,j)*RHS;
        }
    }

    // Compute dual solution

    for (TIndex i=0;i<kAct+lAct;i++)
    {
        y[i] = 0;

        if (RestrType(i)!=BASIC_UB && RestrType(i)!=BASIC_LB) continue;

        TVar j2 = RevIndex(i);

        for (TIndex j=0;j<lAct;j++)
            y[i] += baseInv->Coeff(j,j2)*Cost(j);
    }

    dataValid = true;

    M.Shutdown(LOG_METH2,"...Solutions are updated");
}


TFloat goblinLPSolver::X(TVar i)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=lAct)  NoSuchVar("X",i);

    #endif

    if (!dataValid) EvaluateBasis();

    return x[i];
}


TFloat goblinLPSolver::Y(TRestr i,TLowerUpper rt)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchRestr("Y",i);

    #endif

    if (TRestrType(rt)!=RestrType(i)) return 0;

    if (!dataValid) EvaluateBasis();

    return y[i];
}


TFloat goblinLPSolver::Tableau(TIndex i,TIndex j)
    const throw(ERRange,ERRejected)
{
    // Returns A_j(A_B)^(-1)e_i

    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchRestr("Tableau",i);
    if (j>=kAct+lAct)  NoSuchRestr("Tableau",j);

    if (RevIndex(i)==NoVar) 
        Error(ERR_REJECTED,"Tableau","Non-basic row");

    #endif

    if (!baseValid) EvaluateBasis();

    TVar i0 = RevIndex(i);

    if (j>=kAct) return baseInv->Coeff(j-kAct,i0);

    TFloat element = 0;

    for (TVar k=0;k<lAct;k++)
        element += Coeff(j,k)*baseInv->Coeff(k,i0);

    return element;
}


TFloat goblinLPSolver::BaseInverse(TIndex i,TIndex j)
    const throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (i>=kAct+lAct)  NoSuchVar("BaseInverse",i);
    if (j>=lAct)  NoSuchVar("BaseInverse",j);

    if (RevIndex(i)==NoVar) 
        Error(ERR_REJECTED,"BaseInverse","Non-basic row");

    #endif

    if (!baseValid) EvaluateBasis();

    return baseInv->Coeff(RevIndex(i),j);
}
