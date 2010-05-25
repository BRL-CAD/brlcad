
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, April 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   configuration.h
/// \brief  Macro definitions for compile time configuration of the library
///
/// These macros can be unset when the library is used for specific solvers
/// rather than the graph browser application.

#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_


// (1) Alternative Sizes of Node/Arc Indices

//  #define _SMALL_ARCS_
//  #define _BIG_ARCS_
    #define _BIG_NODES_


// (2) Optional Writing of Logging Information

    #define _LOGGING_


// (3) Exhaustive Error Detection

    #define _FAILSAVE_


// (4) Optional Tracing Module

    #define _TRACING_


// (5) Optional Monitoring Module

    #define _HEAP_MON_


// (6) Optional Timers and Duality Gaps

    #define _TIMERS_


// (7) Support for progress bar and runtime estimation

    #define _PROGRESS_


// (8) Buffers used for logging / errors

    #define LOGBUFFERSIZE 256


#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, April 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   configuration.h
/// \brief  Macro definitions for compile time configuration of the library
///
/// These macros can be unset when the library is used for specific solvers
/// rather than the graph browser application.

#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_


// (1) Alternative Sizes of Node/Arc Indices

//  #define _SMALL_ARCS_
//  #define _BIG_ARCS_
    #define _BIG_NODES_


// (2) Optional Writing of Logging Information

    #define _LOGGING_


// (3) Exhaustive Error Detection

    #define _FAILSAVE_


// (4) Optional Tracing Module

    #define _TRACING_


// (5) Optional Monitoring Module

    #define _HEAP_MON_


// (6) Optional Timers and Duality Gaps

    #define _TIMERS_


// (7) Support for progress bar and runtime estimation

    #define _PROGRESS_


// (8) Buffers used for logging / errors

    #define LOGBUFFERSIZE 256


#endif
