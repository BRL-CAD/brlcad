
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   mixedGraph.cpp
/// \brief  Constructor methods for mixed graph objects

#include "mixedGraph.h"


mixedGraph::mixedGraph(TNode _n,goblinController& _CT) throw() :
    managedObject(_CT),
    abstractMixedGraph(_n,TArc(0)),
    X(static_cast<const mixedGraph&>(*this))
{
    X.SetCDemand(0);

    LogEntry(LOG_MEM,"...Mixed graph instanciated");
}


mixedGraph::mixedGraph(const char* fileName,goblinController& _CT)
    throw(ERFile,ERParse) : managedObject(_CT),
    abstractMixedGraph(TNode(0),TArc(0)),
    X(static_cast<const mixedGraph&>(*this))
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    LogEntry(LOG_IO,"Loading mixed graph...");
    if (!CT.logIO && CT.logMem) LogEntry(LOG_MEM,"Loading mixed graph...");

    goblinImport F(fileName,CT);

    CT.sourceNodeInFile = CT.targetNodeInFile = CT.rootNodeInFile = NoNode;

    F.Scan("mixed");
    ReadAllData(F);

    SetSourceNode((CT.sourceNodeInFile<n) ? CT.sourceNodeInFile : NoNode);
    SetTargetNode((CT.targetNodeInFile<n) ? CT.targetNodeInFile : NoNode);
    SetRootNode  ((CT.rootNodeInFile  <n) ? CT.rootNodeInFile   : NoNode);

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


mixedGraph::mixedGraph(abstractMixedGraph& G,TOption options) throw() :
    managedObject(G.Context()), abstractMixedGraph(G.N(),TArc(0)), X(*this)
{
    X.SetCapacity(G.N(),2*G.M(),G.N()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    TIndex* originalArc = (options & OPT_MAPPINGS) ? new TIndex[2*G.M()] : NULL;

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

        if (originalArc) originalArc[a1] = a;

        X.SetOrientation(2*a,G.Orientation(2*a));
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

    if (options & OPT_MAPPINGS)
    {
        TIndex* originalArcExport = registers.RawArray<TIndex>(*this,TokRegOriginalArc);
        memcpy(originalArcExport,originalArc,sizeof(TIndex)*m);
        delete[] originalArc;
    }

    LogEntry(LOG_MEM,"...Mixed graph clone generated");

    if (CT.traceLevel==2) Display();
}


unsigned long mixedGraph::Size() const throw()
{
    return 
          sizeof(mixedGraph)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated();
}


unsigned long mixedGraph::Allocated() const throw()
{
    return 0;
}


mixedGraph::~mixedGraph() throw()
{
    LogEntry(LOG_MEM,"...Mixed graph disallocated");

    if (CT.traceLevel==2) Display();
}


lineGraph::lineGraph(abstractMixedGraph &G,TOption options) throw(ERRejected) :
    managedObject(G.Context()),
    mixedGraph(G.M(),G.Context())
{
    #if defined(_FAILSAVE_)

    if (G.M()>=CT.MaxNode())
        Error(ERR_REJECTED,"lineGraph","Number of arcs is out of range");

    #endif

    LogEntry(LOG_MAN,"Generating line graph...");

    // X.SetCapacity(G.M(),G.M()*(G.M()-1)/2,G.M()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    for (TDim i=0;i<G.Dim();i++)
    {
        for (TNode v=0;v<G.M();v++)
        {
            X.SetC(v,i,(G.C(G.StartNode((TArc)2*v),i)+G.C(G.EndNode((TArc)2*v),i))/2);
        }
    }

    for (TNode v=0;v<G.N();v++)
    {
        TArc a1 = G.First(v);

        if (a1==NoArc) continue;

        do
        {
            TArc a2 = G.First(v);

            do
            {
                if (a1==a2)
                {
                    // Skip equal arcs
                }
                else if (   (G.Blocking(a2) || G.Blocking(a1^1))
                         && (options & LG_DIRECTED)
                        )
                {
                    // Do nothing since arc directions are incompatible
                }
                else if (   !(G.Blocking(a1) || G.Blocking(a2^1))
                         || !(options & LG_DIRECTED)
                        )
                {
                    // Add an undirected edge, but prevent from adding it twice
                    if (a1<a2)
                    {
                        TArc a = InsertArc((TNode)a1>>1,(TNode)a2>>1);
                        X.SetOrientation(2*a,0);
                    }
                }
                else
                {
                    // Add a directed arc
                    TArc a = InsertArc((TNode)a1>>1,(TNode)a2>>1);
                    X.SetOrientation(2*a,1);
                }

                a2 = G.Right(a2,v);
            }
            while (a2!=G.First(v));

            a1 = G.Right(a1,v);
        }
        while (a1!=G.First(v));
    }

    X.SetCapacity(N(),M(),L());

    if (CT.traceLevel==2) Display();
}


inducedSubgraph::inducedSubgraph(abstractMixedGraph& G,
    const indexSet<TNode>& V,const TOption options) throw(ERRejected) :
    managedObject(G.Context()),
    mixedGraph(TNode(1),G.Context())
{
    X.SetCapacity(G.N(),G.M(),G.N()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    TIndex* originalNode = (options & OPT_MAPPINGS) ? new TIndex[G.N()] : NULL;
    TIndex* originalArc  = (options & OPT_MAPPINGS) ? new TIndex[G.M()] : NULL;

    TNode* mapNodes = new TNode[G.N()];
    for (TNode v=0;v<G.N();v++) mapNodes[v] = NoNode;

    bool first = true;

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
        Adj = new goblinHashTable<TArc,TArc>(2*n*n,G.M(),NoArc,CT);

    for (TArc a=0;a<G.M();a++)
    {
        TNode u = G.StartNode(2*a);
        TNode v = G.EndNode(2*a);

        if (!(V.IsMember(u) && V.IsMember(v))) continue;

        if (u==v && (options & OPT_NO_LOOPS)) continue;

        TNode u2 = mapNodes[u];
        TNode v2 = mapNodes[v];

        TCap tmpCap = InfCap;

        if (options & OPT_SUB)
            tmpCap = TCap(G.Sub(2*a));
        else tmpCap = G.UCap(2*a);

        if (tmpCap<=0) continue;

        TFloat l = G.Length(2*a);

        if (options & OPT_PARALLELS)
        {
            TArc a1 = InsertArc(u2,v2,tmpCap,l,G.LCap(2*a));
            X.SetOrientation(2*a1,G.Orientation(2*a));

            if (originalArc) originalArc[a1] = 2*a;

            continue;
        }

        TArc j1 = 2*(n*u2+v2)+G.Orientation(2*a);
        TArc a1 = Adj -> Key(j1);

        if (a1!=NoArc)
        {
            if (Length(2*a1)>l)
            {
                X.SetLength(2*a1,l);
                X.SetUCap(2*a1,tmpCap);
                X.SetLCap(2*a1,G.LCap(2*a));
            }

            continue;
        }

        TArc j2 = 2*(n*v2+u2)+G.Orientation(2*a);
        TArc a2 = Adj -> Key(j2);

        if (G.Orientation(2*a)==0 && a2!=NoArc)
        {
            if (Length(2*a2)>l)
            {
                X.SetLength(2*a2,l);
                X.SetUCap(2*a2,tmpCap);
                X.SetLCap(2*a2,G.LCap(2*a));
            }

            continue;
        }

        a1 = InsertArc(u2,v2,tmpCap,l,G.LCap(2*a));
        X.SetOrientation(2*a1,G.Orientation(2*a));
        Adj -> ChangeKey(j1,a1);

        if (originalArc) originalArc[a1] = 2*a;
    }

    delete[] mapNodes;
    if (Adj!=NULL) delete Adj;

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

    LogEntry(LOG_MEM,"...Induced subgraph instanciated");
}


inducedSubgraph::inducedSubgraph(abstractMixedGraph& G,
    const indexSet<TNode>& V,const indexSet<TArc>& A,
    const TOption options) throw(ERRejected) :
    managedObject(G.Context()),
    mixedGraph(TNode(1),G.Context())
{
    X.SetCapacity(G.N(),G.M(),G.N()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    TIndex* originalNode = (options & OPT_MAPPINGS) ? new TIndex[G.N()] : NULL;
    TIndex* originalArc  = (options & OPT_MAPPINGS) ? new TIndex[G.M()] : NULL;

    TNode* mapNodes = new TNode[G.N()];
    for (TNode v=0;v<G.N();v++) mapNodes[v] = NoNode;

    bool first = true;

    for (TArc a=A.First();a<G.M();a=A.Successor(a))
    {
        TNode u = G.StartNode(2*a);
        TNode v = G.EndNode(2*a);

        if (u==NoNode || v==NoNode) continue; // Cancelled arc

        TNode u2 = mapNodes[u];

        if (u2==NoNode)
        {
            if (!first) InsertNode();
            else first = false;

            u2 = mapNodes[u] = n-1;
            if (originalNode) originalNode[u2] = u;

            X.SetDemand(n-1,G.Demand(u));

            for (TDim i=0;i<G.Dim();i++) X.SetC(n-1,i,G.C(u,i));
        }

        TNode v2 = mapNodes[v];

        if (v2==NoNode)
        {
            InsertNode();

            v2 = mapNodes[v] = n-1;
            if (originalNode) originalNode[v2] = v;

            X.SetDemand(n-1,G.Demand(v));

            for (TDim i=0;i<G.Dim();i++) X.SetC(n-1,i,G.C(v,i));
        }
    }

    goblinHashTable<TArc,TArc>* Adj = NULL;

    if (!(options & OPT_PARALLELS))
        Adj = new goblinHashTable<TArc,TArc>(2*n*n,G.M(),NoArc,CT);

    for (TArc a=A.First();a<G.M();a=A.Successor(a))
    {
        TNode u = G.StartNode(2*a);
        TNode v = G.EndNode(2*a);

        if (u==NoNode || v==NoNode) continue; // Cancelled arc

        if (!(V.IsMember(u) && V.IsMember(v))) continue;

        if (u==v && (options & OPT_NO_LOOPS)) continue;

        TNode u2 = mapNodes[u];
        TNode v2 = mapNodes[v];

        TCap tmpCap = InfCap;

        if (options & OPT_SUB)
            tmpCap = TCap(G.Sub(2*a));
        else tmpCap = G.UCap(2*a);

        if (tmpCap<=0) continue;

        TFloat l = G.Length(2*a);

        if (options & OPT_PARALLELS)
        {
            TArc a1 = InsertArc(u2,v2,tmpCap,l,G.LCap(2*a));
            X.SetOrientation(2*a1,G.Orientation(2*a));

            if (originalArc) originalArc[a1] = 2*a;

            continue;
        }

        TArc j1 = 2*(n*u2+v2)+G.Orientation(2*a);
        TArc a1 = Adj -> Key(j1);

        if (a1!=NoArc)
        {
            if (Length(2*a1)>l)
            {
                X.SetLength(2*a1,l);
                X.SetUCap(2*a1,tmpCap);
                X.SetLCap(2*a1,G.LCap(2*a));
            }

            continue;
        }

        TArc j2 = 2*(n*v2+u2)+G.Orientation(2*a);
        TArc a2 = Adj -> Key(j2);

        if (G.Orientation(2*a)==0 && a2!=NoArc)
        {
            if (Length(2*a2)>l)
            {
                X.SetLength(2*a2,l);
                X.SetUCap(2*a2,tmpCap);
                X.SetLCap(2*a2,G.LCap(2*a));
            }

            continue;
        }

        a1 = InsertArc(u2,v2,tmpCap,l,G.LCap(2*a));
        X.SetOrientation(2*a1,G.Orientation(2*a));
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

    LogEntry(LOG_MEM,"...Induced subgraph instanciated");
}


colourContraction::colourContraction(abstractMixedGraph& G,
    const TOption options) throw() :
    managedObject(G.Context()),
    mixedGraph(TNode(1),G.Context())
{
    LogEntry(LOG_MAN,"Contracting colours...");

    X.SetCapacity(G.N(),G.M(),G.N()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    TNode* mapColour = new TNode[G.N()];
    for (TNode v=0;v<G.N();v++) mapColour[v] = NoNode;

    TNode j = 0;
    for (TNode v=0;v<G.N();v++)
    {
        if (mapColour[G.NodeColour(v)]==NoNode)
        {
            if (j>0) InsertNode();

            mapColour[G.NodeColour(v)] = j++;

            for (TDim i=0;i<G.Dim();++i)
            {
                X.SetC(mapColour[G.NodeColour(v)],i,G.C(v,i));
            }
        }
    }

    for (TNode v=0;v<G.N();++v)
    {
        SetNodeColour(mapColour[G.NodeColour(v)],G.NodeColour(v));
    }

    goblinHashTable<TArc,TArc>* Adj = NULL;

    if (!(options & OPT_PARALLELS))
        Adj = new goblinHashTable<TArc,TArc>(2*n*n,G.M(),NoArc,CT);

    for (TArc a=0;a<G.M();a++)
    {
        TNode u = G.StartNode(2*a);
        TNode v = G.EndNode(2*a);
        TNode u2 = mapColour[G.NodeColour(u)];
        TNode v2 = mapColour[G.NodeColour(v)];

        if (u2==v2) continue;

        TCap tmpCap = InfCap;

        if (options & OPT_SUB)
            tmpCap = TCap(G.Sub(2*a));
        else tmpCap = G.UCap(2*a);

        if (tmpCap<=0) continue;

        TFloat l = G.Length(2*a);

        if (options & OPT_PARALLELS)
        {
            TArc a1 = InsertArc(u2,v2,tmpCap,l,G.LCap(2*a));
            X.SetOrientation(2*a1,G.Orientation(2*a));
            continue;
        }

        TArc j1 = 2*(n*u2+v2)+G.Orientation(2*a);
        TArc a1 = Adj -> Key(j1);

        if (a1!=NoArc)
        {
            if (Length(2*a1)>l)
            {
                X.SetLength(2*a1,l);
                X.SetUCap(2*a1,tmpCap);
                X.SetLCap(2*a1,G.LCap(2*a));
            }

            continue;
        }

        TArc j2 = 2*(n*v2+u2)+G.Orientation(2*a);
        TArc a2 = Adj -> Key(j2);

        if (G.Orientation(2*a)==0 && a2!=NoArc)
        {
            if (Length(2*a2)>l)
            {
                X.SetLength(2*a2,l);
                X.SetUCap(2*a2,tmpCap);
                X.SetLCap(2*a2,G.LCap(2*a));
            }

            continue;
        }

        a1 = InsertArc(u2,v2,tmpCap,l,G.LCap(2*a));
        X.SetOrientation(2*a1,G.Orientation(2*a));
        Adj -> ChangeKey(j1,a1);
    }

    delete[] mapColour;

    if (Adj) delete Adj;

    X.SetCapacity(N(),M(),L());

    if (CT.traceLevel==2) Display();
}


explicitSurfaceGraph::explicitSurfaceGraph(abstractMixedGraph& G,
    nestedFamily<TNode>& S,TFloat* modLength,TArc* inArc) throw() :
    managedObject(G.Context()),
    mixedGraph(2*G.N(),G.Context())
{
    ImportLayoutData(G);

    TArc* pred = InitPredecessors();

    for (TArc a=0;a<G.M();a++)
    {
        TNode u = G.StartNode(2*a);
        TNode v = G.EndNode(2*a);
        TCap thisCap = 0;
        if (S.Set(u)!=S.Set(v)) thisCap = G.UCap(2*a);
        TArc a2 = NoArc;

        if (modLength)
        {
            a2 = InsertArc(S.Set(u),S.Set(v),thisCap,modLength[2*a]);
        }
        else
        {
            a2 = InsertArc(S.Set(u),S.Set(v),thisCap,0);
        }

        X.SetOrientation(2*a2,G.Orientation(2*a));
    }

    if (G.Dim()>0)
    {
        TNode v = 0;
        for (;v<n;v++)
        {
            SetNodeVisibility(v,false);
            pred[v] = inArc[v];
        }

        for (v=0;v<G.N();v++)
        {
            TNode w = S.Set(v);
            if (First(w)!=NoArc)
            {
                X.SetC(w,0,(G.C(v,0)));
                X.SetC(w,1,(G.C(v,1)));
            }
        }

        X.Layout_ArcRouting();
        X.Layout_AdoptBoundingBox(G);
    }
}


explicitSubdivision::explicitSubdivision(abstractMixedGraph& G,const TOption options) throw() :
    managedObject(G.Context()),
    mixedGraph(G.N(),G.Context())
{
    X.SetCapacity(G.N()+G.NI(),G.M()+G.NI(),G.N()+G.NI());
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    TIndex* originalNode = (options & OPT_MAPPINGS) ? new TIndex[G.N()+G.NI()] : NULL;
    TIndex* originalArc  = (options & OPT_MAPPINGS) ? new TIndex[G.M()+G.NI()] : NULL;

    for (TNode v=0;v<G.N();v++)
    {
        for (TDim i=0;i<G.Dim();i++)
        {
            X.SetC(v,i,G.C(v,i));
        }

        if (originalNode) originalNode[v] = v;
    }

    for (TArc a=0;a<G.M();a++)
    {
        TNode u = G.StartNode(2*a);
        TNode v = G.EndNode(2*a);
        TNode w = G.ArcLabelAnchor(2*a);
        TNode x = u;
        TCap thisLCap = G.LCap(2*a);
        TCap thisUCap = G.UCap(2*a);
        TCap thisLength = G.Length(2*a);
        char thisOrientation = G.Orientation(2*a);

        if (w!=NoNode)
        {
            w = G.ThreadSuccessor(w);

            while (w!=NoNode)
            {
                TNode y = InsertNode();
                TArc aNew = InsertArc(x,y,thisUCap,thisLength,thisLCap);
                X.SetOrientation(2*aNew,thisOrientation);

                for (TDim i=0;i<G.Dim();i++)
                {
                    X.SetC(y,i,G.C(w,i));
                }

                if (originalNode) originalNode[y] = w;

                if (originalArc) originalArc[aNew] = 2*a;

                x = y;
                w = G.ThreadSuccessor(w);
            }
        }
        else w = u;

        TArc aNew = InsertArc(x,v,thisUCap,thisLength,thisLCap);
        X.SetOrientation(2*aNew,thisOrientation);

        if (originalArc) originalArc[aNew] = 2*a;
    }

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
}
