
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Birk Eisermann, April 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   globals.h
/// \brief  Global typedefs and constant declarations

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

// Standard C/C++ declarations

#include <climits>
#include <cfloat>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <exception>
#include <new>

using namespace std;

// GOBLIN Configuration

#include "configuration.h"

// Configuration dependent system headers

#if defined(_TIMERS_)

#include <sys/times.h>
#include <unistd.h>

#endif



// Basic Types

#ifdef _BIG_NODES_

typedef unsigned long   TNode;

#else

typedef unsigned short  TNode;

#endif


#ifdef _SMALL_ARCS_

typedef unsigned short  TArc;

#else

typedef unsigned long   TArc;

#endif


typedef float           TCap;
typedef double          TFloat;
typedef unsigned long   THandle;
typedef unsigned long   TIndex;
typedef unsigned long   TSize;
typedef unsigned char   TDim;
typedef unsigned short  TOption;
typedef unsigned long   TVar;
typedef unsigned long   TRestr;
typedef char*           TString;


// Limits and Undefined Symbols

extern const TNode     NoNode;
extern const TArc      NoArc;
extern const TCap      InfCap;
extern const TFloat    InfFloat;
extern const THandle   NoHandle;
extern const TIndex    NoIndex;
extern const TDim      NoCoord;
extern const TVar      NoVar;
extern const TRestr    NoRestr;
/*
template <typename TItem>  TItem UNDEFINED();

template <typename TItem> unsigned char  UNDEFINED() {return UCHAR_MAX;}
template <typename TItem> unsigned short UNDEFINED() {return USHRT_MAX;}
template <typename TItem> unsigned int   UNDEFINED() {return UINT_MAX;}
template <typename TItem> unsigned long  UNDEFINED() {return ULONG_MAX;}
*/

// Timer, module, author and bibliographic indices

enum TTimer {
    TimerSolve = 0,             TimerIO = 1,
    TimerUnionFind = 2,         TimerHash = 3,
    TimerPrioQ = 4,             TimerPricing = 5,
    TimerQTest = 6,             TimerPivoting = 7,
    TimerSPTree = 8,            TimerMinTree = 9,
    TimerComponents = 10,       TimerMaxFlow = 11,
    TimerMinCut = 12,           TimerMinCFlow = 13,
    TimerMatching = 14,         TimerBranch = 15,
    TimerLpSolve = 16,          TimerTsp = 17,
    TimerColour = 18,           TimerSteiner = 19,
    TimerStable = 20,           TimerMaxCut = 21,
    TimerTreePack = 22,         TimerTJoin = 23,
    TimerCycleCancel = 24,      TimerMaxBalFlow = 25,
    TimerPrimalDual = 26,       TimerPlanarity = 27,
    TimerDrawing = 28,          TimerMIP = 29,
    TimerBNS = 30,              TimerLocalSearch = 31,
    TimerHeuristics = 32,       TimerStrongConn = 33,
    NoTimer = 34
};

enum TModule {
    ModRoot = 0,                ModLpSolve = 1,
    ModColour = 2,              ModStable = 3,
    ModBranch = 4,              ModTSP = 5,
    ModSteiner = 6,             ModMaxCut = 7,
    ModMinTree = 8,             ModMaxFlow = 9,
    ModMinCFlow = 10,           ModComponents = 11,
    ModMinCut = 12,             ModSPTree = 13,
    ModTreePack = 14,           ModMatching = 15,
    ModTJoin = 16,              ModPostman = 17,
    ModLpPricing = 18,          ModLpQTest = 19,
    ModLpPivoting = 20,         ModCycleCancel = 21,
    ModMaxBalFlow = 22,         ModMinCBalFlow = 23,
    ModPlanarity = 24,          ModGLPK = 25,
    ModGraphLoad = 26,          ModEdmondsKarp = 27,
    ModDinic = 28,              ModPushRelabel = 29,
    ModPrim = 30,               ModPrim2 = 31,
    ModKruskal = 32,            ModEdmondsArb = 33,
    ModPlanarityDMP = 34,       ModDikjstra = 35,
    ModBellmanFord = 36,        ModFloydWarshall = 37,
    ModFIFOLabelCorrect = 38,   ModKarpMeanCycle = 39,
    ModKleinCanceling = 40,     ModShortPath2 = 41,
    ModMeanCycleCanceling = 42, ModNetworkSimplex = 43,
    ModCostScaling = 44,        ModBusackerGowen = 45,
    ModSubgradOptTSP = 46,      ModTreeApproxTSP = 47,
    ModChristofides = 48,       Mod2OptTSP = 49,
    ModNodeInsert = 50,         ModRandomTour = 51,
    ModFarthestInsert = 52,     ModMehlhorn = 53,
    ModStrongComponents = 54,   ModBiconnectivity = 55,
    ModSchnorr = 56,            ModHaoOrlin = 57,
    ModNodeIdentification = 58, ModHierholzer = 59,
    ModDAGSearch = 60,          ModPrimalDual = 61,
    ModAnstee = 62,             ModMicaliVazirani = 63,
    ModBalAugment = 64,         ModBalScaling = 65,
    ModBNSExact = 66,           ModBNSDepth = 67,
    ModBNSBreadth = 68,         ModEnhancedPD = 69,
    ModBNS = 70,                ModMaxCutTJoin = 71,
    ModMaxCutGRASP = 72,        ModLMCOrder = 73,
    ModConvexDrawing = 74,      ModSpringEmbedder = 75,
    ModGEM = 76,                ModDakin = 77,
    ModMCFCapScaling = 78,      ModKandinsky=79,
    Mod4Orthogonal = 80,        ModVisibilityRepr = 81,
    ModLayeredFDP = 82,         ModFeedbackArcSet = 83,
    ModLayering = 84,           ModSeriesParallel = 85,
    ModStaircase = 86,          ModPlanarityHoTa = 87,
    ModStrongConn = 88,         ModForceDirected = 89,
    ModChordality = 90,         ModOrthoFlowRed = 91,
    Mod4OrthoCompaction = 92,   NoModule = 93
};

enum TAuthor {
    AuthCFP = 0,        AuthBSchmidt = 1,   AuthMSchwank = 2,   AuthBEisermann = 3,
    AuthAMakhorin = 4,  NoAuthor = 5
};

enum TReference {
    RefJun97 = 0,       RefCPCS98 = 1,      RefGoMi84 = 2,      RefAMO93 = 3,
    RefGPR96 = 4,       RefEdKa72 = 5,      RefDin70 = 6,       RefGoTa88 = 7,
    RefPrim57 = 8,      RefKru56 = 9,       RefEdm67 = 10,      RefDMP64 = 11,
    RefGou88 = 12,      RefDij59 = 13,      RefBel58 = 14,      RefFlo62 = 15,
    RefKar78 = 16,      RefEdJo73 = 17,     RefBuGo61 = 18,     RefKle67 = 19,
    RefGoTa89 = 20,     RefGoTa90 = 21,     RefHeKa70 = 22,     RefChr76 = 23,
    RefCro58 = 24,      RefRei94 = 25,      RefPrSt02 = 26,     RefTar72 = 27,
    RefAHU83 = 28,      RefSch79 = 29,      RefHie1873 = 30,    RefPaRi90 = 31,
    RefHaOr94 = 32,     RefEd65a = 33,      RefFrJu99a = 34,    RefFrJu99b = 35,
    RefFrJu01a = 36,    RefFrJu01b = 37,    RefFrJu02 = 38,     RefMiVa80 = 39,
    RefKoSt95 = 40,     RefGoKa96 = 41,     RefAns87 = 42,      RefKaMu74 = 43,
    RefPaCo80 = 44,     RefAns85 = 45,      RefOrDo72 = 46,     RefFPRR02 = 47,
    RefKan96 = 48,      RefEad84 = 49,      RefFLM94 = 50,      RefSch03 = 51,
    RefBre79 = 52,      RefBiKa94 = 53,     RefBMT97 = 54,      RefKaWa01 = 55,
    RefRoTa86 = 56,     RefKan93 = 57,      RefVTL82 = 58,      RefFKK96 = 59,
    RefHoTa74 = 60,     RefWil84 = 61,      RefEis06 = 62,      RefHCPV00 = 63,
    RefDaKu87 = 64,     NoReference = 65
};


// Module Data Base
/// \cond DOCUMENT_DATABASE

struct TModuleStruct {
    char*           moduleName;
    TTimer          moduleTimer;
    TAuthor         implementor1;
    TAuthor         implementor2;
    char*           encodingDate;
    char*           revisionDate;
    TReference      originalReference;
    TReference      authorsReference;
    TReference      textBook;
};

extern const TModuleStruct listOfModules[];

struct TTimerStruct {
    char*           timerName;
    bool            fullFeatured;
};

extern const TTimerStruct listOfTimers[];

struct TAuthorStruct {
    char*           name;
    char*           affiliation;
    char*           e_mail;
};

extern const TAuthorStruct listOfAuthors[];

struct TReferenceStruct {
    char*           refKey;
    char*           authors;
    char*           title;
    char*           type;
    char*           collection;
    char*           editors;
    int             volume;
    char*           pages;
    char*           publisher;
    int             year;
};

extern const TReferenceStruct listOfReferences[];

/// \endcond


/// \addtogroup groupAttributes
/// @{

typedef unsigned short TPoolEnum;

enum TOptGraphTokens {
    TokGraphRepresentation = 0,
    TokGraphObjectives = 1,
    TokGraphGeometry = 2,
    TokGraphLayout = 3,
    TokGraphRegisters = 4,
    TokGraphConfigure = 5,
    TokGraphCommodities = 6,
    TokGraphBound = 7,
    TokGraphLength = 8,
    TokGraphEndSection = 9,
    NoTokGrasph = 10
};

enum TOptReprTokens {
    TokReprUCap = 0,
    TokReprLCap = 1,
    TokReprLength = 2,
    TokReprDemand = 3,
    TokReprOrientation = 4,
    TokReprFirst = 5,
    TokReprRight = 6,
    TokReprStartNode = 7,
    TokReprIncidences = 8,
    TokReprNNodes = 9,
    TokReprNArcs = 10,
    TokReprComm0 = 11,
    TokReprEndSection = 12,
    NoTokRepr = 13
};

enum TOptGeometryTokens {
    TokGeoDim = 0,
    TokGeoMetric = 1,
    TokGeoCoordinates = 2,
    TokGeoAxis0 = 3,
    TokGeoAxis1 = 4,
    TokGeoAxis2 = 5,
    TokGeoMinBound = 6,
    TokGeoMaxBound = 7,
    TokGeoEndSection = 8,
    NoTokGeo = 9
};

enum TOptLayoutTokens {
    TokLayoutModel = 0,
    TokLayoutStyle = 1,
    TokLayoutArcLabel = 2,
    TokLayoutThread = 3,
    TokLayoutInterpolate = 4,
    TokLayoutHiddenNode = 5,
    TokLayoutHiddenArc = 6,
    TokLayoutExteriorArc = 7,
    TokLayoutArcWidthMode = 8,
    TokLayoutArcWidthMin = 9,
    TokLayoutArcWidthMax = 10,
    TokLayoutArcStippleMode = 11,
    TokLayoutArcVisibilityMode = 12,
    TokLayoutArcShapeMode = 13,
    TokLayoutArcColourMode = 14,
    TokLayoutArrowDisplayMode = 15,
    TokLayoutArrowPosMode = 16,
    TokLayoutArrowSize = 17,
    TokLayoutNodeShapeMode = 18,
    TokLayoutNodeColourMode = 19,
    TokLayoutNodeSize = 20,
    TokLayoutNodeFontSize = 21,
    TokLayoutArcFontSize = 22,
    TokLayoutNodeFontType = 23,
    TokLayoutArcFontType = 24,
    TokLayoutGridDisplayMode = 25,
    TokLayoutFineSpacing = 26,
    TokLayoutBendSpacing = 27,
    TokLayoutNodeSpacing = 28,
    TokLayoutNodeLabelFormat = 29,
    TokLayoutArcLabelFormat = 30,
    TokLayoutEndSection = 31,
    NoTokLayout = 32
};

enum TOptRegTokens {
    TokRegLabel = 0,
    TokRegPredecessor = 1,
    TokRegSubgraph = 2,
    TokRegPotential = 3,
    TokRegNodeColour = 4,
    TokRegEdgeColour = 5,
    TokRegColour = 6,
    TokRegOriginalNode = 7,
    TokRegOriginalArc = 8,

    TokV_T,
    TokEdgeType,
    TokPreO,
    TokPostO,
    TokLow1,
    TokLow2,

    TokRegEndSection,
    NoTokRegister
};

/// Specifiers for the type of attribute elements
enum TBaseType {
    TYPE_NODE_INDEX  = 0,  ///< Attribute values are node indices of type TNode
    TYPE_ARC_INDEX   = 1,  ///< Attribute values are arc indices of type TArc
    TYPE_FLOAT_VALUE = 2,  ///< Attribute values are of type TFloat
    TYPE_CAP_VALUE   = 3,  ///< Attribute values are of type TCap
    TYPE_INDEX       = 4,  ///< Attribute values are of type TIndex
    TYPE_ORIENTATION = 5,  ///< 
    TYPE_INT         = 6,  ///< Attribute values are of type int
    TYPE_DOUBLE      = 7,  ///< Attribute values are of type double
    TYPE_BOOL        = 8,  ///< Attribute values are of type bool
    TYPE_CHAR        = 9,  ///< Attribute values are of type char
    TYPE_VAR_INDEX   = 10,  ///< Attribute values are of type TVar
    TYPE_RESTR_INDEX = 11, ///< Attribute values are of type TRestr
    TYPE_SPECIAL     = 12  ///<
};

/// Specifiers for the dimensional type of attributes
enum TArrayDim {
    DIM_GRAPH_NODES  = 0, ///< Attribute concerns graph nodes
    DIM_GRAPH_ARCS   = 1, ///< Attribute concerns graph arcs neglecting 
    DIM_ARCS_TWICE   = 2, ///< Attribute concerns graph arcs 
    DIM_LAYOUT_NODES = 3, ///< Attribute concerns layout points
    DIM_SINGLETON    = 4, ///< Attribute denotes a singleton
    DIM_PAIR         = 5, ///< Attribute denotes a pair
    DIM_STRING       = 6  ///< Attribute denotes a null terminated string
};

/// Specifiers for the goblinRootObject::SizeInfo() function
enum TSizeInfo {
    SIZE_ACTUAL   = 0,  ///< Return the actual size of potential attributes
    SIZE_RESERVED = 1   ///< Return the reserved size of potential attributes
};

/// \brief  Template function returning a pointer to the default value of a given base type
///
/// \param _type  A base type specifier
/// \return       A pointer to the default value for this base type
///
/// This is needed when generating
void* DefaultValueAsVoidPtr(TBaseType _type) throw();

/// \brief  Template function returning a default value of a given base type
///
/// \param _type  A base type specifier
/// \return       The default value for this base type
template <typename T> inline T DefaultValue(TBaseType _type) throw()
{
    return *((T*)DefaultValueAsVoidPtr(_type));
}

/// \brief  Tabular imformation required by attribute pools
struct TPoolTable {
    char*           tokenLabel;     ///< Clear text name of the attribute
    TBaseType       arrayType;      ///< The base type of attribute elements
    TArrayDim       arrayDim;       ///< The dimensional type of this attribute
    TPoolEnum       primaryIndex;   ///< If \ref tokenLabel is an alias, thsi is the primary index in the pool table
};

/// The virtual pool table representing the top-level layer in the hierarchical
/// file format. This is used by the graph import procedure to parse native graph descriptions
extern const TPoolTable listOfGraphPars[];

/// The pool table for the \ref abstractMixedGraph::RepresentationalData() attribute pool
extern const TPoolTable listOfReprPars[];

// extern const TPoolTable listOfObjectivePars[];

/// The pool table for the \ref abstractMixedGraph::Geometry() attribute pool
extern const TPoolTable listOfGeometryPars[];

/// The pool table for the \ref abstractMixedGraph::LayoutData() attribute pool
extern const TPoolTable listOfLayoutPars[];

/// The pool table for the \ref abstractMixedGraph::Registers() attribute pool
extern const TPoolTable listOfRegisters[];

/// @}


enum TLayoutModel {
    LAYOUT_DEFAULT             = -1,
    LAYOUT_FREESTYLE_POLYGONES =  0,
    LAYOUT_FREESTYLE_CURVES    =  1,
    LAYOUT_ORTHO_SMALL         =  2,
    LAYOUT_ORTHO_BIG           =  3,
    LAYOUT_VISIBILITY          =  4,
    LAYOUT_KANDINSKI           =  5,
    LAYOUT_STRAIGHT_2DIM       =  6,
    LAYOUT_LAYERED             =  7,
    LAYOUT_NONE                =  8
};


/// \brief  Possible modes for the computation of arc display ends
enum TPortMode {
    PORTS_IMPLICIT = 0, ///< Arc display ends are computed to match the displayed node size
    PORTS_EXPLICIT = 1  ///< Arc display ends are specified by explicit layout nodes
};


// Message Classes

/// \addtogroup groupLogging
/// @{

enum msgType {
    ERR_CHECK   =  0, ///< 
    ERR_PARSE   =  1, ///< File format violation
    ERR_FILE    =  2, ///< File access error
    ERR_RANGE   =  3, ///< Variable range violation
    ERR_REJECTED=  4, ///< Method does not apply to this object
    ERR_INTERNAL=  5, ///< Library internal errors
    MSG_APPEND  =  6, ///< Continue the previous line in log file
    MSG_WARN    =  7, ///< Neglectable error
    MSG_TRACE   =  8, ///< Text display of data objects (top-level)
    MSG_TRACE2  =  9, ///< Text display of data objects (details)
    MSG_ECHO    = 11, ///< Explicit logging by a shell command
    LOG_IO      = 12, ///< File I/O operations
    LOG_MAN     = 13, ///< Object manipulations
    LOG_MEM     = 14, ///< Object instanciations
    LOG_RES     = 16, ///< Computational results (top-level)
    LOG_RES2    = 17, ///< Computational results (details)
    LOG_METH    = 18, ///< Course of algorithms (top-level)
    LOG_METH2   = 19, ///< Course of algorithms (details)
    LOG_SHELL   = 22, ///< Output from the shell code
    LOG_TIMERS  = 23, ///< Timer output
    LOG_GAPS    = 24, ///< Improvements of the objective value and the dual bound
    NO_MSG      = 26  ///<
};

/// @}



// Exception Handling

/// Base class for all exceptions raised by the library
class ERGoblin                              {};

/// Base class for all file I/O related errors
class ERIO          : protected ERGoblin    {};

/// Exception raised in case of unavailable input files or output directories
class ERFile        : protected ERIO        {};

/// Exception raised in case of syntactically incorrect input files
class ERParse       : protected ERIO        {};

/// Exceptions representing internal errors
class ERInternal    : protected ERGoblin    {};

/// Exception raised in case of inapplicable methods
class ERRejected    : protected ERGoblin    {};

/// Exception raised in case of range violations
class ERRange       : protected ERRejected  {};

/// Exception raised in replace of a regular return code, not considered an error
class ERCheck       : protected ERRejected  {};


// Memory management

/// \addtogroup groupHeapMonitoring
/// @{

extern size_t goblinHeapSize;   ///< Current heap size
extern size_t goblinMaxSize;    ///< Maximal heap size in advance
extern size_t goblinNFragments; ///< Current number of memory fragments
extern size_t goblinNAllocs;    ///< Total number of allocations
extern size_t goblinNObjects;   ///< Current number of managed objects

/// \brief  Memory reallocation operation, conforming with the overloaded new/delete
void* GoblinRealloc(void* p,size_t size) throw (std::bad_alloc);

#ifdef _HEAP_MON_

void* operator new(size_t size) throw (std::bad_alloc);
void* operator new(size_t size,const std::nothrow_t&) throw ();
void* operator new[](size_t size) throw (std::bad_alloc);
void* operator new[](size_t size,const std::nothrow_t&) throw ();

void operator delete(void *p) throw ();
void operator delete(void *p,size_t) throw ();
void operator delete(void *p,const std::nothrow_t&) throw ();
void operator delete[](void *p) throw ();
void operator delete[](void *p,size_t) throw ();
void operator delete[](void *p,const std::nothrow_t&) throw ();

/// @}

#endif

THandle NewObjectHandle() throw();

const double PI = 3.141592653589793238512808959406186204433;


#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Birk Eisermann, April 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   globals.h
/// \brief  Global typedefs and constant declarations

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

// Standard C/C++ declarations

#include <climits>
#include <cfloat>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <exception>
#include <new>

using namespace std;

// GOBLIN Configuration

#include "configuration.h"

// Configuration dependent system headers

#if defined(_TIMERS_)

#include <sys/times.h>
#include <unistd.h>

#endif



// Basic Types

#ifdef _BIG_NODES_

typedef unsigned long   TNode;

#else

typedef unsigned short  TNode;

#endif


#ifdef _SMALL_ARCS_

typedef unsigned short  TArc;

#else

typedef unsigned long   TArc;

#endif


typedef float           TCap;
typedef double          TFloat;
typedef unsigned long   THandle;
typedef unsigned long   TIndex;
typedef unsigned long   TSize;
typedef unsigned char   TDim;
typedef unsigned short  TOption;
typedef unsigned long   TVar;
typedef unsigned long   TRestr;
typedef char*           TString;


// Limits and Undefined Symbols

extern const TNode     NoNode;
extern const TArc      NoArc;
extern const TCap      InfCap;
extern const TFloat    InfFloat;
extern const THandle   NoHandle;
extern const TIndex    NoIndex;
extern const TDim      NoCoord;
extern const TVar      NoVar;
extern const TRestr    NoRestr;
/*
template <typename TItem>  TItem UNDEFINED();

template <typename TItem> unsigned char  UNDEFINED() {return UCHAR_MAX;}
template <typename TItem> unsigned short UNDEFINED() {return USHRT_MAX;}
template <typename TItem> unsigned int   UNDEFINED() {return UINT_MAX;}
template <typename TItem> unsigned long  UNDEFINED() {return ULONG_MAX;}
*/

// Timer, module, author and bibliographic indices

enum TTimer {
    TimerSolve = 0,             TimerIO = 1,
    TimerUnionFind = 2,         TimerHash = 3,
    TimerPrioQ = 4,             TimerPricing = 5,
    TimerQTest = 6,             TimerPivoting = 7,
    TimerSPTree = 8,            TimerMinTree = 9,
    TimerComponents = 10,       TimerMaxFlow = 11,
    TimerMinCut = 12,           TimerMinCFlow = 13,
    TimerMatching = 14,         TimerBranch = 15,
    TimerLpSolve = 16,          TimerTsp = 17,
    TimerColour = 18,           TimerSteiner = 19,
    TimerStable = 20,           TimerMaxCut = 21,
    TimerTreePack = 22,         TimerTJoin = 23,
    TimerCycleCancel = 24,      TimerMaxBalFlow = 25,
    TimerPrimalDual = 26,       TimerPlanarity = 27,
    TimerDrawing = 28,          TimerMIP = 29,
    TimerBNS = 30,              TimerLocalSearch = 31,
    TimerHeuristics = 32,       TimerStrongConn = 33,
    NoTimer = 34
};

enum TModule {
    ModRoot = 0,                ModLpSolve = 1,
    ModColour = 2,              ModStable = 3,
    ModBranch = 4,              ModTSP = 5,
    ModSteiner = 6,             ModMaxCut = 7,
    ModMinTree = 8,             ModMaxFlow = 9,
    ModMinCFlow = 10,           ModComponents = 11,
    ModMinCut = 12,             ModSPTree = 13,
    ModTreePack = 14,           ModMatching = 15,
    ModTJoin = 16,              ModPostman = 17,
    ModLpPricing = 18,          ModLpQTest = 19,
    ModLpPivoting = 20,         ModCycleCancel = 21,
    ModMaxBalFlow = 22,         ModMinCBalFlow = 23,
    ModPlanarity = 24,          ModGLPK = 25,
    ModGraphLoad = 26,          ModEdmondsKarp = 27,
    ModDinic = 28,              ModPushRelabel = 29,
    ModPrim = 30,               ModPrim2 = 31,
    ModKruskal = 32,            ModEdmondsArb = 33,
    ModPlanarityDMP = 34,       ModDikjstra = 35,
    ModBellmanFord = 36,        ModFloydWarshall = 37,
    ModFIFOLabelCorrect = 38,   ModKarpMeanCycle = 39,
    ModKleinCanceling = 40,     ModShortPath2 = 41,
    ModMeanCycleCanceling = 42, ModNetworkSimplex = 43,
    ModCostScaling = 44,        ModBusackerGowen = 45,
    ModSubgradOptTSP = 46,      ModTreeApproxTSP = 47,
    ModChristofides = 48,       Mod2OptTSP = 49,
    ModNodeInsert = 50,         ModRandomTour = 51,
    ModFarthestInsert = 52,     ModMehlhorn = 53,
    ModStrongComponents = 54,   ModBiconnectivity = 55,
    ModSchnorr = 56,            ModHaoOrlin = 57,
    ModNodeIdentification = 58, ModHierholzer = 59,
    ModDAGSearch = 60,          ModPrimalDual = 61,
    ModAnstee = 62,             ModMicaliVazirani = 63,
    ModBalAugment = 64,         ModBalScaling = 65,
    ModBNSExact = 66,           ModBNSDepth = 67,
    ModBNSBreadth = 68,         ModEnhancedPD = 69,
    ModBNS = 70,                ModMaxCutTJoin = 71,
    ModMaxCutGRASP = 72,        ModLMCOrder = 73,
    ModConvexDrawing = 74,      ModSpringEmbedder = 75,
    ModGEM = 76,                ModDakin = 77,
    ModMCFCapScaling = 78,      ModKandinsky=79,
    Mod4Orthogonal = 80,        ModVisibilityRepr = 81,
    ModLayeredFDP = 82,         ModFeedbackArcSet = 83,
    ModLayering = 84,           ModSeriesParallel = 85,
    ModStaircase = 86,          ModPlanarityHoTa = 87,
    ModStrongConn = 88,         ModForceDirected = 89,
    ModChordality = 90,         ModOrthoFlowRed = 91,
    Mod4OrthoCompaction = 92,   NoModule = 93
};

enum TAuthor {
    AuthCFP = 0,        AuthBSchmidt = 1,   AuthMSchwank = 2,   AuthBEisermann = 3,
    AuthAMakhorin = 4,  NoAuthor = 5
};

enum TReference {
    RefJun97 = 0,       RefCPCS98 = 1,      RefGoMi84 = 2,      RefAMO93 = 3,
    RefGPR96 = 4,       RefEdKa72 = 5,      RefDin70 = 6,       RefGoTa88 = 7,
    RefPrim57 = 8,      RefKru56 = 9,       RefEdm67 = 10,      RefDMP64 = 11,
    RefGou88 = 12,      RefDij59 = 13,      RefBel58 = 14,      RefFlo62 = 15,
    RefKar78 = 16,      RefEdJo73 = 17,     RefBuGo61 = 18,     RefKle67 = 19,
    RefGoTa89 = 20,     RefGoTa90 = 21,     RefHeKa70 = 22,     RefChr76 = 23,
    RefCro58 = 24,      RefRei94 = 25,      RefPrSt02 = 26,     RefTar72 = 27,
    RefAHU83 = 28,      RefSch79 = 29,      RefHie1873 = 30,    RefPaRi90 = 31,
    RefHaOr94 = 32,     RefEd65a = 33,      RefFrJu99a = 34,    RefFrJu99b = 35,
    RefFrJu01a = 36,    RefFrJu01b = 37,    RefFrJu02 = 38,     RefMiVa80 = 39,
    RefKoSt95 = 40,     RefGoKa96 = 41,     RefAns87 = 42,      RefKaMu74 = 43,
    RefPaCo80 = 44,     RefAns85 = 45,      RefOrDo72 = 46,     RefFPRR02 = 47,
    RefKan96 = 48,      RefEad84 = 49,      RefFLM94 = 50,      RefSch03 = 51,
    RefBre79 = 52,      RefBiKa94 = 53,     RefBMT97 = 54,      RefKaWa01 = 55,
    RefRoTa86 = 56,     RefKan93 = 57,      RefVTL82 = 58,      RefFKK96 = 59,
    RefHoTa74 = 60,     RefWil84 = 61,      RefEis06 = 62,      RefHCPV00 = 63,
    RefDaKu87 = 64,     NoReference = 65
};


// Module Data Base
/// \cond DOCUMENT_DATABASE

struct TModuleStruct {
    char*           moduleName;
    TTimer          moduleTimer;
    TAuthor         implementor1;
    TAuthor         implementor2;
    char*           encodingDate;
    char*           revisionDate;
    TReference      originalReference;
    TReference      authorsReference;
    TReference      textBook;
};

extern const TModuleStruct listOfModules[];

struct TTimerStruct {
    char*           timerName;
    bool            fullFeatured;
};

extern const TTimerStruct listOfTimers[];

struct TAuthorStruct {
    char*           name;
    char*           affiliation;
    char*           e_mail;
};

extern const TAuthorStruct listOfAuthors[];

struct TReferenceStruct {
    char*           refKey;
    char*           authors;
    char*           title;
    char*           type;
    char*           collection;
    char*           editors;
    int             volume;
    char*           pages;
    char*           publisher;
    int             year;
};

extern const TReferenceStruct listOfReferences[];

/// \endcond


/// \addtogroup groupAttributes
/// @{

typedef unsigned short TPoolEnum;

enum TOptGraphTokens {
    TokGraphRepresentation = 0,
    TokGraphObjectives = 1,
    TokGraphGeometry = 2,
    TokGraphLayout = 3,
    TokGraphRegisters = 4,
    TokGraphConfigure = 5,
    TokGraphCommodities = 6,
    TokGraphBound = 7,
    TokGraphLength = 8,
    TokGraphEndSection = 9,
    NoTokGrasph = 10
};

enum TOptReprTokens {
    TokReprUCap = 0,
    TokReprLCap = 1,
    TokReprLength = 2,
    TokReprDemand = 3,
    TokReprOrientation = 4,
    TokReprFirst = 5,
    TokReprRight = 6,
    TokReprStartNode = 7,
    TokReprIncidences = 8,
    TokReprNNodes = 9,
    TokReprNArcs = 10,
    TokReprComm0 = 11,
    TokReprEndSection = 12,
    NoTokRepr = 13
};

enum TOptGeometryTokens {
    TokGeoDim = 0,
    TokGeoMetric = 1,
    TokGeoCoordinates = 2,
    TokGeoAxis0 = 3,
    TokGeoAxis1 = 4,
    TokGeoAxis2 = 5,
    TokGeoMinBound = 6,
    TokGeoMaxBound = 7,
    TokGeoEndSection = 8,
    NoTokGeo = 9
};

enum TOptLayoutTokens {
    TokLayoutModel = 0,
    TokLayoutStyle = 1,
    TokLayoutArcLabel = 2,
    TokLayoutThread = 3,
    TokLayoutInterpolate = 4,
    TokLayoutHiddenNode = 5,
    TokLayoutHiddenArc = 6,
    TokLayoutExteriorArc = 7,
    TokLayoutArcWidthMode = 8,
    TokLayoutArcWidthMin = 9,
    TokLayoutArcWidthMax = 10,
    TokLayoutArcStippleMode = 11,
    TokLayoutArcVisibilityMode = 12,
    TokLayoutArcShapeMode = 13,
    TokLayoutArcColourMode = 14,
    TokLayoutArrowDisplayMode = 15,
    TokLayoutArrowPosMode = 16,
    TokLayoutArrowSize = 17,
    TokLayoutNodeShapeMode = 18,
    TokLayoutNodeColourMode = 19,
    TokLayoutNodeSize = 20,
    TokLayoutNodeFontSize = 21,
    TokLayoutArcFontSize = 22,
    TokLayoutNodeFontType = 23,
    TokLayoutArcFontType = 24,
    TokLayoutGridDisplayMode = 25,
    TokLayoutFineSpacing = 26,
    TokLayoutBendSpacing = 27,
    TokLayoutNodeSpacing = 28,
    TokLayoutNodeLabelFormat = 29,
    TokLayoutArcLabelFormat = 30,
    TokLayoutEndSection = 31,
    NoTokLayout = 32
};

enum TOptRegTokens {
    TokRegLabel = 0,
    TokRegPredecessor = 1,
    TokRegSubgraph = 2,
    TokRegPotential = 3,
    TokRegNodeColour = 4,
    TokRegEdgeColour = 5,
    TokRegColour = 6,
    TokRegOriginalNode = 7,
    TokRegOriginalArc = 8,

    TokV_T,
    TokEdgeType,
    TokPreO,
    TokPostO,
    TokLow1,
    TokLow2,

    TokRegEndSection,
    NoTokRegister
};

/// Specifiers for the type of attribute elements
enum TBaseType {
    TYPE_NODE_INDEX  = 0,  ///< Attribute values are node indices of type TNode
    TYPE_ARC_INDEX   = 1,  ///< Attribute values are arc indices of type TArc
    TYPE_FLOAT_VALUE = 2,  ///< Attribute values are of type TFloat
    TYPE_CAP_VALUE   = 3,  ///< Attribute values are of type TCap
    TYPE_INDEX       = 4,  ///< Attribute values are of type TIndex
    TYPE_ORIENTATION = 5,  ///< 
    TYPE_INT         = 6,  ///< Attribute values are of type int
    TYPE_DOUBLE      = 7,  ///< Attribute values are of type double
    TYPE_BOOL        = 8,  ///< Attribute values are of type bool
    TYPE_CHAR        = 9,  ///< Attribute values are of type char
    TYPE_VAR_INDEX   = 10,  ///< Attribute values are of type TVar
    TYPE_RESTR_INDEX = 11, ///< Attribute values are of type TRestr
    TYPE_SPECIAL     = 12  ///<
};

/// Specifiers for the dimensional type of attributes
enum TArrayDim {
    DIM_GRAPH_NODES  = 0, ///< Attribute concerns graph nodes
    DIM_GRAPH_ARCS   = 1, ///< Attribute concerns graph arcs neglecting 
    DIM_ARCS_TWICE   = 2, ///< Attribute concerns graph arcs 
    DIM_LAYOUT_NODES = 3, ///< Attribute concerns layout points
    DIM_SINGLETON    = 4, ///< Attribute denotes a singleton
    DIM_PAIR         = 5, ///< Attribute denotes a pair
    DIM_STRING       = 6  ///< Attribute denotes a null terminated string
};

/// Specifiers for the goblinRootObject::SizeInfo() function
enum TSizeInfo {
    SIZE_ACTUAL   = 0,  ///< Return the actual size of potential attributes
    SIZE_RESERVED = 1   ///< Return the reserved size of potential attributes
};

/// \brief  Template function returning a pointer to the default value of a given base type
///
/// \param _type  A base type specifier
/// \return       A pointer to the default value for this base type
///
/// This is needed when generating
void* DefaultValueAsVoidPtr(TBaseType _type) throw();

/// \brief  Template function returning a default value of a given base type
///
/// \param _type  A base type specifier
/// \return       The default value for this base type
template <typename T> inline T DefaultValue(TBaseType _type) throw()
{
    return *((T*)DefaultValueAsVoidPtr(_type));
}

/// \brief  Tabular imformation required by attribute pools
struct TPoolTable {
    char*           tokenLabel;     ///< Clear text name of the attribute
    TBaseType       arrayType;      ///< The base type of attribute elements
    TArrayDim       arrayDim;       ///< The dimensional type of this attribute
    TPoolEnum       primaryIndex;   ///< If \ref tokenLabel is an alias, thsi is the primary index in the pool table
};

/// The virtual pool table representing the top-level layer in the hierarchical
/// file format. This is used by the graph import procedure to parse native graph descriptions
extern const TPoolTable listOfGraphPars[];

/// The pool table for the \ref abstractMixedGraph::RepresentationalData() attribute pool
extern const TPoolTable listOfReprPars[];

// extern const TPoolTable listOfObjectivePars[];

/// The pool table for the \ref abstractMixedGraph::Geometry() attribute pool
extern const TPoolTable listOfGeometryPars[];

/// The pool table for the \ref abstractMixedGraph::LayoutData() attribute pool
extern const TPoolTable listOfLayoutPars[];

/// The pool table for the \ref abstractMixedGraph::Registers() attribute pool
extern const TPoolTable listOfRegisters[];

/// @}


enum TLayoutModel {
    LAYOUT_DEFAULT             = -1,
    LAYOUT_FREESTYLE_POLYGONES =  0,
    LAYOUT_FREESTYLE_CURVES    =  1,
    LAYOUT_ORTHO_SMALL         =  2,
    LAYOUT_ORTHO_BIG           =  3,
    LAYOUT_VISIBILITY          =  4,
    LAYOUT_KANDINSKI           =  5,
    LAYOUT_STRAIGHT_2DIM       =  6,
    LAYOUT_LAYERED             =  7,
    LAYOUT_NONE                =  8
};


/// \brief  Possible modes for the computation of arc display ends
enum TPortMode {
    PORTS_IMPLICIT = 0, ///< Arc display ends are computed to match the displayed node size
    PORTS_EXPLICIT = 1  ///< Arc display ends are specified by explicit layout nodes
};


// Message Classes

/// \addtogroup groupLogging
/// @{

enum msgType {
    ERR_CHECK   =  0, ///< 
    ERR_PARSE   =  1, ///< File format violation
    ERR_FILE    =  2, ///< File access error
    ERR_RANGE   =  3, ///< Variable range violation
    ERR_REJECTED=  4, ///< Method does not apply to this object
    ERR_INTERNAL=  5, ///< Library internal errors
    MSG_APPEND  =  6, ///< Continue the previous line in log file
    MSG_WARN    =  7, ///< Neglectable error
    MSG_TRACE   =  8, ///< Text display of data objects (top-level)
    MSG_TRACE2  =  9, ///< Text display of data objects (details)
    MSG_ECHO    = 11, ///< Explicit logging by a shell command
    LOG_IO      = 12, ///< File I/O operations
    LOG_MAN     = 13, ///< Object manipulations
    LOG_MEM     = 14, ///< Object instanciations
    LOG_RES     = 16, ///< Computational results (top-level)
    LOG_RES2    = 17, ///< Computational results (details)
    LOG_METH    = 18, ///< Course of algorithms (top-level)
    LOG_METH2   = 19, ///< Course of algorithms (details)
    LOG_SHELL   = 22, ///< Output from the shell code
    LOG_TIMERS  = 23, ///< Timer output
    LOG_GAPS    = 24, ///< Improvements of the objective value and the dual bound
    NO_MSG      = 26  ///<
};

/// @}



// Exception Handling

/// Base class for all exceptions raised by the library
class ERGoblin                              {};

/// Base class for all file I/O related errors
class ERIO          : protected ERGoblin    {};

/// Exception raised in case of unavailable input files or output directories
class ERFile        : protected ERIO        {};

/// Exception raised in case of syntactically incorrect input files
class ERParse       : protected ERIO        {};

/// Exceptions representing internal errors
class ERInternal    : protected ERGoblin    {};

/// Exception raised in case of inapplicable methods
class ERRejected    : protected ERGoblin    {};

/// Exception raised in case of range violations
class ERRange       : protected ERRejected  {};

/// Exception raised in replace of a regular return code, not considered an error
class ERCheck       : protected ERRejected  {};


// Memory management

/// \addtogroup groupHeapMonitoring
/// @{

extern size_t goblinHeapSize;   ///< Current heap size
extern size_t goblinMaxSize;    ///< Maximal heap size in advance
extern size_t goblinNFragments; ///< Current number of memory fragments
extern size_t goblinNAllocs;    ///< Total number of allocations
extern size_t goblinNObjects;   ///< Current number of managed objects

/// \brief  Memory reallocation operation, conforming with the overloaded new/delete
void* GoblinRealloc(void* p,size_t size) throw (std::bad_alloc);

#ifdef _HEAP_MON_

void* operator new(size_t size) throw (std::bad_alloc);
void* operator new(size_t size,const std::nothrow_t&) throw ();
void* operator new[](size_t size) throw (std::bad_alloc);
void* operator new[](size_t size,const std::nothrow_t&) throw ();

void operator delete(void *p) throw ();
void operator delete(void *p,size_t) throw ();
void operator delete(void *p,const std::nothrow_t&) throw ();
void operator delete[](void *p) throw ();
void operator delete[](void *p,size_t) throw ();
void operator delete[](void *p,const std::nothrow_t&) throw ();

/// @}

#endif

THandle NewObjectHandle() throw();

const double PI = 3.141592653589793238512808959406186204433;


#endif
