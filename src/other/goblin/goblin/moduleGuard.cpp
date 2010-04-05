
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, March 2008
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   moduleGuard.cpp
/// \brief  #moduleGuard class implementation

#include "moduleGuard.h"


moduleGuard::moduleGuard(TModule _guardedModule,const managedObject& X,TOption _options)
    throw() : guardedModule(_guardedModule),CT(X.Context()),
    OH(X.Handle()),options(_options & NO_INDENT)
{
    CT.OpenFold(guardedModule,_options);

    #if defined(_TIMERS_)

    CT.globalTimer[listOfModules[guardedModule].moduleTimer] -> Enable();

    #endif

    parent = CT.activeGuard;
    CT.activeGuard = this;

    #ifdef _PROGRESS_

    InitProgressCounter(1.0);

    #endif

    if (   (_options & SYNC_BOUNDS)
        && parent
//        && listOfModules[guardedModule].moduleTimer
//            == listOfModules[parent->guardedModule].moduleTimer
       )
    {
        boundMaster = parent->boundMaster;
    }
    else
    {
        boundMaster = this;
        InitBounds();
    }
}


moduleGuard::moduleGuard(TModule _guardedModule,const managedObject& X,
    const char* logEntry,TOption _options) throw() :
    guardedModule(_guardedModule),CT(X.Context()),
    OH(X.Handle()),options(_options)
{
    CT.OpenFold(guardedModule,NO_INDENT);
    CT.LogEntry(LOG_METH,OH,const_cast<char*>(logEntry));

    if (!(_options & NO_INDENT)) CT.IncreaseLogLevel();

    #if defined(_TIMERS_)

    CT.globalTimer[listOfModules[guardedModule].moduleTimer] -> Enable();

    #endif

    parent = CT.activeGuard;
    CT.activeGuard = this;

    #ifdef _PROGRESS_

    InitProgressCounter(1.0);

    #endif

    if (   (_options & SYNC_BOUNDS)
        && parent
//        && listOfModules[guardedModule].moduleTimer
//            == listOfModules[parent->guardedModule].moduleTimer
       )
    {
        boundMaster = parent->boundMaster;
    }
    else
    {
        boundMaster = this;
        InitBounds();
    }
}


moduleGuard::~moduleGuard() throw()
{
    Shutdown();
}


void moduleGuard::Shutdown(msgType msg,const char* logEntry) throw()
{
    if (guardedModule==NoModule) return;

    #if defined(_TIMERS_)

    goblinTimer* TM = CT.globalTimer[listOfModules[guardedModule].moduleTimer];

    if (TM->Disable() && CT.logTimers && TM->AccTime()>0.001)
    {
        sprintf(CT.logBuffer,"Timer report (%s)",
            listOfTimers[listOfModules[guardedModule].moduleTimer].timerName);
        CT.LogEntry(LOG_TIMERS,OH,CT.logBuffer);

        sprintf(CT.logBuffer,"  Cumulated times : %9.0f ms",TM->AccTime());
        CT.LogEntry(LOG_TIMERS,OH,CT.logBuffer);
        CT.IncreaseLogLevel();

        if (TM->PrevTime()+0.001<TM->AccTime())
        {
            sprintf(CT.logBuffer,"Previous round  : %9.0f ms",TM->PrevTime());
            CT.LogEntry(LOG_TIMERS,OH,CT.logBuffer);
            sprintf(CT.logBuffer,"Minimum         : %9.0f ms",TM->MinTime());
            CT.LogEntry(LOG_TIMERS,OH,CT.logBuffer);
            sprintf(CT.logBuffer,"Average         : %9.0f ms",TM->AvTime());
            CT.LogEntry(LOG_TIMERS,OH,CT.logBuffer);
            sprintf(CT.logBuffer,"Maximum         : %9.0f ms",TM->MaxTime());
            CT.LogEntry(LOG_TIMERS,OH,CT.logBuffer);
        }

        if (TM->FullInfo())
        {
            for (unsigned i=0;i<NoTimer;i++)
            {
                double thisTime = TM->ChildTime(TTimer(i));

                if (TTimer(i)!=listOfModules[guardedModule].moduleTimer
                    && thisTime>0.001)
                {
                    sprintf(CT.logBuffer,"%-15s : %9.0f ms (%4.1f %%)",
                        listOfTimers[i].timerName,
                        thisTime,thisTime/TM->PrevTime()*100);
                    CT.LogEntry(LOG_TIMERS,OH,CT.logBuffer);
                }
            }
        }

        CT.DecreaseLogLevel();
    }

    #endif

    CT.CloseFold(guardedModule,options);

    if (msg!=NO_MSG && logEntry)
    {
        CT.LogEntry(msg,OH,const_cast<char*>(logEntry));
    }

    guardedModule = NoModule;

    CT.activeGuard = parent;
}


#ifdef _PROGRESS_

void moduleGuard::InitProgressCounter(double _maxProgress,double _step) throw()
{
    progress = 0.0;
    maxProgress = _maxProgress;
    nextProgress = _step;
}


void moduleGuard::SetProgressMax(double _maxProgress) throw()
{
    if (_maxProgress>=progress && _maxProgress<maxProgress)
    {
        maxProgress = _maxProgress;
    }
}


void moduleGuard::SetProgressCounter(double _progress) throw()
{
    progress = _progress;
}


void moduleGuard::SetProgressNext(double step) throw()
{
    nextProgress = step;
}


void moduleGuard::ProgressStep() throw()
{
    progress += nextProgress;
}


void moduleGuard::ProgressStep(double step) throw()
{
    progress += step;
}

#endif


void moduleGuard::Trace(double step) throw()
{
    #ifdef _PROGRESS_

    ProgressStep(step);

    #endif

    CT.Trace(OH,static_cast<unsigned long>(step));
}


void moduleGuard::Trace(managedObject& X,double step) throw()
{
    #ifdef _PROGRESS_

    ProgressStep(step);

    #endif

    CT.Trace(X.Handle(),static_cast<unsigned long>(step));
}


void moduleGuard::SetLowerBound(double _lower)
    const throw(ERRejected)
{
    SetBounds(_lower,boundMaster->upperBound);
}


void moduleGuard::SetUpperBound(double _upper)
    const throw(ERRejected)
{
    SetBounds(boundMaster->lowerBound,_upper);
}


void moduleGuard::SetBounds(double _lower,double _upper)
    const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (_upper<boundMaster->lowerBound-CT.epsilon)
    {
        sprintf(CT.logBuffer,"Trying to override lower bound %g with %g",
            boundMaster->lowerBound,_upper);
        CT.Error(ERR_REJECTED,OH,"SetBounds",CT.logBuffer);
    }

    if (_lower>boundMaster->upperBound+CT.epsilon)
    {
        sprintf(CT.logBuffer,"Trying to override upper bound %g with %g",
            boundMaster->upperBound,_lower);
        CT.Error(ERR_REJECTED,OH,"SetBounds",CT.logBuffer);
    }

    #endif

    bool reducedGap = false;

    if (_upper<boundMaster->upperBound)
    {
        boundMaster->upperBound = _upper;
        reducedGap = true;
    }

    if (_lower>boundMaster->lowerBound)
    {
        boundMaster->lowerBound = _lower;
        reducedGap = true;
    }

    if (CT.logGaps && CT.logEventHandler && reducedGap)
    {
        sprintf(CT.logBuffer,"Gap (%s) changes to",
            listOfModules[boundMaster->guardedModule].moduleName);

        if (boundMaster->lowerBound>-InfFloat)
        {
            if (boundMaster->lowerBound<InfFloat)
            {
                sprintf(CT.logBuffer,"%s [%.3f",CT.logBuffer,boundMaster->lowerBound);
            }
            else
            {
                sprintf(CT.logBuffer,"%s [infinity",CT.logBuffer);
            }
        }
        else
        {
            sprintf(CT.logBuffer,"%s [-infinity",CT.logBuffer);
        }

        if (boundMaster->upperBound>-InfFloat)
        {
            if (boundMaster->upperBound<InfFloat)
            {
                sprintf(CT.logBuffer,"%s,%.3f]",CT.logBuffer,boundMaster->upperBound);
            }
            else
            {
                sprintf(CT.logBuffer,"%s,infinity]",CT.logBuffer);
            }
        }
        else
        {
            sprintf(CT.logBuffer,"%s,-infinity]",CT.logBuffer);
        }

        CT.LogEntry(LOG_GAPS,OH,CT.logBuffer);
    }
}


void moduleGuard::InitBounds(double _lower,double _upper) const throw()
{
    boundMaster->lowerBound = _lower;
    boundMaster->upperBound = _upper;
}


double moduleGuard::LowerBound() const throw()
{
    return boundMaster->lowerBound;
}


double moduleGuard::UpperBound() const throw()
{
    return boundMaster->upperBound;
}
