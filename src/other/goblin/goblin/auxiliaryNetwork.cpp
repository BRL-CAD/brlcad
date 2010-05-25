
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   auxiliaryNetwork.cpp
/// \brief  #layeredAuxNetwork class implementation

#include "auxiliaryNetwork.h"


layeredAuxNetwork::layeredAuxNetwork(abstractDiGraph& _G,TNode _s) throw() :
    managedObject(_G.Context()),
    abstractDiGraph(_G.N(),_G.M()),
    G(_G), s(_s), pred(_G.GetPredecessors())
{
    G.MakeRef();
    blocked = new staticQueue<TNode>(n,CT);

    outDegree = new TArc[n];
    successor = new TArc*[n];
    inDegree = new TArc[n];
    currentDegree = new TArc[n];
    prop = new TArc*[n];

    for (TNode i=0;i<n;i++)
    {
        outDegree[i] = 0;
        inDegree[i] = 0;
        successor[i] = NULL;
        prop[i] = NULL;
    }

    I = new iLayeredAuxNetwork(*this);

    if (!pred) pred = G.InitPredecessors();

    Phase = 1;
    align = "";

    SetLayoutParameter(TokLayoutNodeLabelFormat,"#1");
    SetLayoutParameter(TokLayoutArcLabelFormat,"#2");

    LogEntry(LOG_MEM,"Layered auxiliary network instanciated...");
}


void layeredAuxNetwork::Init() throw()
{
    #if defined(_FAILSAVE_)

    if (Phase!=1) Error(ERR_REJECTED,"Init","Inapplicable in phase 2");

    #endif

    for (TNode i=0;i<n;i++)
    {
        outDegree[i] = 0;
        inDegree[i] = 0;
        delete[] successor[i];
        successor[i] = NULL;
        delete[] prop[i];
        prop[i] = NULL;
    }
}


layeredAuxNetwork::~layeredAuxNetwork() throw()
{
    for (TNode i=0;i<n;i++)
    {
        delete[] successor[i];
        delete[] prop[i];
    }

    delete[] outDegree;
    delete[] successor;
    delete[] inDegree;
    delete[] currentDegree;
    delete[] prop;
    delete blocked;
    delete I;

    G.ReleaseRef();

    LogEntry(LOG_MEM,"...Layered auxiliary network disallocated");
}


unsigned long layeredAuxNetwork::Size() const throw()
{
    return
          sizeof(layeredAuxNetwork)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractDiGraph::Allocated()
        + layeredAuxNetwork::Allocated();
}


unsigned long layeredAuxNetwork::Allocated() const throw()
{
    unsigned long tmpSize
        = 3*n*sizeof(TArc)      // outDegree[],inDegree[],currentDegree[]
        + 2*n*sizeof(TArc*);    // prop[],successor[]

    for (TNode i=0;i<n;i++)     // prop[][],successor[][]
        tmpSize += (outDegree[i]+inDegree[i])*sizeof(TArc);

    return tmpSize;
}


investigator *layeredAuxNetwork::NewInvestigator() const throw()
{
    return new iLayeredAuxNetwork(*this);
}


TNode layeredAuxNetwork::StartNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("StartNode",a);

    #endif

    return G.StartNode(a);
}


TNode layeredAuxNetwork::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    return G.EndNode(a);
}


TArc layeredAuxNetwork::First(TNode v) const throw(ERRange,ERRejected)
{
    Error(ERR_REJECTED,"First","Not implemented");
    throw ERRejected();
}


TArc layeredAuxNetwork::Right(TArc a,TNode u) const throw(ERRange,ERRejected)
{
    Error(ERR_REJECTED,"Right","Not implemented");
    throw ERRejected();
}


TCap layeredAuxNetwork::UCap(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("UCap",a);

    #endif

    if (!Blocking(2*(a>>1))) return (TCap)G.ResCap(2*(a>>1));
    if (!Blocking(2*(a>>1)+1)) return (TCap)G.ResCap(2*(a>>1)+1);

    return 0;
}


bool layeredAuxNetwork::Blocking(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Blocking",a);

    #endif

    return
           G.ResCap(a)==0
        || Blocked(G.StartNode(a))
        ||  G.Dist(G.StartNode(a))!=G.Dist(G.EndNode(a))-1;
}


char layeredAuxNetwork::Orientation(TArc a) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Orientation",a);

    #endif

    if (!Blocking(2*(a>>1))) return 1;
    if (!Blocking(2*(a>>1)+1)) return 2;

    return 0;
}


TFloat layeredAuxNetwork::Dist(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Dist",v);

    #endif

    return G.Dist(v);
}


void layeredAuxNetwork::Phase1() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Phase==1) Error(ERR_REJECTED,"Phase1","Already in phase 1");

    #endif

    Phase = 1;
}


void layeredAuxNetwork::InsertProp(TArc a) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("InsertProp",a);

    if (Phase==2) Error(ERR_REJECTED,"InsertProp","Inapplicable in phase 2");

    #endif

    TNode u = EndNode(a);
    TNode v = StartNode(a);

    inDegree[u]++;
    prop[u] = (TArc*)GoblinRealloc(prop[u],sizeof(TArc)*(int)inDegree[u]);
    prop[u][inDegree[u]-1] = a;

    outDegree[v]++;
    successor[v] = (TArc*)GoblinRealloc(successor[v],sizeof(TArc)*(int)outDegree[v]);
    successor[v][outDegree[v]-1] = a;
}


void layeredAuxNetwork::Phase2() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Phase==2) Error(ERR_REJECTED,"Phase2","Already in phase 2");

    #endif

    for (TNode i=0;i<n;i++) currentDegree[i] = inDegree[i];

    I -> Reset();
    Phase = 2;
}


bool layeredAuxNetwork::Blocked(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Blocked",v);

    #endif

    if (v==s) return false;
    else return (currentDegree[v]==0);
}


TFloat layeredAuxNetwork::FindPath(TNode t) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (t>=n) NoSuchNode("FindPath",t);

    if (Phase==1) Error(ERR_REJECTED,"FindPath","Inapplicable in phase 1");

    #endif

    TNode v = t;
    TFloat Lambda = InfFloat;

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Augmenting by path (%lu",static_cast<unsigned long>(v));
        LH = LogStart(LOG_METH2,CT.logBuffer);
    }

    #endif

    while (v!=s)
    {
        TArc a = I->Peek(v);
        TNode u = StartNode(a);

        while (Blocked(u) || G.ResCap(a)==0)
        {
            I -> Read(v);
            a = I->Peek(v);
            u = StartNode(a);
        }

        pred[v] = a;
        v = u;

        if (G.ResCap(a)<Lambda) Lambda = G.ResCap(a);

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(v));
            LogAppend(LH,CT.logBuffer);
        }

        #endif
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH,")");

    #endif

    if (Lambda<=0) InternalError("FindPath","Path has no residual capacity");

    return Lambda;
}


void layeredAuxNetwork::TopErasure(TArc a) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("TopErasure",a);

    if (Phase==1) Error(ERR_REJECTED,"TopErasure","Inapplicable in phase 1");

    #endif

    TNode x = EndNode(a);
    I -> Read(x);

    currentDegree[x]--;

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Node %lu has indegree %lu",
            static_cast<unsigned long>(x),
            static_cast<unsigned long>(currentDegree[x]));
        LogEntry(LOG_METH2,CT.logBuffer);
    }

    #endif

    if (currentDegree[x]==0)
    {
        blocked -> Insert(x);
        while (!(blocked -> Empty()))
        {
            x = blocked->Delete();

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Deleting node %lu...",
                    static_cast<unsigned long>(x));
                LogEntry(LOG_METH2,CT.logBuffer);
                OpenFold();
            }

            #endif

            for (TArc i=0;i<outDegree[x];i++)
            {
                TArc aa = successor[x][i];
                TNode y = EndNode(aa);

                if (G.ResCap(aa)>0)
                {
                    currentDegree[y]--;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"Node %lu has indegree %lu",
                            static_cast<unsigned long>(y),
                            static_cast<unsigned long>(currentDegree[y]));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    if (currentDegree[y]==0) blocked -> Insert(y);
                }
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1) CloseFold();

            #endif
        }
    }
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, January 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   auxiliaryNetwork.cpp
/// \brief  #layeredAuxNetwork class implementation

#include "auxiliaryNetwork.h"


layeredAuxNetwork::layeredAuxNetwork(abstractDiGraph& _G,TNode _s) throw() :
    managedObject(_G.Context()),
    abstractDiGraph(_G.N(),_G.M()),
    G(_G), s(_s), pred(_G.GetPredecessors())
{
    G.MakeRef();
    blocked = new staticQueue<TNode>(n,CT);

    outDegree = new TArc[n];
    successor = new TArc*[n];
    inDegree = new TArc[n];
    currentDegree = new TArc[n];
    prop = new TArc*[n];

    for (TNode i=0;i<n;i++)
    {
        outDegree[i] = 0;
        inDegree[i] = 0;
        successor[i] = NULL;
        prop[i] = NULL;
    }

    I = new iLayeredAuxNetwork(*this);

    if (!pred) pred = G.InitPredecessors();

    Phase = 1;
    align = "";

    SetLayoutParameter(TokLayoutNodeLabelFormat,"#1");
    SetLayoutParameter(TokLayoutArcLabelFormat,"#2");

    LogEntry(LOG_MEM,"Layered auxiliary network instanciated...");
}


void layeredAuxNetwork::Init() throw()
{
    #if defined(_FAILSAVE_)

    if (Phase!=1) Error(ERR_REJECTED,"Init","Inapplicable in phase 2");

    #endif

    for (TNode i=0;i<n;i++)
    {
        outDegree[i] = 0;
        inDegree[i] = 0;
        delete[] successor[i];
        successor[i] = NULL;
        delete[] prop[i];
        prop[i] = NULL;
    }
}


layeredAuxNetwork::~layeredAuxNetwork() throw()
{
    for (TNode i=0;i<n;i++)
    {
        delete[] successor[i];
        delete[] prop[i];
    }

    delete[] outDegree;
    delete[] successor;
    delete[] inDegree;
    delete[] currentDegree;
    delete[] prop;
    delete blocked;
    delete I;

    G.ReleaseRef();

    LogEntry(LOG_MEM,"...Layered auxiliary network disallocated");
}


unsigned long layeredAuxNetwork::Size() const throw()
{
    return
          sizeof(layeredAuxNetwork)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractDiGraph::Allocated()
        + layeredAuxNetwork::Allocated();
}


unsigned long layeredAuxNetwork::Allocated() const throw()
{
    unsigned long tmpSize
        = 3*n*sizeof(TArc)      // outDegree[],inDegree[],currentDegree[]
        + 2*n*sizeof(TArc*);    // prop[],successor[]

    for (TNode i=0;i<n;i++)     // prop[][],successor[][]
        tmpSize += (outDegree[i]+inDegree[i])*sizeof(TArc);

    return tmpSize;
}


investigator *layeredAuxNetwork::NewInvestigator() const throw()
{
    return new iLayeredAuxNetwork(*this);
}


TNode layeredAuxNetwork::StartNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("StartNode",a);

    #endif

    return G.StartNode(a);
}


TNode layeredAuxNetwork::EndNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("EndNode",a);

    #endif

    return G.EndNode(a);
}


TArc layeredAuxNetwork::First(TNode v) const throw(ERRange,ERRejected)
{
    Error(ERR_REJECTED,"First","Not implemented");
    throw ERRejected();
}


TArc layeredAuxNetwork::Right(TArc a,TNode u) const throw(ERRange,ERRejected)
{
    Error(ERR_REJECTED,"Right","Not implemented");
    throw ERRejected();
}


TCap layeredAuxNetwork::UCap(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("UCap",a);

    #endif

    if (!Blocking(2*(a>>1))) return (TCap)G.ResCap(2*(a>>1));
    if (!Blocking(2*(a>>1)+1)) return (TCap)G.ResCap(2*(a>>1)+1);

    return 0;
}


bool layeredAuxNetwork::Blocking(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Blocking",a);

    #endif

    return
           G.ResCap(a)==0
        || Blocked(G.StartNode(a))
        ||  G.Dist(G.StartNode(a))!=G.Dist(G.EndNode(a))-1;
}


char layeredAuxNetwork::Orientation(TArc a) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("Orientation",a);

    #endif

    if (!Blocking(2*(a>>1))) return 1;
    if (!Blocking(2*(a>>1)+1)) return 2;

    return 0;
}


TFloat layeredAuxNetwork::Dist(TNode v) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Dist",v);

    #endif

    return G.Dist(v);
}


void layeredAuxNetwork::Phase1() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Phase==1) Error(ERR_REJECTED,"Phase1","Already in phase 1");

    #endif

    Phase = 1;
}


void layeredAuxNetwork::InsertProp(TArc a) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("InsertProp",a);

    if (Phase==2) Error(ERR_REJECTED,"InsertProp","Inapplicable in phase 2");

    #endif

    TNode u = EndNode(a);
    TNode v = StartNode(a);

    inDegree[u]++;
    prop[u] = (TArc*)GoblinRealloc(prop[u],sizeof(TArc)*(int)inDegree[u]);
    prop[u][inDegree[u]-1] = a;

    outDegree[v]++;
    successor[v] = (TArc*)GoblinRealloc(successor[v],sizeof(TArc)*(int)outDegree[v]);
    successor[v][outDegree[v]-1] = a;
}


void layeredAuxNetwork::Phase2() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Phase==2) Error(ERR_REJECTED,"Phase2","Already in phase 2");

    #endif

    for (TNode i=0;i<n;i++) currentDegree[i] = inDegree[i];

    I -> Reset();
    Phase = 2;
}


bool layeredAuxNetwork::Blocked(TNode v) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=n) NoSuchNode("Blocked",v);

    #endif

    if (v==s) return false;
    else return (currentDegree[v]==0);
}


TFloat layeredAuxNetwork::FindPath(TNode t) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (t>=n) NoSuchNode("FindPath",t);

    if (Phase==1) Error(ERR_REJECTED,"FindPath","Inapplicable in phase 1");

    #endif

    TNode v = t;
    TFloat Lambda = InfFloat;

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Augmenting by path (%lu",static_cast<unsigned long>(v));
        LH = LogStart(LOG_METH2,CT.logBuffer);
    }

    #endif

    while (v!=s)
    {
        TArc a = I->Peek(v);
        TNode u = StartNode(a);

        while (Blocked(u) || G.ResCap(a)==0)
        {
            I -> Read(v);
            a = I->Peek(v);
            u = StartNode(a);
        }

        pred[v] = a;
        v = u;

        if (G.ResCap(a)<Lambda) Lambda = G.ResCap(a);

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(v));
            LogAppend(LH,CT.logBuffer);
        }

        #endif
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH,")");

    #endif

    if (Lambda<=0) InternalError("FindPath","Path has no residual capacity");

    return Lambda;
}


void layeredAuxNetwork::TopErasure(TArc a) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("TopErasure",a);

    if (Phase==1) Error(ERR_REJECTED,"TopErasure","Inapplicable in phase 1");

    #endif

    TNode x = EndNode(a);
    I -> Read(x);

    currentDegree[x]--;

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Node %lu has indegree %lu",
            static_cast<unsigned long>(x),
            static_cast<unsigned long>(currentDegree[x]));
        LogEntry(LOG_METH2,CT.logBuffer);
    }

    #endif

    if (currentDegree[x]==0)
    {
        blocked -> Insert(x);
        while (!(blocked -> Empty()))
        {
            x = blocked->Delete();

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Deleting node %lu...",
                    static_cast<unsigned long>(x));
                LogEntry(LOG_METH2,CT.logBuffer);
                OpenFold();
            }

            #endif

            for (TArc i=0;i<outDegree[x];i++)
            {
                TArc aa = successor[x][i];
                TNode y = EndNode(aa);

                if (G.ResCap(aa)>0)
                {
                    currentDegree[y]--;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"Node %lu has indegree %lu",
                            static_cast<unsigned long>(y),
                            static_cast<unsigned long>(currentDegree[y]));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    if (currentDegree[y]==0) blocked -> Insert(y);
                }
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1) CloseFold();

            #endif
        }
    }
}
