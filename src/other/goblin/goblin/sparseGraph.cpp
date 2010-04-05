
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, December 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   sparseGraph.cpp
/// \brief  Constructor methods for sparse undirected graph objects

#include "sparseGraph.h"


sparseGraph::sparseGraph(TNode _n,goblinController& _CT,bool _mode) throw() :
    managedObject(_CT),
    abstractGraph(_n,TArc(0)),
    X(static_cast<const sparseGraph&>(*this))
{
    X.SetCDemand(1);
    mode = _mode;

    LogEntry(LOG_MEM,"...Sparse graph instanciated");
}


sparseGraph::sparseGraph(const char* fileName,goblinController& _CT) throw(ERFile,ERParse) :
    managedObject(_CT),
    abstractGraph(TNode(0),TArc(0)),
    X(static_cast<const sparseGraph&>(*this))
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerIO] -> Enable();

    #endif

    LogEntry(LOG_IO,"Loading graph...");

    if (!CT.logIO && CT.logMem) LogEntry(LOG_MEM,"Loading graph...");

    goblinImport F(fileName,CT);

    CT.sourceNodeInFile = CT.targetNodeInFile = CT.rootNodeInFile = NoNode;

    F.Scan("graph");
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


sparseGraph::sparseGraph(abstractMixedGraph& G,TOption options) throw() :
    managedObject(G.Context()),
    abstractGraph(G.N(),TArc(0)),
    X(static_cast<const sparseGraph&>(*this))
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

        LogEntry(LOG_MEM,"...Graph clone generated");
    }
    else
    {
        LogEntry(LOG_MAN,"Computing underlying graph...");

        TNode* adjacent = new TNode[n];
        for (TNode w=0;w<n;w++) adjacent[w] = NoNode;

        THandle H = G.Investigate();
        investigator &I = G.Investigator(H);

        for (TNode u=0;u<n;u++)
        {
            for (TDim i=0;i<G.Dim();i++) X.SetC(u,i,G.C(u,i));

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                TNode v = G.EndNode(a);
                TCap tmpCap = (options & OPT_SUB) ? TCap(G.Sub(a)) : G.UCap(a);

                if (   tmpCap>0 && u<v
                    && (adjacent[v]!=u || (options & OPT_PARALLELS))
                   )
                {
                    TArc a1 = InsertArc(u,v,tmpCap,G.Length(a),G.LCap(a));
                    adjacent[v] = u;

                    if (originalArc) originalArc[a1] = a;
                }
            }
        }
        G.Close(H);

        delete[] adjacent;

        X.SetCapacity(N(),M(),L());
    }

    if (options & OPT_MAPPINGS)
    {
        TIndex* originalArcExport = registers.RawArray<TIndex>(*this,TokRegOriginalArc);
        memcpy(originalArcExport,originalArc,sizeof(TIndex)*m);
        delete[] originalArc;
    }
}


unsigned long sparseGraph::Size() const throw()
{
    return
          sizeof(sparseGraph)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractGraph::Allocated();
}


unsigned long sparseGraph::Allocated() const throw()
{
    return 0;
}


sparseGraph::~sparseGraph() throw()
{
    LogEntry(LOG_MEM,"...Sparse graph disallocated");

    if (CT.traceLevel==2 && !mode) Display();
}


complementaryGraph::complementaryGraph(abstractMixedGraph &G,TOption options)
    throw(ERRejected) :
    managedObject(G.Context()),
    sparseGraph(G.N(),G.Context())
{
    #if defined(_FAILSAVE_)

    if (TArc(G.N())*(TArc(G.N()-1))/2>=CT.MaxArc())
        Error(ERR_REJECTED,"complementaryGraph","Number of arcs is out of range");

    #endif

    LogEntry(LOG_MAN,"Generating complementary graph...");

    X.SetCapacity(G.N(),G.N()*(G.N()-1)/2,G.N()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    if (G.Dim()>0)
    {
        for (TNode v=0;v<n;v++)
            for (TDim i=0;i<G.Dim();i++) X.SetC(v,i,G.C(v,i));
    }

    for (TNode u=0;u<n;u++)
    {
        for (TNode v=u+1;v<n;v++)
        {
            if (G.Adjacency(u,v)==NoArc && G.Adjacency(v,u)==NoArc)
            {
                if (CT.Rand(2)) InsertArc(v,u);
                else InsertArc(u,v);
            }
        }
    }

    X.SetCapacity(N(),M(),L());

    if (CT.traceLevel==2) Display();
}


planarLineGraph::planarLineGraph(abstractMixedGraph &G,TOption options) throw(ERRejected) :
    managedObject(G.Context()),
    sparseGraph(G.M(),G.Context())
{
    #if defined(_FAILSAVE_)

    if (G.M()>=CT.MaxNode())
        Error(ERR_REJECTED,"planarLineGraph","Number of arcs is out of range");

    #endif

    LogEntry(LOG_MAN,"Generating planar line graph...");

    X.SetCapacity(G.M(),2*G.M(),G.M()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    for (TDim i=0;i<G.Dim();i++)
    {
        for (TNode v=0;v<G.M();v++)
        {
            X.SetC(v,i,(G.C(G.StartNode((TArc)2*v),i)+G.C(G.EndNode((TArc)2*v),i))/2);
        }
    }

    TArc* mapToArcLeft  = new TArc[2*G.M()];
    TArc* mapToArcRight = new TArc[2*G.M()];
    TArc aExtG = G.ExteriorArc();
    TArc aExt  = NoArc;

    for (TArc a1=0;a1<G.M();a1++)
    {
        TArc a2 = G.Right(2*a1,G.StartNode(2*a1));
        TArc aNew = InsertArc((TNode)a1,(TNode)(a2>>1));
        mapToArcLeft[a2] = mapToArcRight[2*a1] = aNew;

        if (aExtG==2*a1+1) aExt = 2*aNew;

        a2 = G.Right(2*a1+1,G.StartNode(2*a1+1));
        aNew = InsertArc((TNode)a1,(TNode)(a2>>1));
        mapToArcLeft[a2] = mapToArcRight[2*a1+1] = aNew;

        if (aExtG==2*a1) aExt = 2*aNew;
    }


    // Order the incidences

    for (TArc a=0;a<G.M();a++)
    {
        TArc a1 = 2*mapToArcRight[2*a];
        TArc a2 = 2*mapToArcLeft[2*a]+1;
        TArc a3 = 2*mapToArcRight[2*a+1];
        TArc a4 = 2*mapToArcLeft[2*a+1]+1;

        X.SetRight(a1,a2);
        X.SetRight(a2,a3);
        X.SetRight(a3,a4);
        X.SetRight(a4,a1);
    }

    delete[] mapToArcLeft;
    delete[] mapToArcRight;

    if (aExt!=NoArc) MarkExteriorFace(aExt);

    if (CT.traceLevel==2) Display();
}


vertexTruncation::vertexTruncation(abstractMixedGraph &G,TOption options) throw(ERRejected) :
    managedObject(G.Context()),
    sparseGraph(2*G.M(),G.Context())
{
    #if defined(_FAILSAVE_)

    if (G.ExtractEmbedding(PLANEXT_DEFAULT)==NoNode)
    {
        Error(ERR_REJECTED,"vertexTruncation","Input graph is not embedded");
    }

    #endif

    X.SetCapacity(2*G.M(),3*G.M(),2*G.M()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    // Map the original edges

    // Determine the original node degrees (only for coordinate assignment)
    TArc* degG = new TArc[G.N()];
    for (TNode v=0;v<G.N();v++) degG[v] = 0;
    for (TArc a=0;a<2*G.M();a++) degG[G.StartNode(a)]++;

    for (TArc a=0;a<G.M();a++)
    {
        InsertArc(2*a,2*a+1);

        // Determine the coordinates of the generated nodes as convex combinations
        // from the original nodes. The used parameter lambda will lead to regular
        // faces when the original graph is regular
        TNode v = G.StartNode(2*a);
        TNode w = G.EndNode(2*a);

        TFloat alpha = PI*(degG[v]-2)/(2.0*degG[v]);
        TFloat lambda = 0.5 / (1.0+sin(alpha));

        for (TDim i=0;i<G.Dim();i++)
        {
            X.SetC(2*a,i,(1-lambda)*G.C(v,i)+lambda*G.C(w,i));
        }

        alpha = PI*(degG[w]-2)/(2.0*degG[w]);
        lambda = 0.5 / (1.0+sin(alpha));

        for (TDim i=0;i<G.Dim();i++)
        {
            X.SetC(2*a+1,i,lambda*G.C(v,i)+(1-lambda)*G.C(w,i));
        }
    }

    delete[] degG;


    // Map the former nodes to cycles of the adjacent arcs
    TArc* mapToArcRight = new TArc[2*G.M()];

    for (TNode v=0;v<G.N();v++)
    {
        TArc a = G.First(v);

        if (a==NoArc)
        {
            Error(ERR_REJECTED,"vertexTruncation","Isolated node detected");
        }

        do
        {
            TArc a2 = G.Right(a,v);
            TArc aNew = InsertArc(a,a2);
            mapToArcRight[a] = 2*aNew;
            a = a2;
        }
        while (a!=G.First(v));
    }


    // Order the incidences (Note that every node has degree 3)
    for (TArc a=0;a<2*G.M();a++) X.SetRight(a,mapToArcRight[a]);

    if (G.ExteriorArc()!=NoArc) MarkExteriorFace(G.ExteriorArc());

    delete[] mapToArcRight;

    if (CT.traceLevel==2) Display();
}


facetSeparation::facetSeparation(abstractMixedGraph &G,TOptRotation mode) throw(ERRejected) :
    managedObject(G.Context()),
    sparseGraph(2*G.M(),G.Context())
{
    #if defined(_FAILSAVE_)

    if (G.ExtractEmbedding(PLANEXT_DEFAULT)==NoNode)
    {
        Error(ERR_REJECTED,"vertexTruncation","Input graph is not embedded");
    }

    #endif

    X.SetCapacity(2*G.M(),((mode==ROT_NONE) ? 4 : 5)*G.M(),2*G.M()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    // Generate a cycle for every original node. The nodes on this cycle are
    // identified with the original arcs

    for (TArc a=0;a<2*G.M();a++)
    {
        TArc a1 = G.Right(a,G.StartNode(a));

        InsertArc(a,a1);

        // Determine the coordinates of the generated nodes as convex combinations
        // from the original nodes.
        TNode u = G.StartNode(a);
        TNode v = G.EndNode(a);
        TNode w = G.EndNode(a1);

        for (TDim i=0;i<G.Dim();i++)
        {
            if (mode==ROT_LEFT)
            {
                X.SetC(a1,i,0.5*G.C(u,i)+0.3*G.C(v,i)+0.2*G.C(w,i));
            }
            else if (mode==ROT_RIGHT)
            {
                X.SetC(a1,i,0.5*G.C(u,i)+0.2*G.C(v,i)+0.3*G.C(w,i));
            }
            else // if (mode==ROT_NONE)
            {
                X.SetC(a1,i,0.6*G.C(u,i)+0.2*G.C(v,i)+0.2*G.C(w,i));
            }
        }
    }

    // Map the original arcs and regions
    for (TArc a=0;a<2*G.M();a++)
    {
        TArc a2 = G.Right(a,G.StartNode(a));
        InsertArc(a2,a^1);
    }

    // Order the incidences to support planar representations
    if (mode==ROT_LEFT)
    {
        for (TArc a=0;a<G.M();a++)
        {
            TArc a2 = G.Right(2*a,G.StartNode(2*a));
            TArc a3 = G.Right(2*a+1,G.StartNode(2*a+1));

            InsertArc(a2,a3);
        }

        for (TArc a=0;a<2*G.M();a++)
        {
            TArc a2 = G.Right(a,G.StartNode(a));

            X.SetRight(2*a2,2*a+1);
            X.SetRight(2*a+1,8*G.M()+a);
            X.SetRight(8*G.M()+a,4*G.M()+2*a);
            X.SetRight(4*G.M()+2*a,4*G.M()+2*(a2^1)+1);
            X.SetRight(4*G.M()+2*(a2^1)+1,2*a2);
            X.SetFirst(a2,4*G.M()+2*a);
        }
    }
    else if (mode==ROT_RIGHT)
    {
        for (TArc a=0;a<G.M();a++)
        {
            InsertArc(2*a,2*a+1);
        }

        for (TArc a=0;a<2*G.M();a++)
        {
            TArc a2 = G.Right(a,G.StartNode(a));

            X.SetRight(2*a2,2*a+1);
            X.SetRight(2*a+1,4*G.M()+2*a);
            X.SetRight(4*G.M()+2*a,4*G.M()+2*(a2^1)+1);
            X.SetRight(4*G.M()+2*(a2^1)+1,8*G.M()+a2);
            X.SetRight(8*G.M()+a2,2*a2);
            X.SetFirst(a2,4*G.M()+2*a);
        }
    }
    else // if (mode==ROT_NONE)
    {
        for (TArc a=0;a<2*G.M();a++)
        {
            TArc a2 = G.Right(a,G.StartNode(a));

            X.SetRight(2*a2,2*a+1);
            X.SetRight(2*a+1,4*G.M()+2*a);
            X.SetRight(4*G.M()+2*a,4*G.M()+2*(a2^1)+1);
            X.SetRight(4*G.M()+2*(a2^1)+1,2*a2);
            X.SetFirst(a2,4*G.M()+2*a);
        }
    }

    if (CT.traceLevel==2) Display();
}


dualGraph::dualGraph(abstractMixedGraph &G,TOption options) throw(ERRejected) :
    managedObject(G.Context()),
    sparseGraph(G.M()-G.N()+2,G.Context())
{
    #if defined(_FAILSAVE_)

    if (G.M()-G.N()+2>=CT.MaxNode())
        Error(ERR_REJECTED,"dualGraph","Number of regions is out of range");

    #endif

    X.SetCapacity(G.M()-G.N()+2,G.M(),G.M()-G.N()+4);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    // Place nodes
    if (G.ExtractEmbedding(PLANEXT_DUAL,
            reinterpret_cast<void*>(static_cast<abstractMixedGraph*>(this)))==NoNode)
    {
        Error(ERR_REJECTED,"dualGraph","Input graph is not embedded");
    }

    // Insert edges
    TArc* predArc = new TArc[2*G.M()];

    for (TArc a=0;a<G.M();a++)
    {
        TNode u = G.Face(2*a);
        TNode v = G.Face(2*a+1);

        InsertArc(v,u);

        predArc[2*a]   = G.Right(2*a,G.StartNode(2*a));
        predArc[2*a+1] = G.Right(2*a+1,G.StartNode(2*a+1));
    }

    // Set up dual planar representation
    X.ReorderIncidences(predArc);

    delete[] predArc;

    if (G.Dim()>=2) X.Layout_ArcRouting();

    if (CT.traceLevel==2) Display();
}


spreadOutRegular::spreadOutRegular(abstractMixedGraph &G,TOption options) throw(ERRejected) :
    managedObject(G.Context()),
    sparseGraph(G.N(),G.Context())
{
    #if defined(_FAILSAVE_)

    if (G.ExtractEmbedding(PLANEXT_DEFAULT)==NoNode)
    {
        Error(ERR_REJECTED,"spreadOutRegular","Input graph is not embedded");
    }

    #endif

    ImportLayoutData(G);

    TArc* predG = G.GetPredecessors();

    #if defined(_FAILSAVE_)

    if (!predG)
        Error(ERR_REJECTED,"spreadOutRegular","Missing predecessor labels");

    #endif

    // Determine the start nodes assigned with the (at most) two arcs in
    // the outerplanar graph assigned with each arc in the original graph

    TNode nodeCount = 0;
    TNode* startNode1 = new TNode[2*G.M()];
    TNode* startNode2 = new TNode[2*G.M()];

    for (TNode v=0;v<G.N();v++)
    {
        TArc a = G.First(v);
        TArc thisDegree = 0;

        do
        {
            if (    predG[G.EndNode(a)]==a
                 || predG[G.EndNode(a^1)]==(a^1) )
            {
                thisDegree++;
            }

            a = G.Right(a,v);
        }
        while (a!=G.First(v));

        if (thisDegree==0)
        {

            delete[] startNode1;
            delete[] startNode2;

            Error(ERR_REJECTED,"spreadOutRegular",
                "Predecessor labels do not constitute a spanning tree");
        }

        while (   predG[G.EndNode(a)]!=a
               && predG[G.EndNode(a^1)]!=(a^1) )
        {
            a = G.Right(a,v);
        }

        TArc a0 = a;
        TNode vStart = v;

        do
        {
            if (   predG[G.EndNode(a)]==a
                || predG[G.EndNode(a^1)]==(a^1) )
            {
                startNode2[a] = vStart;
            }
            else
            {
                startNode2[a] = NoNode;
            }

            a = G.Right(a,v);
            startNode1[a] = vStart;

            if (   a!=a0
                && (   predG[G.EndNode(a)]==a
                    || predG[G.EndNode(a^1)]==(a^1) )
               )
            {
                vStart = InsertNode();
                nodeCount++;
            }
        }
        while (a!=a0);
    }


    // Generate the edges 

    TArc* mapToArc1  = new TArc[G.M()];
    TArc* mapToArc2  = new TArc[G.M()];

    for (TArc a=0;a<G.M();a++)
    {
        if (startNode2[2*a]==NoNode)
        {
            // Not a predecessor arc
            mapToArc1[a] = InsertArc(startNode1[2*a],startNode1[2*a+1]);
            mapToArc2[a] = NoArc;
        }
        else
        {
            // Predecessor arcs are mapped twice
            mapToArc1[a] = InsertArc(startNode1[2*a],startNode2[2*a+1]);
            mapToArc2[a] = InsertArc(startNode1[2*a+1],startNode2[2*a]);
        }
    }


    // Order the incidences to obtain an outerplanar representation

    for (TArc a=0;a<2*G.M();a++)
    {
        if (mapToArc2[a>>1]==NoArc) continue;

        TArc firstIndex = NoArc;

        if (a&1)
        {
            firstIndex = 2*mapToArc2[a>>1]+1;
        }
        else
        {
            firstIndex = 2*mapToArc1[a>>1]+1;
        }

        TArc a0 = a^1;
        TNode v = G.StartNode(a0);
        TArc thisIndex = firstIndex;
        TArc rightIndex = NoArc;

        do
        {
            TArc ar  = G.Right(a0,v);

            if (mapToArc2[ar>>1]==NoArc)
            {
                rightIndex = 2*mapToArc1[ar>>1]|(ar&1);
            }
            else if (ar&1)
            {
                rightIndex = 2*mapToArc2[ar>>1];
            }
            else
            {
                rightIndex = 2*mapToArc1[ar>>1];
            }

            if (StartNode(rightIndex)!= StartNode(thisIndex))
            {
                rightIndex ^= 1;
            }

            X.SetRight(thisIndex,rightIndex);
            a0 = ar;
            thisIndex = rightIndex;
        }
        while (mapToArc2[a0>>1]==NoArc);

        X.SetRight(thisIndex,firstIndex);
        X.SetFirst(StartNode(thisIndex),thisIndex);
        SetExteriorArc(firstIndex);
    }

    delete[] mapToArc1;
    delete[] mapToArc2;

    delete[] startNode1;
    delete[] startNode2;

    Layout_Equilateral();

    if (CT.traceLevel==2) Display();
}


mycielskianGraph::mycielskianGraph(abstractMixedGraph &G,TOption options) throw(ERRejected) :
    managedObject(G.Context()),
    sparseGraph(G,OPT_CLONE | (options & OPT_MAPPINGS))
{
    #if defined(_FAILSAVE_)

    if (2*n+1>=CT.MaxNode())
        Error(ERR_REJECTED,"mycielskianGraph","Number of nodes is out of range");

    #endif

    TNode n0 = G.N();
    TNode m0 = G.M();

    X.SetCapacity(2*n0+1,3*m0+n0,2*n0+3);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    // Add a star graph
    for (TNode i=0;i<n0+1;++i) InsertNode();

    TNode centerNode = 2*n0;
    TFloat centerPos[3] = {0.0,0.0,0.0};

    for (TNode i=0;i<n0;++i)
    {
        InsertArc(n0+i,centerNode);

        for (TNode j=0;j<G.Dim() && j<3;++j) centerPos[j] += C(i,j);
    }

    for (TNode j=0;j<G.Dim() && j<3;++j)
    {
        X.SetC(centerNode,j,centerPos[j]/n0);
    }

    for (TNode i=0;i<n0;++i)
    {
        for (TNode j=0;j<G.Dim() && j<3;++j)
        {
            X.SetC(n0+i,j,(C(i,j)+C(centerNode,j))/2.0);
        }
    }

    // Insert further edges guide by the original arcs
    for (TArc a=0;a<m0;++a)
    {
        TNode u = G.StartNode(2*a);
        TNode v = G.StartNode(2*a+1);

        InsertArc(v,n0+u);
        InsertArc(n0+v,u);
    }

    if (G.Dim()>=2)
    {
        X.Layout_ArcRouting();
    }

    if (options & OPT_SUB)
    {
        TNode* nodeColour = InitNodeColours(1);
        TArc* edgeColour = InitEdgeColours();

        for (TArc a=0;a<m0;++a)
        {
            edgeColour[a] = 0;
        }

        for (TNode i=0;i<n0;++i)
        {
            edgeColour[G.M()+i] = 1;
            nodeColour[i] = 0;
        }
    }

    if (options & OPT_MAPPINGS)
    {
        TIndex* originalNode = registers.RawArray<TIndex>(*this,TokRegOriginalNode);
        TIndex* originalArc = registers.GetArray<TIndex>(TokRegOriginalArc);

        for (TArc a=0;a<m0;++a)
        {
            originalArc[a] = originalArc[m0+n0+a] = 2*a;
            originalArc[m0+n0+m0+a] = 2*a+1;
        }

        for (TNode i=0;i<n0;++i)
        {
            originalNode[i] = originalNode[n0+i] = i;
            originalArc[m0+i] = NoArc;
        }

        originalNode[centerNode] = NoNode;
    }

    if (CT.traceLevel==2) Display();
}


mycielskianGraph::mycielskianGraph(unsigned k,goblinController& _CT)
    throw(ERRejected) : managedObject(_CT), sparseGraph(TNode(0),_CT)
{
    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    if (k<=2)
    {
        InsertNode();
        InsertNode();
        InsertArc(0,1);
        X.SetC(0,0,0);
        X.SetC(0,1,nodeSpacing);
        X.SetC(1,0,nodeSpacing);
        X.SetC(1,1,0);
        return;
    }

    sparseGraph* G = new mycielskianGraph(k-1,_CT);

    TNode n0 = G->N();
    TNode m0 = G->M();

    #if defined(_FAILSAVE_)

    if (2*n0+1>=CT.MaxNode())
        Error(ERR_REJECTED,"mycielskianGraph","Number of nodes is out of range");

    #endif

    X.SetCapacity(2*n0+1,3*m0+n0,2*n0+3);

    // Plain copy of G
    AddGraphByNodes(*G,MERGE_OVERLAY);

    // Add a star graph
    for (TNode i=0;i<n0+1;++i)
    {
        InsertNode();

        X.SetC(n0+i,0,(k-1)*nodeSpacing*sin(i/TFloat(n0)*PI/2.0));
        X.SetC(n0+i,1,(k-1)*nodeSpacing*cos(i/TFloat(n0)*PI/2.0));
    }

    X.Layout_SetBoundingInterval(0,-nodeSpacing,k*nodeSpacing);
    X.Layout_SetBoundingInterval(1,-nodeSpacing,k*nodeSpacing);

    TNode centerNode = 2*n0;

    for (TNode i=0;i<n0;++i)
    {
        InsertArc(n0+i,centerNode);
    }

    // Insert further edges guide by the original arcs
    for (TArc a=0;a<m0;++a)
    {
        TNode u = G->StartNode(2*a);
        TNode v = G->StartNode(2*a+1);

        InsertArc(v,n0+u);
        InsertArc(n0+v,u);
    }

    delete G;

    if (CT.traceLevel==2) Display();
}


voronoiDiagram::voronoiDiagram(abstractMixedGraph& _G,const indexSet<TNode>& _Terminals) throw() :
    managedObject(_G.Context()),
    sparseGraph(_G.VoronoiRegions(_Terminals),_G.Context(),true),
    G(_G),Terminals(_Terminals)
{
    LogEntry(LOG_MAN,"Contracting partial trees...");

    X.SetCapacity(G.N(),G.M(),G.N()+2);
    X.Layout_AdoptBoundingBox(G);
    ImportLayoutData(G);

    TNode* mapNodes = new TNode[G.N()];

    for (TNode v=0;v<G.N();v++) mapNodes[v] = NoNode;

    TNode i = 0;

    for (TNode v=0;v<G.N();v++)
    {
        if (mapNodes[G.Find(v)]==NoNode) mapNodes[G.Find(v)] = i++;
        mapNodes[v] = mapNodes[G.Find(v)];

        if (Terminals.IsMember(v))
        {
            for (TDim i=0;i<G.Dim();i++) X.SetC(mapNodes[v],i,G.C(v,i));
        }
    }

    revMap = new TArc[G.M()];
    goblinHashTable<TArc,TArc> Adj(i*i,G.M(),NoArc,CT);

    for (TArc a=0;a<G.M();a++)
    {
        TNode u = G.StartNode(2*a);
        TNode v = G.EndNode(2*a);
        TNode u2 = mapNodes[G.Find(u)];
        TNode v2 = mapNodes[G.Find(v)];

        if (u2!=v2)
        {
            TFloat l = G.Dist(u)+G.Dist(v)+G.Length(2*a);

            TArc j = i*u2+v2;

            if (u2>v2) j = i*v2+u2;

            TArc a2 = Adj.Key(j);

            if (a2==NoArc)
            {
                a2 = InsertArc(u2,v2,1,l);
                Adj.ChangeKey(j,a2);
                revMap[a2] = a;
            }
            else
            {
                if (Length(2*a2)>l)
                {
                    X.SetLength(2*a2,l);
                    revMap[a2] = a;
                }
            }
        }
    }

    delete[] mapNodes;

    X.SetCapacity(N(),M(),L());

    if (CT.traceLevel==2) Display();
}


TFloat voronoiDiagram::UpdateSubgraph() throw()
{
    LogEntry(LOG_METH,"Mapping tree to original graph...");

    TFloat ret = 0;

    G.InitSubgraph();

    TArc* pred = GetPredecessors();

    for (TNode v=0;v<n;v++)
    {
        if (pred[v]==NoArc) continue;

        TArc a = revMap[pred[v]>>1];

        G.SetSub(2*a,1);

        TNode u = G.StartNode(2*a);

        while (!Terminals.IsMember(u))
        {
            TArc a = G.Pred(u);
            G.SetSub(a,1);
            u = G.StartNode(a);
        }

        u = G.EndNode(2*a);

        while (!Terminals.IsMember(u))
        {
            TArc a = G.Pred(u);
            G.SetSub(a,1);
            u = G.StartNode(a);
        }
    }

    return ret;
}


voronoiDiagram::~voronoiDiagram() throw()
{
    if (CT.traceLevel==2) Display();

    delete[] revMap;

    LogEntry(LOG_MAN,"...Voronoi diagram deleted");
}


triangularGraph::triangularGraph(TNode cardinality,goblinController& _CT) throw() :
    managedObject(_CT),
    sparseGraph(TNode(0),_CT)
{
    LogEntry(LOG_MAN,"Generating triangular graph...");

    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    TNode** node = new TNode*[cardinality];

    for (TNode i=0;i<cardinality;++i)
    {
        node[i] = new TNode[cardinality];

        for (TNode j=i+1;j<cardinality;++j)
        {
            node[i][j] = InsertNode();

            for (TNode k=0;k<i;k++)   InsertArc(node[i][j],node[k][j]);
            for (TNode k=i+1;k<j;k++) InsertArc(node[i][j],node[i][k]);
            for (TNode k=0;k<i;k++)   InsertArc(node[i][j],node[k][i]);
        }
    }

    for (TNode i=0;i<(cardinality-1)/2;++i)
    {
        TFloat thisRadius = nodeSpacing*(cardinality/2-i+1);

        for (TNode j=0;j<cardinality;++j)
        {
            TNode k = (j+i+1)%cardinality;
            TNode v = (j<k) ? node[j][k] : node[k][j];

            SetC(v,0,thisRadius*sin((2*j+i)*PI/cardinality));
            SetC(v,1,thisRadius*cos((2*j+i)*PI/cardinality));
        }
    }

    if (cardinality%2==0)
    {
        TFloat thisRadius = nodeSpacing;

        for (TNode j=0;j<cardinality/2;++j)
        {
            TNode k = j+cardinality/2;
            SetC(node[j][k],0,thisRadius*sin((4*j+cardinality/2)*PI/cardinality));
            SetC(node[j][k],1,thisRadius*cos((4*j+cardinality/2)*PI/cardinality));
        }
    }

    for (TNode i=0;i<cardinality;i++) delete[] node[i];
    delete[] node;

    Layout_ConvertModel(LAYOUT_FREESTYLE_POLYGONES);

    TFloat maxRadius = nodeSpacing*(cardinality/2+2);
    X.Layout_SetBoundingInterval(0,-maxRadius,maxRadius);
    X.Layout_SetBoundingInterval(1,-maxRadius,maxRadius);
}


static void expandSet(TIndex *factorialTable,bool *charVector,TNode groundSetCardinality,
    TNode subsetCardinality,TNode setIndex) throw()
{
    if (groundSetCardinality<=subsetCardinality)
    {
        for (TNode i=0;i<groundSetCardinality;++i) charVector[i] = true;

        return;
    }

    TIndex nReduced =   factorialTable[groundSetCardinality-1] /
                     (  factorialTable[subsetCardinality]
                      * factorialTable[groundSetCardinality-subsetCardinality-1] );

    if (setIndex<nReduced)
    {
        expandSet(factorialTable,charVector,groundSetCardinality-1,subsetCardinality,setIndex);
        charVector[groundSetCardinality-1] = false;
        return;
    }

    expandSet(factorialTable,charVector,groundSetCardinality-1,subsetCardinality-1,setIndex-nReduced);
    charVector[groundSetCardinality-1] = true;
}


intersectionGraph::intersectionGraph(TNode groundSetCardinality,TNode subsetCardinality,
    TNode minimumIntersection,TNode maximumIntersection,goblinController& _CT) throw() :
    managedObject(_CT),
    sparseGraph(TNode(0),_CT)
{
    LogEntry(LOG_MAN,"Generating intersection graph...");

    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    TIndex *factorialTable = new TIndex[groundSetCardinality+1];
    factorialTable[0] = 1;

    for (TIndex i=0;i<groundSetCardinality;++i) factorialTable[i+1] = factorialTable[i]*(i+1);

    TIndex nTotal =    factorialTable[groundSetCardinality] /
                    (  factorialTable[subsetCardinality]
                     * factorialTable[groundSetCardinality-subsetCardinality] );

    TNode step = 0;
    TNode nReduced = 1;
    TNode nReducedOld = 0;
    bool* uVector = new bool[groundSetCardinality];
    bool* vVector = new bool[groundSetCardinality];

    for (TNode v=0;v<nTotal;++v)
    {
        InsertNode();

        SetC(v,0,nodeSpacing*step*sin((v-nReducedOld)*2*PI/(nReduced-nReducedOld)));
        SetC(v,1,nodeSpacing*step*cos((v-nReducedOld)*2*PI/(nReduced-nReducedOld)));

        if (nReduced-1<=v)
        {
            ++step;
            nReducedOld = nReduced;
            nReduced = factorialTable[subsetCardinality+step] / (factorialTable[subsetCardinality]*factorialTable[step]);
        }

        expandSet(factorialTable,vVector,groundSetCardinality,subsetCardinality,v);

        for (TNode u=0;u<v;++u)
        {
            expandSet(factorialTable,uVector,groundSetCardinality,subsetCardinality,u);

            TIndex meetCardinality = 0;

            for (TIndex i=0;i<groundSetCardinality;++i)
            {
                if (uVector[i] && vVector[i]) ++meetCardinality;
            }

            if (   meetCardinality>=minimumIntersection
                && meetCardinality<=maximumIntersection ) InsertArc(u,v);
        }
    }

    delete[] vVector;
    delete[] uVector;
    delete[] factorialTable;

    TFloat maxRadius = nodeSpacing*(step+1);
    X.Layout_SetBoundingInterval(0,-maxRadius,maxRadius);
    X.Layout_SetBoundingInterval(1,-maxRadius,maxRadius);
}


sierpinskiTriangle::sierpinskiTriangle(TNode depth,goblinController &_CT) throw() :
    managedObject(_CT),
    sparseGraph(TNode(0),_CT)
{
    LogEntry(LOG_MAN,"Generating Sierpinski triangle...");

    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);
    TFloat radius = (nodeSpacing>0) ? nodeSpacing : 1.0;

    sparseGraph* smallTriangle = (depth==0) ? this : new sparseGraph(TNode(0),CT);
    sparseGraph* bigTriangle = NULL;

    for (int j=0;j<3;j++)
    {
        smallTriangle -> InsertNode();
        smallTriangle -> SetC(j,0,radius*sin(j*2*PI/3));
        smallTriangle -> SetC(j,1,-radius*cos(j*2*PI/3));
    }


    smallTriangle -> InsertArc(0,1);
    smallTriangle -> InsertArc(1,2);
    smallTriangle -> InsertArc(2,0);

    for (TNode i=0;i<depth;i++)
    {
        TNode smallN = smallTriangle->N();
        TNode bigN = 3*smallN-3;

        if (depth==i+1)
        {
            for (TNode v=0;v<bigN;v++) InsertNode();

            bigTriangle = this;
        }
        else
        {
            bigTriangle = new sparseGraph(bigN,CT);
        }


        for (unsigned j=0;j<3;j++)
        {
            bigTriangle -> SetC(j,0,2*radius*sin(j*2*PI/3));
            bigTriangle -> SetC(j,1,-2*radius*cos(j*2*PI/3));
            bigTriangle -> SetC(3+j,0,smallTriangle->C(j,0));
            bigTriangle -> SetC(3+j,1,-smallTriangle->C(j,1));

            for (TNode u=3;u<smallN;++u)
            {
                bigTriangle -> SetC(6+j*(smallN-3)+(u-3),0,smallTriangle->C(u,0)+radius*sin(j*2*PI/3));
                bigTriangle -> SetC(6+j*(smallN-3)+(u-3),1,smallTriangle->C(u,1)-radius*cos(j*2*PI/3));
            }

            for (TArc a=0;a<smallTriangle->M();++a)
            {
                TNode v[2],w[2];
                v[0] = smallTriangle->StartNode(2*a);
                v[1] = smallTriangle->EndNode(2*a);

                for (int k=0;k<2;k++)
                {
                    if (v[k]==j)
                    {
                        w[k] = j;
                    }
                    else if (v[k]<3)
                    {
                        w[k] = 3+(v[k]+j)%3;
                    }
                    else
                    {
                        w[k] = 6+j*(smallN-3)+v[k]-3;
                    }
                }

                bigTriangle -> InsertArc(w[0],w[1]);
            }
        }

        radius *= 2;

        delete smallTriangle;
        smallTriangle = bigTriangle;
    }

    X.Layout_SetBoundingInterval(0,C(2,0)-nodeSpacing,C(1,0)+nodeSpacing);
    X.Layout_SetBoundingInterval(1,C(0,1)-nodeSpacing,C(2,1)+nodeSpacing);

    IncidenceOrderFromDrawing();

    Layout_ConvertModel(LAYOUT_FREESTYLE_POLYGONES);
}


openGrid::openGrid(TNode _k,TNode _l,TOptGrid shape,goblinController &_CT) throw(ERRejected) :
    managedObject(_CT),
    sparseGraph(TNode(0),_CT)
{
    LogEntry(LOG_MAN,"Generating open grid...");

    Layout_ConvertModel(LAYOUT_FREESTYLE_POLYGONES);
    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    _k = (_k<2) ? 2 : _k;
    _l = (_l<1) ? 1 : _l;

    if (shape==GRID_HEXAGONAL)
    {
        _l |= 1;
        _k -= (_k&1);
        X.SetCapacity(_k*_l,_k*(_l-1)+(_k-1)*(_l+1)/2-(_k-2)/2,_k*_l+2);
    }
    else if (shape==GRID_TRIANGULAR)
    {
        X.SetCapacity(_k*_l,_k*(_l-1)+(_k-1)*_l+(_k-1)*(_l-1),_k*_l+2);
    }
    else // if (shape==GRID_SQUARE)
    {
        X.SetCapacity(_k*_l,_k*(_l-1)+(_k-1)*_l,_k*_l+2);
    }

    for (TNode v=0;v<_k*_l;++v) InsertNode();

    if (shape==GRID_HEXAGONAL)
    {
        for (TNode k=0;k<_k;k++)
        {
            for (TNode l=0;l<_l;l++)
            {
                if (l<_l-1) InsertArc(k*_l+l,k*_l+(l+1));

                if ((k+l)%2==0 && k<_k-1)
                {
                    InsertArc(k*_l+l,(k+1)*_l+l);
                }

                X.SetC(k*_l+l,0,nodeSpacing*cos(PI/6.0)*l);
                X.SetC(k*_l+l,1,nodeSpacing*(k+(cos(PI/6.0)*0.5)*(k+(k+l+1)%2)));
            }
        }

        X.Layout_SetBoundingInterval(0,-nodeSpacing,nodeSpacing*((_l-1)*cos(PI/6.0)+1));
        X.Layout_SetBoundingInterval(1,-nodeSpacing,nodeSpacing*(_k-1+(cos(PI/6.0)*0.5)*_k+1));
    }
    else if (shape==GRID_SQUARE)
    {
        for (TNode k=0;k<_k;k++)
        {
            for (TNode l=0;l<_l;l++)
            {
                if (l<_l-1) InsertArc(k*_l+l,k*_l+(l+1));
                if (k<_k-1) InsertArc(k*_l+l,(k+1)*_l+l);

                X.SetC(k*_l+l,0,nodeSpacing*l);
                X.SetC(k*_l+l,1,nodeSpacing*k);
            }
        }

        X.Layout_SetBoundingInterval(0,-nodeSpacing,_l*nodeSpacing);
        X.Layout_SetBoundingInterval(1,-nodeSpacing,_k*nodeSpacing);
    }
    else // if (shape==GRID_TRIANGULAR)
    {
        for (TNode k=0;k<_k;k++)
        {
            for (TNode l=0;l<_l;l++)
            {
                if (l<_l-1)           InsertArc(k*_l+l,k*_l+(l+1));
                if (k<_k-1)           InsertArc(k*_l+l,(k+1)*_l+l);
                if (l<_l-1 && k<_k-1) InsertArc(k*_l+l,(k+1)*_l+(l+1));

                X.SetC(k*_l+l,0,nodeSpacing*(l+0.5*(_k-k-1)));
                X.SetC(k*_l+l,1,nodeSpacing*cos(PI/6.0)*k);
            }
        }

        X.Layout_SetBoundingInterval(0,-nodeSpacing,((_l-1.0)+(_k-1.0)*0.5+1)*nodeSpacing);
        X.Layout_SetBoundingInterval(1,-nodeSpacing,((_k-1.0)*cos(PI/6.0)+1)*nodeSpacing);
    }

    IncidenceOrderFromDrawing();
}


polarGrid::polarGrid(TNode _k,TNode _l,TNode _p,TOptPolar facets,TOptPolar dim,
        goblinController& _CT) throw(ERRejected) :
    managedObject(_CT),
    sparseGraph(TNode(0),_CT)
{
    LogEntry(LOG_MAN,"Generating spheric grid...");

    Layout_ConvertModel(LAYOUT_FREESTYLE_POLYGONES);
    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    _k = (_k<1) ? 1 : _k;
    _l = (_l<3) ? 3 : _l;
    _p = (_p>2) ? 2 : _p;

    for (TNode v=0;v<_k*_l+_p;++v) InsertNode();

    TFloat r = nodeSpacing/PI*_l;
    TFloat R = r*_k;
    TArc m0 = 0;

    X.Layout_SetBoundingInterval(0,-R-nodeSpacing,R+nodeSpacing);
    X.Layout_SetBoundingInterval(1,-R-nodeSpacing,R+nodeSpacing);

    if (dim==POLAR_CONE)
    {
        X.Layout_SetBoundingInterval(2,-nodeSpacing,_k*nodeSpacing);
    }
    else if (dim==POLAR_TUBE)
    {
        X.Layout_SetBoundingInterval(2,-nodeSpacing,_k*nodeSpacing);
    }
    else if (dim==POLAR_HEMISPHERE)
    {
        X.Layout_SetBoundingInterval(2,-nodeSpacing,R+nodeSpacing);
    }
    else if (dim==POLAR_DISC)
    {
        // No need to assign a bounding interval for the 3rd component
    }
    else // if (dim==POLAR_SPHERE || dim==POLAR_DEFAULT)
    {
        dim = POLAR_SPHERE;
        X.Layout_SetBoundingInterval(2,-R-nodeSpacing,R+nodeSpacing);
    }

    if (_p>0)
    {
        // Connect the "south" pole to the nodes in the first row
        for (TNode l=0;l<_l;l++) InsertArc(_k*_l,l);

        X.SetC(_k*_l,0,0);
        X.SetC(_k*_l,1,0);
        X.SetC(_k*_l,2,-R);

        for (TNode l=0;l<_l;l++) X.SetRight(2*l,2*((l+1)%_l));

        X.SetFirst(_k*_l,0);

        m0 = _l;
    }


    for (TNode k=0;k<_k;k++)
    {
        for (TNode l=0;l<_l;l++)
        {
            TFloat shift = (facets==POLAR_TRIANGULAR) ? 0.5*k : 0;
            TFloat thisAngle = 2*PI*(l-shift)/_l;

            if (dim==POLAR_DISC)
            {
                // Draw on a disc
                TFloat thisRadius = (k+1)*r;

                X.SetC(k*_l+l,0,-sin(thisAngle)*thisRadius);
                X.SetC(k*_l+l,1, cos(thisAngle)*thisRadius);
            }
            else if (dim==POLAR_CONE)
            {
                // Draw on a cone
                TFloat thisRadius = (k+1)*r;

                X.SetC(k*_l+l,0,-sin(thisAngle)*thisRadius);
                X.SetC(k*_l+l,1, cos(thisAngle)*thisRadius);
                X.SetC(k*_l+l,2,k*nodeSpacing);
            }
            else if (dim==POLAR_TUBE)
            {
                // Draw on a cylinder
                X.SetC(k*_l+l,0,-sin(thisAngle)*r);
                X.SetC(k*_l+l,1, cos(thisAngle)*r);
                X.SetC(k*_l+l,2,k*nodeSpacing);
            }
            else if (dim==POLAR_HEMISPHERE)
            {
                // Draw on an hemisphere
                TNode K =2*_k+1;
                TFloat thisRadius = sin(PI*(k+1)/K)*R;

                X.SetC(k*_l+l,0,-sin(thisAngle)*thisRadius);
                X.SetC(k*_l+l,1, cos(thisAngle)*thisRadius);
                X.SetC(k*_l+l,2,cos(PI*(k+1)/K)*R);
            }
            else // if (dim==POLAR_SPHERE)
            {
                // Draw on a sphere
                TFloat thisRadius = sin(PI*(k+1)/(_k+1))*R;

                X.SetC(k*_l+l,0,-sin(thisAngle)*thisRadius);
                X.SetC(k*_l+l,1, cos(thisAngle)*thisRadius);
                X.SetC(k*_l+l,2,cos(PI*(k+1)/(_k+1))*R);
            }

            // Connect this node to a node in a neighboring column
            InsertArc(k*_l+l,k*_l+((l+1)%_l));
        }

        for (TNode l=0;l<_l && k<_k-1;l++)
        {
            TArc a[6];
            int idx = 0;

            // Connect this node to a node in the subsequent row
            a[idx++] = 2*InsertArc(k*_l+l,((k+1)%_k)*_l+l);

            if (facets==POLAR_SQUARE)
            {
                a[idx++] = 2*(m0+2*k*_l+l);

                if (k>0 || _p>0)
                {
                    a[idx++] = 2*(m0+2*k*_l-_l+l)+1;
                }

                a[idx++] = 2*(m0+2*k*_l+(_l+l-1)%_l)+1;
            }
            else // (facets==POLAR_TRIANGULAR)
            {
                // Connect this node to another node in the subsequent row
                a[idx++] = 2*InsertArc(k*_l+l,((k+1)%_k)*_l+((l+1)%_l));

                a[idx++] = 2*(m0+3*k*_l+l);

                if (k>0)
                {
                    a[idx++] = 2*(m0+3*k*_l-2*_l+2*l)+1;
                    a[idx++] = 2*(m0+3*k*_l-2*_l+2*((_l+l-1)%_l)+1)+1;
                }
                else if (_p>0)
                {
                    a[idx++] = 2*l+1;
                }

                a[idx++] = 2*(m0+3*k*_l+(_l+l-1)%_l)+1;
            }

            for (int jdx=0;jdx<idx;++jdx)
            {
                X.SetRight(a[jdx],a[(jdx+1)%idx]);
            }

            X.SetFirst(k*_l+l,a[0]);
        }
    }


    if (_p>1)
    {
        // Connect the nodes in the final row to the "north" pole
        for (TNode l=0;l<_l;++l) InsertArc((_k-1)*_l+l,_k*_l+1);

        X.SetC(_k*_l+1,0,0);
        X.SetC(_k*_l+1,1,0);
        X.SetC(_k*_l+1,2,R);

        for (TNode l=_l;l>0;--l) X.SetRight(2*(m-_l+(_l+l-1)%_l)+1,2*(m-_l+(_l+l-2)%_l)+1);

        X.SetFirst(_k*_l+1,2*(m-_l)+1);
    }


    for (TNode l=0;l<_l;l++)
    {
        TArc a[5];
        int idx = 0;

        if (_p>1)
        {
            a[idx++] = 2*(m-_l+l);
        }

        if (facets==POLAR_SQUARE)
        {
            a[idx++] = 2*(m0+2*(_k-1)*_l+l);

            if (_k>1 || _p>0)
            {
                a[idx++] = 2*(m0+2*(_k-1)*_l-_l+l)+1;
            }

            a[idx++] = 2*(m0+2*(_k-1)*_l+(_l+l-1)%_l)+1;
        }
        else // (facets==POLAR_TRIANGULAR)
        {
            a[idx++] = 2*(m0+3*(_k-1)*_l+l);

            if (_k>1)
            {
                a[idx++] = 2*(m0+3*(_k-1)*_l-2*_l+2*l)+1;
                a[idx++] = 2*(m0+3*(_k-1)*_l-2*_l+2*((_l+l-1)%_l)+1)+1;
            }
            else if (_p>0)
            {
                a[idx++] = 2*l+1;
            }

            a[idx++] = 2*(m0+3*(_k-1)*_l+(_l+l-1)%_l)+1;
        }

        for (int jdx=0;jdx<idx;++jdx)
        {
            X.SetRight(a[jdx],a[(jdx+1)%idx]);
        }

        X.SetFirst((_k-1)*_l+l,a[0]);
    }

    if (_p<2 && dim!=POLAR_TUBE && dim!=POLAR_SPHERE) MarkExteriorFace(2*m-2);
}


static long ggt(long x,long y)
{
    long a = (abs(x)>abs(y)) ? abs(x) : abs(y);
    long b = (abs(x)>abs(y)) ? abs(y) : abs(x);

    if (a==0) return 1;
    if (b==0) return a;

    long c = a%b;

    while (c!=0)
    {
        a = b;
        b = c;
        c = a%b;
    }

    return b;
}


toroidalGrid::toroidalGrid(unsigned short hSkew,unsigned short vSize,short vSkew,unsigned short hSize,
    TOptTorus facets,goblinController &_CT) throw(ERRejected) :
    managedObject(_CT),
    sparseGraph(TNode(0),_CT)
{
    LogEntry(LOG_MAN,"Generating toroidal grid...");

    Layout_ConvertModel(LAYOUT_FREESTYLE_POLYGONES);
    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    if (vSize<hSkew)
        Error(ERR_REJECTED,"toroidalGrid","Incompatible parameters hSkew and vSize");

    if (facets==TORUS_HEXAGONAL)
    {
        // Sort out parameter sets with wrap-around mismatches
        if ((hSize-hSkew)%3!=0)
            Error(ERR_REJECTED,"toroidalGrid","Incompatible parameters hSkew and hSize");
        if ((vSize-vSkew)%3!=0)
            Error(ERR_REJECTED,"toroidalGrid","Incompatible parameters vSkew and vSize");
    }

    long nAct = hSize*vSize-hSkew*vSkew;

    if (nAct<=0)
        Error(ERR_REJECTED,"toroidalGrid","Incompatible parameters");

    for (TNode v=0;v<TNode(nAct);++v) InsertNode();

    // Define vectors (dx1,dy1) and (dx2,dy2) which generate two recurrent
    // lines in the torus map. The graph nodes are the intersection points
    // of these two lines, and the arcs are the minimal line segments
    // connecting intersection points
    TFloat dx1 = nodeSpacing * vSize * hSize / nAct;
    TFloat dy1 = nodeSpacing * vSize * hSkew / nAct;
    TFloat dx2 = nodeSpacing * vSkew * hSize / nAct;
    TFloat dy2 = nodeSpacing * vSize * hSize / nAct;

    TFloat width  = hSize*nodeSpacing;
    TFloat height = vSize*nodeSpacing;

    X.Layout_SetBoundingInterval(0,0.0,width);
    X.Layout_SetBoundingInterval(1,0.0,height);

    // By construction, n is divisible by gcd
    long gcd = ggt(vSize,hSkew);
    TNode numOrbits = abs(gcd);
    TNode orbitLength = n/numOrbits;


    // First, generate the arcs spanned by (dx1,dy1)
    for (TNode u=0;u<n;++u)
    {
        TFloat CX0 = u*dx1 + (u/(n/gcd))*dx2;
        TFloat CY0 = u*dy1 + (u/(n/gcd))*dy2;

        // Reduce the coordinates of u
        CX0 -= floor(CX0/width)*width;
        CY0 -= floor(CY0/height)*height;

        X.SetC(u,0,CX0);
        X.SetC(u,1,CY0);

        // If x1 and x2 are not relatively prime, the node set falls into gcd orbits
        TNode v = (u+1)%(n/gcd)+(u/(n/gcd))*(n/gcd);
        InsertArc(u,v);
    }

    // Next, generate the arcs spanned by (dx2,dy2)
    for (TNode u=0;u<n;++u)
    {
        TFloat CX1 = C(u,0)+dx2;
        TFloat CY1 = C(u,1)+dy2;

        // Reduce the coordinates of u
        CX1 -= floor(CX1/width) *width;
        CY1 -= floor(CY1/height)*height;

        for (TNode v=0;v<n;++v)
        {
            if (   (   fabs(C(v,0)-CX1)>nodeSpacing/2.0
                    && fabs(C(v,0)-CX1+width)>nodeSpacing/2.0
                    && fabs(C(v,0)-CX1-width)>nodeSpacing/2.0
                   )
                || (   fabs(C(v,1)-CY1)>nodeSpacing/2.0
                    && fabs(C(v,1)-CY1+height)>nodeSpacing/2.0
                    && fabs(C(v,1)-CY1-height)>nodeSpacing/2.0
                   )
               )
            {
                continue;
            }

            // So we have determined the index v of the node at (CX1,CY1)

            InsertArc(u,v);
            break;
        }
    }

    if (facets!=TORUS_QUADRILATERAL)
    {
        // Generate the arcs spanned by (dx1+dx2,dy1+dy2)

        for (TArc a=TNode(n);a<TNode(2*n);++a)
        {
            TNode u = StartNode(2*a);
            TNode v = EndNode(2*a);

            v = (v+1)%orbitLength+(v/orbitLength)*orbitLength;
            InsertArc(u,v);
        }
    }


    for (TNode u=0;u<n;++u)
    {
        TFloat cx = C(u,0)+nodeSpacing/2.0;
        TFloat cy = C(u,1)+nodeSpacing/2.0;
        X.SetC(u,0,cx-floor(cx/width)*width);
        X.SetC(u,1,cy-floor(cy/height)*height);
    }


    // Set up the routing of all arcs which roll over the boundary
    TNode layoutNode[8];

    for (TArc a=0;a<m;++a)
    {
        TNode u = StartNode(2*a);
        TNode v = EndNode(2*a);

        TFloat ux = C(u,0);
        TFloat uy = C(u,1);
        TFloat vx = C(v,0);
        TFloat vy = C(v,1);

        TFloat dx = ((a<TArc(n) || a>=TArc(2*n)) ? dx1 : 0.0) + ((a>=TArc(n)) ? dx2 : 0.0);
        TFloat dy = ((a<TArc(n) || a>=TArc(2*n)) ? dy1 : 0.0) + ((a>=TArc(n)) ? dy2 : 0.0);
        TFloat ex = vx-ux;
        TFloat ey = vy-uy;

        if (fabs(dx-ex)>0.001*nodeSpacing || fabs(dy-ey)>0.001*nodeSpacing)
        {
            // Rollover case

            // Determine the relative portions of a to be displayed
            const TFloat eps = 10e-12;
            TFloat lambda1x = fabs(dx)<eps*width  ? InfFloat : (dx<0.0) ? -ux/dx :  (width -ux)/dx;
            TFloat lambda2x = fabs(dx)<eps*width  ? InfFloat : (dx>0.0) ?  vx/dx : -(width -vx)/dx;
            TFloat lambda1y = fabs(dy)<eps*height ? InfFloat : (dy<0.0) ? -uy/dy :  (height-uy)/dy;
            TFloat lambda2y = fabs(dy)<eps*height ? InfFloat : (dy>0.0) ?  vy/dy : -(height-vy)/dy;
            TFloat lambda1 = (lambda1x>lambda1y) ? lambda1y : lambda1x;
            TFloat lambda2 = (lambda2x>lambda2y) ? lambda2y : lambda2x;

            if (lambda1+lambda2>0.999)
            {
                // Simple rollover
                X.ProvideEdgeControlPoints(a,layoutNode,5,PORTS_IMPLICIT);
                SetC(ArcLabelAnchor(2*a),0,ux+0.5*lambda1*dx);
                SetC(ArcLabelAnchor(2*a),1,uy+0.5*lambda1*dy);
                SetC(layoutNode[1],0,ux+lambda1*dx);
                SetC(layoutNode[1],1,uy+lambda1*dy);
                SetC(layoutNode[2],0,-InfFloat);
                SetC(layoutNode[2],1,-InfFloat);
                SetC(layoutNode[3],0,vx-lambda2*dx);
                SetC(layoutNode[3],1,vy-lambda2*dy);
            }
            else
            {
                // Double rollover (both vertical and horizontal)
                X.ProvideEdgeControlPoints(a,layoutNode,8,PORTS_IMPLICIT);
                SetC(ArcLabelAnchor(2*a),0,ux+0.5*lambda1*dx);
                SetC(ArcLabelAnchor(2*a),1,uy+0.5*lambda1*dy);
                SetC(layoutNode[1],0,ux+lambda1*dx);
                SetC(layoutNode[1],1,uy+lambda1*dy);
                SetC(layoutNode[2],0,-InfFloat);
                SetC(layoutNode[2],1,-InfFloat);

                if (lambda1x>lambda1y)
                {
                    SetC(layoutNode[3],0,ux+lambda1*dx);
                    SetC(layoutNode[3],1,uy+lambda1*dy+((dy<0.0) ?  height : -height));
                    SetC(layoutNode[4],0,vx-lambda2*dx+((dx<0.0) ? - width :   width));
                    SetC(layoutNode[4],1,vy-lambda2*dy);
                }
                else
                {
                    SetC(layoutNode[3],0,ux+lambda1*dx+((dx<0.0) ?   width : - width));
                    SetC(layoutNode[3],1,uy+lambda1*dy);
                    SetC(layoutNode[4],0,vx-lambda2*dx);
                    SetC(layoutNode[4],1,vy-lambda2*dy+((dy<0.0) ? -height :  height));
                }

                SetC(layoutNode[5],0,-InfFloat);
                SetC(layoutNode[5],1,-InfFloat);
                SetC(layoutNode[6],0,vx-lambda2*dx);
                SetC(layoutNode[6],1,vy-lambda2*dy);
            }
        }
    }


    if (facets==TORUS_HEXAGONAL)
    {
        // Cancel every third node
        for (TNode u=0;u<n;++u)
        {
            TNode orbitNumber = u/orbitLength;

            if ((u+orbitNumber)%3==2) X.CancelNode(u);
        }

        // Delete the canceled nodes
        X.DeleteNodes();
    }
}


moebiusLadder::moebiusLadder(TNode _n,goblinController& _CT) throw(ERRejected) :
    managedObject(_CT), sparseGraph(_n,_CT)
{
    LogEntry(LOG_MAN,"Generating Moebius ladder...");

    X.SetCapacity(n,(n%2==0) ? n*3/2 : n*2,n+2);

    Layout_ConvertModel(LAYOUT_FREESTYLE_CURVES);

    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);
    TFloat radius = nodeSpacing*n/10.0;

    for (TNode i=0;i<n;++i)
    {
        SetC(i,0,radius*sin(i*2*PI/n));
        SetC(i,1,radius*cos(i*2*PI/n));

        InsertArc(i,(i+1)%n);
    }

    X.SetFirst(0,0);

    TNode layoutNode[5];

    if (n%2==0)
    {
        // Even number of nodes: 3-regular, known as Mobius ladder

        for (TNode j=0;j<(n/2);++j)
        {
            TArc a = InsertArc(j,(j+(n/2))%n);
            X.ProvideEdgeControlPoints(a,layoutNode,5,PORTS_IMPLICIT);

            // The two visible control points must be antipodal
            SetC(layoutNode[1],0,1.5*radius*sin(j*2*PI/n));
            SetC(layoutNode[1],1,1.5*radius*cos(j*2*PI/n));
            SetC(layoutNode[3],0,1.5*radius*sin((j+n/2.0)*2*PI/n));
            SetC(layoutNode[3],1,1.5*radius*cos((j+n/2.0)*2*PI/n));

            // Later, this layout node will be made invisible
            // by assinging a bounding perimeter 2*radius
            SetC(layoutNode[2],0,3.0*radius*sin((j+n/4.0)*2*PI/n));
            SetC(layoutNode[2],1,3.0*radius*cos((j+n/4.0)*2*PI/n));
        }
    }
    else
    {
        // Odd number of nodes: 4-regular, known as Mobius lattice

        TNode k = n/2;

        for (TNode j=0;j<n;++j)
        {
            TNode target = (j+k)%n;
            X.SetFirst(target,2*target);
            TArc a = InsertArc(j,target);

            X.ProvideEdgeControlPoints(a,layoutNode,5,PORTS_IMPLICIT);

            // The two visible control points must be antipodal
            SetC(layoutNode[1],0,1.5*radius*sin((j  -0.25)*2*PI/n));
            SetC(layoutNode[1],1,1.5*radius*cos((j  -0.25)*2*PI/n));
            SetC(layoutNode[3],0,1.5*radius*sin((j+k+0.25)*2*PI/n));
            SetC(layoutNode[3],1,1.5*radius*cos((j+k+0.25)*2*PI/n));

            // Later, this layout node will be made invisible
            // by assinging a bounding perimeter 2*radius
            SetC(layoutNode[2],0,3.0*radius*sin((j+k/2.0+n/2.0)*2*PI/n));
            SetC(layoutNode[2],1,3.0*radius*cos((j+k/2.0+n/2.0)*2*PI/n));
        }
    }

    for (TNode j=0;j<n;++j) X.SetFirst(j,2*j);

    MarkExteriorFace(0);

    X.Layout_SetBoundingInterval(0,-2*radius,2*radius);
    X.Layout_SetBoundingInterval(1,-2*radius,2*radius);
}


generalizedPetersen::generalizedPetersen(TNode perimeter,TNode skew,goblinController& _CT) throw(ERRejected) :
    managedObject(_CT), sparseGraph(2*perimeter,_CT)
{
    LogEntry(LOG_MAN,"Generating Petersen graph...");

    X.SetCapacity(n,3*perimeter);

    Layout_ConvertModel(LAYOUT_FREESTYLE_CURVES);

    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);
    TFloat radius = nodeSpacing*n/10.0;

    for (TNode i=0;i<perimeter;++i)
    {
        SetC(i,0,2*radius*sin(i*2*PI/perimeter));
        SetC(i,1,2*radius*cos(i*2*PI/perimeter));

        InsertArc(i,(i+1)%perimeter);
    }

    for (TNode i=0;i<perimeter;++i)
    {
        SetC(perimeter+i,0,radius*sin(i*2*PI/perimeter));
        SetC(perimeter+i,1,radius*cos(i*2*PI/perimeter));

        InsertArc(perimeter+i,perimeter+(i+skew)%perimeter);
    }

    for (TNode i=0;i<perimeter;++i)
    {
        InsertArc(i,perimeter+i);
    }

    X.Layout_SetBoundingInterval(0,-3*radius,3*radius);
    X.Layout_SetBoundingInterval(1,-3*radius,3*radius);
}


static TNode GetNodeIndexFromHexGridPos(TNode dim,TNode k,TNode l) throw()
{
    if (k>=2*dim-1 || l>=2*dim-1) return NoNode;

    if (k<dim-1)
    {
        if (l+k<dim-1) return NoNode;

        return k*(2*dim-1)        // #Nodes in the square grid, atop of u
            - (dim-1)*dim/2       // #Nodes in the upper left triangle omitted when moving from square to hexagon
            + (dim-k-1)*(dim-k)/2 // #Nodes in this triangle not atop of u
            + l-(dim-1-k);        // #Nodes in this row left-hand of u
    }
    else
    {
        if (k+l>3*dim-3) return NoNode;

        return k*(2*dim-1)        // #Nodes in the square grid, atop of u
            - (dim-1)*dim/2       // #Nodes in the upper left triangle omitted when moving from square to hexagon
            - (k-dim)*(k-dim+1)/2 // #Nodes in the lower right triangle omitted when moving from square to hexagon
                                  // which are atop of u
            + l;                  // #Nodes in this row left-hand of u
    }
}


gridCompletion::gridCompletion(TNode dim,TOptShape shape,goblinController &_CT) throw(ERRejected) :
    managedObject(_CT),
    sparseGraph(TNode(0),_CT)
{
    LogEntry(LOG_MAN,"Generating grid completion...");

    Layout_ConvertModel(LAYOUT_FREESTYLE_POLYGONES);
    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    dim = (dim<2) ? 2 : dim;

    if (shape==SHAPE_HEXAGONAL)
    {
        // Regular hexagon shape. Modeled in a square grid of edge length d
        TNode d = 2*dim-1;

        // Iterate on the rows
        for (TNode k=0;k<d;k++)
        {
            // Iterate on the columns
            for (TNode l=0;l<d;l++)
            {
                TNode u = GetNodeIndexFromHexGridPos(dim,k,l);

                if (u==NoNode) continue;

                if (InsertNode()!=u)
                    InternalError("gridCompletion","Inconsistent node indices");

                X.SetC(u,0,nodeSpacing*(l+0.5*k-TFloat(dim-1)*1.5));
                X.SetC(u,1,nodeSpacing*cos(PI/6.0)*(k-TFloat(dim-1)));

                // Add incidences for nodes in the same row
                for (TNode j=0;j<l;++j)
                {
                    TNode v = GetNodeIndexFromHexGridPos(dim,k,j);

                    if (v!=NoNode) InsertArc(v,u);
                }

                // Add incidences for nodes in the same column
                for (TNode j=0;j<k;++j)
                {
                    TNode v = GetNodeIndexFromHexGridPos(dim,j,l);

                    if (v!=NoNode) InsertArc(v,u);
                }

                // Add incidences for nodes in the same backward diagonal
                for (TNode j=0;j<k;++j)
                {
                    TNode v = GetNodeIndexFromHexGridPos(dim,j,l+(k-j));

                    if (v!=NoNode) InsertArc(v,u);
                }
            }
        }

        X.Layout_SetBoundingInterval(0,-TFloat(dim)*nodeSpacing,TFloat(dim)*nodeSpacing);
        X.Layout_SetBoundingInterval(1,-TFloat(dim)*nodeSpacing*cos(PI/6.0),TFloat(dim)*nodeSpacing*cos(PI/6.0));
    }
    else if (shape==SHAPE_SQUARE)
    {
        // Chessboard case

        // Iterate on the rows
        for (TNode k=0;k<dim;++k)
        {
            // Iterate on the columns
            for (TNode l=0;l<dim;++l)
            {
                TNode u = k*dim+l;

                if (InsertNode()!=u)
                    InternalError("gridCompletion","Inconsistent node indices");

                X.SetC(u,0,nodeSpacing*l);
                X.SetC(u,1,nodeSpacing*k);

                // Add incidences for nodes in the same row
                for (TNode j=0;j<l;++j)
                {
                    TNode v = k*dim+j;
                    InsertArc(v,u);
                }

                // Add incidences for nodes in the same column
                for (TNode j=0;j<k;++j)
                {
                    TNode v = j*dim+l;
                    InsertArc(v,u);
                }

                // Add incidences for nodes in the same forward diagonal
                for (TNode j=0;j<k && j<l;++j)
                {
                    TNode v = (k<l) ? j*dim+(l-k+j) : (k-l+j)*dim+j;
                    InsertArc(v,u);
                }

                // Add incidences for nodes in the same backward diagonal
                for (TNode j=0;j<k;++j)
                {
                    if (k+l-j>=dim) continue;

                    TNode v = j*dim+(k+l-j);
                    InsertArc(v,u);
                }
            }
        }

        X.Layout_SetBoundingInterval(0,-nodeSpacing,dim*nodeSpacing);
        X.Layout_SetBoundingInterval(1,-nodeSpacing,dim*nodeSpacing);
    }
    else // if (shape==SHAPE_TRIANGULAR)
    {
        // Regular triangular shape. Modeled in a square grid of edge length _k

        // Iterate on the rows
        for (TNode k=0;k<dim;++k)
        {
            // Iterate on the columns
            for (TNode l=0;l<dim-k;++l)
            {
                TNode u = k*dim-(k-1)*k/2+l;

                if (InsertNode()!=u)
                    InternalError("gridCompletion","Inconsistent node indices");

                X.SetC(u,0,nodeSpacing*(l+0.5*k));
                X.SetC(u,1,nodeSpacing*cos(PI/6.0)*k);

                // Add incidences for nodes in the same row
                for (TNode j=0;j<l;++j)
                {
                    TNode v = k*dim-(k-1)*k/2+j;
                    InsertArc(v,u);
                }

                // Add incidences for nodes in the same column
                for (TNode j=0;j<k;++j)
                {
                    TNode v = j*dim-(j-1)*j/2+l;
                    InsertArc(v,u);
                }

                // Add incidences for nodes in the same backward diagonal
                for (TNode j=0;j<k;++j)
                {
                    TNode v = j*dim-(j-1)*j/2+(k+l-j);
                    InsertArc(v,u);
                }
            }
        }

        X.Layout_SetBoundingInterval(0,-nodeSpacing,dim*nodeSpacing);
        X.Layout_SetBoundingInterval(1,-nodeSpacing,dim*nodeSpacing*cos(PI/6.0));
    }

    IncidenceOrderFromDrawing();
}


permutationGraph::permutationGraph(TNode numNodes,TNode* map,goblinController& _CT) throw() :
    managedObject(_CT),
    sparseGraph(TNode(numNodes),_CT)
{
    Layout_ConvertModel(LAYOUT_FREESTYLE_POLYGONES);

    TNode* nodeColour = NULL;

    if (map)
    {
        LogEntry(LOG_MAN,"Generating permutation graph...");

        nodeColour = RawNodeColours();

        for (TNode u=0;u<n;++u) nodeColour[u] = map[u];
    }
    else
    {
        LogEntry(LOG_MAN,"Generating random permutation graph...");

        nodeColour = RandomNodeOrder();
    }

    for (TNode v=0;v<n;++v)
    {
        for (TNode u=0;u<v;++u)
        {
            if (nodeColour[v]>nodeColour[u]) InsertArc(u,v);
        }
    }

    Layout_Circular();
}


thresholdGraph::thresholdGraph(TNode numNodes,TFloat threshold,TFloat* nodeWeight,
    goblinController& _CT) throw() :
    managedObject(_CT),
    sparseGraph(TNode(numNodes),_CT)
{
    LogEntry(LOG_MAN,"Generating threshold graph...");
    GenerateThis(threshold,nodeWeight,0,0);
}


thresholdGraph::thresholdGraph(TNode numNodes,TFloat threshold,
    long randMin,long randMax,goblinController& _CT) throw() :
    managedObject(_CT),
    sparseGraph(TNode(numNodes),_CT)
{
    LogEntry(LOG_MAN,"Generating random threshold graph...");
    GenerateThis(threshold,NULL,randMin,randMax);
}


void thresholdGraph::GenerateThis(TFloat threshold,TFloat* nodeWeight,
    long randMin,long randMax) throw()
{
    Layout_ConvertModel(LAYOUT_FREESTYLE_POLYGONES);

    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    for (TNode v=0;v<n;++v)
    {
        SetC(v,0,nodeWeight ? nodeWeight[v] : TFloat(randMin)+CT.Rand(randMax-randMin+1));
        SetC(v,1,nodeSpacing*v);

        for (TNode u=0;u<v;++u)
        {
            if (C(u,0)+C(v,0)>=threshold) InsertArc(u,v);
        }
    }

    if (!nodeWeight)
    {
        for (TNode v=0;v<n;++v) SetC(v,0,nodeSpacing*C(v,0));

        X.Layout_SetBoundingInterval(0,nodeSpacing*(randMin-1),nodeSpacing*(randMax+1));
    }

    X.Layout_SetBoundingInterval(1,-nodeSpacing,n*nodeSpacing);
}


intervalGraph::intervalGraph(TNode numNodes,TFloat* minRange,TFloat* maxRange,
    goblinController& _CT) throw() :
    managedObject(_CT),
    sparseGraph(TNode(numNodes),_CT)
{
    LogEntry(LOG_MAN,"Generating interval graph...");
    GenerateThis(minRange,maxRange,0);
}


intervalGraph::intervalGraph(TNode numNodes,TIndex valueRange,goblinController& _CT) throw() :
    managedObject(_CT),
    sparseGraph(TNode(numNodes),_CT)
{
    LogEntry(LOG_MAN,"Generating random interval graph...");
    GenerateThis(NULL,NULL,valueRange);
}


void intervalGraph::GenerateThis(TFloat* minRange,TFloat* maxRange,TIndex valueRange) throw()
{
    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);

    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);
    SyncSpacingParameters(TokLayoutNodeSpacing,nodeSpacing);

    for (TNode v=0;v<n;++v)
    {
        TFloat min = minRange ? minRange[v] : TFloat(CT.Rand(valueRange)*nodeSpacing);
        TFloat max = maxRange ? maxRange[v] : TFloat(CT.Rand(valueRange)*nodeSpacing);

        if (min>max)
        {
            TFloat swap = min;
            min = max;
            max = swap;
        }

        SetC(v,0,(min+max)/2.0);
        SetC(v,1,nodeSpacing*v);

        TNode w = X.InsertThreadSuccessor(v);
        SetC(w,0,(max-min)/2);
        SetC(w,1,0);

        for (TNode u=0;u<v;++u)
        {
            TFloat medU = C(u,0);
            w = ThreadSuccessor(u);
            TFloat maxU = medU+C(w,0);
            TFloat minU = medU-C(w,0);

            if (min>maxU) continue;
            if (minU>max) continue;

            InsertArc(u,v);
        }
    }

    Layout_ConvertModel(LAYOUT_VISIBILITY);

    X.Layout_SetBoundingInterval(0,-nodeSpacing,valueRange*nodeSpacing);
    X.Layout_SetBoundingInterval(1,-nodeSpacing,n*nodeSpacing);

    SetLayoutParameter(TokLayoutNodeShapeMode,NODE_SHAPE_BOX);
    SetLayoutParameter(TokLayoutArcVisibilityMode,graphDisplayProxy::ARC_DISPLAY_HIDE_ALL);
}
