
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   goblinController.cpp
/// \brief  #goblinController and #goblinRootObject class implementations

#include <typeinfo>

#include "goblinController.h"
#include "version.h"
#include "hashTable.h"
#include "fileImport.h"
#include "fileExport.h"
#include "lpSolver.h"
#include "abstractMixedGraph.h"  // For several enum values
#include "canvasBuilder.h"  // For several enum values
#include "moduleGuard.h"


goblinRootObject::~goblinRootObject()
{
    --goblinNObjects;
}


bool goblinRootObject::IsGraphObject() const
{
    return false;
}


bool goblinRootObject::IsMixedILP() const
{
    return false;
}


size_t goblinRootObject::SizeInfo(TArrayDim arrayDim,TSizeInfo size) const throw()
{
    return 0;
}


void goblinRootObject::WriteSpecial(
    goblinExport& F,const attributePool& pool,TPoolEnum token) const throw()
{
}


void goblinRootObject::ReadSpecial(
    goblinImport&,attributePool&,TPoolEnum) throw(ERParse)
{
}


#ifdef _HEAP_MON_


void* goblinRootObject::operator new(size_t size) throw (std::bad_alloc)
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;

        *((size_t*)(p)) = size;

        return (size_t*)(p)+1;
    }

    throw std::bad_alloc();
}


void* goblinRootObject::operator new[](size_t size) throw (std::bad_alloc)
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;
        *((size_t*)(p)) = size;

        return (size_t*)(p)+1;
    }

    throw std::bad_alloc();
}


void* goblinRootObject::operator new(size_t size,const std::nothrow_t& type)
    throw ()
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;

        *((size_t*)(p)) = size;

        return (size_t*)(p)+1;
    }

    if (false) throw std::bad_alloc();

    return static_cast<void*>(NULL);
}


void* goblinRootObject::operator new[](size_t size,const std::nothrow_t& type)
    throw ()
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;
        *((size_t*)(p)) = size;

        return (size_t*)(p)+1;
    }

    if (false) throw std::bad_alloc();

    return static_cast<void*>(NULL);
}


void* goblinRootObject::GoblinRealloc(void* p,size_t size) const throw (std::bad_alloc)
{
    if (p==NULL)
    {
        if (size==0) return NULL;

        p = malloc(size+sizeof(size_t));
        if (p)
        {
            goblinHeapSize += size;

            if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

            goblinNFragments++;
            goblinNAllocs++;

            *((size_t*)(p)) = size;

            return (size_t*)(p)+1;
        }

        throw std::bad_alloc();
    }
    else
    {
        size_t old_size = *((size_t*)(p)-1);

        if (size==old_size) return p;

        goblinHeapSize += size-old_size;

        if (size==0)
        {
            goblinNFragments--;
            free((size_t*)(p)-1);

            return NULL;
        }

        size_t* q = (size_t*)malloc(size+sizeof(size_t));
        if (q)
        {
            if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

            goblinNFragments++;

            *q = size;

            if (size>old_size)
                memcpy(q+1,p,old_size);
            else memcpy(q+1,p,size);

            free((size_t*)(p)-1);

            return q+1;
        }

        throw std::bad_alloc();
    }

    throw std::bad_alloc();
}


void goblinRootObject::operator delete(void *p) throw ()
{
    if (p==NULL) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void  goblinRootObject::operator delete[](void *p) throw ()
{
    if (p==NULL) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void goblinRootObject::operator delete(void *p,const std::nothrow_t&)
    throw ()
{
    if (p==NULL) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void  goblinRootObject::operator delete[](void *p,const std::nothrow_t&)
    throw ()
{
    if (p==NULL) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


#endif


char const*         goblinController::pMipFactory = NULL;
char*               goblinController::controllerTable = NULL;
goblinRootObject*   goblinController::firstController = NULL;


goblinController::goblinController() throw()
{
    objectTable = NULL;
    firstObject = NULL;
    masterObject = NULL;

    objectExtension = "gob";

    newObjectHandler = &NewObjectHandle;

    sourceNodeInFile = NoNode;
    targetNodeInFile = NoNode;
    rootNodeInFile   = NoNode;


    // Global timer table

    #if defined(_TIMERS_)

    globalTimer = new pGoblinTimer[unsigned(NoTimer)];

    for (unsigned i=0;i<NoTimer;i++)
        if (listOfTimers[i].fullFeatured)
            globalTimer[i] = new goblinTimer();
        else globalTimer[i] = new goblinTimer(globalTimer);

    #endif


    // Logging Information

    logLevel = 0;
    moduleNestingLevel = 0;
    activeGuard = NULL;

    logMeth   = 1;
    logMem    = 0;
    logMan    = 1;
    logIO     = 0;
    logRes    = 1;
    logWarn   = 0;
    logTimers = 0;
    logGaps   = 0;
    logDepth  = 15;

    logStream = &clog;
    logEventHandler = NULL;
    savedLogEventHandler = NULL;

    savedErrorOriginator = NoHandle;
    savedErrorMsgType = NO_MSG;
    suppressCount = 0;


    // Graph Browser

    displayMode  = 2;
    displayZoom  = 1.0;
    pixelWidth   = 1.0;
    pixelHeight  = 1.0;

    wallpaper = new char[1];
    strcpy (wallpaper,"");


    // Tracing Module

    traceLevel       = 1;
    commLevel        = 2;
    traceData        = 0;
    threshold        = 1;
    traceStep        = 0;
    ResetCounters();

    traceEventHandler = NULL;


    // Method Selection

    methFailSave    = 0;
    checkMem        = 0;

    methSPX         = abstractMixedGraph::SPX_FIFO;
    methMST         = abstractMixedGraph::MST_PRIM2;
    methMXF         = abstractMixedGraph::MXF_DINIC;
    methMCC         = abstractMixedGraph::MCC_PREFLOW_HIGH;
    methMCFST       = abstractMixedGraph::MCF_ST_DIJKSTRA;
    methMCF         = abstractMixedGraph::MCF_BF_SIMPLEX;
    methNWPricing   = 0;
    methMaxBalFlow  = 2;
    methBNS         = 0;
    methDSU         = 1;
    methPQ          = 1;
    methLocal       = managedObject::LOCAL_OPTIMIZE;
    methSolve       = 2;
    methMinCBalFlow = 1;
    methPrimalDual  = 2;
    methModLength   = 0;
    methCandidates  = 5;
    methColour      = 0;
    methHeurTSP     = abstractMixedGraph::TSP_HEUR_CHRISTOFIDES;
    methRelaxTSP1   = abstractMixedGraph::TSP_RELAX_FAST;
    methRelaxTSP2   = abstractMixedGraph::TSP_RELAX_FAST;
    methMaxCut      = abstractMixedGraph::MAX_CUT_GRASP;

    methLP          = mipInstance::SIMPLEX_AUTO;
    methLPPricing   = mipInstance::FIRST_FIT;
    methLPQTest     = mipInstance::EXACT;
    methLPStart     = mipInstance::START_AUTO;

    maxBBIterations = -1;
    maxBBNodes      = 20;

    methFDP         = abstractMixedGraph::FDP_GEM;
    methPlanarity   = abstractMixedGraph::PLANAR_DMP;
    methOrthogonal  = abstractMixedGraph::ORTHO_EXPLICIT;
    methOrthoRefine = abstractMixedGraph::ORTHO_REFINE_SWEEP;

    stopSignalHandler = NULL;

    SetExternalPrecision(10,3);
    SetBounds(NoNode,NoArc,NoHandle,NoIndex,NoIndex);

    epsilon   = 0.0001;
    tolerance = 0.001;

    SetRandomBounds(0,99);
    randUCap      = 0;
    randLCap      = 0;
    randLength    = 1;
    randGeometry  = 1;
    randParallels = 0;

    isDefault = true;

    OH = LinkController();

    goblinHashTable<THandle,long unsigned>* useTable =
            new goblinHashTable<THandle,long unsigned>(
                NoHandle,1000,(long unsigned)NULL,*this);

    objectTable = reinterpret_cast<char*>(useTable);

    // Let objectTable contain itself ;)
    RegisterObject(useTable,useTable->Handle());

    if (!controllerTable)
    {
        goblinHashTable<THandle,long unsigned>* useTable2 =
                new goblinHashTable<THandle,long unsigned>(
                    NoHandle,50,(long unsigned)NULL,*this);

        controllerTable = reinterpret_cast<char*>(useTable2);

        // Let objectTable contain controllerTable ;)
        RegisterObject(useTable,useTable2->Handle());

        // Let controllerTable contain this controller (goblinDefaultContext)
        RegisterController();
    }


    // Module entry points
    if (!pMipFactory)
    {
        mipFactory* tmpPtr = new nativeLPFactory();
        pMipFactory = reinterpret_cast<char*>(tmpPtr);
    }

    if (logMem) LogEntry(LOG_MEM,NoHandle,"...Default context allocated.");
}


goblinController::goblinController(const goblinController &masterContext,
    bool hidden) throw()
{
    objectTable = NULL;
    masterObject = NULL;
    firstObject = NULL;

    objectExtension = "gob";

    newObjectHandler = masterContext.newObjectHandler;

    sourceNodeInFile = masterContext.sourceNodeInFile;
    targetNodeInFile = masterContext.targetNodeInFile;
    rootNodeInFile   = masterContext.rootNodeInFile;


    // Global timer table

    #if defined(_TIMERS_)

    globalTimer = new pGoblinTimer[unsigned(NoTimer)];

    for (unsigned i=0;i<NoTimer;i++)
        if (listOfTimers[i].fullFeatured)
            globalTimer[i] = new goblinTimer(globalTimer);
        else globalTimer[i] = new goblinTimer();

    #endif


    // Logging Information

    logLevel = 0;
    moduleNestingLevel = 0;
    activeGuard = NULL;

    logMeth   = masterContext.logMeth;
    logMem    = masterContext.logMem;
    logMan    = masterContext.logMan;
    logIO     = masterContext.logIO;
    logRes    = masterContext.logRes;
    logWarn   = masterContext.logWarn;
    logTimers = masterContext.logTimers;
    logGaps   = masterContext.logGaps;
    logDepth  = masterContext.logDepth;

    logStream     = masterContext.logStream;
    suppressCount = 0;

    if (hidden) logEventHandler = NULL;
    else logEventHandler = masterContext.logEventHandler;

    savedLogEventHandler = NULL;
    savedErrorOriginator = NoHandle;
    savedErrorMsgType = NO_MSG;


    // Graph Browser

    displayMode = masterContext.displayMode;
    displayZoom = masterContext.displayZoom;
    pixelWidth  = masterContext.pixelWidth;
    pixelHeight = masterContext.pixelHeight;
    legenda     = masterContext.legenda;
    wallpaper   = new char[strlen(masterContext.wallpaper)+1];
    strcpy(wallpaper,masterContext.wallpaper);


    // Tracing Module

    traceLevel       = masterContext.traceLevel;
    commLevel        = masterContext.commLevel;
    traceData        = masterContext.traceData;
    threshold        = masterContext.threshold;
    traceStep        = masterContext.traceStep;
    ResetCounters();

    traceEventHandler = masterContext.traceEventHandler;


    // Mathematical Methods

    methFailSave    = masterContext.methFailSave;
    checkMem        = masterContext.checkMem;

    methSPX         = masterContext.methSPX;
    methMST         = masterContext.methMST;
    methMXF         = masterContext.methMXF;
    methMCC         = masterContext.methMCC;
    methMCFST       = masterContext.methMCFST;
    methMCF         = masterContext.methMCF;
    methNWPricing   = masterContext.methNWPricing;
    methMaxBalFlow  = masterContext.methMaxBalFlow;
    methBNS         = masterContext.methBNS;
    methDSU         = masterContext.methDSU;
    methPQ          = masterContext.methPQ;
    methHeurTSP     = masterContext.methHeurTSP;
    methLocal       = masterContext.methLocal;
    methSolve       = masterContext.methSolve;
    methMinCBalFlow = masterContext.methMinCBalFlow;
    methPrimalDual  = masterContext.methPrimalDual;
    methModLength   = masterContext.methModLength;
    methCandidates  = masterContext.methCandidates;
    methColour      = masterContext.methColour;
    methRelaxTSP1   = masterContext.methRelaxTSP1;
    methRelaxTSP2   = masterContext.methRelaxTSP2;
    methMaxCut      = masterContext.methMaxCut;

    methLP          = masterContext.methLP;
    methLPPricing   = masterContext.methLPPricing;
    methLPQTest     = masterContext.methLPQTest;
    methLPStart     = masterContext.methLPStart;

    maxBBIterations = masterContext.maxBBIterations;
    maxBBNodes      = masterContext.maxBBNodes;

    methFDP         = masterContext.methFDP;
    methPlanarity   = masterContext.methPlanarity;
    methOrthogonal  = masterContext.methOrthogonal;
    methOrthoRefine = masterContext.methOrthoRefine;

    stopSignalHandler = NULL;

    SetExternalPrecision(
        masterContext.externalFloatLength,
        masterContext.externalPrecision);
    SetBounds(
        masterContext.maxNode,
        masterContext.maxArc,
        masterContext.maxHandle,
        masterContext.maxIndex,
        masterContext.maxInt);

    epsilon   = masterContext.epsilon;
    tolerance = masterContext.tolerance;

    SetRandomBounds(masterContext.randMin,masterContext.randMax);
    randUCap      = masterContext.randUCap;
    randLCap      = masterContext.randLCap;
    randLength    = masterContext.randLength;
    randGeometry  = masterContext.randGeometry;
    randParallels = masterContext.randParallels;

    isDefault = false;

    OH = LinkController();

    goblinHashTable<THandle,long unsigned>* useTable =
            new goblinHashTable<THandle,long unsigned>(
                NoHandle,1000,(long unsigned)NULL,*this);

    objectTable = reinterpret_cast<char*>(useTable);

    // Let objectTable contain itself ;)
    RegisterObject(useTable,useTable->Handle());

    // Let controllerTable contain this controller (goblinDefaultContext)
    RegisterController();

    LogEntry(LOG_MEM,NoHandle,"...Context allocated.");
}


int goblinController::MajorVersion() throw()
{
    return _GOBLIN_MAJOR_VERSION_;
}


int goblinController::MinorVersion() throw()
{
    return _GOBLIN_MINOR_VERSION_;
}


char* goblinController::PatchLevel() throw()
{
    return _GOBLIN_REVISION_;
}


int goblinController::FindParam(int pc,const char* pStr[],const char* token,
    int offset) throw()
{
    for (int i=offset;i<pc;i++)
        if (strncmp(pStr[i],token,20)==0) return i;

    return 0;
}


int goblinController::FindParam(int pc,char* pStr[],const char* token,
    int offset) throw()
{
    for (int i=offset;i<pc;i++)
        if (strncmp(pStr[i],token,20)==0) return i;

    return 0;
}


void goblinController::Configure(int ParamCount,const char* ParamStr[])
    throw()
{
    int pc = FindParam(ParamCount,ParamStr,"-silent");

    if (pc>0)
    {
        logMeth = 0;
        logMem = 0;
        logMan = 0;
        logIO = 0;
        logRes = 0;
        logWarn = 0;
    }

    pc = FindParam(ParamCount,ParamStr,"-report");

    if (pc>0)
    {
        logMeth = 1;
        logMem = 0;
        logMan = 1;
        logIO = 1;
        logRes = 1;
        logWarn = 0;
    }

    pc = FindParam(ParamCount,ParamStr,"-details");

    if (pc>0)
    {
        logMeth = 2;
        logMem = 0;
        logMan = 1;
        logIO = 1;
        logRes = 1;
        logWarn = 0;
    }

    pc = FindParam(ParamCount,ParamStr,"-debug");

    if (pc>0)
    {
        logMeth = 2;
        logMem = 1;
        logMan = 1;
        logIO = 1;
        logRes = 2;
        logWarn = 1;
    }

    pc = FindParam(ParamCount,ParamStr,"-sourceNode");
    if (pc>0 && pc<ParamCount-1) sourceNodeInFile = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-targetNode");
    if (pc>0 && pc<ParamCount-1) targetNodeInFile = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-rootNode");
    if (pc>0 && pc<ParamCount-1) rootNodeInFile = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logMeth");
    if (pc>0 && pc<ParamCount-1) logMeth = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logMem");
    if (pc>0 && pc<ParamCount-1) logMem = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logMan");
    if (pc>0 && pc<ParamCount-1) logMan = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logIO");
    if (pc>0 && pc<ParamCount-1) logIO = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logRes");
    if (pc>0 && pc<ParamCount-1) logRes = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logWarn");
    if (pc>0 && pc<ParamCount-1) logWarn = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logTimers");
    if (pc>0 && pc<ParamCount-1) logTimers = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logGaps");
    if (pc>0 && pc<ParamCount-1) logGaps = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logDepth");
    if (pc>0 && pc<ParamCount-1) logDepth = atoi(ParamStr[pc+1]);


    pc = FindParam(ParamCount,ParamStr,"-displayMode");
    if (pc>0 && pc<ParamCount-1) displayMode = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-displayZoom");
    if (pc>0 && pc<ParamCount-1) displayZoom = atof(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-pixelWidth");
    if (pc>0 && pc<ParamCount-1) pixelWidth = atof(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-pixelHeight");
    if (pc>0 && pc<ParamCount-1) pixelHeight = atof(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-legenda");
    if (pc>0 && pc<ParamCount-1) legenda = atoi(ParamStr[pc+1]);


    pc = FindParam(ParamCount,ParamStr,"-wallpaper");
    if (pc>0 && pc<ParamCount-1)
    {
        wallpaper =
            (char*)GoblinRealloc(wallpaper,strlen(ParamStr[pc+1])+1);
        strcpy(wallpaper,ParamStr[pc+1]);
    }


    pc = FindParam(ParamCount,ParamStr,"-traceLevel");
    if (pc>0 && pc<ParamCount-1) traceLevel = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-commLevel");
    if (pc>0 && pc<ParamCount-1) commLevel = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-traceData");
    if (pc>0 && pc<ParamCount-1) traceData = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-traceStep");
    if (pc>0 && pc<ParamCount-1) traceStep = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-threshold");
    if (pc>0 && pc<ParamCount-1) threshold = atoi(ParamStr[pc+1]);


    pc = FindParam(ParamCount,ParamStr,"-methFailSave");
    if (pc>0 && pc<ParamCount-1) methFailSave = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-checkMem");
    if (pc>0 && pc<ParamCount-1) checkMem = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-fs");
    if (pc>0) methFailSave = 1;


    pc = FindParam(ParamCount,ParamStr,"-methSPX");
    if (pc>0 && pc<ParamCount-1) methSPX = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMST");
    if (pc>0 && pc<ParamCount-1) methMST = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMXF");
    if (pc>0 && pc<ParamCount-1) methMXF = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMCC");
    if (pc>0 && pc<ParamCount-1) methMCC = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMCFST");
    if (pc>0 && pc<ParamCount-1) methMCFST = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMCF");
    if (pc>0 && pc<ParamCount-1) methMCF = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methNWPricing");
    if (pc>0 && pc<ParamCount-1) methNWPricing = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMaxBalFlow");
    if (pc>0 && pc<ParamCount-1) methMaxBalFlow = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methBNS");
    if (pc>0 && pc<ParamCount-1) methBNS = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methDSU");
    if (pc>0 && pc<ParamCount-1) methDSU = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methPQ");
    if (pc>0 && pc<ParamCount-1) methPQ = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methLocal");
    if (pc>0 && pc<ParamCount-1) methLocal = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methSolve");
    if (pc>0 && pc<ParamCount-1) methSolve = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMinCBalFlow");
    if (pc>0 && pc<ParamCount-1) methMinCBalFlow = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methPrimalDual");
    if (pc>0 && pc<ParamCount-1) methPrimalDual = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methModLength");
    if (pc>0 && pc<ParamCount-1) methModLength = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methCandidates");
    if (pc>0 && pc<ParamCount-1) methCandidates = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methHeurTSP");
    if (pc>0 && pc<ParamCount-1) methHeurTSP = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methRelaxTSP1");
    if (pc>0 && pc<ParamCount-1) methRelaxTSP1 = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methRelaxTSP2");
    if (pc>0 && pc<ParamCount-1) methRelaxTSP2 = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMaxCut");
    if (pc>0 && pc<ParamCount-1) methMaxCut = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methLP");
    if (pc>0 && pc<ParamCount-1) methLP = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methLPPricing");
    if (pc>0 && pc<ParamCount-1) methLPPricing = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methLPQTest");
    if (pc>0 && pc<ParamCount-1) methLPQTest = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methLPStart");
    if (pc>0 && pc<ParamCount-1) methLPStart = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-maxBBIterations");
    if (pc>0 && pc<ParamCount-1) maxBBIterations = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-maxBBNodes");
    if (pc>0 && pc<ParamCount-1) maxBBNodes = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methFDP");
    if (pc>0 && pc<ParamCount-1) methFDP = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methPlanarity");
    if (pc>0 && pc<ParamCount-1) methPlanarity = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methOrthogonal");
    if (pc>0 && pc<ParamCount-1) methOrthogonal = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methOrthoRefine");
    if (pc>0 && pc<ParamCount-1) methOrthoRefine = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-rand");
    if (pc>0 && pc<ParamCount-1) SetRandomBounds(atoi(ParamStr[pc+1]),atoi(ParamStr[pc+2]));

    pc = FindParam(ParamCount,ParamStr,"-randGeometry");
    if (pc>0 && pc<ParamCount-1) randGeometry = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-randParallels");
    if (pc>0 && pc<ParamCount-1) randParallels = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-randLength");
    if (pc>0 && pc<ParamCount-1) randLength = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-randUCap");
    if (pc>0 && pc<ParamCount-1) randUCap = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-randLCap");
    if (pc>0 && pc<ParamCount-1) randLCap = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-seed");
    if (pc>0 && pc<ParamCount-1) srand(atoi(ParamStr[pc+1]));

    pc = FindParam(ParamCount,ParamStr,"-epsilon");
    if (pc>0 && pc<ParamCount-1) epsilon = atol(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-tolerance");
    if (pc>0 && pc<ParamCount-1) tolerance = atol(ParamStr[pc+1]);
}


void goblinController::ReadConfiguration(const char* impFileName)
    throw(ERFile,ERParse)
{
    goblinImport F(impFileName,*this);
    F.Scan("configure");
    F.ReadConfiguration();
}


void goblinController::WriteConfiguration(const char* expFileName) throw(ERFile)
{
    goblinExport F(expFileName,*this);
    F.WriteConfiguration(*this);
}


char* goblinController::Display() const throw()
{
    sprintf(logBuffer,"Controller at 0x%.8lx (handle %ld)",
        reinterpret_cast<unsigned long>(this),OH);
    LogEntry(MSG_TRACE,NoHandle,logBuffer);

    goblinRootObject* pObject = firstObject;

    while (pObject!=NULL)
    {
        sprintf(logBuffer,"  %-.20s at 0x%.8lx (handle %ld)",
            typeid(*pObject).name(),
            reinterpret_cast<unsigned long>(pObject),pObject->Handle());
        LogEntry(MSG_TRACE2,NoHandle,logBuffer);

        pObject = pObject->nextObject;
    }

    return NULL;
}


void goblinController::DisplayAll() const throw()
{
    goblinRootObject* pController = firstController;

    while (pController)
    {
        goblinController* thisController = dynamic_cast<goblinController*>(pController);

        void (*tempLogEventHandler)(msgType,TModule,THandle,char*) =
            thisController->logEventHandler;
        thisController->logEventHandler = logEventHandler;

        pController -> Display();
        pController = pController->nextObject;

        thisController->logEventHandler = tempLogEventHandler;
    }
}


void goblinController::ReleaseLPModule() throw()
{
    if (pMipFactory) return;

    char* tmpPtr = const_cast<char*>(pMipFactory);
    delete reinterpret_cast<mipFactory*>(tmpPtr);
    pMipFactory = NULL;
}


goblinController::~goblinController() throw()
{
    if (!(firstController->nextObject))
    {
        // If no further controller remains, the LP module must be released
        ReleaseLPModule();

        // controllerTable is not referenced any longer
        delete reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(
            controllerTable);
    }
    else
    {
        // Deregister from controllerTable
        goblinHashTable<THandle,long unsigned>* useTable =
            reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(
                controllerTable);
        useTable -> ChangeKey(OH,(long unsigned)NULL);
    }

    #if defined(_FAILSAVE_)

    if (firstObject->nextObject)
    {
        Error(MSG_WARN,NoHandle,"goblinController",
            "There are undestroyed objects");
    }

    if (logLevel!=0)
        Error(MSG_WARN,NoHandle,"goblinController","Mismatch in logging module");

    #endif

    // Invalidate objectTable before deleting it
    char* tmpObjTable = objectTable;
    objectTable = NULL;
    delete reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(tmpObjTable);

    delete[] wallpaper;

    #if defined(_TIMERS_)

    for (unsigned i=0;i<NoTimer;i++) delete globalTimer[i];
    delete[] globalTimer;

    #endif

    // Unlink from controller list
    if (static_cast<goblinRootObject*>(this)==firstController)
    {
        firstController = nextObject;
        if (firstController) firstController->prevObject = NULL;
    }
    else
    {
        prevObject->nextObject = nextObject;
        if (nextObject) nextObject->prevObject = prevObject;
    }

    if (!isDefault && logMem)
        LogEntry(LOG_MEM,NoHandle,"...Context disallocated.");
}


unsigned long goblinController::Size() const throw()
{
    unsigned long tmpSize
        = sizeof(goblinController)
        + sizeof(char[LOGBUFFERSIZE])
        + strlen(wallpaper)+1;

    goblinRootObject* thisObject = firstObject;

    while (thisObject!=NULL)
    {
        tmpSize += thisObject->Size();
        thisObject = thisObject->nextObject;
    }

    return tmpSize;
}


THandle goblinController::LinkController()
    throw(ERRejected)
{
    // Receive a new handle
    THandle thisHandle = newObjectHandler();

    if (logMem)
    {
        sprintf(logBuffer,"Constructing controller with handle <%ld>...",
            thisHandle);
        LogEntry(LOG_MEM,NoHandle,logBuffer);
    }

    // Update linked list of member objects
    nextObject = firstController;
    prevObject = NULL;

    if (firstController) firstController->prevObject = this;

    firstController = this;

    if (controllerTable) RegisterController();

    return thisHandle;
}


void goblinController::RegisterController()
    throw(ERRejected)
{
    // Update the mapping from controller handles to raw pointers
    goblinHashTable<THandle,long unsigned>* useTable =
        reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(
            controllerTable);

    useTable -> ChangeKey(OH,reinterpret_cast<long unsigned>(
        static_cast<goblinRootObject*>(this)));
}


THandle goblinController::InsertObject(goblinRootObject* thisObject)
    throw(ERRejected)
{
    // Receive a new handle
    THandle thisHandle = newObjectHandler();

    if (logMem)
    {
        sprintf(logBuffer,"Constructing object with handle <%ld>...",
            thisHandle);
        LogEntry(LOG_MEM,NoHandle,logBuffer);
    }

    // Update linked list of member objects
    thisObject->nextObject = firstObject;
    thisObject->prevObject = NULL;

    if (firstObject) firstObject->prevObject = thisObject;

    firstObject = thisObject;

    if (objectTable) RegisterObject(thisObject,thisHandle);

    return thisHandle;
}


void goblinController::RegisterObject(goblinRootObject* thisObject,
    THandle thisHandle) throw(ERRejected)
{
    // Update the mapping from object handles to raw pointers
    goblinHashTable<THandle,long unsigned>* useTable =
        reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(objectTable);

    useTable -> ChangeKey(thisHandle,reinterpret_cast<long unsigned>(thisObject));
}


void goblinController::DeleteObject(const goblinRootObject* thisObject)
    throw(ERRejected)
{
    THandle thisHandle = thisObject->Handle();

    goblinHashTable<THandle,long unsigned>* useTable =
        reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(objectTable);

    #if defined(_FAILSAVE_)

    if (objectTable && useTable->Key(thisHandle)==(long unsigned)NULL)
        Error(ERR_REJECTED,NoHandle,"DeleteObject","Context mismatch");

    #endif

    // Delete object from objectTable
    if (objectTable)
        useTable -> ChangeKey(thisHandle,(long unsigned)NULL);

    // Update linked list of member objects
    if (thisObject==firstObject)
    {
        firstObject = thisObject->nextObject;
        if (firstObject) firstObject->prevObject = NULL;
    }
    else
    {
        thisObject->prevObject->nextObject = thisObject->nextObject;
        if (thisObject->nextObject)
            thisObject->nextObject->prevObject = thisObject->prevObject;
    }

    // Reset master object if necessary
    if (masterObject==thisObject) masterObject = NULL;

    if (logMem)
    {
        sprintf(logBuffer,"...Object with handle <%ld> deleted",thisHandle);
        LogEntry(LOG_MEM,NoHandle,logBuffer);
    }
}


goblinRootObject* goblinController::ObjectPointer(THandle thisHandle)
    const throw(ERRejected)
{
    goblinHashTable<THandle,long unsigned>* useTable =
        reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(objectTable);

    goblinRootObject* thisObject =
        reinterpret_cast<goblinRootObject*>(useTable->Key(thisHandle));

    return thisObject;
}


goblinRootObject* goblinController::Lookup(THandle thisHandle)
    const throw(ERRejected)
{
    goblinRootObject* ret = ObjectPointer(thisHandle);

    if (ret) return ret;

    goblinRootObject* pController = firstController;

    while (pController)
    {
        if (thisHandle==pController->Handle()) return pController;

        goblinController* thisController = dynamic_cast<goblinController*>(pController);

        if (thisController!=this)
        {
            ret = thisController->ObjectPointer(thisHandle);

            if (ret) return ret;
        }

        pController = pController->nextObject;
    }

    return NULL;
}


void goblinController::SetMaster(THandle HH) throw(ERRejected)
{
    goblinRootObject* thisObject = ObjectPointer(HH);

    #if defined(_FAILSAVE_)

    if (!thisObject)
        Error(ERR_REJECTED,NoHandle,"SetMaster","No such handle");

    #endif

    masterObject = thisObject;
}


THandle goblinController::Master() throw(ERCheck)
{
    if (!masterObject)
        Error(ERR_CHECK,NoHandle,"Master","No master object defined");

    return masterObject->Handle();
}


const char* goblinController::Label() const throw()
{
    if (!masterObject) return "unnamed";

    return masterObject->Label();
}


bool goblinController::SolverRunning() const throw()
{
    return (stopSignalHandler==NULL || stopSignalHandler());
}


unsigned long goblinController::Rand(unsigned long x) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (x>goblinRandMax)
        Error(ERR_RANGE,NoHandle,"Rand","Out of Range");

    #endif

    unsigned long thisRandMax = goblinRandMax-(goblinRandMax%x);

    unsigned long ret = goblinRandMax;
    while (ret>=thisRandMax)
        ret = (unsigned long)((rand()*(RAND_MAX+1)+rand())%x);
    return ret;
}


TFloat goblinController::UnsignedRand() throw()
{
    int thisMin = randMin;
    if (thisMin<0) thisMin = 0;

    return TFloat(Rand(randMax-thisMin+1))+TFloat(thisMin);
}


TFloat goblinController::SignedRand() throw()
{
    return TFloat(Rand(randMax-randMin+1))+TFloat(randMin);
}


void goblinController::SetRandomBounds(long int mMinRand,long int mMaxRand)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (mMinRand>mMaxRand || mMinRand<-maxInt || mMaxRand>maxInt)
        Error(ERR_REJECTED,NoHandle,"SetRandomBounds","Out of range");

    #endif

    randMin = mMinRand;
    randMax = mMaxRand;
}


void goblinController::SetExternalPrecision(int width,int prec)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (width<prec+3 || width>15)
        Error(ERR_REJECTED,NoHandle,"SetExternalPrecision","Out of range");

    #endif

    externalFloatLength = width;
    externalPrecision   = prec;
}


void goblinController::SetBounds(TNode cMaxNode,TArc cMaxArc,
    THandle cMaxHandle,TIndex cMaxIndex,long int cMaxInt) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (cMaxNode>cMaxArc || cMaxArc>cMaxIndex ||
        cMaxHandle>cMaxIndex || cMaxIndex>TIndex(cMaxInt))
        Error(ERR_REJECTED,NoHandle,"SetBounds","Out of range");

    if (cMaxNode>NoNode || cMaxArc>NoArc || cMaxHandle>NoHandle ||
        cMaxIndex>NoIndex)
        Error(ERR_REJECTED,NoHandle,"SetBounds","Out of range");

    TFloat precisePower = 1;

    for (char i=0;i<externalFloatLength;i++) precisePower *= 10;

    if (cMaxInt>precisePower)
        Error(ERR_REJECTED,NoHandle,"SetBounds","Out of range");

    #endif

    maxNode   = cMaxNode;
    maxArc    = cMaxArc;
    maxHandle = cMaxHandle;
    maxIndex  = cMaxIndex;
    maxInt    = cMaxInt;
}


static char lengthCheckBuffer[25];

int goblinController::ExternalIntLength(long val) const throw()
{
    sprintf(lengthCheckBuffer,"%ld",val);
    int l1 = int(strlen(lengthCheckBuffer));

    if (l1 < externalFloatLength)
        return l1;
    else return externalFloatLength;
}


int goblinController::ExternalFloatLength(TFloat val) const throw()
{
    sprintf(lengthCheckBuffer,"%.*f",externalPrecision,double(val));
    int l1 = int(strlen(lengthCheckBuffer));

    if (l1 < externalFloatLength)
        return l1;
    else return externalFloatLength;
}


void goblinController::IncreaseLogLevel() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (logLevel==INT_MAX)
        Error(ERR_REJECTED,NoHandle,"IncreaseLogLevel","Log depth overflow");

    #endif

    logLevel++;
}


void goblinController::DecreaseLogLevel() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (logLevel==0)
        Error(ERR_REJECTED,NoHandle,"DecreaseLogLevel",
            "Mismatch in logging module");

    #endif

    clog.flush();
    logLevel--;
}


void goblinController::OpenFold(TModule mod,TOption opt) throw(ERRejected)
{
    if ( !(opt & moduleGuard::NO_INDENT) ) IncreaseLogLevel();

    if (moduleNestingLevel<MAX_MODULE_NESTING)
        nestedModule[moduleNestingLevel] = mod;

// cout << "OPEN : " << moduleNestingLevel << " " << listOfModules[mod].moduleName << " " << endl;
    moduleNestingLevel++;

    if ( (opt & moduleGuard::SHOW_TITLE) &&
         (moduleNestingLevel==1 || mod!=nestedModule[moduleNestingLevel-2])
       )
    {
        sprintf(logBuffer,"<%s>",listOfModules[mod].moduleName);
        LogEntry(LOG_METH2,NoHandle,logBuffer);
    }
}


void goblinController::CloseFold(TModule mod,TOption opt) throw(ERRejected)
{
    if ( !(opt & moduleGuard::NO_INDENT) ) DecreaseLogLevel();

    moduleNestingLevel--;
// cout << "CLOSE : " << moduleNestingLevel << " " << listOfModules[mod].moduleName << " " << endl;

    #ifdef _PROGRESS_

    // Mark the progress counter "finished", not "initial"
    if (moduleNestingLevel==progressHigh)
    {
        progressHigh = -2;
    }
    else if (moduleNestingLevel==progressLow)
    {
        progressLow = -2;
    }

    #endif

    #if defined(_FAILSAVE_)

    if (moduleNestingLevel<MAX_MODULE_NESTING &&
          nestedModule[moduleNestingLevel] != mod)
        Error(ERR_REJECTED,NoHandle,"CloseFold",
            "Mismatching fold declarations");

    #endif
}


#ifdef _PROGRESS_

double goblinController::EstimatedExecutionTime() const throw()
{
    moduleGuard* iteratedGuard = activeGuard;

    if (!iteratedGuard) return 0.0;

    while (iteratedGuard->parent) iteratedGuard = iteratedGuard->parent;

    return (globalTimer[listOfModules[iteratedGuard->guardedModule].moduleTimer]->CurrTime())
        / ProgressCounter();
}


double goblinController::ProgressCounter() const throw()
{
    double iteratedProgress = 0.0;
    moduleGuard* iteratedGuard = activeGuard;

    while (iteratedGuard)
    {
        iteratedProgress =
            (iteratedGuard->progress + iteratedGuard->nextProgress*iteratedProgress) / iteratedGuard->maxProgress;

        if (iteratedProgress>1.0) iteratedProgress = 1.0;
/*
cout << " " << listOfModules[iteratedGuard->guardedModule].moduleName << ": "
    << iteratedGuard->progress << " " << iteratedGuard->nextProgress  << " " << iteratedGuard->maxProgress << endl;
*/
        iteratedGuard = iteratedGuard->parent;
    }
// cout << "Progress = " << iteratedProgress << endl;
    return iteratedProgress;
}

#endif


void goblinController::SuppressLogging() throw()
{
    if (suppressCount==0)
    {
        savedLogEventHandler = logEventHandler;
        savedTraceLevel = traceLevel;
        logEventHandler = NULL;
    }

    suppressCount++;
}


void goblinController::RestoreLogging() throw()
{
    suppressCount--;

    if (suppressCount==0)
    {
        traceLevel = savedTraceLevel;
        logEventHandler = savedLogEventHandler;
    }
}


void goblinController::PlainLogEventHandler(
    msgType msg,TModule mod,THandle OH,char* logEntry)
    throw()
{
    ostream &cgob = *logStream;

    if (msg != MSG_APPEND)
    {
        cgob << endl;

        if (logLevel<logDepth) cgob.width(2*logLevel);
        else cgob.width(2*logDepth);

        cgob.fill(' ');
        cgob << "";
    }

    cgob << logEntry << flush;
}


void goblinController::DefaultLogEventHandler(
    msgType msg,TModule mod,THandle OH,char* logEntry)
    throw()
{
    ostream &cgob = *logStream;

    if (msg != MSG_APPEND)
    {
        cgob << endl;
        cgob.width(3);
        cgob << msg << " ";
        cgob.width(3);
        cgob << mod << " ";
        cgob.width(8);

        if (OH!=NoHandle) cgob << OH << " ";
        else cgob << "-1" << " ";

        cgob.width(3);
        cgob << logLevel << " ";

        if (logLevel<logDepth) cgob.width(2*logLevel);
        else cgob.width(2*logDepth);

        cgob.fill(' ');
        cgob << "";
    }

    cgob << logEntry << flush;
}


THandle goblinController::LogFilter(msgType msg,THandle OH,const char* logEntry) const throw()
{
    void (*usedLogEventHandler)(msgType,TModule,THandle,char*)
        = logEventHandler;

    if (!logEventHandler)
    {
        if (!savedLogEventHandler) return NoHandle;

        // Process errors even if logging is suppressed

        switch (msg)
        {
            case ERR_INTERNAL:
            case ERR_RANGE:
            case ERR_REJECTED:
            case ERR_CHECK:
            case ERR_FILE:
            case ERR_PARSE:
            {
                usedLogEventHandler = savedLogEventHandler;
                break;
            }
            default:
            {
                return NoHandle;
            }
        }
    }

    if ((!logMem    && msg==LOG_MEM)     ||
        (!logIO     && msg==LOG_IO)      ||
        (!logMeth   && msg==LOG_METH)    ||
        (logMeth<2  && msg==LOG_METH2)   ||
        (!logMan    && msg==LOG_MAN)     ||
        (!logRes    && msg==LOG_RES)     ||
        (logRes<2   && msg==LOG_RES2)    ||
        (!logWarn   && msg==MSG_WARN)    ||
        (!logTimers && msg==LOG_TIMERS)  ||
        (!logGaps   && msg==LOG_GAPS)
       )
    {
        return NoHandle;
    }

    TModule mod = ModRoot;

    if (moduleNestingLevel>=MAX_MODULE_NESTING)
    {
        mod = nestedModule[MAX_MODULE_NESTING-1];
    }
    else if (moduleNestingLevel>0)
    {
        mod = nestedModule[moduleNestingLevel-1];
    }

    usedLogEventHandler(msg,mod,OH,const_cast<char*>(logEntry));

    return 0;
}


void goblinController::LogEntry(msgType msg,THandle OH,const char* logEntry) const throw()
{
    if (compoundLogEntry || msg==MSG_APPEND) return;

    LogFilter(msg,OH,logEntry);
}


THandle goblinController::LogStart(msgType msg,THandle OH,const char* logEntry) const throw()
{
    if (compoundLogEntry || msg==MSG_APPEND) return NoHandle;

    THandle ret = LogFilter(msg,OH,logEntry);

    if (ret!=NoHandle) compoundLogEntry = true;

    return ret;
}


void goblinController::LogAppend(THandle LH,const char* logEntry) const throw()
{
    if (LH==NoHandle) return;

    LogFilter(MSG_APPEND,LH,logEntry);
}


void goblinController::LogEnd(THandle LH,const char* logEntry) const throw()
{
    if (LH==NoHandle) return;

    if (logEntry) LogFilter(MSG_APPEND,LH,logEntry);

    compoundLogEntry = false;
}


void goblinController::Error(msgType msg,THandle OH,const char* methodName,
    const char* description) throw(ERRange,ERRejected,ERCheck,ERFile,ERParse)
{
    // Maintain information for GOSH shell

    strcpy(savedErrorMethodName,methodName);
    strcpy(savedErrorDescription,description);
    savedErrorOriginator = OH;
    savedErrorMsgType = msg;

    if (!logWarn && msg==MSG_WARN) return;

    compoundLogEntry = false;

    if (!logWarn && msg==ERR_CHECK) throw ERCheck();

    LogEntry(msg,OH,"::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");

    if (methodName)
    {
        THandle LH = LogStart(msg,OH,methodName);
        LogAppend(LH," - ");
        LogEnd(LH,description);
    }
    else LogEntry(msg,OH,description);

    if (OH!=NoHandle)
    {
        const char* thisLabel = ObjectPointer(OH)->Label();

        if (thisLabel)
        {
            sprintf(logBuffer,"Object \"%s\"",thisLabel);
        }
        else sprintf(logBuffer,"Object #%ld",(unsigned long)OH);
    }
    else sprintf(logBuffer,"Controller object");

    LogEntry(msg,OH,logBuffer);
    sprintf(logBuffer,", before trace point #%d",breakPoint);
    LogEntry(MSG_APPEND,OH,logBuffer);
    LogEntry(msg,OH,"::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");

    switch (msg)
    {
        case MSG_WARN:     return;
        case ERR_RANGE:    throw ERRange();
        case ERR_REJECTED: throw ERRejected();
        case ERR_CHECK:    throw ERCheck();
        case ERR_FILE:     throw ERFile();
        case ERR_PARSE:    throw ERParse();
        default:           throw ERInternal();
    }
}


void goblinController::Trace(THandle HH,unsigned long step) throw()
{
    if (traceLevel<=0) return;

    if (traceStep>0 && traceStep-traceCounter>step)
    {
        traceCounter += step;
        return;
    }

    traceCounter = 0;
    breakPoint ++;

    switch (traceLevel)
    {
        case 1:
        {
            cout << ".";
            cout.flush();
            break;
        }

        #if defined(_TRACING_)

        case 2:
        {
            break;
        }
        case 3:
        {
            if (breakPoint>threshold) ObjectPointer(HH)->Display();

            break;
        }
        case 4:
        {
            if (breakPoint<=threshold) return;

            cout << endl;

            char* tmpLabel = ObjectPointer(HH)->Display();

            if (tmpLabel!=NULL)
            {
                int objStrLen = strlen(tmpLabel)+10;
                char* objectStr = new char[objStrLen+1];
                sprintf(objectStr,"%s.%d",tmpLabel,fileCounter);

                char* commandStr = new char[objStrLen+10];

                if (displayMode==1)
                {
                    sprintf(commandStr,"xfig %s.fig",objectStr);
                    cout << "Starting Xfig..." << endl;
                    system (commandStr);
                }

                if (displayMode==2)
                {
                    sprintf(commandStr,"wish display %s",objectStr);
                    cout << "Starting browser..." << endl;
                    system (commandStr);
                }

                delete commandStr;
                delete objectStr;
            }
            else
            {
                cout << endl << "Click <Enter> to proceed";
                while (getchar()==EOF) {};
            }

            break;
        }

        #endif

        default:
        {
            sprintf(logBuffer,"Unsupported trace level: %d",traceLevel);
            Error(MSG_WARN,NoHandle,"Trace",logBuffer);
        }
    }
}


void goblinController::ResetCounters() throw()
{
    traceCounter       = 0;
    breakPoint         = 1;
    fileCounter        = 0;
    breakLevel         = INT_MAX;
    logLevel           = 0;
    moduleNestingLevel = 0;
    activeGuard        = NULL;
    compoundLogEntry   = false;

    if (suppressCount>0)
    {
        suppressCount = 1;
        RestoreLogging();
    }

    #if defined(_PROGRESS_)

    progressHigh = -1;
    progressLow  = -1;

    #endif
}


void goblinController::ResetTimer(TTimer timer) throw(ERRange)
{
    #if defined(_TIMERS_)

    if (timer>=NoTimer)
    {
        sprintf(logBuffer,"No such timer: %d",timer);
        Error(ERR_RANGE,NoHandle,"ResetTimer",logBuffer);
    }

    globalTimer[timer] -> Reset();

    #endif
}


void goblinController::ResetTimers() throw()
{
    #if defined(_TIMERS_)

    for (unsigned i=0;i<NoTimer;i++) globalTimer[i] -> Reset();

    #endif
}


goblinController goblinDefaultContext;

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   goblinController.cpp
/// \brief  #goblinController and #goblinRootObject class implementations

#include <typeinfo>

#include "goblinController.h"
#include "version.h"
#include "hashTable.h"
#include "fileImport.h"
#include "fileExport.h"
#include "lpSolver.h"
#include "abstractMixedGraph.h"  // For several enum values
#include "canvasBuilder.h"  // For several enum values
#include "moduleGuard.h"


goblinRootObject::~goblinRootObject()
{
    --goblinNObjects;
}


bool goblinRootObject::IsGraphObject() const
{
    return false;
}


bool goblinRootObject::IsMixedILP() const
{
    return false;
}


size_t goblinRootObject::SizeInfo(TArrayDim arrayDim,TSizeInfo size) const throw()
{
    return 0;
}


void goblinRootObject::WriteSpecial(
    goblinExport& F,const attributePool& pool,TPoolEnum token) const throw()
{
}


void goblinRootObject::ReadSpecial(
    goblinImport&,attributePool&,TPoolEnum) throw(ERParse)
{
}


#ifdef _HEAP_MON_


void* goblinRootObject::operator new(size_t size) throw (std::bad_alloc)
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;

        *((size_t*)(p)) = size;

        return (size_t*)(p)+1;
    }

    throw std::bad_alloc();
}


void* goblinRootObject::operator new[](size_t size) throw (std::bad_alloc)
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;
        *((size_t*)(p)) = size;

        return (size_t*)(p)+1;
    }

    throw std::bad_alloc();
}


void* goblinRootObject::operator new(size_t size,const std::nothrow_t& type)
    throw ()
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;

        *((size_t*)(p)) = size;

        return (size_t*)(p)+1;
    }

    if (false) throw std::bad_alloc();

    return static_cast<void*>(NULL);
}


void* goblinRootObject::operator new[](size_t size,const std::nothrow_t& type)
    throw ()
{
    if (size==0) return static_cast<void*>(NULL);

    if (void* p = malloc(size+sizeof(size_t)))
    {
        goblinHeapSize += size;

        if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

        goblinNFragments++;
        goblinNAllocs++;
        *((size_t*)(p)) = size;

        return (size_t*)(p)+1;
    }

    if (false) throw std::bad_alloc();

    return static_cast<void*>(NULL);
}


void* goblinRootObject::GoblinRealloc(void* p,size_t size) const throw (std::bad_alloc)
{
    if (p==NULL)
    {
        if (size==0) return NULL;

        p = malloc(size+sizeof(size_t));
        if (p)
        {
            goblinHeapSize += size;

            if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

            goblinNFragments++;
            goblinNAllocs++;

            *((size_t*)(p)) = size;

            return (size_t*)(p)+1;
        }

        throw std::bad_alloc();
    }
    else
    {
        size_t old_size = *((size_t*)(p)-1);

        if (size==old_size) return p;

        goblinHeapSize += size-old_size;

        if (size==0)
        {
            goblinNFragments--;
            free((size_t*)(p)-1);

            return NULL;
        }

        size_t* q = (size_t*)malloc(size+sizeof(size_t));
        if (q)
        {
            if (goblinHeapSize>goblinMaxSize) goblinMaxSize = goblinHeapSize;

            goblinNFragments++;

            *q = size;

            if (size>old_size)
                memcpy(q+1,p,old_size);
            else memcpy(q+1,p,size);

            free((size_t*)(p)-1);

            return q+1;
        }

        throw std::bad_alloc();
    }

    throw std::bad_alloc();
}


void goblinRootObject::operator delete(void *p) throw ()
{
    if (p==NULL) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void  goblinRootObject::operator delete[](void *p) throw ()
{
    if (p==NULL) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void goblinRootObject::operator delete(void *p,const std::nothrow_t&)
    throw ()
{
    if (p==NULL) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


void  goblinRootObject::operator delete[](void *p,const std::nothrow_t&)
    throw ()
{
    if (p==NULL) return;

    size_t size = *((size_t*)(p)-1);

    goblinHeapSize -= size;
    goblinNFragments--;

    free((size_t*)(p)-1);
}


#endif


char const*         goblinController::pMipFactory = NULL;
char*               goblinController::controllerTable = NULL;
goblinRootObject*   goblinController::firstController = NULL;


goblinController::goblinController() throw()
{
    objectTable = NULL;
    firstObject = NULL;
    masterObject = NULL;

    objectExtension = "gob";

    newObjectHandler = &NewObjectHandle;

    sourceNodeInFile = NoNode;
    targetNodeInFile = NoNode;
    rootNodeInFile   = NoNode;


    // Global timer table

    #if defined(_TIMERS_)

    globalTimer = new pGoblinTimer[unsigned(NoTimer)];

    for (unsigned i=0;i<NoTimer;i++)
        if (listOfTimers[i].fullFeatured)
            globalTimer[i] = new goblinTimer();
        else globalTimer[i] = new goblinTimer(globalTimer);

    #endif


    // Logging Information

    logLevel = 0;
    moduleNestingLevel = 0;
    activeGuard = NULL;

    logMeth   = 1;
    logMem    = 0;
    logMan    = 1;
    logIO     = 0;
    logRes    = 1;
    logWarn   = 0;
    logTimers = 0;
    logGaps   = 0;
    logDepth  = 15;

    logStream = &clog;
    logEventHandler = NULL;
    savedLogEventHandler = NULL;

    savedErrorOriginator = NoHandle;
    savedErrorMsgType = NO_MSG;
    suppressCount = 0;


    // Graph Browser

    displayMode  = 2;
    displayZoom  = 1.0;
    pixelWidth   = 1.0;
    pixelHeight  = 1.0;

    wallpaper = new char[1];
    strcpy (wallpaper,"");


    // Tracing Module

    traceLevel       = 1;
    commLevel        = 2;
    traceData        = 0;
    threshold        = 1;
    traceStep        = 0;
    ResetCounters();

    traceEventHandler = NULL;


    // Method Selection

    methFailSave    = 0;
    checkMem        = 0;

    methSPX         = abstractMixedGraph::SPX_FIFO;
    methMST         = abstractMixedGraph::MST_PRIM2;
    methMXF         = abstractMixedGraph::MXF_DINIC;
    methMCC         = abstractMixedGraph::MCC_PREFLOW_HIGH;
    methMCFST       = abstractMixedGraph::MCF_ST_DIJKSTRA;
    methMCF         = abstractMixedGraph::MCF_BF_SIMPLEX;
    methNWPricing   = 0;
    methMaxBalFlow  = 2;
    methBNS         = 0;
    methDSU         = 1;
    methPQ          = 1;
    methLocal       = managedObject::LOCAL_OPTIMIZE;
    methSolve       = 2;
    methMinCBalFlow = 1;
    methPrimalDual  = 2;
    methModLength   = 0;
    methCandidates  = 5;
    methColour      = 0;
    methHeurTSP     = abstractMixedGraph::TSP_HEUR_CHRISTOFIDES;
    methRelaxTSP1   = abstractMixedGraph::TSP_RELAX_FAST;
    methRelaxTSP2   = abstractMixedGraph::TSP_RELAX_FAST;
    methMaxCut      = abstractMixedGraph::MAX_CUT_GRASP;

    methLP          = mipInstance::SIMPLEX_AUTO;
    methLPPricing   = mipInstance::FIRST_FIT;
    methLPQTest     = mipInstance::EXACT;
    methLPStart     = mipInstance::START_AUTO;

    maxBBIterations = -1;
    maxBBNodes      = 20;

    methFDP         = abstractMixedGraph::FDP_GEM;
    methPlanarity   = abstractMixedGraph::PLANAR_DMP;
    methOrthogonal  = abstractMixedGraph::ORTHO_EXPLICIT;
    methOrthoRefine = abstractMixedGraph::ORTHO_REFINE_SWEEP;

    stopSignalHandler = NULL;

    SetExternalPrecision(10,3);
    SetBounds(NoNode,NoArc,NoHandle,NoIndex,NoIndex);

    epsilon   = 0.0001;
    tolerance = 0.001;

    SetRandomBounds(0,99);
    randUCap      = 0;
    randLCap      = 0;
    randLength    = 1;
    randGeometry  = 1;
    randParallels = 0;

    isDefault = true;

    OH = LinkController();

    goblinHashTable<THandle,long unsigned>* useTable =
            new goblinHashTable<THandle,long unsigned>(
                NoHandle,1000,(long unsigned)NULL,*this);

    objectTable = reinterpret_cast<char*>(useTable);

    // Let objectTable contain itself ;)
    RegisterObject(useTable,useTable->Handle());

    if (!controllerTable)
    {
        goblinHashTable<THandle,long unsigned>* useTable2 =
                new goblinHashTable<THandle,long unsigned>(
                    NoHandle,50,(long unsigned)NULL,*this);

        controllerTable = reinterpret_cast<char*>(useTable2);

        // Let objectTable contain controllerTable ;)
        RegisterObject(useTable,useTable2->Handle());

        // Let controllerTable contain this controller (goblinDefaultContext)
        RegisterController();
    }


    // Module entry points
    if (!pMipFactory)
    {
        mipFactory* tmpPtr = new nativeLPFactory();
        pMipFactory = reinterpret_cast<char*>(tmpPtr);
    }

    if (logMem) LogEntry(LOG_MEM,NoHandle,"...Default context allocated.");
}


goblinController::goblinController(const goblinController &masterContext,
    bool hidden) throw()
{
    objectTable = NULL;
    masterObject = NULL;
    firstObject = NULL;

    objectExtension = "gob";

    newObjectHandler = masterContext.newObjectHandler;

    sourceNodeInFile = masterContext.sourceNodeInFile;
    targetNodeInFile = masterContext.targetNodeInFile;
    rootNodeInFile   = masterContext.rootNodeInFile;


    // Global timer table

    #if defined(_TIMERS_)

    globalTimer = new pGoblinTimer[unsigned(NoTimer)];

    for (unsigned i=0;i<NoTimer;i++)
        if (listOfTimers[i].fullFeatured)
            globalTimer[i] = new goblinTimer(globalTimer);
        else globalTimer[i] = new goblinTimer();

    #endif


    // Logging Information

    logLevel = 0;
    moduleNestingLevel = 0;
    activeGuard = NULL;

    logMeth   = masterContext.logMeth;
    logMem    = masterContext.logMem;
    logMan    = masterContext.logMan;
    logIO     = masterContext.logIO;
    logRes    = masterContext.logRes;
    logWarn   = masterContext.logWarn;
    logTimers = masterContext.logTimers;
    logGaps   = masterContext.logGaps;
    logDepth  = masterContext.logDepth;

    logStream     = masterContext.logStream;
    suppressCount = 0;

    if (hidden) logEventHandler = NULL;
    else logEventHandler = masterContext.logEventHandler;

    savedLogEventHandler = NULL;
    savedErrorOriginator = NoHandle;
    savedErrorMsgType = NO_MSG;


    // Graph Browser

    displayMode = masterContext.displayMode;
    displayZoom = masterContext.displayZoom;
    pixelWidth  = masterContext.pixelWidth;
    pixelHeight = masterContext.pixelHeight;
    legenda     = masterContext.legenda;
    wallpaper   = new char[strlen(masterContext.wallpaper)+1];
    strcpy(wallpaper,masterContext.wallpaper);


    // Tracing Module

    traceLevel       = masterContext.traceLevel;
    commLevel        = masterContext.commLevel;
    traceData        = masterContext.traceData;
    threshold        = masterContext.threshold;
    traceStep        = masterContext.traceStep;
    ResetCounters();

    traceEventHandler = masterContext.traceEventHandler;


    // Mathematical Methods

    methFailSave    = masterContext.methFailSave;
    checkMem        = masterContext.checkMem;

    methSPX         = masterContext.methSPX;
    methMST         = masterContext.methMST;
    methMXF         = masterContext.methMXF;
    methMCC         = masterContext.methMCC;
    methMCFST       = masterContext.methMCFST;
    methMCF         = masterContext.methMCF;
    methNWPricing   = masterContext.methNWPricing;
    methMaxBalFlow  = masterContext.methMaxBalFlow;
    methBNS         = masterContext.methBNS;
    methDSU         = masterContext.methDSU;
    methPQ          = masterContext.methPQ;
    methHeurTSP     = masterContext.methHeurTSP;
    methLocal       = masterContext.methLocal;
    methSolve       = masterContext.methSolve;
    methMinCBalFlow = masterContext.methMinCBalFlow;
    methPrimalDual  = masterContext.methPrimalDual;
    methModLength   = masterContext.methModLength;
    methCandidates  = masterContext.methCandidates;
    methColour      = masterContext.methColour;
    methRelaxTSP1   = masterContext.methRelaxTSP1;
    methRelaxTSP2   = masterContext.methRelaxTSP2;
    methMaxCut      = masterContext.methMaxCut;

    methLP          = masterContext.methLP;
    methLPPricing   = masterContext.methLPPricing;
    methLPQTest     = masterContext.methLPQTest;
    methLPStart     = masterContext.methLPStart;

    maxBBIterations = masterContext.maxBBIterations;
    maxBBNodes      = masterContext.maxBBNodes;

    methFDP         = masterContext.methFDP;
    methPlanarity   = masterContext.methPlanarity;
    methOrthogonal  = masterContext.methOrthogonal;
    methOrthoRefine = masterContext.methOrthoRefine;

    stopSignalHandler = NULL;

    SetExternalPrecision(
        masterContext.externalFloatLength,
        masterContext.externalPrecision);
    SetBounds(
        masterContext.maxNode,
        masterContext.maxArc,
        masterContext.maxHandle,
        masterContext.maxIndex,
        masterContext.maxInt);

    epsilon   = masterContext.epsilon;
    tolerance = masterContext.tolerance;

    SetRandomBounds(masterContext.randMin,masterContext.randMax);
    randUCap      = masterContext.randUCap;
    randLCap      = masterContext.randLCap;
    randLength    = masterContext.randLength;
    randGeometry  = masterContext.randGeometry;
    randParallels = masterContext.randParallels;

    isDefault = false;

    OH = LinkController();

    goblinHashTable<THandle,long unsigned>* useTable =
            new goblinHashTable<THandle,long unsigned>(
                NoHandle,1000,(long unsigned)NULL,*this);

    objectTable = reinterpret_cast<char*>(useTable);

    // Let objectTable contain itself ;)
    RegisterObject(useTable,useTable->Handle());

    // Let controllerTable contain this controller (goblinDefaultContext)
    RegisterController();

    LogEntry(LOG_MEM,NoHandle,"...Context allocated.");
}


int goblinController::MajorVersion() throw()
{
    return _GOBLIN_MAJOR_VERSION_;
}


int goblinController::MinorVersion() throw()
{
    return _GOBLIN_MINOR_VERSION_;
}


char* goblinController::PatchLevel() throw()
{
    return _GOBLIN_REVISION_;
}


int goblinController::FindParam(int pc,const char* pStr[],const char* token,
    int offset) throw()
{
    for (int i=offset;i<pc;i++)
        if (strncmp(pStr[i],token,20)==0) return i;

    return 0;
}


int goblinController::FindParam(int pc,char* pStr[],const char* token,
    int offset) throw()
{
    for (int i=offset;i<pc;i++)
        if (strncmp(pStr[i],token,20)==0) return i;

    return 0;
}


void goblinController::Configure(int ParamCount,const char* ParamStr[])
    throw()
{
    int pc = FindParam(ParamCount,ParamStr,"-silent");

    if (pc>0)
    {
        logMeth = 0;
        logMem = 0;
        logMan = 0;
        logIO = 0;
        logRes = 0;
        logWarn = 0;
    }

    pc = FindParam(ParamCount,ParamStr,"-report");

    if (pc>0)
    {
        logMeth = 1;
        logMem = 0;
        logMan = 1;
        logIO = 1;
        logRes = 1;
        logWarn = 0;
    }

    pc = FindParam(ParamCount,ParamStr,"-details");

    if (pc>0)
    {
        logMeth = 2;
        logMem = 0;
        logMan = 1;
        logIO = 1;
        logRes = 1;
        logWarn = 0;
    }

    pc = FindParam(ParamCount,ParamStr,"-debug");

    if (pc>0)
    {
        logMeth = 2;
        logMem = 1;
        logMan = 1;
        logIO = 1;
        logRes = 2;
        logWarn = 1;
    }

    pc = FindParam(ParamCount,ParamStr,"-sourceNode");
    if (pc>0 && pc<ParamCount-1) sourceNodeInFile = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-targetNode");
    if (pc>0 && pc<ParamCount-1) targetNodeInFile = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-rootNode");
    if (pc>0 && pc<ParamCount-1) rootNodeInFile = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logMeth");
    if (pc>0 && pc<ParamCount-1) logMeth = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logMem");
    if (pc>0 && pc<ParamCount-1) logMem = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logMan");
    if (pc>0 && pc<ParamCount-1) logMan = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logIO");
    if (pc>0 && pc<ParamCount-1) logIO = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logRes");
    if (pc>0 && pc<ParamCount-1) logRes = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logWarn");
    if (pc>0 && pc<ParamCount-1) logWarn = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logTimers");
    if (pc>0 && pc<ParamCount-1) logTimers = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logGaps");
    if (pc>0 && pc<ParamCount-1) logGaps = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-logDepth");
    if (pc>0 && pc<ParamCount-1) logDepth = atoi(ParamStr[pc+1]);


    pc = FindParam(ParamCount,ParamStr,"-displayMode");
    if (pc>0 && pc<ParamCount-1) displayMode = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-displayZoom");
    if (pc>0 && pc<ParamCount-1) displayZoom = atof(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-pixelWidth");
    if (pc>0 && pc<ParamCount-1) pixelWidth = atof(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-pixelHeight");
    if (pc>0 && pc<ParamCount-1) pixelHeight = atof(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-legenda");
    if (pc>0 && pc<ParamCount-1) legenda = atoi(ParamStr[pc+1]);


    pc = FindParam(ParamCount,ParamStr,"-wallpaper");
    if (pc>0 && pc<ParamCount-1)
    {
        wallpaper =
            (char*)GoblinRealloc(wallpaper,strlen(ParamStr[pc+1])+1);
        strcpy(wallpaper,ParamStr[pc+1]);
    }


    pc = FindParam(ParamCount,ParamStr,"-traceLevel");
    if (pc>0 && pc<ParamCount-1) traceLevel = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-commLevel");
    if (pc>0 && pc<ParamCount-1) commLevel = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-traceData");
    if (pc>0 && pc<ParamCount-1) traceData = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-traceStep");
    if (pc>0 && pc<ParamCount-1) traceStep = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-threshold");
    if (pc>0 && pc<ParamCount-1) threshold = atoi(ParamStr[pc+1]);


    pc = FindParam(ParamCount,ParamStr,"-methFailSave");
    if (pc>0 && pc<ParamCount-1) methFailSave = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-checkMem");
    if (pc>0 && pc<ParamCount-1) checkMem = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-fs");
    if (pc>0) methFailSave = 1;


    pc = FindParam(ParamCount,ParamStr,"-methSPX");
    if (pc>0 && pc<ParamCount-1) methSPX = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMST");
    if (pc>0 && pc<ParamCount-1) methMST = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMXF");
    if (pc>0 && pc<ParamCount-1) methMXF = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMCC");
    if (pc>0 && pc<ParamCount-1) methMCC = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMCFST");
    if (pc>0 && pc<ParamCount-1) methMCFST = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMCF");
    if (pc>0 && pc<ParamCount-1) methMCF = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methNWPricing");
    if (pc>0 && pc<ParamCount-1) methNWPricing = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMaxBalFlow");
    if (pc>0 && pc<ParamCount-1) methMaxBalFlow = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methBNS");
    if (pc>0 && pc<ParamCount-1) methBNS = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methDSU");
    if (pc>0 && pc<ParamCount-1) methDSU = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methPQ");
    if (pc>0 && pc<ParamCount-1) methPQ = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methLocal");
    if (pc>0 && pc<ParamCount-1) methLocal = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methSolve");
    if (pc>0 && pc<ParamCount-1) methSolve = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMinCBalFlow");
    if (pc>0 && pc<ParamCount-1) methMinCBalFlow = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methPrimalDual");
    if (pc>0 && pc<ParamCount-1) methPrimalDual = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methModLength");
    if (pc>0 && pc<ParamCount-1) methModLength = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methCandidates");
    if (pc>0 && pc<ParamCount-1) methCandidates = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methHeurTSP");
    if (pc>0 && pc<ParamCount-1) methHeurTSP = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methRelaxTSP1");
    if (pc>0 && pc<ParamCount-1) methRelaxTSP1 = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methRelaxTSP2");
    if (pc>0 && pc<ParamCount-1) methRelaxTSP2 = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methMaxCut");
    if (pc>0 && pc<ParamCount-1) methMaxCut = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methLP");
    if (pc>0 && pc<ParamCount-1) methLP = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methLPPricing");
    if (pc>0 && pc<ParamCount-1) methLPPricing = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methLPQTest");
    if (pc>0 && pc<ParamCount-1) methLPQTest = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methLPStart");
    if (pc>0 && pc<ParamCount-1) methLPStart = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-maxBBIterations");
    if (pc>0 && pc<ParamCount-1) maxBBIterations = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-maxBBNodes");
    if (pc>0 && pc<ParamCount-1) maxBBNodes = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methFDP");
    if (pc>0 && pc<ParamCount-1) methFDP = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methPlanarity");
    if (pc>0 && pc<ParamCount-1) methPlanarity = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methOrthogonal");
    if (pc>0 && pc<ParamCount-1) methOrthogonal = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-methOrthoRefine");
    if (pc>0 && pc<ParamCount-1) methOrthoRefine = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-rand");
    if (pc>0 && pc<ParamCount-1) SetRandomBounds(atoi(ParamStr[pc+1]),atoi(ParamStr[pc+2]));

    pc = FindParam(ParamCount,ParamStr,"-randGeometry");
    if (pc>0 && pc<ParamCount-1) randGeometry = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-randParallels");
    if (pc>0 && pc<ParamCount-1) randParallels = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-randLength");
    if (pc>0 && pc<ParamCount-1) randLength = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-randUCap");
    if (pc>0 && pc<ParamCount-1) randUCap = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-randLCap");
    if (pc>0 && pc<ParamCount-1) randLCap = atoi(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-seed");
    if (pc>0 && pc<ParamCount-1) srand(atoi(ParamStr[pc+1]));

    pc = FindParam(ParamCount,ParamStr,"-epsilon");
    if (pc>0 && pc<ParamCount-1) epsilon = atol(ParamStr[pc+1]);

    pc = FindParam(ParamCount,ParamStr,"-tolerance");
    if (pc>0 && pc<ParamCount-1) tolerance = atol(ParamStr[pc+1]);
}


void goblinController::ReadConfiguration(const char* impFileName)
    throw(ERFile,ERParse)
{
    goblinImport F(impFileName,*this);
    F.Scan("configure");
    F.ReadConfiguration();
}


void goblinController::WriteConfiguration(const char* expFileName) throw(ERFile)
{
    goblinExport F(expFileName,*this);
    F.WriteConfiguration(*this);
}


char* goblinController::Display() const throw()
{
    sprintf(logBuffer,"Controller at 0x%.8lx (handle %ld)",
        reinterpret_cast<unsigned long>(this),OH);
    LogEntry(MSG_TRACE,NoHandle,logBuffer);

    goblinRootObject* pObject = firstObject;

    while (pObject!=NULL)
    {
        sprintf(logBuffer,"  %-.20s at 0x%.8lx (handle %ld)",
            typeid(*pObject).name(),
            reinterpret_cast<unsigned long>(pObject),pObject->Handle());
        LogEntry(MSG_TRACE2,NoHandle,logBuffer);

        pObject = pObject->nextObject;
    }

    return NULL;
}


void goblinController::DisplayAll() const throw()
{
    goblinRootObject* pController = firstController;

    while (pController)
    {
        goblinController* thisController = dynamic_cast<goblinController*>(pController);

        void (*tempLogEventHandler)(msgType,TModule,THandle,char*) =
            thisController->logEventHandler;
        thisController->logEventHandler = logEventHandler;

        pController -> Display();
        pController = pController->nextObject;

        thisController->logEventHandler = tempLogEventHandler;
    }
}


void goblinController::ReleaseLPModule() throw()
{
    if (pMipFactory) return;

    char* tmpPtr = const_cast<char*>(pMipFactory);
    delete reinterpret_cast<mipFactory*>(tmpPtr);
    pMipFactory = NULL;
}


goblinController::~goblinController() throw()
{
    if (!(firstController->nextObject))
    {
        // If no further controller remains, the LP module must be released
        ReleaseLPModule();

        // controllerTable is not referenced any longer
        delete reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(
            controllerTable);
    }
    else
    {
        // Deregister from controllerTable
        goblinHashTable<THandle,long unsigned>* useTable =
            reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(
                controllerTable);
        useTable -> ChangeKey(OH,(long unsigned)NULL);
    }

    #if defined(_FAILSAVE_)

    if (firstObject->nextObject)
    {
        Error(MSG_WARN,NoHandle,"goblinController",
            "There are undestroyed objects");
    }

    if (logLevel!=0)
        Error(MSG_WARN,NoHandle,"goblinController","Mismatch in logging module");

    #endif

    // Invalidate objectTable before deleting it
    char* tmpObjTable = objectTable;
    objectTable = NULL;
    delete reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(tmpObjTable);

    delete[] wallpaper;

    #if defined(_TIMERS_)

    for (unsigned i=0;i<NoTimer;i++) delete globalTimer[i];
    delete[] globalTimer;

    #endif

    // Unlink from controller list
    if (static_cast<goblinRootObject*>(this)==firstController)
    {
        firstController = nextObject;
        if (firstController) firstController->prevObject = NULL;
    }
    else
    {
        prevObject->nextObject = nextObject;
        if (nextObject) nextObject->prevObject = prevObject;
    }

    if (!isDefault && logMem)
        LogEntry(LOG_MEM,NoHandle,"...Context disallocated.");
}


unsigned long goblinController::Size() const throw()
{
    unsigned long tmpSize
        = sizeof(goblinController)
        + sizeof(char[LOGBUFFERSIZE])
        + strlen(wallpaper)+1;

    goblinRootObject* thisObject = firstObject;

    while (thisObject!=NULL)
    {
        tmpSize += thisObject->Size();
        thisObject = thisObject->nextObject;
    }

    return tmpSize;
}


THandle goblinController::LinkController()
    throw(ERRejected)
{
    // Receive a new handle
    THandle thisHandle = newObjectHandler();

    if (logMem)
    {
        sprintf(logBuffer,"Constructing controller with handle <%ld>...",
            thisHandle);
        LogEntry(LOG_MEM,NoHandle,logBuffer);
    }

    // Update linked list of member objects
    nextObject = firstController;
    prevObject = NULL;

    if (firstController) firstController->prevObject = this;

    firstController = this;

    if (controllerTable) RegisterController();

    return thisHandle;
}


void goblinController::RegisterController()
    throw(ERRejected)
{
    // Update the mapping from controller handles to raw pointers
    goblinHashTable<THandle,long unsigned>* useTable =
        reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(
            controllerTable);

    useTable -> ChangeKey(OH,reinterpret_cast<long unsigned>(
        static_cast<goblinRootObject*>(this)));
}


THandle goblinController::InsertObject(goblinRootObject* thisObject)
    throw(ERRejected)
{
    // Receive a new handle
    THandle thisHandle = newObjectHandler();

    if (logMem)
    {
        sprintf(logBuffer,"Constructing object with handle <%ld>...",
            thisHandle);
        LogEntry(LOG_MEM,NoHandle,logBuffer);
    }

    // Update linked list of member objects
    thisObject->nextObject = firstObject;
    thisObject->prevObject = NULL;

    if (firstObject) firstObject->prevObject = thisObject;

    firstObject = thisObject;

    if (objectTable) RegisterObject(thisObject,thisHandle);

    return thisHandle;
}


void goblinController::RegisterObject(goblinRootObject* thisObject,
    THandle thisHandle) throw(ERRejected)
{
    // Update the mapping from object handles to raw pointers
    goblinHashTable<THandle,long unsigned>* useTable =
        reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(objectTable);

    useTable -> ChangeKey(thisHandle,reinterpret_cast<long unsigned>(thisObject));
}


void goblinController::DeleteObject(const goblinRootObject* thisObject)
    throw(ERRejected)
{
    THandle thisHandle = thisObject->Handle();

    goblinHashTable<THandle,long unsigned>* useTable =
        reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(objectTable);

    #if defined(_FAILSAVE_)

    if (objectTable && useTable->Key(thisHandle)==(long unsigned)NULL)
        Error(ERR_REJECTED,NoHandle,"DeleteObject","Context mismatch");

    #endif

    // Delete object from objectTable
    if (objectTable)
        useTable -> ChangeKey(thisHandle,(long unsigned)NULL);

    // Update linked list of member objects
    if (thisObject==firstObject)
    {
        firstObject = thisObject->nextObject;
        if (firstObject) firstObject->prevObject = NULL;
    }
    else
    {
        thisObject->prevObject->nextObject = thisObject->nextObject;
        if (thisObject->nextObject)
            thisObject->nextObject->prevObject = thisObject->prevObject;
    }

    // Reset master object if necessary
    if (masterObject==thisObject) masterObject = NULL;

    if (logMem)
    {
        sprintf(logBuffer,"...Object with handle <%ld> deleted",thisHandle);
        LogEntry(LOG_MEM,NoHandle,logBuffer);
    }
}


goblinRootObject* goblinController::ObjectPointer(THandle thisHandle)
    const throw(ERRejected)
{
    goblinHashTable<THandle,long unsigned>* useTable =
        reinterpret_cast<goblinHashTable<THandle,long unsigned>*>(objectTable);

    goblinRootObject* thisObject =
        reinterpret_cast<goblinRootObject*>(useTable->Key(thisHandle));

    return thisObject;
}


goblinRootObject* goblinController::Lookup(THandle thisHandle)
    const throw(ERRejected)
{
    goblinRootObject* ret = ObjectPointer(thisHandle);

    if (ret) return ret;

    goblinRootObject* pController = firstController;

    while (pController)
    {
        if (thisHandle==pController->Handle()) return pController;

        goblinController* thisController = dynamic_cast<goblinController*>(pController);

        if (thisController!=this)
        {
            ret = thisController->ObjectPointer(thisHandle);

            if (ret) return ret;
        }

        pController = pController->nextObject;
    }

    return NULL;
}


void goblinController::SetMaster(THandle HH) throw(ERRejected)
{
    goblinRootObject* thisObject = ObjectPointer(HH);

    #if defined(_FAILSAVE_)

    if (!thisObject)
        Error(ERR_REJECTED,NoHandle,"SetMaster","No such handle");

    #endif

    masterObject = thisObject;
}


THandle goblinController::Master() throw(ERCheck)
{
    if (!masterObject)
        Error(ERR_CHECK,NoHandle,"Master","No master object defined");

    return masterObject->Handle();
}


const char* goblinController::Label() const throw()
{
    if (!masterObject) return "unnamed";

    return masterObject->Label();
}


bool goblinController::SolverRunning() const throw()
{
    return (stopSignalHandler==NULL || stopSignalHandler());
}


unsigned long goblinController::Rand(unsigned long x) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (x>goblinRandMax)
        Error(ERR_RANGE,NoHandle,"Rand","Out of Range");

    #endif

    unsigned long thisRandMax = goblinRandMax-(goblinRandMax%x);

    unsigned long ret = goblinRandMax;
    while (ret>=thisRandMax)
        ret = (unsigned long)((rand()*(RAND_MAX+1)+rand())%x);
    return ret;
}


TFloat goblinController::UnsignedRand() throw()
{
    int thisMin = randMin;
    if (thisMin<0) thisMin = 0;

    return TFloat(Rand(randMax-thisMin+1))+TFloat(thisMin);
}


TFloat goblinController::SignedRand() throw()
{
    return TFloat(Rand(randMax-randMin+1))+TFloat(randMin);
}


void goblinController::SetRandomBounds(long int mMinRand,long int mMaxRand)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (mMinRand>mMaxRand || mMinRand<-maxInt || mMaxRand>maxInt)
        Error(ERR_REJECTED,NoHandle,"SetRandomBounds","Out of range");

    #endif

    randMin = mMinRand;
    randMax = mMaxRand;
}


void goblinController::SetExternalPrecision(int width,int prec)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (width<prec+3 || width>15)
        Error(ERR_REJECTED,NoHandle,"SetExternalPrecision","Out of range");

    #endif

    externalFloatLength = width;
    externalPrecision   = prec;
}


void goblinController::SetBounds(TNode cMaxNode,TArc cMaxArc,
    THandle cMaxHandle,TIndex cMaxIndex,long int cMaxInt) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (cMaxNode>cMaxArc || cMaxArc>cMaxIndex ||
        cMaxHandle>cMaxIndex || cMaxIndex>TIndex(cMaxInt))
        Error(ERR_REJECTED,NoHandle,"SetBounds","Out of range");

    if (cMaxNode>NoNode || cMaxArc>NoArc || cMaxHandle>NoHandle ||
        cMaxIndex>NoIndex)
        Error(ERR_REJECTED,NoHandle,"SetBounds","Out of range");

    TFloat precisePower = 1;

    for (char i=0;i<externalFloatLength;i++) precisePower *= 10;

    if (cMaxInt>precisePower)
        Error(ERR_REJECTED,NoHandle,"SetBounds","Out of range");

    #endif

    maxNode   = cMaxNode;
    maxArc    = cMaxArc;
    maxHandle = cMaxHandle;
    maxIndex  = cMaxIndex;
    maxInt    = cMaxInt;
}


static char lengthCheckBuffer[25];

int goblinController::ExternalIntLength(long val) const throw()
{
    sprintf(lengthCheckBuffer,"%ld",val);
    int l1 = int(strlen(lengthCheckBuffer));

    if (l1 < externalFloatLength)
        return l1;
    else return externalFloatLength;
}


int goblinController::ExternalFloatLength(TFloat val) const throw()
{
    sprintf(lengthCheckBuffer,"%.*f",externalPrecision,double(val));
    int l1 = int(strlen(lengthCheckBuffer));

    if (l1 < externalFloatLength)
        return l1;
    else return externalFloatLength;
}


void goblinController::IncreaseLogLevel() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (logLevel==INT_MAX)
        Error(ERR_REJECTED,NoHandle,"IncreaseLogLevel","Log depth overflow");

    #endif

    logLevel++;
}


void goblinController::DecreaseLogLevel() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (logLevel==0)
        Error(ERR_REJECTED,NoHandle,"DecreaseLogLevel",
            "Mismatch in logging module");

    #endif

    clog.flush();
    logLevel--;
}


void goblinController::OpenFold(TModule mod,TOption opt) throw(ERRejected)
{
    if ( !(opt & moduleGuard::NO_INDENT) ) IncreaseLogLevel();

    if (moduleNestingLevel<MAX_MODULE_NESTING)
        nestedModule[moduleNestingLevel] = mod;

// cout << "OPEN : " << moduleNestingLevel << " " << listOfModules[mod].moduleName << " " << endl;
    moduleNestingLevel++;

    if ( (opt & moduleGuard::SHOW_TITLE) &&
         (moduleNestingLevel==1 || mod!=nestedModule[moduleNestingLevel-2])
       )
    {
        sprintf(logBuffer,"<%s>",listOfModules[mod].moduleName);
        LogEntry(LOG_METH2,NoHandle,logBuffer);
    }
}


void goblinController::CloseFold(TModule mod,TOption opt) throw(ERRejected)
{
    if ( !(opt & moduleGuard::NO_INDENT) ) DecreaseLogLevel();

    moduleNestingLevel--;
// cout << "CLOSE : " << moduleNestingLevel << " " << listOfModules[mod].moduleName << " " << endl;

    #ifdef _PROGRESS_

    // Mark the progress counter "finished", not "initial"
    if (moduleNestingLevel==progressHigh)
    {
        progressHigh = -2;
    }
    else if (moduleNestingLevel==progressLow)
    {
        progressLow = -2;
    }

    #endif

    #if defined(_FAILSAVE_)

    if (moduleNestingLevel<MAX_MODULE_NESTING &&
          nestedModule[moduleNestingLevel] != mod)
        Error(ERR_REJECTED,NoHandle,"CloseFold",
            "Mismatching fold declarations");

    #endif
}


#ifdef _PROGRESS_

double goblinController::EstimatedExecutionTime() const throw()
{
    moduleGuard* iteratedGuard = activeGuard;

    if (!iteratedGuard) return 0.0;

    while (iteratedGuard->parent) iteratedGuard = iteratedGuard->parent;

    return (globalTimer[listOfModules[iteratedGuard->guardedModule].moduleTimer]->CurrTime())
        / ProgressCounter();
}


double goblinController::ProgressCounter() const throw()
{
    double iteratedProgress = 0.0;
    moduleGuard* iteratedGuard = activeGuard;

    while (iteratedGuard)
    {
        iteratedProgress =
            (iteratedGuard->progress + iteratedGuard->nextProgress*iteratedProgress) / iteratedGuard->maxProgress;

        if (iteratedProgress>1.0) iteratedProgress = 1.0;
/*
cout << " " << listOfModules[iteratedGuard->guardedModule].moduleName << ": "
    << iteratedGuard->progress << " " << iteratedGuard->nextProgress  << " " << iteratedGuard->maxProgress << endl;
*/
        iteratedGuard = iteratedGuard->parent;
    }
// cout << "Progress = " << iteratedProgress << endl;
    return iteratedProgress;
}

#endif


void goblinController::SuppressLogging() throw()
{
    if (suppressCount==0)
    {
        savedLogEventHandler = logEventHandler;
        savedTraceLevel = traceLevel;
        logEventHandler = NULL;
    }

    suppressCount++;
}


void goblinController::RestoreLogging() throw()
{
    suppressCount--;

    if (suppressCount==0)
    {
        traceLevel = savedTraceLevel;
        logEventHandler = savedLogEventHandler;
    }
}


void goblinController::PlainLogEventHandler(
    msgType msg,TModule mod,THandle OH,char* logEntry)
    throw()
{
    ostream &cgob = *logStream;

    if (msg != MSG_APPEND)
    {
        cgob << endl;

        if (logLevel<logDepth) cgob.width(2*logLevel);
        else cgob.width(2*logDepth);

        cgob.fill(' ');
        cgob << "";
    }

    cgob << logEntry << flush;
}


void goblinController::DefaultLogEventHandler(
    msgType msg,TModule mod,THandle OH,char* logEntry)
    throw()
{
    ostream &cgob = *logStream;

    if (msg != MSG_APPEND)
    {
        cgob << endl;
        cgob.width(3);
        cgob << msg << " ";
        cgob.width(3);
        cgob << mod << " ";
        cgob.width(8);

        if (OH!=NoHandle) cgob << OH << " ";
        else cgob << "-1" << " ";

        cgob.width(3);
        cgob << logLevel << " ";

        if (logLevel<logDepth) cgob.width(2*logLevel);
        else cgob.width(2*logDepth);

        cgob.fill(' ');
        cgob << "";
    }

    cgob << logEntry << flush;
}


THandle goblinController::LogFilter(msgType msg,THandle OH,const char* logEntry) const throw()
{
    void (*usedLogEventHandler)(msgType,TModule,THandle,char*)
        = logEventHandler;

    if (!logEventHandler)
    {
        if (!savedLogEventHandler) return NoHandle;

        // Process errors even if logging is suppressed

        switch (msg)
        {
            case ERR_INTERNAL:
            case ERR_RANGE:
            case ERR_REJECTED:
            case ERR_CHECK:
            case ERR_FILE:
            case ERR_PARSE:
            {
                usedLogEventHandler = savedLogEventHandler;
                break;
            }
            default:
            {
                return NoHandle;
            }
        }
    }

    if ((!logMem    && msg==LOG_MEM)     ||
        (!logIO     && msg==LOG_IO)      ||
        (!logMeth   && msg==LOG_METH)    ||
        (logMeth<2  && msg==LOG_METH2)   ||
        (!logMan    && msg==LOG_MAN)     ||
        (!logRes    && msg==LOG_RES)     ||
        (logRes<2   && msg==LOG_RES2)    ||
        (!logWarn   && msg==MSG_WARN)    ||
        (!logTimers && msg==LOG_TIMERS)  ||
        (!logGaps   && msg==LOG_GAPS)
       )
    {
        return NoHandle;
    }

    TModule mod = ModRoot;

    if (moduleNestingLevel>=MAX_MODULE_NESTING)
    {
        mod = nestedModule[MAX_MODULE_NESTING-1];
    }
    else if (moduleNestingLevel>0)
    {
        mod = nestedModule[moduleNestingLevel-1];
    }

    usedLogEventHandler(msg,mod,OH,const_cast<char*>(logEntry));

    return 0;
}


void goblinController::LogEntry(msgType msg,THandle OH,const char* logEntry) const throw()
{
    if (compoundLogEntry || msg==MSG_APPEND) return;

    LogFilter(msg,OH,logEntry);
}


THandle goblinController::LogStart(msgType msg,THandle OH,const char* logEntry) const throw()
{
    if (compoundLogEntry || msg==MSG_APPEND) return NoHandle;

    THandle ret = LogFilter(msg,OH,logEntry);

    if (ret!=NoHandle) compoundLogEntry = true;

    return ret;
}


void goblinController::LogAppend(THandle LH,const char* logEntry) const throw()
{
    if (LH==NoHandle) return;

    LogFilter(MSG_APPEND,LH,logEntry);
}


void goblinController::LogEnd(THandle LH,const char* logEntry) const throw()
{
    if (LH==NoHandle) return;

    if (logEntry) LogFilter(MSG_APPEND,LH,logEntry);

    compoundLogEntry = false;
}


void goblinController::Error(msgType msg,THandle OH,const char* methodName,
    const char* description) throw(ERRange,ERRejected,ERCheck,ERFile,ERParse)
{
    // Maintain information for GOSH shell

    strcpy(savedErrorMethodName,methodName);
    strcpy(savedErrorDescription,description);
    savedErrorOriginator = OH;
    savedErrorMsgType = msg;

    if (!logWarn && msg==MSG_WARN) return;

    compoundLogEntry = false;

    if (!logWarn && msg==ERR_CHECK) throw ERCheck();

    LogEntry(msg,OH,"::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");

    if (methodName)
    {
        THandle LH = LogStart(msg,OH,methodName);
        LogAppend(LH," - ");
        LogEnd(LH,description);
    }
    else LogEntry(msg,OH,description);

    if (OH!=NoHandle)
    {
        const char* thisLabel = ObjectPointer(OH)->Label();

        if (thisLabel)
        {
            sprintf(logBuffer,"Object \"%s\"",thisLabel);
        }
        else sprintf(logBuffer,"Object #%ld",(unsigned long)OH);
    }
    else sprintf(logBuffer,"Controller object");

    LogEntry(msg,OH,logBuffer);
    sprintf(logBuffer,", before trace point #%d",breakPoint);
    LogEntry(MSG_APPEND,OH,logBuffer);
    LogEntry(msg,OH,"::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");

    switch (msg)
    {
        case MSG_WARN:     return;
        case ERR_RANGE:    throw ERRange();
        case ERR_REJECTED: throw ERRejected();
        case ERR_CHECK:    throw ERCheck();
        case ERR_FILE:     throw ERFile();
        case ERR_PARSE:    throw ERParse();
        default:           throw ERInternal();
    }
}


void goblinController::Trace(THandle HH,unsigned long step) throw()
{
    if (traceLevel<=0) return;

    if (traceStep>0 && traceStep-traceCounter>step)
    {
        traceCounter += step;
        return;
    }

    traceCounter = 0;
    breakPoint ++;

    switch (traceLevel)
    {
        case 1:
        {
            cout << ".";
            cout.flush();
            break;
        }

        #if defined(_TRACING_)

        case 2:
        {
            break;
        }
        case 3:
        {
            if (breakPoint>threshold) ObjectPointer(HH)->Display();

            break;
        }
        case 4:
        {
            if (breakPoint<=threshold) return;

            cout << endl;

            char* tmpLabel = ObjectPointer(HH)->Display();

            if (tmpLabel!=NULL)
            {
                int objStrLen = strlen(tmpLabel)+10;
                char* objectStr = new char[objStrLen+1];
                sprintf(objectStr,"%s.%d",tmpLabel,fileCounter);

                char* commandStr = new char[objStrLen+10];

                if (displayMode==1)
                {
                    sprintf(commandStr,"xfig %s.fig",objectStr);
                    cout << "Starting Xfig..." << endl;
                    system (commandStr);
                }

                if (displayMode==2)
                {
                    sprintf(commandStr,"wish display %s",objectStr);
                    cout << "Starting browser..." << endl;
                    system (commandStr);
                }

                delete commandStr;
                delete objectStr;
            }
            else
            {
                cout << endl << "Click <Enter> to proceed";
                while (getchar()==EOF) {};
            }

            break;
        }

        #endif

        default:
        {
            sprintf(logBuffer,"Unsupported trace level: %d",traceLevel);
            Error(MSG_WARN,NoHandle,"Trace",logBuffer);
        }
    }
}


void goblinController::ResetCounters() throw()
{
    traceCounter       = 0;
    breakPoint         = 1;
    fileCounter        = 0;
    breakLevel         = INT_MAX;
    logLevel           = 0;
    moduleNestingLevel = 0;
    activeGuard        = NULL;
    compoundLogEntry   = false;

    if (suppressCount>0)
    {
        suppressCount = 1;
        RestoreLogging();
    }

    #if defined(_PROGRESS_)

    progressHigh = -1;
    progressLow  = -1;

    #endif
}


void goblinController::ResetTimer(TTimer timer) throw(ERRange)
{
    #if defined(_TIMERS_)

    if (timer>=NoTimer)
    {
        sprintf(logBuffer,"No such timer: %d",timer);
        Error(ERR_RANGE,NoHandle,"ResetTimer",logBuffer);
    }

    globalTimer[timer] -> Reset();

    #endif
}


void goblinController::ResetTimers() throw()
{
    #if defined(_TIMERS_)

    for (unsigned i=0;i<NoTimer;i++) globalTimer[i] -> Reset();

    #endif
}


goblinController goblinDefaultContext;
