
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Bernhard Schmidt, January 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   lpSolver.cpp
/// \brief  Primal and dual simplex code for the #goblinLPSolver class

#include "lpSolver.h"
#include "moduleGuard.h"


//****************************************************************************//
//                         Primal Phase II                                    //
//****************************************************************************//


TVar goblinLPSolver::PricePrimal()
    throw(ERRejected)
{
    moduleGuard M(ModLpPricing,*this,moduleGuard::NO_INDENT);

    for (TVar j=0;j<lAct;j++)
    {
        TRestr ind = Index(j);

        if (LBound(ind)>=UBound(ind)) continue;

        TFloat test = Y(ind,TLowerUpper(RestrType(ind)));

        if ((  LBound(ind)== -InfFloat && 
               UBound(ind)== InfFloat &&
               fabs(test)>TOLERANCE         ) ||
            (RestrType(ind)==BASIC_LB && test< -TOLERANCE) ||
            (RestrType(ind)==BASIC_UB && test> TOLERANCE)
           )
        {
            sprintf(CT.logBuffer,"Pivot variable is %ld...",j);
            M.Shutdown(LOG_METH2,CT.logBuffer);

            return j;
        }
    }

    return NoVar;
}


TRestr  goblinLPSolver::QTestPrimal(TVar i)
    throw(ERRejected)
{
    moduleGuard M(ModLpQTest,*this,moduleGuard::NO_INDENT);

    TRestr k = NoRestr;
    TFloat lambda = InfFloat;
    TFloat pivotAbs = 0;

    short sign = 1;

    if (Y(Index(i),LOWER)+Y(Index(i),UPPER)<0) sign=-1;

    TFloat pivotElt = 0;
    TFloat schrittweite = -1;

    // case that restriction Index(i) reaches other bound

    if (sign==1 && LBound(Index(i))!= -InfFloat)
    {
        lambda = Slack(Index(i),LOWER)+DELTA;
    }

    if (sign==-1 && UBound(Index(i))!=InfFloat)
    {
        lambda = Slack(Index(i),UPPER)+DELTA;
    }

    // nonbasic restrictions

    for (TRestr j=0;j<kAct+lAct;j++)
    {
        if (RestrType(j)==BASIC_LB || RestrType(j)==BASIC_UB) continue;

        pivotElt = -sign*Tableau(Index(i),j);

        if (fabs(pivotElt)<1e-10) continue;

        if (pivotElt<0)
        {
            if (LBound(j)==-InfFloat) continue;

            TFloat test = -(Slack(j,LOWER)+DELTA)/pivotElt;

            if (test<lambda) lambda = test;
        }
        else
        {
            if (UBound(j)==InfFloat) continue;

            TFloat test = (Slack(j,UPPER)+DELTA)/pivotElt;

            if (test < lambda) lambda = test;
        }
    }

    if (sign==1 && LBound(Index(i))!=-InfFloat && Slack(Index(i),LOWER)<=lambda)
    {
        k = Index(i);
        pivotAbs = 1;
        schrittweite = Slack(Index(i),LOWER);
    }

    if (sign==-1 && UBound(Index(i))!=InfFloat && Slack(Index(i),UPPER)<=lambda)
    {
        k = Index(i);
        pivotAbs = 1;  
        schrittweite = Slack(Index(i),UPPER); 
    }

    for (TRestr j=0;j<kAct+lAct;j++)
    {
        if (RestrType(j)==BASIC_LB || RestrType(j)==BASIC_UB) continue;

        pivotElt = -sign*Tableau(Index(i),j);

        if (fabs(pivotElt)<1e-10) continue;

        if (pivotElt<0)
        {
            if (LBound(j)==-InfFloat) continue;

            if (fabs(pivotElt)>pivotAbs && (-Slack(j,LOWER)/pivotElt)<=lambda)
            {
                k = j;
                pivotAbs = fabs(pivotElt); 
                schrittweite = -Slack(j,LOWER)/pivotElt; 
            }
        }
        else
        {
            if (UBound(j)==InfFloat) continue;

            if (fabs(pivotElt)>pivotAbs && (Slack(j,UPPER)/pivotElt)<=lambda)
            {
                k = j;
                pivotAbs = fabs(pivotElt);
                schrittweite = Slack(j,UPPER)/pivotElt; 
            }
        }
    }

    sprintf(CT.logBuffer,"...Entering row is %ld",k);
    M.Shutdown(LOG_METH2,CT.logBuffer);

    return k;
}


TFloat goblinLPSolver::SolvePrimal()
    throw()
{
    moduleGuard M(ModLpSolve,*this);

    TFloat ret = InfFloat;
    unsigned long itCount = 0;

    while (CT.SolverRunning())
    {
        pivotColumn = PricePrimal();
        if (pivotColumn==NoVar)
        {
            ret = ObjVal();
            break;
        }

        pivotRow = QTestPrimal(pivotColumn);

        if (pivotRow==NoRestr) break;

        short sign = 1;

        if (Y(Index(pivotColumn),LOWER)+Y(Index(pivotColumn),UPPER)<0) sign=-1;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"...Leaving row is %ld",Index(pivotColumn));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        TFloat pivotElt = -sign*Tableau(Index(pivotColumn),pivotRow); 

        if (pivotElt<0)
        {
            pivotDir = LOWER;
            LogEntry(LOG_METH2,"...Entering at lower bound");
        }
        else
        {
            pivotDir = UPPER;
            LogEntry(LOG_METH2,"...Entering at upper bound");
        }

        M.Trace();

        Pivot(Index(pivotColumn),pivotRow,pivotDir);

        itCount++;
    }

    pivotColumn = NoVar;

    if (CT.logMeth==1)
    {
        sprintf(CT.logBuffer,"%ld pivots in total",itCount);
        M.Shutdown(LOG_METH,CT.logBuffer);
    }

    return ret;
}


//****************************************************************************//
//                            Dual Phase II                                   //
//****************************************************************************//


TRestr goblinLPSolver::PriceDual()
    throw(ERRejected) 
{
    moduleGuard M(ModLpPricing,*this,moduleGuard::NO_INDENT);

    for (TRestr j=0;j<kAct+lAct-1;j++)
    {
        if (RestrType(j)==BASIC_LB || RestrType(j)==BASIC_UB) continue;

        if (Slack(j,LOWER)< -TOLERANCE || Slack(j,UPPER)< -TOLERANCE)
        {
            sprintf(CT.logBuffer,"Entering row is %ld...",j);
            M.Shutdown(LOG_METH2,CT.logBuffer);

            return j;
        }
    }

    return NoRestr;
}


TVar goblinLPSolver::QTestDual(TRestr k)
    throw(ERRejected) 
{
    moduleGuard M(ModLpQTest,*this,moduleGuard::NO_INDENT);

    short sign = 1;

    if (Slack(k,UPPER)<0) sign = -1;

    TFloat lambda = InfFloat;
    TVar i = NoVar;

    for (TVar j=0;j<lAct;j++)
    {
        TRestr ind = Index(j);

        if (LBound(ind)>=UBound(ind)) continue;

        TFloat pivotElt = sign*Tableau(Index(j),k);
        TFloat redKosten = Y(ind,TLowerUpper(RestrType(ind))); 

        if (LBound(ind)== -InfFloat && 
            UBound(ind)==InfFloat &&
            fabs(pivotElt)>EPSILON)
        {
            i = j;
            break;
        }

        if (((pivotElt>EPSILON && RestrType(ind)== BASIC_LB) ||
             (pivotElt< -EPSILON && RestrType(ind)== BASIC_UB)) &&
            redKosten/pivotElt < lambda)
        {
            i = j;
            lambda = redKosten/pivotElt;
        }
    }

    sprintf(CT.logBuffer,"...Pivot variable is %ld",i);
    M.Shutdown(LOG_METH2,CT.logBuffer);

    return i;
}


TFloat goblinLPSolver::SolveDual()
    throw() 
{
    moduleGuard M(ModLpSolve,*this);

    TFloat ret = InfFloat;
    unsigned long itCount = 0;

    while (CT.SolverRunning())
    {
        pivotRow = PriceDual();

        if (pivotRow==NoRestr)
        {
            ret = ObjVal();
            break;
        }

        pivotColumn = QTestDual(pivotRow);

        if (pivotColumn==NoVar) break;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"...Leaving row is %ld",Index(pivotColumn));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        if (Slack(pivotRow,LOWER)<0)
        {
            pivotDir = LOWER;
            LogEntry(LOG_METH2,"...Entering at lower bound");
        }
        else
        {
            pivotDir = UPPER;
            LogEntry(LOG_METH2,"...Entering at upper bound");
        }

        M.Trace();

        Pivot(Index(pivotColumn),pivotRow,pivotDir);

        itCount++;
    }

    if (CT.logMeth==1)
    {
        sprintf(CT.logBuffer,"%ld pivots in total",itCount);
        M.Shutdown(LOG_METH,CT.logBuffer);
    }

    return ret;
}


//****************************************************************************//
//                            Primal Phase I                                  //
//****************************************************************************//


void goblinLPSolver::DuallyFeasibleBasis() throw()
{
    if (CT.methLPStart==int(START_LRANGE)) ResetBasis();

    if (baseInitial)
    {
        for (TVar i=0;i<lAct;i++)
        {
            if (UBound(i+kAct)==InfFloat && Cost(i)<0)
            {
                SetIndex(i+kAct,i,LOWER);
                SetCost(i,0);
            }

            if (LBound(i+kAct)== -InfFloat && Cost(i)>0)
            {
                SetIndex(i+kAct,i,UPPER);
                SetCost(i,0);
            }

            if (UBound(i+kAct)!=InfFloat && Cost(i)<=0)
                SetIndex(i+kAct,i,UPPER);
            else SetIndex(i+kAct,i,LOWER);
        }

        DefaultBasisInverse();
    }
    else
    {
        EvaluateBasis(); // Remove this later

        for (TVar j=0;j<lAct;j++)
        {
            TRestr i = Index(j);
            TFloat c = Y(j,TLowerUpper(RestrType(i)));

            if (RestrType(i)==BASIC_UB && c>0)
            {
                SetRestrType(i,LOWER);
                SetCost(j,Cost(j)+c);
            }

            if (RestrType(i)==BASIC_LB && c<0)
            {
                SetRestrType(i,UPPER);
                SetCost(j,Cost(j)-c);
            }

            dataValid = true;
        }
    }
}


bool goblinLPSolver::StartPrimal()
    throw()
{
    moduleGuard M(ModLpSolve,*this,moduleGuard::NO_INDENT);

    TFloat* costBackup = new TFloat[lAct]; 

    for (TVar i=0;i<lAct;i++) costBackup[i] = Cost(i);

    DuallyFeasibleBasis();

    TFloat result = SolveDual();

    for (TVar i=0;i<lAct;i++) SetCost(i,costBackup[i]);  

    delete[] costBackup;

    baseValid = true;

    SolutionUpdate();

    return (result!=InfFloat);
}


//****************************************************************************//
//                            Dual Phase I                                    //
//****************************************************************************//


void goblinLPSolver::PrimallyFeasibleBasis()
    throw()
{
    if (CT.methLPStart==int(START_LRANGE)) ResetBasis();

    if (baseInitial)
    {
        for (TRestr j=0;j<lAct;j++)
        {
            if (LRange(j)!= -InfFloat ||
                (LRange(j)== -InfFloat && URange(j)== InfFloat) )
                SetIndex(j+kAct,j,LOWER);
            else SetIndex(j+kAct,j,UPPER);
        }

        DefaultBasisInverse();
    }
    else
    {
        EvaluateBasis(); // Remove this later
    }

    for (TRestr j=0;j<kAct;j++)
    {
        TFloat slackLB = Slack(j,LOWER);

        if (slackLB<0) SetLBound(j,LBound(j)+slackLB);
        else
        {
            dataValid = true;
            TFloat slackUB = Slack(j,UPPER);
            if(slackUB<0) SetUBound(j,UBound(j)-slackUB);
        }

        dataValid = true;
    }

    if (baseInitial) DefaultBasisInverse();
}


bool goblinLPSolver::StartDual()
    throw()
{
    moduleGuard M(ModLpSolve,*this,moduleGuard::NO_INDENT);

    TFloat* lBackup = new TFloat[kAct];
    TFloat* uBackup = new TFloat[kAct];

    for (TRestr j=0;j<kAct;j++)
    {
        lBackup[j] = LBound(j);
        uBackup[j] = UBound(j);
    }

    PrimallyFeasibleBasis();

    TFloat result = SolvePrimal();

    for (TRestr j=0;j<kAct;j++)
    {
        SetLBound(j,lBackup[j]);
        SetUBound(j,uBackup[j]);
    }

    baseValid = true;

    SolutionUpdate();

    delete[] lBackup;
    delete[] uBackup;

    return (result!=InfFloat);
}

//****************************************************************************//
//****************************************************************************//
