
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   sparseBigraph.cpp
/// \brief  Constructor methods for sparse bipartite graph objects

#include "sparseBigraph.h"


sparseBiGraph::sparseBiGraph(TNode _n1,TNode _n2,goblinController& _CT) throw() :
    managedObject(_CT),
    abstractBiGraph(_n1,_n2,TArc(0)),
    X(static_cast<const sparseBiGraph&>(*this))
{
    X.SetCDemand(1);

    LogEntry(LOG_MEM,"...Sparse bigraph instanciated");
}


sparseBiGraph::sparseBiGraph(const char* fileName,goblinController& _CT) throw(ERFile,ERParse) :
    managedObject(_CT),
    abstractBiGraph(TNode(0),TNode(0),TArc(0)),
    X(static_cast<const sparseBiGraph&>(*this))
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    LogEntry(LOG_IO,"Loading bigraph...");
    if (!CT.logIO && CT.logMem) LogEntry(LOG_MEM,"Loading bigraph...");

    goblinImport F(fileName,CT);

    CT.sourceNodeInFile = CT.targetNodeInFile = CT.rootNodeInFile = NoNode;

    F.Scan("bigraph");
    ReadAllData(F);

    SetSourceNode((CT.sourceNodeInFile<n) ? CT.sourceNodeInFile : NoNode);
    SetTargetNode((CT.targetNodeInFile<n) ? CT.targetNodeInFile : NoNode);
    SetRootNode  ((CT.rootNodeInFile  <n) ? CT.rootNodeInFile   : NoNode);

    for (TArc a=0;a<m;a++)
    {
        if (StartNode(2*a)>=n1 && EndNode(2*a)<n1)
        {
            X.FlipArc(2*a);
        }
        else if (StartNode(2*a)>=n1 || EndNode(2*a)<n1)
        {
            Error(ERR_PARSE,"sparseBiGraph",
                "End nodes must be in different partitions");
        }
    }

    int l = strlen(fileName)-4;
    char* tmpLabel = new char[l+1];
    memcpy(tmpLabel,fileName,l);
    tmpLabel[l] = 0;
    SetLabel(tmpLabel);
    delete[] tmpLabel;
    CT.SetMaster(Handle());

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif
}


sparseBiGraph::sparseBiGraph(abstractBiGraph &G,TOption options) throw() :
    managedObject(G.Context()),
    abstractBiGraph(G.N1(),G.N2(),TArc(0)),
    X(static_cast<const sparseBiGraph&>(*this))
{
    X.SetCapacity(G.N(),G.M(),G.N()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    TIndex* originalArc = (options & OPT_MAPPINGS) ? new TIndex[G.M()] : NULL;

    if (options & OPT_CLONE)
    {
        for (TNode u=0;u<n;u++)
        {
            X.SetDemand(u,G.Demand(u));

            for (TDim i=0;i<G.Dim();i++) X.SetC(u,i,G.C(u,i));
        }

        for (TArc a=0;a<G.M();a++)
        {
            TNode u = G.StartNode(2*a);
            TNode v = G.EndNode(2*a);
            TCap tmpCap = (options & OPT_SUB) ? TCap(G.Sub(2*a)) : G.UCap(2*a);

            TArc a1 = InsertArc(u,v,tmpCap,G.Length(2*a),G.LCap(2*a));

            if (originalArc) originalArc[a1] = 2*a;
        }

        for (TNode u=0;u<n;u++)
        {
            TArc a = G.First(u);

            if (a==NoArc) continue;

            do
            {
                TArc a2 = G.Right(a,u);
                X.SetRight(a,a2);
                a = a2;
            }
            while (a!=G.First(u));

            X.SetFirst(u,a);
        }

        if (G.ExteriorArc()!=NoArc)
        {
            face = new TNode[2*m];

            for (TArc i=0;i<2*m;i++) face[i] = G.Face(i);

            SetExteriorArc(G.ExteriorArc());
        }

        LogEntry(LOG_MEM,"...Bigraph clone generated");
    }
    else
    {
        LogEntry(LOG_MAN,"Computing underlying bigraph...");

        TNode *adjacent = new TNode[n];

        for (TNode w=0;w<n;w++) adjacent[w] = NoNode;

        THandle H = G.Investigate();
        investigator &I = G.Investigator(H);

        for (TNode u=0;u<n;u++)
        {
            X.SetDemand(u,G.Demand(u));

            for (TDim i=0;i<G.Dim();i++) X.SetC(u,i,G.C(u,i));

            if (u<n1)
            {
                while (I.Active(u))
                {
                    TArc a = I.Read(u);
                    TNode v = G.EndNode(a);
                    TCap tmpCap = (options & OPT_SUB) ? TCap(G.Sub(a)) : G.UCap(a);

                    if (tmpCap>0 && (adjacent[v]!=u || (options & OPT_PARALLELS)))
                    {
                        TArc a1 = InsertArc(u,v,tmpCap,G.Length(a),G.LCap(a));
                        adjacent[v] = u;

                        if (originalArc) originalArc[a1] = a;
                    }
                }
            }
        }

        G.Close(H);

        delete adjacent;

        X.SetCapacity(N(),M(),L());
    }

    if (options & OPT_MAPPINGS)
    {
        TIndex* originalArcExport = registers.RawArray<TIndex>(*this,TokRegOriginalArc);
        memcpy(originalArcExport,originalArc,sizeof(TIndex)*m);
        delete[] originalArc;
    }

    if (CT.traceLevel==2) Display();
}


unsigned long sparseBiGraph::Size() const throw()
{
    return
          sizeof(sparseBiGraph)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractGraph::Allocated()
        + abstractBiGraph::Allocated();
}


unsigned long sparseBiGraph::Allocated() const throw()
{
    return 0;
}


TNode sparseBiGraph::SwapNode(TNode u) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("SwapNode",u);

    if (First(u)!=NoArc)
        Error(ERR_REJECTED,"SwapNode","Node must be isolated");

    #endif

    TNode v = n1;

    if (u<n1)
    {
        n1--;
        v = n1;
    }
    else
    {
        n1++;
    }

    if (u!=v) X.SwapNodes(u,v);

    return v;
}


sparseBiGraph::~sparseBiGraph() throw()
{
    LogEntry(LOG_MEM,"...Sparse bigraph disallocated");

    if (CT.traceLevel==2) Display();
}


inducedBigraph::inducedBigraph(abstractMixedGraph& G,
    const indexSet<TNode>& U,const indexSet<TNode>& V,
    const TOption options) throw(ERRejected) :
    managedObject(G.Context()),
    sparseBiGraph(TNode(1),TNode(0),G.Context())
{
    X.SetCapacity(G.N(),G.M(),G.N()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    TIndex* originalNode = (options & OPT_MAPPINGS) ? new TIndex[G.N()] : NULL;
    TIndex* originalArc  = (options & OPT_MAPPINGS) ? new TIndex[G.M()] : NULL;

    TNode* mapNodes = new TNode[G.N()];

    for (TNode v=0;v<G.N();v++) mapNodes[v] = NoNode;

    bool first = true;

    for (TNode u=U.First();u<G.N();u=U.Successor(u))
    {
        if (V.IsMember(u))
            Error(ERR_REJECTED,"inducedBigraph","Node sets must be disjoint");

        if (!first) InsertNode();
        else first = false;

        TNode u2 = mapNodes[u] = n-1;
        if (originalNode) originalNode[u2] = u;

        X.SetDemand(n-1,G.Demand(u));
        for (TDim i=0;i<G.Dim();i++) X.SetC(n-1,i,G.C(u,i));
    }

    n1 = n;

    for (TNode u=V.First();u<G.N();u=V.Successor(u))
    {
        if (!first) InsertNode();
        else first = false;

        TNode u2 = mapNodes[u] = n-1;
        if (originalNode) originalNode[u2] = u;

        X.SetDemand(n-1,G.Demand(u));
        for (TDim i=0;i<G.Dim();i++) X.SetC(n-1,i,G.C(u,i));
    }

    goblinHashTable<TArc,TArc>* Adj = NULL;

    if (!(options & OPT_PARALLELS))
        Adj = new goblinHashTable<TArc,TArc>(n*n,G.M(),NoArc,CT);

    for (TArc a=0;a<2*G.M();a++)
    {
        TNode u = G.StartNode(a);
        TNode v = G.EndNode(a);
        TNode u2 = mapNodes[u];
        TNode v2 = mapNodes[v];

        if (!U.IsMember(u) || !V.IsMember(v)) continue;

        if (u==v && (options & OPT_NO_LOOPS)) continue;

        TCap tmpCap = InfCap;

        if (options & OPT_SUB)
            tmpCap = TCap(G.Sub(a));
        else tmpCap = G.UCap(a);

        if (tmpCap<=0) continue;

        TFloat l = G.Length(a);

        if (options & OPT_PARALLELS)
        {
            TArc a1 = InsertArc(u2,v2,tmpCap,l,G.LCap(a));

            if (originalArc) originalArc[a1] = 2*a;

            continue;
        }

        TArc j1 = n*u2+v2;
        TArc a1 = Adj -> Key(j1);

        if (a1!=NoArc)
        {
            if (Length(2*a1)>l)
            {
                X.SetLength(2*a1,l);
                X.SetUCap(2*a1,tmpCap);
                X.SetLCap(2*a1,G.LCap(a));
            }

            continue;
        }

        TArc j2 = n*v2+u2;
        TArc a2 = Adj -> Key(j2);

        if (G.Orientation(a)==0 && a2!=NoArc)
        {
            if (Length(2*a2)>l)
            {
                X.SetLength(2*a2,l);
                X.SetUCap(2*a2,tmpCap);
                X.SetLCap(2*a2,G.LCap(a));
            }

            continue;
        }

        a1 = InsertArc(u2,v2,tmpCap,l,G.LCap(a));
        Adj -> ChangeKey(j1,a1);

        if (originalArc) originalArc[a1] = 2*a;
    }

    delete[] mapNodes;

    if (Adj) delete Adj;

    X.SetCapacity(N(),M(),L());

    if (options & OPT_MAPPINGS)
    {
        TIndex* originalNodeExport = registers.RawArray<TIndex>(*this,TokRegOriginalNode);
        TIndex* originalArcExport  = registers.RawArray<TIndex>(*this,TokRegOriginalArc);

        memcpy(originalNodeExport,originalNode,sizeof(TIndex)*n);
        memcpy(originalArcExport ,originalArc, sizeof(TIndex)*m);

        delete[] originalNode;
        delete[] originalArc;
    }

    LogEntry(LOG_MEM,"...Induced bigraph instanciated");
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   sparseBigraph.cpp
/// \brief  Constructor methods for sparse bipartite graph objects

#include "sparseBigraph.h"


sparseBiGraph::sparseBiGraph(TNode _n1,TNode _n2,goblinController& _CT) throw() :
    managedObject(_CT),
    abstractBiGraph(_n1,_n2,TArc(0)),
    X(static_cast<const sparseBiGraph&>(*this))
{
    X.SetCDemand(1);

    LogEntry(LOG_MEM,"...Sparse bigraph instanciated");
}


sparseBiGraph::sparseBiGraph(const char* fileName,goblinController& _CT) throw(ERFile,ERParse) :
    managedObject(_CT),
    abstractBiGraph(TNode(0),TNode(0),TArc(0)),
    X(static_cast<const sparseBiGraph&>(*this))
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    LogEntry(LOG_IO,"Loading bigraph...");
    if (!CT.logIO && CT.logMem) LogEntry(LOG_MEM,"Loading bigraph...");

    goblinImport F(fileName,CT);

    CT.sourceNodeInFile = CT.targetNodeInFile = CT.rootNodeInFile = NoNode;

    F.Scan("bigraph");
    ReadAllData(F);

    SetSourceNode((CT.sourceNodeInFile<n) ? CT.sourceNodeInFile : NoNode);
    SetTargetNode((CT.targetNodeInFile<n) ? CT.targetNodeInFile : NoNode);
    SetRootNode  ((CT.rootNodeInFile  <n) ? CT.rootNodeInFile   : NoNode);

    for (TArc a=0;a<m;a++)
    {
        if (StartNode(2*a)>=n1 && EndNode(2*a)<n1)
        {
            X.FlipArc(2*a);
        }
        else if (StartNode(2*a)>=n1 || EndNode(2*a)<n1)
        {
            Error(ERR_PARSE,"sparseBiGraph",
                "End nodes must be in different partitions");
        }
    }

    int l = strlen(fileName)-4;
    char* tmpLabel = new char[l+1];
    memcpy(tmpLabel,fileName,l);
    tmpLabel[l] = 0;
    SetLabel(tmpLabel);
    delete[] tmpLabel;
    CT.SetMaster(Handle());

    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Disable();

    #endif
}


sparseBiGraph::sparseBiGraph(abstractBiGraph &G,TOption options) throw() :
    managedObject(G.Context()),
    abstractBiGraph(G.N1(),G.N2(),TArc(0)),
    X(static_cast<const sparseBiGraph&>(*this))
{
    X.SetCapacity(G.N(),G.M(),G.N()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    TIndex* originalArc = (options & OPT_MAPPINGS) ? new TIndex[G.M()] : NULL;

    if (options & OPT_CLONE)
    {
        for (TNode u=0;u<n;u++)
        {
            X.SetDemand(u,G.Demand(u));

            for (TDim i=0;i<G.Dim();i++) X.SetC(u,i,G.C(u,i));
        }

        for (TArc a=0;a<G.M();a++)
        {
            TNode u = G.StartNode(2*a);
            TNode v = G.EndNode(2*a);
            TCap tmpCap = (options & OPT_SUB) ? TCap(G.Sub(2*a)) : G.UCap(2*a);

            TArc a1 = InsertArc(u,v,tmpCap,G.Length(2*a),G.LCap(2*a));

            if (originalArc) originalArc[a1] = 2*a;
        }

        for (TNode u=0;u<n;u++)
        {
            TArc a = G.First(u);

            if (a==NoArc) continue;

            do
            {
                TArc a2 = G.Right(a,u);
                X.SetRight(a,a2);
                a = a2;
            }
            while (a!=G.First(u));

            X.SetFirst(u,a);
        }

        if (G.ExteriorArc()!=NoArc)
        {
            face = new TNode[2*m];

            for (TArc i=0;i<2*m;i++) face[i] = G.Face(i);

            SetExteriorArc(G.ExteriorArc());
        }

        LogEntry(LOG_MEM,"...Bigraph clone generated");
    }
    else
    {
        LogEntry(LOG_MAN,"Computing underlying bigraph...");

        TNode *adjacent = new TNode[n];

        for (TNode w=0;w<n;w++) adjacent[w] = NoNode;

        THandle H = G.Investigate();
        investigator &I = G.Investigator(H);

        for (TNode u=0;u<n;u++)
        {
            X.SetDemand(u,G.Demand(u));

            for (TDim i=0;i<G.Dim();i++) X.SetC(u,i,G.C(u,i));

            if (u<n1)
            {
                while (I.Active(u))
                {
                    TArc a = I.Read(u);
                    TNode v = G.EndNode(a);
                    TCap tmpCap = (options & OPT_SUB) ? TCap(G.Sub(a)) : G.UCap(a);

                    if (tmpCap>0 && (adjacent[v]!=u || (options & OPT_PARALLELS)))
                    {
                        TArc a1 = InsertArc(u,v,tmpCap,G.Length(a),G.LCap(a));
                        adjacent[v] = u;

                        if (originalArc) originalArc[a1] = a;
                    }
                }
            }
        }

        G.Close(H);

        delete adjacent;

        X.SetCapacity(N(),M(),L());
    }

    if (options & OPT_MAPPINGS)
    {
        TIndex* originalArcExport = registers.RawArray<TIndex>(*this,TokRegOriginalArc);
        memcpy(originalArcExport,originalArc,sizeof(TIndex)*m);
        delete[] originalArc;
    }

    if (CT.traceLevel==2) Display();
}


unsigned long sparseBiGraph::Size() const throw()
{
    return
          sizeof(sparseBiGraph)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractGraph::Allocated()
        + abstractBiGraph::Allocated();
}


unsigned long sparseBiGraph::Allocated() const throw()
{
    return 0;
}


TNode sparseBiGraph::SwapNode(TNode u) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=n) NoSuchNode("SwapNode",u);

    if (First(u)!=NoArc)
        Error(ERR_REJECTED,"SwapNode","Node must be isolated");

    #endif

    TNode v = n1;

    if (u<n1)
    {
        n1--;
        v = n1;
    }
    else
    {
        n1++;
    }

    if (u!=v) X.SwapNodes(u,v);

    return v;
}


sparseBiGraph::~sparseBiGraph() throw()
{
    LogEntry(LOG_MEM,"...Sparse bigraph disallocated");

    if (CT.traceLevel==2) Display();
}


inducedBigraph::inducedBigraph(abstractMixedGraph& G,
    const indexSet<TNode>& U,const indexSet<TNode>& V,
    const TOption options) throw(ERRejected) :
    managedObject(G.Context()),
    sparseBiGraph(TNode(1),TNode(0),G.Context())
{
    X.SetCapacity(G.N(),G.M(),G.N()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    TIndex* originalNode = (options & OPT_MAPPINGS) ? new TIndex[G.N()] : NULL;
    TIndex* originalArc  = (options & OPT_MAPPINGS) ? new TIndex[G.M()] : NULL;

    TNode* mapNodes = new TNode[G.N()];

    for (TNode v=0;v<G.N();v++) mapNodes[v] = NoNode;

    bool first = true;

    for (TNode u=U.First();u<G.N();u=U.Successor(u))
    {
        if (V.IsMember(u))
            Error(ERR_REJECTED,"inducedBigraph","Node sets must be disjoint");

        if (!first) InsertNode();
        else first = false;

        TNode u2 = mapNodes[u] = n-1;
        if (originalNode) originalNode[u2] = u;

        X.SetDemand(n-1,G.Demand(u));
        for (TDim i=0;i<G.Dim();i++) X.SetC(n-1,i,G.C(u,i));
    }

    n1 = n;

    for (TNode u=V.First();u<G.N();u=V.Successor(u))
    {
        if (!first) InsertNode();
        else first = false;

        TNode u2 = mapNodes[u] = n-1;
        if (originalNode) originalNode[u2] = u;

        X.SetDemand(n-1,G.Demand(u));
        for (TDim i=0;i<G.Dim();i++) X.SetC(n-1,i,G.C(u,i));
    }

    goblinHashTable<TArc,TArc>* Adj = NULL;

    if (!(options & OPT_PARALLELS))
        Adj = new goblinHashTable<TArc,TArc>(n*n,G.M(),NoArc,CT);

    for (TArc a=0;a<2*G.M();a++)
    {
        TNode u = G.StartNode(a);
        TNode v = G.EndNode(a);
        TNode u2 = mapNodes[u];
        TNode v2 = mapNodes[v];

        if (!U.IsMember(u) || !V.IsMember(v)) continue;

        if (u==v && (options & OPT_NO_LOOPS)) continue;

        TCap tmpCap = InfCap;

        if (options & OPT_SUB)
            tmpCap = TCap(G.Sub(a));
        else tmpCap = G.UCap(a);

        if (tmpCap<=0) continue;

        TFloat l = G.Length(a);

        if (options & OPT_PARALLELS)
        {
            TArc a1 = InsertArc(u2,v2,tmpCap,l,G.LCap(a));

            if (originalArc) originalArc[a1] = 2*a;

            continue;
        }

        TArc j1 = n*u2+v2;
        TArc a1 = Adj -> Key(j1);

        if (a1!=NoArc)
        {
            if (Length(2*a1)>l)
            {
                X.SetLength(2*a1,l);
                X.SetUCap(2*a1,tmpCap);
                X.SetLCap(2*a1,G.LCap(a));
            }

            continue;
        }

        TArc j2 = n*v2+u2;
        TArc a2 = Adj -> Key(j2);

        if (G.Orientation(a)==0 && a2!=NoArc)
        {
            if (Length(2*a2)>l)
            {
                X.SetLength(2*a2,l);
                X.SetUCap(2*a2,tmpCap);
                X.SetLCap(2*a2,G.LCap(a));
            }

            continue;
        }

        a1 = InsertArc(u2,v2,tmpCap,l,G.LCap(a));
        Adj -> ChangeKey(j1,a1);

        if (originalArc) originalArc[a1] = 2*a;
    }

    delete[] mapNodes;

    if (Adj) delete Adj;

    X.SetCapacity(N(),M(),L());

    if (options & OPT_MAPPINGS)
    {
        TIndex* originalNodeExport = registers.RawArray<TIndex>(*this,TokRegOriginalNode);
        TIndex* originalArcExport  = registers.RawArray<TIndex>(*this,TokRegOriginalArc);

        memcpy(originalNodeExport,originalNode,sizeof(TIndex)*n);
        memcpy(originalArcExport ,originalArc, sizeof(TIndex)*m);

        delete[] originalNode;
        delete[] originalArc;
    }

    LogEntry(LOG_MEM,"...Induced bigraph instanciated");
}
