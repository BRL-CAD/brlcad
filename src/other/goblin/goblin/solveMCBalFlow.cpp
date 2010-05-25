
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveMCBalFlow.cpp
/// \brief  Methods for weighted balanced network flow problems

#include "abstractBalanced.h"
#include "balancedToBalanced.h"
#include "surfaceGraph.h"
#include "moduleGuard.h"


TFloat abstractBalancedFNW::MinCBalFlow(TNode s) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("MinCBalFlow",s);

    if (MaxLCap()>0)
        Error(ERR_REJECTED,"MinCBalFlow","Non-trivial lower bounds");

    #endif

    sprintf(CT.logBuffer,"Computing minimum cost balanced (%lu,%lu)-flow...",
        static_cast<unsigned long>(s),static_cast<unsigned long>(s^1));
    moduleGuard M(ModMinCBalFlow,*this,CT.logBuffer);

    TFloat ret = InfFloat;

    switch (CT.methMinCBalFlow)
    {
        case 0:
        {
            ret = PrimalDual(s);
            break;
        }
        case 1:
        {
            ret = EnhancedPD(s);
            break;
        }
        default:
        {
            UnknownOption("MinCBalFlow",CT.methMinCBalFlow);
            throw ERRejected();
        }
    }

    return ret;
}


TFloat abstractBalancedFNW::PrimalDual(TNode s) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("PrimalDual",s);

    if (MaxLCap()>0) Error(ERR_REJECTED,"PrimalDual","Non-trivial lower bounds");

    #endif

    moduleGuard M(ModPrimalDual,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    TFloat ret = Weight();
    M.SetLowerBound(ret);

    Symmetrize();

    surfaceGraph G(*this);

    switch (CT.methPrimalDual)
    {
        case 0:
        {
            G.PrimalDual0(s);
            break;
        }
        case 1:
        case 2:
        {
            G.PrimalDual1(s);
            break;
        }
        default:
        {
            UnknownOption("PrimalDual",CT.methPrimalDual);
        }
    }

    #if defined(_FAILSAVE_)

    if (CT.methFailSave==1 && !G.Compatible())
    {
        InternalError("PrimalDual","Solutions are corrupted");
    }

    #endif

    ret = Weight();

    M.SetBounds(ret,ret);

    return ret;
}


TFloat abstractBalancedFNW::EnhancedPD(TNode s) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("EnhancedPD",s);

    if (MaxLCap()>0)
        Error(ERR_REJECTED,"EnhancedPD","Non-trivial lower bounds");

    #endif

    moduleGuard M(ModEnhancedPD,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n1+1+n1*n1, n1);

    #endif

    MinCostSTFlow(s,s^1);

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(1);

    #endif

    CancelEven();

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(n1*n1);

    #endif

    TFloat ret = CancelPD();

    return ret;
}


TFloat abstractBalancedFNW::CancelPD() throw()
{
    #if defined(_FAILSAVE_)

    if (Q==NULL) Error(ERR_REJECTED,"CancelPD","No odd cycles present");

    #endif

    balancedToBalanced G(*this);

    LogEntry(LOG_METH,"Refining balanced flow...");

    return G.PrimalDual(G.DefaultSourceNode());
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveMCBalFlow.cpp
/// \brief  Methods for weighted balanced network flow problems

#include "abstractBalanced.h"
#include "balancedToBalanced.h"
#include "surfaceGraph.h"
#include "moduleGuard.h"


TFloat abstractBalancedFNW::MinCBalFlow(TNode s) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("MinCBalFlow",s);

    if (MaxLCap()>0)
        Error(ERR_REJECTED,"MinCBalFlow","Non-trivial lower bounds");

    #endif

    sprintf(CT.logBuffer,"Computing minimum cost balanced (%lu,%lu)-flow...",
        static_cast<unsigned long>(s),static_cast<unsigned long>(s^1));
    moduleGuard M(ModMinCBalFlow,*this,CT.logBuffer);

    TFloat ret = InfFloat;

    switch (CT.methMinCBalFlow)
    {
        case 0:
        {
            ret = PrimalDual(s);
            break;
        }
        case 1:
        {
            ret = EnhancedPD(s);
            break;
        }
        default:
        {
            UnknownOption("MinCBalFlow",CT.methMinCBalFlow);
            throw ERRejected();
        }
    }

    return ret;
}


TFloat abstractBalancedFNW::PrimalDual(TNode s) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("PrimalDual",s);

    if (MaxLCap()>0) Error(ERR_REJECTED,"PrimalDual","Non-trivial lower bounds");

    #endif

    moduleGuard M(ModPrimalDual,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    TFloat ret = Weight();
    M.SetLowerBound(ret);

    Symmetrize();

    surfaceGraph G(*this);

    switch (CT.methPrimalDual)
    {
        case 0:
        {
            G.PrimalDual0(s);
            break;
        }
        case 1:
        case 2:
        {
            G.PrimalDual1(s);
            break;
        }
        default:
        {
            UnknownOption("PrimalDual",CT.methPrimalDual);
        }
    }

    #if defined(_FAILSAVE_)

    if (CT.methFailSave==1 && !G.Compatible())
    {
        InternalError("PrimalDual","Solutions are corrupted");
    }

    #endif

    ret = Weight();

    M.SetBounds(ret,ret);

    return ret;
}


TFloat abstractBalancedFNW::EnhancedPD(TNode s) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("EnhancedPD",s);

    if (MaxLCap()>0)
        Error(ERR_REJECTED,"EnhancedPD","Non-trivial lower bounds");

    #endif

    moduleGuard M(ModEnhancedPD,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n1+1+n1*n1, n1);

    #endif

    MinCostSTFlow(s,s^1);

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(1);

    #endif

    CancelEven();

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(n1*n1);

    #endif

    TFloat ret = CancelPD();

    return ret;
}


TFloat abstractBalancedFNW::CancelPD() throw()
{
    #if defined(_FAILSAVE_)

    if (Q==NULL) Error(ERR_REJECTED,"CancelPD","No odd cycles present");

    #endif

    balancedToBalanced G(*this);

    LogEntry(LOG_METH,"Refining balanced flow...");

    return G.PrimalDual(G.DefaultSourceNode());
}
