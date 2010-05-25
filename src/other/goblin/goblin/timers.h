
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, June 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   timers.h
/// \brief  #goblinTimer class interface

#ifndef _TIMERS_H_
#define _TIMERS_H_

#include "globals.h"


#ifdef _TIMERS_

/// \addtogroup groupTimers
/// @{

/// \brief  Module performance measurement

class goblinTimer
{
private:

    double          clockTick;

    double          accTime;
    double          minTime;
    double          maxTime;
    double          prevTime;
    unsigned long   nRounds;

    unsigned long  nestingDepth; ///< Level of nested Enable() calls
    double          startTime;

    double*  savedTime; ///< Global running times at the start or stop of this timer
    goblinTimer**  globalTimer; ///< Pointer to the list of global timers

    TFloat      lowerBound;
    TFloat      upperBound;

public:

    goblinTimer(goblinTimer** = NULL) throw();
    ~goblinTimer() throw();

    /// \brief  Reset the accumulated running times
    void  Reset() throw(ERRejected);

    /// \brief  Start the time measurement if it is not already running
    ///
    /// \retval true  If the timer has not been already enabled
    bool  Enable() throw();

    /// \brief  Stop the time measurement, but only if the top nesting level has been reached
    ///
    /// \retval true  If the timer actually is enabled
    bool  Disable() throw();

    /// \brief  Retrieve the running times, accumulated since the most recent Reset()
    double  AccTime() const throw();

    /// \brief  Retrieve the average running time since the most recent Reset()
    double  AvTime() const throw();

    /// \brief  Retrieve the maximum running time since the most recent Reset()
    double  MaxTime() const throw();

    /// \brief  Retrieve the minimum running time since the most recent Reset()
    double  MinTime() const throw();

    /// \brief  Retrieve the running time of the most recent, completed measurement
    double  PrevTime() const throw();

    /// \brief  Retrieve the running time of a currently running measurement
    double  CurrTime() const throw();

    /// \brief  Retrieve the relative running time of a global timer
    ///
    /// If this timer is enabled, the accumulated times since the most recent
    /// Enable() of this solver are returned. Otherwise, the times in the most
    /// recent Enable() / Disable() interval of this timer are counted.
    double  ChildTime(TTimer tm) const throw();

    /// \brief  Check if the time measurement is currently running
    bool  Enabled() const throw() {return (nestingDepth>0);};

    /// \brief  Check if this timer can access a global timer list
    bool  FullInfo() const throw() {return (savedTime!=NULL);};

};

/// @}


typedef goblinTimer* pGoblinTimer;

#endif

#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, June 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   timers.h
/// \brief  #goblinTimer class interface

#ifndef _TIMERS_H_
#define _TIMERS_H_

#include "globals.h"


#ifdef _TIMERS_

/// \addtogroup groupTimers
/// @{

/// \brief  Module performance measurement

class goblinTimer
{
private:

    double          clockTick;

    double          accTime;
    double          minTime;
    double          maxTime;
    double          prevTime;
    unsigned long   nRounds;

    unsigned long  nestingDepth; ///< Level of nested Enable() calls
    double          startTime;

    double*  savedTime; ///< Global running times at the start or stop of this timer
    goblinTimer**  globalTimer; ///< Pointer to the list of global timers

    TFloat      lowerBound;
    TFloat      upperBound;

public:

    goblinTimer(goblinTimer** = NULL) throw();
    ~goblinTimer() throw();

    /// \brief  Reset the accumulated running times
    void  Reset() throw(ERRejected);

    /// \brief  Start the time measurement if it is not already running
    ///
    /// \retval true  If the timer has not been already enabled
    bool  Enable() throw();

    /// \brief  Stop the time measurement, but only if the top nesting level has been reached
    ///
    /// \retval true  If the timer actually is enabled
    bool  Disable() throw();

    /// \brief  Retrieve the running times, accumulated since the most recent Reset()
    double  AccTime() const throw();

    /// \brief  Retrieve the average running time since the most recent Reset()
    double  AvTime() const throw();

    /// \brief  Retrieve the maximum running time since the most recent Reset()
    double  MaxTime() const throw();

    /// \brief  Retrieve the minimum running time since the most recent Reset()
    double  MinTime() const throw();

    /// \brief  Retrieve the running time of the most recent, completed measurement
    double  PrevTime() const throw();

    /// \brief  Retrieve the running time of a currently running measurement
    double  CurrTime() const throw();

    /// \brief  Retrieve the relative running time of a global timer
    ///
    /// If this timer is enabled, the accumulated times since the most recent
    /// Enable() of this solver are returned. Otherwise, the times in the most
    /// recent Enable() / Disable() interval of this timer are counted.
    double  ChildTime(TTimer tm) const throw();

    /// \brief  Check if the time measurement is currently running
    bool  Enabled() const throw() {return (nestingDepth>0);};

    /// \brief  Check if this timer can access a global timer list
    bool  FullInfo() const throw() {return (savedTime!=NULL);};

};

/// @}


typedef goblinTimer* pGoblinTimer;

#endif

#endif
