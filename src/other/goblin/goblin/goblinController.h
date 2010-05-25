
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   goblinController.h
/// \brief  #goblinController and #goblinRootObject class interfaces

#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#include "globals.h"
#include "timers.h"


// Forward declarations
class goblinImport;
class goblinExport;
class attributePool;
class goblinController;
class managedObject;
class sparseDiGraph;
class sparseGraph;
class denseDiGraph;
class denseGraph;
class moduleGuard;


const unsigned long goblinRandMax   = RAND_MAX;
const int MAX_MODULE_NESTING = 100;

/// \brief  Base class for all managed objects

class goblinRootObject
{
friend class goblinController;

private:

    mutable goblinRootObject* prevObject;
    mutable goblinRootObject* nextObject;

protected:

    THandle OH;

public:

    goblinRootObject();
    virtual ~goblinRootObject();

    virtual char*  Display() const throw(ERFile,ERRejected) = 0;
    virtual const char*  Label() const throw() = 0;
    virtual unsigned long  Size() const throw() = 0;


    /// \addtogroup classifications
    /// @{

    /// \brief  Query if this is a graph like data object
    ///
    /// \retval true  This is a graph-like data object
    virtual bool  IsGraphObject() const;

    /// \brief  Query if this is an MIP like data object
    ///
    /// \retval true  This is a MIP or LP data object
    virtual bool  IsMixedILP() const;

    /// @}


    THandle  Handle() const throw();


    /// \addtogroup objectDimensions
    /// @{

public:

    /// \brief  Query a specified object dimension
    ///
    /// \param arrayDim  The entity type (e.g. graph edges)
    /// \param size      Asks either for the actual or for the reserved number of entities
    /// \return          The number of object entities
    ///
    /// This is used by attribute pools to determine the correct attribute capacities
    virtual size_t  SizeInfo(TArrayDim arrayDim,TSizeInfo size) const throw();

    /// @}


    /// \addtogroup groupObjectExport
    /// @{

public:

    /// \brief  Write a pseudo attribute to file
    ///
    /// \param F      An output file stream
    /// \param pool   The attribute pool
    /// \param token  An index in this pool
    ///
    /// This writes data to files which is logically associated with a certain
    /// attribute pool in terms of the file format, but which is not internally
    /// stored as an attribute.
    virtual void  WriteSpecial(goblinExport& F,const attributePool& pool,TPoolEnum token) const throw();

    /// @}


    /// \addtogroup groupObjectImport
    /// @{

public:

    virtual void  ReadSpecial(goblinImport&,attributePool&,TPoolEnum) throw(ERParse);

    /// @}


    // *************************************************************** //
    //                     Local Type Declarations                     //
    // *************************************************************** //

    enum TOwnership {
        OWNED_BY_SENDER = 1,
        OWNED_BY_RECEIVER = 0
    };

#ifdef _HEAP_MON_

    /// \addtogroup groupHeapMonitoring
    /// @{

    void* operator new(size_t size) throw (std::bad_alloc);
    void* operator new(size_t size,const std::nothrow_t&) throw ();
    void* operator new[](size_t size) throw (std::bad_alloc);
    void* operator new[](size_t size,const std::nothrow_t&) throw ();

    void operator delete(void *p) throw ();
    void operator delete(void *p,const std::nothrow_t&) throw ();
    void operator delete[](void *p) throw ();
    void operator delete[](void *p,const std::nothrow_t&) throw ();

    void* GoblinRealloc(void* p,size_t size) const throw (std::bad_alloc);

    /// @}

#endif

};


inline goblinRootObject::goblinRootObject() : OH(THandle(0))
{
    ++goblinNObjects;
}


inline THandle goblinRootObject::Handle() const throw()
{
    return OH;
}



/// \brief  A class for job / session management
///
/// With every data object, a controller object is assigned.
/// This controller object handles the following:
/// - Serves as a container for all related data objects
/// - Runtime configuration of optimization and display methods
/// - Runtime configuration and control of the tracing functionality
/// - Runtime configuration and control of the logging functionality
/// - Modul entry points for LP solvers

class goblinController : public goblinRootObject
{
friend class goblinExport;
friend class mipInstance;
friend class moduleGuard;

private:

    int         externalPrecision;
    int         externalFloatLength;
    TNode       maxNode;
    TArc        maxArc;
    THandle     maxHandle;
    TIndex      maxIndex;
    long int    maxInt;

    int     savedTraceLevel;

    int            breakPoint;
    unsigned long  traceCounter;

    bool        isDefault;

public:

    goblinController() throw();
    goblinController(const goblinController&,bool = false) throw();
    virtual ~goblinController() throw();

    char*   Display() const throw();
    void    DisplayAll() const throw();

    int     FindParam(int pc,char* pStr[],const char* token,int offset = 1) throw();
    int     FindParam(int pc,const char* pStr[],const char* token,int offset = 1) throw();
    void    Configure(int ParamCount,const char* ParamStr[]) throw();

    void    ReadConfiguration(const char*) throw(ERFile,ERParse);
    void    WriteConfiguration(const char*) throw(ERFile);

    char*   objectExtension;

    TNode   sourceNodeInFile;
    TNode   targetNodeInFile;
    TNode   rootNodeInFile;

    #if defined(_TIMERS_)

    pGoblinTimer* globalTimer;

    #endif


    // *************************************************************** //
    //           Module Management                                     //
    // *************************************************************** //

    int    MajorVersion() throw();
    int    MinorVersion() throw();
    char*  PatchLevel() throw();

    static char const*  pMipFactory;

    void                ReleaseLPModule() throw();

    // *************************************************************** //
    //           Memory Management                                     //
    // *************************************************************** //

    bool            checkMem;

    unsigned long   Size() const throw();


    // *************************************************************** //
    //           Object Management                                     //
    // *************************************************************** //

    static char*                controllerTable;
    static goblinRootObject*    firstController;

    char*               objectTable;
    goblinRootObject*   firstObject;
    goblinRootObject*   masterObject;

    THandle     LinkController() throw(ERRejected);
    void        RegisterController() throw(ERRejected);

    THandle     InsertObject(goblinRootObject*) throw(ERRejected);
    void        RegisterObject(goblinRootObject*,THandle) throw(ERRejected);
    void        DeleteObject(const goblinRootObject*) throw(ERRejected);
    bool        IsReferenced() throw()
                    {return (firstObject->nextObject != firstObject);};

    void        SetMaster(THandle) throw(ERRejected);
    THandle     Master() throw(ERCheck);
    const char* Label() const throw();
    goblinRootObject*   ObjectPointer(THandle) const throw(ERRejected);
    goblinRootObject*   Lookup(THandle) const throw(ERRejected);

    THandle (*newObjectHandler)();


    /// \addtogroup groupLogging
    /// @{

    int     logMeth;
    int     logMem;
    int     logMan;
    int     logIO;
    int     logRes;
    int     logWarn;
    int     logTimers;
    int     logGaps;

    int     logDepth;
    int     logLevel;

private:

    TModule nestedModule[MAX_MODULE_NESTING];
    int     moduleNestingLevel;

    unsigned long suppressCount;
    void (*savedLogEventHandler)(msgType,TModule,THandle,char*);

public:

    void IncreaseLogLevel() throw(ERRejected);
    void DecreaseLogLevel() throw(ERRejected);

    void OpenFold(TModule,TOption = 0) throw(ERRejected);
    void CloseFold(TModule,TOption = 0) throw(ERRejected);

    /// @}

#if defined(_PROGRESS_)

    /// \addtogroup groupProgressCounting
    /// @{

private:

    moduleGuard* activeGuard; ///< The most recently allocated guard object
    int progressHigh, progressLow;

public:

    double ProgressCounter() const throw();
    double EstimatedExecutionTime() const throw();

    /// @}

#endif

    /// \addtogroup groupLogging
    /// @{

public:

    void SuppressLogging() throw();
    void RestoreLogging() throw();

    ostream* logStream;
    void (*logEventHandler)(msgType,TModule,THandle,char*);

    void PlainLogEventHandler(msgType,TModule,THandle,char*) throw();
    void DefaultLogEventHandler(msgType,TModule,THandle,char*) throw();

    mutable char logBuffer[LOGBUFFERSIZE];
    mutable bool compoundLogEntry;

private:

    THandle LogFilter(msgType,THandle,const char*) const throw();

public:

    void    LogEntry(msgType,THandle,const char*) const throw();
    THandle LogStart(msgType,THandle,const char*) const throw();
    void    LogAppend(THandle,const char*) const throw();
    void    LogEnd(THandle,const char* = NULL) const throw();

    /// @}


    // *************************************************************** //
    //           Error Handling                                        //
    // *************************************************************** //

    void Error(msgType,THandle,const char*,const char*)
        throw(ERRange,ERRejected,ERCheck,ERFile,ERParse);

    char     savedErrorMethodName[LOGBUFFERSIZE];
    char     savedErrorDescription[LOGBUFFERSIZE];
    THandle  savedErrorOriginator;
    msgType  savedErrorMsgType;

    // *************************************************************** //
    //           Graph Browser                                         //
    // *************************************************************** //

    int    displayMode;
    double displayZoom;
    double pixelWidth;
    double pixelHeight;

    int    legenda;

    char*  wallpaper;


    // *************************************************************** //
    //           Tracing                                               //
    // *************************************************************** //

    int     traceLevel;
    int     commLevel;
    int     breakLevel;
    int     traceData;
    int     threshold;
    int     fileCounter;
    int     traceStep;

    void    Trace(THandle HH,unsigned long priority = 0) throw();
    void    ResetCounters() throw();

    /// \addtogroup groupTimers
    /// @{

    /// \brief  Reset the specified global timer
    ///
    /// \param timer  An index in the global timer list
    void  ResetTimer(TTimer timer) throw(ERRange);

    /// \brief  Reset all global timers
    void  ResetTimers() throw();

    /// @}

    void    IncreaseCounter()
        {if (fileCounter<10000) fileCounter++; else fileCounter = 0;};

    void (*traceEventHandler)(char*);


    // *************************************************************** //
    //           Selection of mathematical methods                     //
    // *************************************************************** //

    int  methFailSave;

    /// Default method for the shortest path solver according to #abstractMixedGraph::TMethSPX
    int methSPX;

    /// Default method for the spanning tree solver according to #abstractMixedGraph::TMethMST
    int methMST;

    /// Default method for the max-flow solver according to #abstractMixedGraph::TMethMXF
    int methMXF;

    /// Default method for the min-cut solver according to #abstractMixedGraph::TMethMCC
    int methMCC;

    /// Default method for the min-cost st-flow solver according to #abstractMixedGraph::TMethMCFST
    int methMCFST;

    /// Default method for the min-cost flow solver according to #abstractMixedGraph::TMethMCF
    int methMCF;
    int methNWPricing;
    int methMaxBalFlow;
    int methBNS;
    int methDSU;
    int methPQ;
    int methLocal;
    int methSolve;
    int methMinCBalFlow;
    int methPrimalDual;
    int methModLength;
    int methCandidates;
    int methColour;

    /// Default TSP heuristic method according to #abstractMixedGraph::THeurTSP
    int methHeurTSP;

    /// Default relaxation method for the TSP initial bounding procedure according to #abstractMixedGraph::TRelaxTSP
    int methRelaxTSP1;

    /// Default relaxation method for the TSP branch & bound procedure according to #abstractMixedGraph::TRelaxTSP
    int methRelaxTSP2;

    /// Default max-cut method according to #abstractMixedGraph::TMethMaxCut
    int methMaxCut;

    int methLP;
    int methLPPricing;
    int methLPQTest;
    int methLPStart;

    int maxBBIterations;
    int maxBBNodes;

    int methFDP;

    /// Default plane embedding method according to #abstractMixedGraph::TMethPlanarity
    int methPlanarity;

    /// Default orthogonal drawing method according to #abstractMixedGraph::TMethOrthogonal
    int methOrthogonal;

    /// Default orthogonal drawing post-optimization method according to #abstractMixedGraph::TMethOrthoRefine
    int methOrthoRefine;

    bool (*stopSignalHandler)();

    bool SolverRunning() const throw();


    // *************************************************************** //
    //           Random Generators                                     //
    // *************************************************************** //

    unsigned long   Rand(unsigned long) throw(ERRange);
    TFloat          UnsignedRand() throw();
    TFloat          SignedRand() throw();
    void            SetRandomBounds(long int,long int) throw(ERRejected);

    int     randMin;
    int     randMax;

    int     randUCap;
    int     randLCap;
    int     randLength;
    int     randGeometry;
    int     randParallels;


    // Precision and External Number Formats

    TFloat  epsilon;
    TFloat  tolerance;

    void     SetExternalPrecision(int,int) throw(ERRejected);
    void     SetBounds(TNode,TArc,THandle,TIndex,long int) throw(ERRejected);
    TNode    MaxNode() const throw() {return maxNode;};
    TArc     MaxArc() const throw() {return maxArc;};
    THandle  MaxHandle() const throw() {return maxHandle;};
    TIndex   MaxIndex() const throw() {return maxIndex;};
    long int MaxInt() const throw() {return maxInt;};

    int      ExternalIntLength(long) const throw();
    int      ExternalFloatLength(TFloat) const throw();

    template <typename T> size_t ExternalLength(TFloat value) const throw()
    {
        return ExternalFloatLength(value);
    }

    template <typename T> size_t ExternalLength(TCap value) const throw()
    {
        return ExternalFloatLength(value);
    }

    template <typename T> size_t ExternalLength(char* label) const throw()
    {
        if (label==NULL) return 2;
        else return strlen(label)+2;
    }

    template <typename T> size_t ExternalLength(T value) const throw()
    {
        // Intended for integer types only
        return ExternalIntLength(value);
    }


    /// \addtogroup groupObjectImport
    /// @{

    /// \brief Enum values for file format specifications
    enum TFileFormat {
        FMT_GOBLIN             = 0, ///< The native file format
        FMT_DIMACS_MIN         = 1, ///< Dimacs format for digraphs, in particular min-cost flow instances
        FMT_DIMACS_EDGE        = 2, ///< Dimacs format for undirected graphs
        FMT_DIMACS_GEOM        = 7, ///< Dimacs format for geometric graphs
        FMT_SQUARE_UCAP        = 3, ///< Edge length adjacency maxtrix format of complete digraphs
        FMT_SQUARE_LENGTH      = 4, ///< Edge capacity adjacency maxtrix format of complete digraphs
        FMT_TRIANGULAR_UCAP    = 5, ///< Edge length adjacency maxtrix format of complete undirected graphs
        FMT_TRIANGULAR_LENGTH  = 6  ///< Edge capacity adjacency maxtrix format of complete undirected graphs
    };

    /// \brief  Import from a file in a format specified by a string value
    ///
    /// \param filename    The input filename
    /// \param formatName  The expected file format
    /// \return            A pointer to the constructed object
    ///
    /// This only looks up the respecitive format enum and then calls ImportFromFile()
    managedObject*  ImportByFormatName(const char* filename,const char* formatName) throw(ERParse);

    /// \brief  Import from a file in a format specified by an enum value
    ///
    /// \param filename  The input filename
    /// \param format    The expected file format
    /// \return          A pointer to the constructed object
    managedObject*  ImportFromFile(const char* filename,TFileFormat format) throw(ERParse);

    /// \brief  Import from a file in a library native format
    ///
    /// \param filename  The input filename
    /// \return          A pointer to the constructed object
    ///
    /// This evaluates the object type as specified in the file, and
    /// calls the respective file constructor
    managedObject*  Import_Native(const char* filename) throw(ERParse);

    /// \brief  Import a DIMACS like specified digraph skeleton into a \ref sparseDiGraph object
    ///
    /// \param filename  The input filename
    /// \return          A pointer to the constructed object
    ///
    /// This scans the file for lines matching the formats
    /// - <code>p min [number of nodes] [number of arcs]</code>,
    /// - <code>n [node index] [node supply]</code> or
    /// - <code>a [start node index] [end node index] [lower bound] [upper bound] [arc length]</code>.
    ///
    /// The problem dimension line must appear once and before the other formats.
    /// All lines not matching these formats are ignored. So both separate comment
    /// lines and trailing comments are possible.
    ///
    /// Nodes indices are running 1,2,..,n. Node description lines are necessary
    /// only for supply/demand values different from zero. For every arc
    /// definition line a graph arc is generated, and the number of arcs
    /// specified in the dimension line is used only to avoid reallocations.
    /// Arc attributes can be ommitted, where the default arc length is 1.0,
    /// and the defaut upper and lower arc capacities are 1.0 and 0.0 respectively.
    ///
    /// Note that the format slightly differs from the format for undirected graphs:
    /// - Arc definition lines start with an "a"
    /// - The order of arc attributes is different
    /// - Node demands are 0.0 by default
    ///
    /// The accepted file format is an extension the Dimacs format specified for
    /// min-cost flow.
    sparseDiGraph*  Import_DimacsMin(const char* filename) throw(ERParse);

    /// \brief  Import a DIMACS like specified graph skeleton into a \ref sparseGraph object
    ///
    /// \param filename  The input filename
    /// \return          A pointer to the constructed object
    ///
    /// This scans the file for lines matching the formats
    /// - <code>p edge [num of nodes] [number of arcs]</code> or
    /// - <code>n [node index] [node demand]</code> or
    /// - <code>e [start node index] [end node index] [arc length] [upper bound] [lower bound]</code>.
    ///
    /// The problem dimension line must appear once and before the other formats.
    /// All lines not matching these formats are ignored. So both separate comment
    /// lines and trailing comments are possible.
    ///
    /// Nodes indices are running 1,2,..,n. Node description lines are necessary
    /// only for demand values different from 1.0. For every edge definition line
    /// a graph edge is generated, and the number of edges specified in the dimension
    /// line is used only to avoid reallocations. Edge attributes can be ommitted,
    /// where the default edge length is 1.0, and the defaut upper and lower edge
    /// capacities are 1.0 and 0.0 respectively.
    ///
    /// Note that the format slightly differs from the format for undirected graphs:
    /// - Edge definition lines start with an "e"
    /// - The order of edge attributes is different
    /// - Node demands are 1.0 by default
    ///
    /// The accepted file format is an extension the Dimacs format for undirected
    /// graphs, especially for matching problem instances.
    sparseGraph*  Import_DimacsEdge(const char* filename) throw(ERParse);

    /// \brief  Import a DIMACS geom file into a \ref denseGraph object
    ///
    /// \param filename  The input filename
    /// \return          A pointer to the constructed object
    ///
    /// This scans the file for lines matching the formats
    /// - <code>p geom [number of nodes] [geometry dimension]</code> or
    /// - <code>v [x-pos value] [y-pos value] [z-pos value]</code>.
    ///
    /// The number of lines of the second pattern must match the number specified
    /// in the problem dimension line. The dimension may be either 1,2 or 3.
    denseGraph*  Import_DimacsGeom(const char* filename) throw(ERParse);

    /// \brief  Import an square matrix into a \ref denseDiGraph object
    ///
    /// \param filename  The input filename
    /// \param format    Specifies the attribute (length/capacity bound) to which values are saved
    /// \return          A pointer to the constructed object
    ///
    /// This expects a square number of arc attribute values in the input file.
    /// The values must delimited by at least one white space, preceding the
    /// numbers. Trailing symbols (such as commata), and even stray strings
    /// (comments) are allowed. It is also not required that matrix rows and
    /// file lines match.
    denseDiGraph*  Import_SquareMatrix(const char* filename,TFileFormat format) throw(ERParse);

    /// \brief  Import a lower triangular matrix into a \ref denseGraph object
    ///
    /// \param filename  The input filename
    /// \param format    Specifies the attribute (length/capacity bound) to which values are saved
    /// \return          A pointer to the constructed object
    ///
    /// This expects an appropriate number of arc attribute values in the input
    /// file. The values must delimited by at least one white space, preceding
    /// the numbers. Trailing symbols (such as commata), and even stray strings
    /// (comments) are allowed. It is also not required that matrix rows and
    /// file lines match.
    denseGraph*  Import_TriangularMatrix(const char* filename,TFileFormat format) throw(ERParse);

    /// @}

};


extern goblinController goblinDefaultContext;


#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   goblinController.h
/// \brief  #goblinController and #goblinRootObject class interfaces

#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#include "globals.h"
#include "timers.h"


// Forward declarations
class goblinImport;
class goblinExport;
class attributePool;
class goblinController;
class managedObject;
class sparseDiGraph;
class sparseGraph;
class denseDiGraph;
class denseGraph;
class moduleGuard;


const unsigned long goblinRandMax   = RAND_MAX;
const int MAX_MODULE_NESTING = 100;

/// \brief  Base class for all managed objects

class goblinRootObject
{
friend class goblinController;

private:

    mutable goblinRootObject* prevObject;
    mutable goblinRootObject* nextObject;

protected:

    THandle OH;

public:

    goblinRootObject();
    virtual ~goblinRootObject();

    virtual char*  Display() const throw(ERFile,ERRejected) = 0;
    virtual const char*  Label() const throw() = 0;
    virtual unsigned long  Size() const throw() = 0;


    /// \addtogroup classifications
    /// @{

    /// \brief  Query if this is a graph like data object
    ///
    /// \retval true  This is a graph-like data object
    virtual bool  IsGraphObject() const;

    /// \brief  Query if this is an MIP like data object
    ///
    /// \retval true  This is a MIP or LP data object
    virtual bool  IsMixedILP() const;

    /// @}


    THandle  Handle() const throw();


    /// \addtogroup objectDimensions
    /// @{

public:

    /// \brief  Query a specified object dimension
    ///
    /// \param arrayDim  The entity type (e.g. graph edges)
    /// \param size      Asks either for the actual or for the reserved number of entities
    /// \return          The number of object entities
    ///
    /// This is used by attribute pools to determine the correct attribute capacities
    virtual size_t  SizeInfo(TArrayDim arrayDim,TSizeInfo size) const throw();

    /// @}


    /// \addtogroup groupObjectExport
    /// @{

public:

    /// \brief  Write a pseudo attribute to file
    ///
    /// \param F      An output file stream
    /// \param pool   The attribute pool
    /// \param token  An index in this pool
    ///
    /// This writes data to files which is logically associated with a certain
    /// attribute pool in terms of the file format, but which is not internally
    /// stored as an attribute.
    virtual void  WriteSpecial(goblinExport& F,const attributePool& pool,TPoolEnum token) const throw();

    /// @}


    /// \addtogroup groupObjectImport
    /// @{

public:

    virtual void  ReadSpecial(goblinImport&,attributePool&,TPoolEnum) throw(ERParse);

    /// @}


    // *************************************************************** //
    //                     Local Type Declarations                     //
    // *************************************************************** //

    enum TOwnership {
        OWNED_BY_SENDER = 1,
        OWNED_BY_RECEIVER = 0
    };

#ifdef _HEAP_MON_

    /// \addtogroup groupHeapMonitoring
    /// @{

    void* operator new(size_t size) throw (std::bad_alloc);
    void* operator new(size_t size,const std::nothrow_t&) throw ();
    void* operator new[](size_t size) throw (std::bad_alloc);
    void* operator new[](size_t size,const std::nothrow_t&) throw ();

    void operator delete(void *p) throw ();
    void operator delete(void *p,const std::nothrow_t&) throw ();
    void operator delete[](void *p) throw ();
    void operator delete[](void *p,const std::nothrow_t&) throw ();

    void* GoblinRealloc(void* p,size_t size) const throw (std::bad_alloc);

    /// @}

#endif

};


inline goblinRootObject::goblinRootObject() : OH(THandle(0))
{
    ++goblinNObjects;
}


inline THandle goblinRootObject::Handle() const throw()
{
    return OH;
}



/// \brief  A class for job / session management
///
/// With every data object, a controller object is assigned.
/// This controller object handles the following:
/// - Serves as a container for all related data objects
/// - Runtime configuration of optimization and display methods
/// - Runtime configuration and control of the tracing functionality
/// - Runtime configuration and control of the logging functionality
/// - Modul entry points for LP solvers

class goblinController : public goblinRootObject
{
friend class goblinExport;
friend class mipInstance;
friend class moduleGuard;

private:

    int         externalPrecision;
    int         externalFloatLength;
    TNode       maxNode;
    TArc        maxArc;
    THandle     maxHandle;
    TIndex      maxIndex;
    long int    maxInt;

    int     savedTraceLevel;

    int            breakPoint;
    unsigned long  traceCounter;

    bool        isDefault;

public:

    goblinController() throw();
    goblinController(const goblinController&,bool = false) throw();
    virtual ~goblinController() throw();

    char*   Display() const throw();
    void    DisplayAll() const throw();

    int     FindParam(int pc,char* pStr[],const char* token,int offset = 1) throw();
    int     FindParam(int pc,const char* pStr[],const char* token,int offset = 1) throw();
    void    Configure(int ParamCount,const char* ParamStr[]) throw();

    void    ReadConfiguration(const char*) throw(ERFile,ERParse);
    void    WriteConfiguration(const char*) throw(ERFile);

    char*   objectExtension;

    TNode   sourceNodeInFile;
    TNode   targetNodeInFile;
    TNode   rootNodeInFile;

    #if defined(_TIMERS_)

    pGoblinTimer* globalTimer;

    #endif


    // *************************************************************** //
    //           Module Management                                     //
    // *************************************************************** //

    int    MajorVersion() throw();
    int    MinorVersion() throw();
    char*  PatchLevel() throw();

    static char const*  pMipFactory;

    void                ReleaseLPModule() throw();

    // *************************************************************** //
    //           Memory Management                                     //
    // *************************************************************** //

    bool            checkMem;

    unsigned long   Size() const throw();


    // *************************************************************** //
    //           Object Management                                     //
    // *************************************************************** //

    static char*                controllerTable;
    static goblinRootObject*    firstController;

    char*               objectTable;
    goblinRootObject*   firstObject;
    goblinRootObject*   masterObject;

    THandle     LinkController() throw(ERRejected);
    void        RegisterController() throw(ERRejected);

    THandle     InsertObject(goblinRootObject*) throw(ERRejected);
    void        RegisterObject(goblinRootObject*,THandle) throw(ERRejected);
    void        DeleteObject(const goblinRootObject*) throw(ERRejected);
    bool        IsReferenced() throw()
                    {return (firstObject->nextObject != firstObject);};

    void        SetMaster(THandle) throw(ERRejected);
    THandle     Master() throw(ERCheck);
    const char* Label() const throw();
    goblinRootObject*   ObjectPointer(THandle) const throw(ERRejected);
    goblinRootObject*   Lookup(THandle) const throw(ERRejected);

    THandle (*newObjectHandler)();


    /// \addtogroup groupLogging
    /// @{

    int     logMeth;
    int     logMem;
    int     logMan;
    int     logIO;
    int     logRes;
    int     logWarn;
    int     logTimers;
    int     logGaps;

    int     logDepth;
    int     logLevel;

private:

    TModule nestedModule[MAX_MODULE_NESTING];
    int     moduleNestingLevel;

    unsigned long suppressCount;
    void (*savedLogEventHandler)(msgType,TModule,THandle,char*);

public:

    void IncreaseLogLevel() throw(ERRejected);
    void DecreaseLogLevel() throw(ERRejected);

    void OpenFold(TModule,TOption = 0) throw(ERRejected);
    void CloseFold(TModule,TOption = 0) throw(ERRejected);

    /// @}

#if defined(_PROGRESS_)

    /// \addtogroup groupProgressCounting
    /// @{

private:

    moduleGuard* activeGuard; ///< The most recently allocated guard object
    int progressHigh, progressLow;

public:

    double ProgressCounter() const throw();
    double EstimatedExecutionTime() const throw();

    /// @}

#endif

    /// \addtogroup groupLogging
    /// @{

public:

    void SuppressLogging() throw();
    void RestoreLogging() throw();

    ostream* logStream;
    void (*logEventHandler)(msgType,TModule,THandle,char*);

    void PlainLogEventHandler(msgType,TModule,THandle,char*) throw();
    void DefaultLogEventHandler(msgType,TModule,THandle,char*) throw();

    mutable char logBuffer[LOGBUFFERSIZE];
    mutable bool compoundLogEntry;

private:

    THandle LogFilter(msgType,THandle,const char*) const throw();

public:

    void    LogEntry(msgType,THandle,const char*) const throw();
    THandle LogStart(msgType,THandle,const char*) const throw();
    void    LogAppend(THandle,const char*) const throw();
    void    LogEnd(THandle,const char* = NULL) const throw();

    /// @}


    // *************************************************************** //
    //           Error Handling                                        //
    // *************************************************************** //

    void Error(msgType,THandle,const char*,const char*)
        throw(ERRange,ERRejected,ERCheck,ERFile,ERParse);

    char     savedErrorMethodName[LOGBUFFERSIZE];
    char     savedErrorDescription[LOGBUFFERSIZE];
    THandle  savedErrorOriginator;
    msgType  savedErrorMsgType;

    // *************************************************************** //
    //           Graph Browser                                         //
    // *************************************************************** //

    int    displayMode;
    double displayZoom;
    double pixelWidth;
    double pixelHeight;

    int    legenda;

    char*  wallpaper;


    // *************************************************************** //
    //           Tracing                                               //
    // *************************************************************** //

    int     traceLevel;
    int     commLevel;
    int     breakLevel;
    int     traceData;
    int     threshold;
    int     fileCounter;
    int     traceStep;

    void    Trace(THandle HH,unsigned long priority = 0) throw();
    void    ResetCounters() throw();

    /// \addtogroup groupTimers
    /// @{

    /// \brief  Reset the specified global timer
    ///
    /// \param timer  An index in the global timer list
    void  ResetTimer(TTimer timer) throw(ERRange);

    /// \brief  Reset all global timers
    void  ResetTimers() throw();

    /// @}

    void    IncreaseCounter()
        {if (fileCounter<10000) fileCounter++; else fileCounter = 0;};

    void (*traceEventHandler)(char*);


    // *************************************************************** //
    //           Selection of mathematical methods                     //
    // *************************************************************** //

    int  methFailSave;

    /// Default method for the shortest path solver according to #abstractMixedGraph::TMethSPX
    int methSPX;

    /// Default method for the spanning tree solver according to #abstractMixedGraph::TMethMST
    int methMST;

    /// Default method for the max-flow solver according to #abstractMixedGraph::TMethMXF
    int methMXF;

    /// Default method for the min-cut solver according to #abstractMixedGraph::TMethMCC
    int methMCC;

    /// Default method for the min-cost st-flow solver according to #abstractMixedGraph::TMethMCFST
    int methMCFST;

    /// Default method for the min-cost flow solver according to #abstractMixedGraph::TMethMCF
    int methMCF;
    int methNWPricing;
    int methMaxBalFlow;
    int methBNS;
    int methDSU;
    int methPQ;
    int methLocal;
    int methSolve;
    int methMinCBalFlow;
    int methPrimalDual;
    int methModLength;
    int methCandidates;
    int methColour;

    /// Default TSP heuristic method according to #abstractMixedGraph::THeurTSP
    int methHeurTSP;

    /// Default relaxation method for the TSP initial bounding procedure according to #abstractMixedGraph::TRelaxTSP
    int methRelaxTSP1;

    /// Default relaxation method for the TSP branch & bound procedure according to #abstractMixedGraph::TRelaxTSP
    int methRelaxTSP2;

    /// Default max-cut method according to #abstractMixedGraph::TMethMaxCut
    int methMaxCut;

    int methLP;
    int methLPPricing;
    int methLPQTest;
    int methLPStart;

    int maxBBIterations;
    int maxBBNodes;

    int methFDP;

    /// Default plane embedding method according to #abstractMixedGraph::TMethPlanarity
    int methPlanarity;

    /// Default orthogonal drawing method according to #abstractMixedGraph::TMethOrthogonal
    int methOrthogonal;

    /// Default orthogonal drawing post-optimization method according to #abstractMixedGraph::TMethOrthoRefine
    int methOrthoRefine;

    bool (*stopSignalHandler)();

    bool SolverRunning() const throw();


    // *************************************************************** //
    //           Random Generators                                     //
    // *************************************************************** //

    unsigned long   Rand(unsigned long) throw(ERRange);
    TFloat          UnsignedRand() throw();
    TFloat          SignedRand() throw();
    void            SetRandomBounds(long int,long int) throw(ERRejected);

    int     randMin;
    int     randMax;

    int     randUCap;
    int     randLCap;
    int     randLength;
    int     randGeometry;
    int     randParallels;


    // Precision and External Number Formats

    TFloat  epsilon;
    TFloat  tolerance;

    void     SetExternalPrecision(int,int) throw(ERRejected);
    void     SetBounds(TNode,TArc,THandle,TIndex,long int) throw(ERRejected);
    TNode    MaxNode() const throw() {return maxNode;};
    TArc     MaxArc() const throw() {return maxArc;};
    THandle  MaxHandle() const throw() {return maxHandle;};
    TIndex   MaxIndex() const throw() {return maxIndex;};
    long int MaxInt() const throw() {return maxInt;};

    int      ExternalIntLength(long) const throw();
    int      ExternalFloatLength(TFloat) const throw();

    template <typename T> size_t ExternalLength(TFloat value) const throw()
    {
        return ExternalFloatLength(value);
    }

    template <typename T> size_t ExternalLength(TCap value) const throw()
    {
        return ExternalFloatLength(value);
    }

    template <typename T> size_t ExternalLength(char* label) const throw()
    {
        if (label==NULL) return 2;
        else return strlen(label)+2;
    }

    template <typename T> size_t ExternalLength(T value) const throw()
    {
        // Intended for integer types only
        return ExternalIntLength(value);
    }


    /// \addtogroup groupObjectImport
    /// @{

    /// \brief Enum values for file format specifications
    enum TFileFormat {
        FMT_GOBLIN             = 0, ///< The native file format
        FMT_DIMACS_MIN         = 1, ///< Dimacs format for digraphs, in particular min-cost flow instances
        FMT_DIMACS_EDGE        = 2, ///< Dimacs format for undirected graphs
        FMT_DIMACS_GEOM        = 7, ///< Dimacs format for geometric graphs
        FMT_SQUARE_UCAP        = 3, ///< Edge length adjacency maxtrix format of complete digraphs
        FMT_SQUARE_LENGTH      = 4, ///< Edge capacity adjacency maxtrix format of complete digraphs
        FMT_TRIANGULAR_UCAP    = 5, ///< Edge length adjacency maxtrix format of complete undirected graphs
        FMT_TRIANGULAR_LENGTH  = 6  ///< Edge capacity adjacency maxtrix format of complete undirected graphs
    };

    /// \brief  Import from a file in a format specified by a string value
    ///
    /// \param filename    The input filename
    /// \param formatName  The expected file format
    /// \return            A pointer to the constructed object
    ///
    /// This only looks up the respecitive format enum and then calls ImportFromFile()
    managedObject*  ImportByFormatName(const char* filename,const char* formatName) throw(ERParse);

    /// \brief  Import from a file in a format specified by an enum value
    ///
    /// \param filename  The input filename
    /// \param format    The expected file format
    /// \return          A pointer to the constructed object
    managedObject*  ImportFromFile(const char* filename,TFileFormat format) throw(ERParse);

    /// \brief  Import from a file in a library native format
    ///
    /// \param filename  The input filename
    /// \return          A pointer to the constructed object
    ///
    /// This evaluates the object type as specified in the file, and
    /// calls the respective file constructor
    managedObject*  Import_Native(const char* filename) throw(ERParse);

    /// \brief  Import a DIMACS like specified digraph skeleton into a \ref sparseDiGraph object
    ///
    /// \param filename  The input filename
    /// \return          A pointer to the constructed object
    ///
    /// This scans the file for lines matching the formats
    /// - <code>p min [number of nodes] [number of arcs]</code>,
    /// - <code>n [node index] [node supply]</code> or
    /// - <code>a [start node index] [end node index] [lower bound] [upper bound] [arc length]</code>.
    ///
    /// The problem dimension line must appear once and before the other formats.
    /// All lines not matching these formats are ignored. So both separate comment
    /// lines and trailing comments are possible.
    ///
    /// Nodes indices are running 1,2,..,n. Node description lines are necessary
    /// only for supply/demand values different from zero. For every arc
    /// definition line a graph arc is generated, and the number of arcs
    /// specified in the dimension line is used only to avoid reallocations.
    /// Arc attributes can be ommitted, where the default arc length is 1.0,
    /// and the defaut upper and lower arc capacities are 1.0 and 0.0 respectively.
    ///
    /// Note that the format slightly differs from the format for undirected graphs:
    /// - Arc definition lines start with an "a"
    /// - The order of arc attributes is different
    /// - Node demands are 0.0 by default
    ///
    /// The accepted file format is an extension the Dimacs format specified for
    /// min-cost flow.
    sparseDiGraph*  Import_DimacsMin(const char* filename) throw(ERParse);

    /// \brief  Import a DIMACS like specified graph skeleton into a \ref sparseGraph object
    ///
    /// \param filename  The input filename
    /// \return          A pointer to the constructed object
    ///
    /// This scans the file for lines matching the formats
    /// - <code>p edge [num of nodes] [number of arcs]</code> or
    /// - <code>n [node index] [node demand]</code> or
    /// - <code>e [start node index] [end node index] [arc length] [upper bound] [lower bound]</code>.
    ///
    /// The problem dimension line must appear once and before the other formats.
    /// All lines not matching these formats are ignored. So both separate comment
    /// lines and trailing comments are possible.
    ///
    /// Nodes indices are running 1,2,..,n. Node description lines are necessary
    /// only for demand values different from 1.0. For every edge definition line
    /// a graph edge is generated, and the number of edges specified in the dimension
    /// line is used only to avoid reallocations. Edge attributes can be ommitted,
    /// where the default edge length is 1.0, and the defaut upper and lower edge
    /// capacities are 1.0 and 0.0 respectively.
    ///
    /// Note that the format slightly differs from the format for undirected graphs:
    /// - Edge definition lines start with an "e"
    /// - The order of edge attributes is different
    /// - Node demands are 1.0 by default
    ///
    /// The accepted file format is an extension the Dimacs format for undirected
    /// graphs, especially for matching problem instances.
    sparseGraph*  Import_DimacsEdge(const char* filename) throw(ERParse);

    /// \brief  Import a DIMACS geom file into a \ref denseGraph object
    ///
    /// \param filename  The input filename
    /// \return          A pointer to the constructed object
    ///
    /// This scans the file for lines matching the formats
    /// - <code>p geom [number of nodes] [geometry dimension]</code> or
    /// - <code>v [x-pos value] [y-pos value] [z-pos value]</code>.
    ///
    /// The number of lines of the second pattern must match the number specified
    /// in the problem dimension line. The dimension may be either 1,2 or 3.
    denseGraph*  Import_DimacsGeom(const char* filename) throw(ERParse);

    /// \brief  Import an square matrix into a \ref denseDiGraph object
    ///
    /// \param filename  The input filename
    /// \param format    Specifies the attribute (length/capacity bound) to which values are saved
    /// \return          A pointer to the constructed object
    ///
    /// This expects a square number of arc attribute values in the input file.
    /// The values must delimited by at least one white space, preceding the
    /// numbers. Trailing symbols (such as commata), and even stray strings
    /// (comments) are allowed. It is also not required that matrix rows and
    /// file lines match.
    denseDiGraph*  Import_SquareMatrix(const char* filename,TFileFormat format) throw(ERParse);

    /// \brief  Import a lower triangular matrix into a \ref denseGraph object
    ///
    /// \param filename  The input filename
    /// \param format    Specifies the attribute (length/capacity bound) to which values are saved
    /// \return          A pointer to the constructed object
    ///
    /// This expects an appropriate number of arc attribute values in the input
    /// file. The values must delimited by at least one white space, preceding
    /// the numbers. Trailing symbols (such as commata), and even stray strings
    /// (comments) are allowed. It is also not required that matrix rows and
    /// file lines match.
    denseGraph*  Import_TriangularMatrix(const char* filename,TFileFormat format) throw(ERParse);

    /// @}

};


extern goblinController goblinDefaultContext;


#endif
