
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, March 2008
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   moduleGuard.h
/// \brief  #moduleGuard class interface

#ifndef _MODULE_GUARD_H_
#define _MODULE_GUARD_H_

#include "managedObject.h"


/// \brief  Guards for all code instrumentation issues associated with a source code module
///
/// This initializes the code instrumentation for a specified source code module,
/// memorizes certain preconditions, and automatically starts and stops the associated
/// timer.
///
/// Guard objects are instanciated as local variables in the context of a
/// mathematical method. By that, the destructor code is executed, even if the
/// guard's context is left by an exception. This is used to reduce the overhead
/// of code instrumentation. In the basic use case, the mathematical method
/// only includes a constructor call and the \ref listOfModules[] an entry
/// to describe the mathematical method.
class moduleGuard
{
friend class goblinController;

public:

    /// \brief  Combinable constructor options
    enum TGuardOptions {
        NO_INDENT   = 1, ///< Do not increase the indentation level at guard construction / destruction
        SHOW_TITLE  = 2, ///< Log the title of the guarded source code module
        SYNC_BOUNDS = 4  ///< Synchronize optimization bounds with the parent guard object
    };

    /// \brief  Start a source code module guard
    ///
    /// \param _guardedModule  The supervised source code module
    /// \param X               An oject to referenced in the logging output
    /// \param _options        A bit combination of \ref TGuardOptions enum values
    ///
    /// If SHOW_TITLE is specified, the module name from \ref listOfModules
    /// is logged.
    moduleGuard(TModule _guardedModule,const managedObject& X,TOption _options=0) throw();

    /// \brief  Start a source code module guard
    ///
    /// \param _guardedModule  The supervised source code module
    /// \param X               An oject to referenced in the logging output
    /// \param logEntry        An initial log message
    /// \param _options        Either 0 or NO_INDENT
    moduleGuard(TModule _guardedModule,const managedObject& X,
                const char* logEntry,TOption _options=0) throw();

    /// \brief  Destroy a source code module guard
    ///
    /// This executes \ref Shutdown() if this method has not been called
    /// before explicitly.
    ~moduleGuard() throw();

    /// \brief  Stop a source code module guard, in occasion, with a log message
    ///
    /// \param msg       The applied logging category
    /// \param logEntry  A final log message of NULL
    ///
    /// This stops the associated timer and reports about the running times if
    /// it is not a nested application of this timer. It then cleans up the
    /// logging indentation level and posts a message, if specified.
    void Shutdown(msgType msg = NO_MSG,const char* logEntry = NULL) throw();

    /// \addtogroup groupOptimiztionBounds
    /// @{

    /// \brief  Initialize the optimization bounds
    ///
    /// \param _lower  An initial lower bound on the optimal objective value
    /// \param _upper  An initial upper bound on the optimal objective value
    void  InitBounds(double _lower=-InfFloat,double _upper=InfFloat) const throw();

    /// \brief  Tighten the optimization bounds
    ///
    /// \param _lower  An improved lower bound on the optimal objective value
    /// \param _upper  An improved upper bound on the optimal objective value
    ///
    /// Other than \ref InitBounds(), this method verifies that the specified
    /// values are not less restrictive and that _lower does not exceed _upper.
    ///
    /// If \ref goblinController::logGaps is set, this method logs the new duality gap.
    void  SetBounds(double _lower,double _upper) const throw(ERRejected);

    /// \brief  Improve the lower optimization bounds
    ///
    /// \param _lower  An improved lower bound on the optimal objective value
    ///
    /// This is only a short cut for \ref SetBounds().
    void  SetLowerBound(double _lower) const throw(ERRejected);

    /// \brief  Improve the upper optimization bounds
    ///
    /// \param _upper  An improved upper bound on the optimal objective value
    ///
    /// This is only a short cut for \ref SetBounds().
    void  SetUpperBound(double _upper) const throw(ERRejected);

    /// \brief  Retrieve the current lower optimization bound
    ///
    /// \return  The best-known lower bound on the optimal objective value
    double  LowerBound() const throw();

    /// \brief  Retrieve the current upper optimization bound
    ///
    /// \return  The best-known upper bound on the optimal objective value
    double  UpperBound() const throw();

    /// @}

    #ifdef _PROGRESS_

    /// \addtogroup groupProgressCounting
    /// @{

    /// \brief  Initialize the progress counting facility
    ///
    /// \param _maxProgress  The maximum progress value
    /// \param _step         The advance of progress in a single step
    ///
    /// This sets the specified values, and also resets the current progress
    /// value.
    void  InitProgressCounter(double _maxProgress,double _step=1.0) throw();

    /// \brief  Specify the total range of progress counting
    ///
    /// \param _maxProgress  The maximum progress value
    ///
    /// This sets the expected maximum progress value. This value can be
    /// lowered dynamically, provided that it does not fall short of the
    /// current progress value.
    void  SetProgressMax(double _maxProgress) throw();

    /// \brief  Specify an absolute progress value
    ///
    /// \param currentProgress  The current progress value
    ///
    /// This ought to be in the range [0,maxProgress], and progress values are
    /// intended to be monotonically increasing.
    void  SetProgressCounter(double currentProgress) throw();

    /// \brief  Forecast the progress feed after the next iteration
    ///
    /// \param step  The expected progress feed after the next iteration
    ///
    /// This value is maintained as long as it is not updated. It tells
    /// about the upcoming ProgressStep() operations. The forecasting is
    /// important only if there are nested module calls which also
    /// implement progress counting. Then those nested progress values
    /// are scaled to subdivide a progress feed of the parent module
    /// context.
    void  SetProgressNext(double step) throw();


    /// \brief  Perform the forecasted progress feed
    ///
    /// The value specified by the most recent \ref SetProgressNext() call is
    /// added on the current absolute progress value.
    void  ProgressStep() throw();


    /// \brief  Perform a progress feed
    ///
    /// \param step  A relative progress value
    ///
    /// The specified value is added on the current absolute progress value.
    /// It might differ from the forecasted value, but then discontinuity arises.
    void  ProgressStep(double step) throw();

    /// @}

    #endif


    /// \addtogroup groupTraceObjects
    /// @{

    /// \brief  Set a trace point and perform a progress feed
    ///
    /// \param step  A relative progress value
    void  Trace(double step = 0) throw();


    /// \brief  Set a trace point for a specified object and perform a progress feed
    ///
    /// \param X     A reference of the object to be traced
    /// \param step  A relative progress value
    void  Trace(managedObject& X,double step = 0) throw();

    /// @}

private:

    TModule  guardedModule; ///< The supervised source code module
    goblinController&   CT; ///< The associated controller object
    THandle  OH;            ///< The handle of the object to be referenced in logging output
    TOption  options;       ///< Either 0 or NO_INDENT
    moduleGuard* parent;    ///< The one-level-higher guard object

    #ifdef _PROGRESS_

    double progress;        ///< The currently achieved progress value
    double maxProgress;     ///< The maximum possible progress value
    double nextProgress;    ///< Forecasted progress step value

    #endif

    double lowerBound;      ///< The best known lower bound on optimization
    double upperBound;      ///< The best known upper bound on optimization
    moduleGuard* boundMaster; ///< The parent guard object to synchronize optimization bound with

};


#endif
