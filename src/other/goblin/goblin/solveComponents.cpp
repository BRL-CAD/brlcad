
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveComponents.cpp
/// \brief  A collection of methods to determine node and edge fixed order connected componemts

#include "abstractMixedGraph.h"
#include "sparseGraph.h"
#include "sparseDigraph.h"
#include "staticStack.h"
#include "moduleGuard.h"


bool abstractMixedGraph::StronglyConnected() throw()
{
    if (m==0 || (COrientation() && Orientation(0)==0))
    {
        // Graph is undirected, at least implicitly
        return Connected();
    }

    moduleGuard M(ModStrongComponents,*this,
        "Computing strongly connected components...");

    #if defined(_PROGRESS_)

    M.InitProgressCounter(2*n);

    #endif

    #if defined(_LOGGING_)

    OpenFold();

    #endif

    TNode i = 0;

    TNode* nodeColour = InitNodeColours();
    TArc*  pred = InitPredecessors();

    THandle H = Investigate();
    investigator &I = Investigator(H);
    staticStack<TNode> S(n,CT);
        // Ordering of the reverse DFS operations

    #if defined(_TRACING_)

    staticStack<TNode> Recovery(n,CT);
        // Needed for reinitialization of predecessors

    #endif

    bool* marked = new bool[n];
    for (TNode v=0;v<n;v++) marked[v] = false;

    for (TNode r=0;r<n;r++)
    {
        if (nodeColour[r]!=NoNode) continue;

        #if defined(_LOGGING_)

        CloseFold();
        LogEntry(LOG_METH2,"Generating forward DFS tree...");
        OpenFold();

        #endif

        TNode u = r;

        while (true)
        {
            if (I.Active(u))
            {
                TArc a = I.Read(u);
                TNode v = EndNode(a);

                if (UCap(a^1)>0 && !Blocking(a^1) && pred[v]==NoArc && nodeColour[v]==NoNode && v!=r)
                {
                    pred[v] = a;
                    u = v;
                }
            }
            else
            {
                // Backtracking

                S.Insert(u);

                #if defined(_TRACING_)

                Recovery.Insert(u);

                #endif

                I.Reset(u);
                marked[u] = true;

                if (u==r) break;
                else u = StartNode(pred[u]);
            }
        }

        M.Trace(S.Cardinality());

        #if defined(_TRACING_)

        while (!Recovery.Empty())
        {
            u = Recovery.Delete();
            pred[u] = NoArc;
        }

        #endif

        while (!S.Empty())
        {
            TNode s = S.Delete();

            if (nodeColour[s]!=NoNode || !marked[s]) continue;

            u = s;

            #if defined(_LOGGING_)

            THandle LH = NoHandle;

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Component %lu: %lu",
                    static_cast<unsigned long>(i),static_cast<unsigned long>(u));
                LH = LogStart(LOG_METH2,CT.logBuffer);
            }

            #endif

            while (true)
            {
                if (I.Active(u))
                {
                    TArc a = I.Read(u);

                    TNode v = EndNode(a);

                    if (UCap(a)>0 && !Blocking(a) && nodeColour[v]==NoNode && marked[v] && v!=s)
                    {
                        pred[v] = a;
                        u = v;
                        marked[u] = false;

                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(u));
                            LogAppend(LH,CT.logBuffer);
                        }

                        #endif
                    }
                }
                else
                {
                    // Backtracking

                    #if defined(_TRACING_)

                    Recovery.Insert(u);

                    #endif

                    I.Reset(u);
                    nodeColour[u] = i;

                    if (u==s) break;
                    else u = StartNode(pred[u]);
                }
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1) LogEnd(LH);

            #endif

            M.Trace(Recovery.Cardinality());

            #if defined(_TRACING_)

            while (!Recovery.Empty())
            {
                u = Recovery.Delete();
                pred[u] = NoArc;
            }

            #endif

            i++;
        }
    }

    Close(H);
    delete[] marked;

    #if defined(_LOGGING_)

    CloseFold();

    #endif

    M.SetBounds(i,i);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Graph has %lu strongly connected components",
            static_cast<unsigned long>(i));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return (i<=1);
}


bool abstractMixedGraph::Connected() throw()
{
    moduleGuard M(ModComponents,*this,"Computing connected components...");

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n);

    #endif

    TNode i = 0;

    TNode* nodeColour = InitNodeColours();
    TArc*  pred = InitPredecessors();

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode r=0;r<n;r++)
    {
        if (nodeColour[r]!=NoNode) continue;

        TNode u = r;

        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Component %lu: %lu",
                static_cast<unsigned long>(i),static_cast<unsigned long>(u));
            LH = LogStart(LOG_METH2,CT.logBuffer);
        }

        #endif

        while (true)
        {
            if (I.Active(u))
            {
                TArc a = I.Read(u);

                TNode v = EndNode(a);

                if (UCap(a)>0 && pred[v]==NoArc && v!=r)
                {
                    pred[v] = a;
                    u = v;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(u));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif
                }
            }
            else
            {
                // Backtracking

                nodeColour[u] = i;

                if (u==r) break;
                else u = StartNode(pred[u]);
            }
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH);

        #endif

        i++;
    }

    Close(H);

    M.Trace(n);
    M.SetBounds(i,i);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Graph has %lu connected components",
            static_cast<unsigned long>(i));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return (i<=1);
}


abstractMixedGraph::TRetDFS abstractMixedGraph::CutNodes(TArc rootArc,TNode* order,TArc* lowArc) throw()
{
    moduleGuard M(ModBiconnectivity,*this,"Computing blocks and cut nodes... ");

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n);

    #endif

    // If the graph is planar, start the search with an exterior arcs preferably
    TArc localRootArc = rootArc;
    for (TNode v=0;v<n && localRootArc>=2*m;v++) localRootArc = First(v);

    if (localRootArc==NoArc)
    {
        M.Shutdown(LOG_RES,"...Graph is void");

        return (n<=1) ? DFS_BICONNECTED : DFS_DISCONNECTED;
    }

    TNode i = 1; // Current ordinal number
    TNode nBlocks = 0; // Current edge colour index
    TNode c = 0; // Current node colour index
    TNode nComponents = 1;
    TNode nLeafs = 0;

    TArc*  pred       = InitPredecessors();
    TNode* nodeColour = InitNodeColours();
    TArc*  edgeColour = InitEdgeColours();

    bool localOrder = (order==NULL);
    if (localOrder) order = new TNode[n];

    bool localLow = (lowArc==NULL);
    if (localLow) lowArc = new TArc[n];

    for (TNode v=0;v<n;v++) lowArc[v] = NoArc;

    TNode* low = new TNode[n];

    staticStack<TNode> Q(n,CT);

    THandle H = Investigate();
    investigator &I = Investigator(H);
    bool firstIteration = true;
    TNode r = 0;

    while (r<n)
    {
        TNode u = r;

        if (firstIteration)
        {
            // Force the root arc to be the first DFS tree arc
            // (this is needed for the st-numbering algorithm)

            TNode t = StartNode(localRootArc);
            TNode s = EndNode(localRootArc);

            low[t] = order[t] = i++;
            low[s] = order[s] = i++;
            pred[s] = localRootArc;

            r = t;
            u = s;

            Q.Insert(t);
            if (t!=s) Q.Insert(s);
        }
        else if (nodeColour[r]!=NoNode)
        {
            // Node r has already been reached
            r++;
            continue;
        }
        else
        {
            nComponents++;
            low[r] = order[r] = i++;
        }

        while (true)
        {
            if (I.Active(u))
            {
                TArc a = I.Read(u);

                TNode v = EndNode(a);

                if (UCap(a)>0)
                {
                    if (pred[v]==NoArc && v!=r)
                    {
                        // Tree edge

                        pred[v] = a;
                        order[v] = i++;
                        low[v] = order[v];
                        lowArc[v] = NoArc;
                        Q.Insert(v);
                        u = v;
                    }
                    else if (pred[u]!=(a^1) && low[u]>order[v])
                    {
                        // Backward edge

                        low[u] = order[v];
                        lowArc[u] = a;
                    }
                }
            }
            else
            {
                // Backtracking

                if (u==r)
                {
                    I.Reset(u);

                    while (I.Active(u))
                    {
                        TArc a = I.Read(u);

                        if (edgeColour[a>>1]==NoArc) edgeColour[a>>1] = nBlocks;
                    }

                    break;
                }

                TNode w = StartNode(pred[u]);
                if (low[u]<order[w])
                {
                    if (low[w]>low[u])
                    {
                        low[w] = low[u];
                        lowArc[w] = lowArc[u];
                    }
                }
                else
                {
                    // A block has just been completed
                    // w is the root of the new block

                    nBlocks++;

                    #if defined(_LOGGING_)

                    THandle LH = NoHandle;

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"Block %lu: %lu,",
                            static_cast<unsigned long>(nBlocks),
                            static_cast<unsigned long>(w));
                        LH = LogStart(LOG_METH2,CT.logBuffer);
                    }

                    #endif

                    // Colour the nodes and arcs of the block
                    // Check if the block forms a leaf in the cut tree

                    TNode v = Q.Delete();
                    bool leaf = true;
                    TNode cardinality = 0;

                    while (v!=u)
                    {
                        if (nodeColour[v]==NoNode)
                        {
                            nodeColour[v] = nBlocks;
                            cardinality++;
                        }
                        else leaf = false;

                        I.Reset(v);
                        while (I.Active(v))
                        {
                            TArc a = I.Read(v);

                            if (edgeColour[a>>1]==NoArc) edgeColour[a>>1] = nBlocks;
                        }

                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,"%lu,",static_cast<unsigned long>(v));
                            LogAppend(LH,CT.logBuffer);
                        }

                        #endif

                        v = Q.Delete();
                    }

                    if (nodeColour[u]==NoNode)
                    {
                        nodeColour[u] = nBlocks;
                        cardinality++;
                    }
                    else leaf = false;

                    if (leaf) nLeafs++;

                    I.Reset(u);

                    while (I.Active(u))
                    {
                        TArc a = I.Read(u);

                        if (edgeColour[a>>1]==NoArc) edgeColour[a>>1] = nBlocks;
                    }

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"%lu",static_cast<unsigned long>(u));
                        LogEnd(LH,CT.logBuffer);
                    }

                    #endif

                    if (w==r && nodeColour[r]==NoNode)
                    {
                        nodeColour[r] = nBlocks;
                        cardinality++;
                    }
                    else
                    {
                        // Mark w as a cut node

                        if (nodeColour[w]==NoNode) cardinality++;

                        nodeColour[w] = CONN_CUT_NODE;
                        c++;
                    }

                    M.Trace(cardinality);
                }

                u = w;
            }
        }

        if (firstIteration)
        {
            firstIteration = false;
            r = 0;
        }
        else r++;
    }

    Close(H);

    delete[] low;

    if (localLow) delete[] lowArc;
    if (localOrder) delete[] order;

    if (localLow) M.SetBounds(nBlocks,nBlocks);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,
            "...Graph has %lu components, %lu blocks and %lu cut nodes",
            static_cast<unsigned long>(nComponents),
            static_cast<unsigned long>(nBlocks),
            static_cast<unsigned long>(c));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    if (nComponents>1)                          return DFS_DISCONNECTED;
    if (nBlocks==1)                             return DFS_BICONNECTED;
    if (   nLeafs>1
        || rootArc==NoArc
        || nodeColour[StartNode(rootArc)]==0
       )                                        return DFS_MULTIPLE_BLOCKS;

    return DFS_ALMOST_BICONNECTED;
}


bool abstractMixedGraph::Biconnected() throw()
{
    moduleGuard M(ModBiconnectivity,*this,"Computing 2-connected components...");

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n+m);

    #endif

    TNode* order = new TNode[n];
    TArc* low = new TArc[n];

    CutNodes(NoArc,order,low);

    InitPartition();
    for (TNode u=0;u<n;u++) Bud(u);

    TArc* pred = GetPredecessors();
    TNode* nodeColour = GetNodeColours();

    TNode ncomponents = n;
    M.SetBounds(1,n);

    for (TArc a=0;a<m;a++)
    {
        TNode v = StartNode(2*a);
        TNode w = EndNode(2*a);

        if ((nodeColour[v]==nodeColour[w] && nodeColour[v]!=CONN_CUT_NODE) ||
            (pred[w]!=2*a && pred[v]!=2*a+1) ||
            (pred[w]==2*a && low[w]!=NoArc && order[EndNode(low[w])]<order[w]) ||
            (pred[v]==2*a+1 && low[v]!=NoArc && order[EndNode(low[v])]<order[v]))
        {
            if (Find(v)!=Find(w))
            {
                ncomponents--;
                Merge(w,v);
                M.SetUpperBound(ncomponents);
            }
        }
    }

    for (TNode u=0;u<n;u++) nodeColour[u] = nodeColour[Find(u)];

    delete[] order;
    delete[] low;

    M.SetLowerBound(ncomponents);
    M.Trace(m);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Graph has %lu 2-edge connected components",
            static_cast<unsigned long>(ncomponents));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return (ncomponents==1);
}


bool abstractMixedGraph::STNumbering(TArc rootArc,TNode source,TNode target) throw(ERRejected)
{
    moduleGuard M(ModBiconnectivity,*this,"Computing st-numbering...");

    #if defined(_PROGRESS_)

    M.InitProgressCounter(m+2*n);

    #endif

    TNode* order = new TNode[n];
    TArc* low = new TArc[n];

    // Obtain a consistent configuration of source, target and rootArc
    if (rootArc<2*m)
    {
        // Layout application use case: A root arc has been passed
        // which will be drawn on the exterior face by the calling method
        source = StartNode(rootArc);
        target = EndNode(rootArc);
    }
    else if (source<n && target<n)
    {
        // Two nodes have been passed. If the graph is embedded,
        // this chooses an exterior rootArc:
        rootArc = First(source);
    }
    else if (ExteriorArc()!=NoArc)
    {
        // Layout application use case: Apply the current embedding
        // and the default root arc
        rootArc = ExteriorArc();
        source = StartNode(rootArc);
        target = EndNode(rootArc);
    }
    else
    {
        // Layout application use case, but no embedding exists.
        // Apply the default source and / or target node.

        if (source>=n) source = DefaultSourceNode();

        if (source>=n)
        {
            source = 0;
            rootArc = First(source);
            target = EndNode(rootArc);
        }
        else
        {
            rootArc = First(source);

            if (target>=n) target = DefaultTargetNode();

            if (target>=n) target = EndNode(rootArc);
        }
    }

    #if defined(_FAILSAVE_)

    if (source==target)
         Error(ERR_REJECTED,"STNumbering","Root arc may not be a loop");

    #endif


    TRetDFS retDFS = CutNodes(rootArc,order,low);

    bool feasible = (retDFS==DFS_BICONNECTED);
    TNode* nodeColour = GetNodeColours();

    if (   retDFS==DFS_ALMOST_BICONNECTED
        && nodeColour[source]!=nodeColour[target]
        && nodeColour[source]!=CONN_CUT_NODE
        && nodeColour[target]!=CONN_CUT_NODE
       )
    {
        // source and target are no cut nodes and occur in different blocks.
        // Verify that these blocks form the leaves in the block-cut tree
        feasible = true;
        TArc* edgeColour = GetEdgeColours();
        TNode targetCutNode = NoNode;
        TNode sourceCutNode = NoNode;

        for (TArc a=0;a<2*m && feasible;a++)
        {
            TNode v = StartNode(a);

            if (nodeColour[v]!=0) continue; 

            if (edgeColour[a>>1]==nodeColour[source])
            {
                if (sourceCutNode!=NoNode && v!=sourceCutNode)
                {
                    feasible = false;
                }
                else sourceCutNode = v;
            }

            if (edgeColour[a>>1]==nodeColour[target])
            {
                if (targetCutNode!=NoNode && v!=targetCutNode)
                {
                    feasible = false;
                }
                else targetCutNode = v;
            }
        }
    }

    if (feasible)
    {
        LogEntry(LOG_METH,"Computing ear decomposition...");

        #if defined(_LOGGING_)

        OpenFold();

        #endif

        TNode* nodeColour = InitNodeColours();
        TArc* edgeColour = InitEdgeColours();
        TArc* pred = GetPredecessors();
        TArc* Q = new TArc[n];

        for (TNode v=0;v<n;v++) Q[v] = NoArc;

        TNode nrVertex = 0;
        TArc nrPath = 1;

        staticStack<TNode> S1(n,CT);

        // The initial path is the (source,target)-path in the DFS tree defined by pred[]

        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Path 0 : %lu",static_cast<unsigned long>(target));
            LH = LogStart(LOG_METH2,CT.logBuffer);
        }

        #endif

        TNode w = target;
        TArc lenPath = 0;

        while (w!=source)
        {
            TArc a2 = pred[w];
            Q[w] = a2;
            edgeColour[a2>>1] = 0;
            w = StartNode(a2);
            Q[w] = a2^1;
            S1.Insert(w);
            lenPath++;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"[%lu]%lu",
                    static_cast<unsigned long>(a2^1),static_cast<unsigned long>(w));
                LogAppend(LH,CT.logBuffer);
            }

            #endif
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1) LogEnd(LH);

        #endif

        M.Trace(lenPath);

        THandle H = Investigate();
        investigator &I = Investigator(H);

        while (!S1.Empty())
        {
            TNode u = S1.Delete();

            while (I.Active(u))
            {
                TArc a = I.Read(u);

                if (pred[u]==(a^1) || edgeColour[a>>1]!=NoArc) continue;

                // Construct path Q from u to another visited node v

                TNode v = EndNode(a);

                TNode w = v;

                if (pred[v]==a)
                {
                    // Use the tree arc pred[v], the non-tree arc low[v]
                    // and the unused tree path connecting these arcs
                    w = EndNode(low[v]);
                    Q[w] = low[v];

                    v = StartNode(low[v]);

                    while (v!=u)
                    {
                        Q[v] = pred[v];
                        v = StartNode(pred[v]);
                    }
                }
                else if (order[v]>order[u])
                {
                    // v is a descendent of u, and some descendents of u
                    // have been already visited and charged with a Q[] label.
                    // Use the arc a and the tree path back to a visited node
                    while (Q[w]==NoArc) w = StartNode(pred[w]);

                    Q[v] = a;

                    while (v!=w)
                    {
                        TArc a2 = pred[v];

                        v = StartNode(a2);
                        Q[v] = a2^1;
                    }
                }
                else
                {
                    Q[w] = a;
                }

                // Push path nodes onto stack S1

                #if defined(_LOGGING_)

                THandle LH = NoHandle;

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Path %lu : %lu",
                        static_cast<unsigned long>(nrPath),static_cast<unsigned long>(w));
                    LH = LogStart(LOG_METH2,CT.logBuffer);
                }

                #endif

                TArc lenPath = 0;

                while (w!=u)
                {
                    TArc a2 = Q[w];
                    edgeColour[a2>>1] = nrPath;
                    w = StartNode(a2);
                    lenPath++;

                    if (w!=u) S1.Insert(w);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"[%lu]%lu",
                            static_cast<unsigned long>(a2^1),static_cast<unsigned long>(w));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif
                }

                #if defined(_LOGGING_)

                if (CT.logMeth>1) LogEnd(LH);

                #endif

                nrPath++;

                M.Trace(lenPath);
            }

            nodeColour[u] = nrVertex;
            nrVertex++;
        }

        nodeColour[target] = nrVertex;

        Close(H);

        delete[] Q;

        #if defined(_LOGGING_)

        CloseFold();

        #endif

        LogEntry(LOG_RES,"...Graph has been split into paths");
    }

    delete[] order;
    delete[] low;

    sprintf(CT.logBuffer, feasible ?
        "...(%lu,%lu)-numbering found" :
        "...No (%lu,%lu)-numbering exists" ,
        static_cast<unsigned long>(source),static_cast<unsigned long>(target));

    M.Shutdown(LOG_RES,CT.logBuffer);

    return feasible;
}


bool abstractMixedGraph::Connected(TCap k) throw()
{
    if (k==1) return Connected();
    if (k==2) return (CutNodes()==DFS_BICONNECTED);

    moduleGuard M(ModComponents,*this);

    ReleasePredecessors();

    TCap kAct = NodeConnectivity();

    return (kAct>=k);
}


bool abstractMixedGraph::EdgeConnected(TCap k) throw()
{
    if (k==1) return Connected();
    if (k==2) return Biconnected();

    sprintf(CT.logBuffer,"Computing %g-edge connected components...",k);
    moduleGuard M(ModComponents,*this,CT.logBuffer);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n-1,1);

    #endif

    M.SetBounds(1,n);

    ReleasePredecessors();
    TNode* nodeColour = InitNodeColours(0);

    TNode cNext = 1;
    TNode cCurrent = 0;
    TNode nMerged = 0;
    TNode* map = new TNode[n]; 
    TNode* revMap = new TNode[n]; 

    while (cCurrent<cNext && CT.SolverRunning())
    {
        TNode nCurrent = 0;
        for (TNode v=0;v<n;v++)
        {
            if (nodeColour[v]==cCurrent)
            {
                revMap[nCurrent] = v;
                map[v] = nCurrent;
                nCurrent++;
            }
            else map[v] = NoNode;
        }

        if (nCurrent>1)
        {
            sparseGraph G(nCurrent,CT);
            sparseRepresentation* GR =
                static_cast<sparseRepresentation*>(G.Representation());
            GR -> SetCapacity(nCurrent,m);

            for (TArc a=0;a<m;a++)
            {
                TNode x = StartNode(2*a);
                TNode y = EndNode(2*a);

                if (nodeColour[x]==cCurrent && nodeColour[y]==cCurrent)
                    G.InsertArc(map[x],map[y],UCap(2*a),0);
            }

            #if defined(_TRACING_)

            if (Dim()>0 && CT.traceLevel>0)
            {
                for (TNode u=0;u<nCurrent;u++)
                {
                    TNode v = revMap[u];

                    GR -> SetC(u,0,C(v,0));
                    GR -> SetC(u,1,C(v,1));
                }
            }

            #endif

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Trying to separate: %lu",
                    static_cast<unsigned long>(revMap[0]));
                THandle LH = LogStart(LOG_METH2,CT.logBuffer);

                for (TNode u=1;u<nCurrent;u++)
                {
                    sprintf(CT.logBuffer,",%lu",
                        static_cast<unsigned long>(revMap[u]));
                    LogAppend(LH,CT.logBuffer);
                }

                LogEnd(LH);
            }

            #endif

            if (G.EdgeConnectivity()>=k)
            {
                nMerged += nCurrent;
                cCurrent++;

                if (n-nMerged+cCurrent>=M.LowerBound())
                {
                    M.SetUpperBound(n-nMerged+cCurrent);
                }
            }
            else
            {
                for (TNode u=0;u<nCurrent;u++)
                {
                    TNode v = revMap[u];

                    if (G.NodeColour(u)==CONN_RIGHT_HAND) nodeColour[v] = cNext;
                }

                cNext++;

                if (cNext<=M.UpperBound())
                {
                    M.SetLowerBound(cNext);
                }
            }
        }
        else
        {
            nMerged += nCurrent;
            cCurrent++;

            if (n-nMerged+cCurrent>=M.LowerBound())
            {
                M.SetUpperBound(n-nMerged+cCurrent);
            }
        }

        #if defined(_PROGRESS_)

        M.SetProgressCounter(n-1+nMerged-cCurrent+cNext);

        #endif
    }

    delete[] map;
    delete[] revMap;

    sprintf(CT.logBuffer,"...Graph has %lu %g-edge connected components",
        static_cast<unsigned long>(cNext),k);
    M.Shutdown(LOG_RES,CT.logBuffer);

    return cNext==1;
}


bool abstractMixedGraph::StronglyConnected(TCap k) throw()
{
    if (k==1) return StronglyConnected();

    moduleGuard M(ModComponents,*this,moduleGuard::NO_INDENT);

    ReleasePredecessors();

    TCap kAct = StrongNodeConnectivity();

    return (kAct>=k);
}


bool abstractMixedGraph::StronglyEdgeConnected(TCap k) throw()
{
    if (k==1) return StronglyConnected();

    sprintf(CT.logBuffer,"Computing strong %g-edge connected components...",k);
    moduleGuard M(ModComponents,*this,CT.logBuffer);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n-1,1);

    #endif

    M.SetBounds(1,n);

    ReleasePredecessors();
    TNode* nodeColour = InitNodeColours(0);

    TNode cNext = 1;
    TNode cCurrent = 0;
    TNode nMerged = 0;
    TNode* map = new TNode[n]; 
    TNode* revMap = new TNode[n]; 

    while (cCurrent<cNext && CT.SolverRunning())
    {
        TNode nCurrent = 0;

        for (TNode v=0;v<n;v++)
        {
            if (nodeColour[v]==cCurrent)
            {
                revMap[nCurrent] = v;
                map[v] = nCurrent;
                nCurrent++;
            }
            else map[v] = NoNode;
        }

        if (nCurrent>1)
        {
            sparseDiGraph G(nCurrent,CT);
            sparseRepresentation* GR =
                static_cast<sparseRepresentation*>(G.Representation());
            GR -> SetCapacity(nCurrent,m);
            G.ImportLayoutData(*this);

            for (TArc a=0;a<m;a++)
            {
                TNode x = StartNode(2*a);
                TNode y = EndNode(2*a);

                if (nodeColour[x]==cCurrent && nodeColour[y]==cCurrent)
                    G.InsertArc(map[x],map[y],UCap(2*a),0);
            }

            #if defined(_TRACING_)

            if (Dim()>0 && CT.traceLevel>0)
            {
                for (TNode u=0;u<nCurrent;u++)
                {
                    TNode v = revMap[u];

                    GR -> SetC(u,0,C(v,0));
                    GR -> SetC(u,1,C(v,1));
                }
            }

            #endif

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Trying to separate: %lu",
                    static_cast<unsigned long>(revMap[0]));
                THandle LH = LogStart(LOG_METH2,CT.logBuffer);

                for (TNode u=1;u<nCurrent;u++)
                {
                    sprintf(CT.logBuffer,",%lu",
                        static_cast<unsigned long>(revMap[u]));
                    LogAppend(LH,CT.logBuffer);
                }

                LogEnd(LH);
            }

            #endif

            if (G.StrongEdgeConnectivity()>=k)
            {
                nMerged += nCurrent;
                cCurrent++;

                if (n-nMerged+cCurrent>=M.LowerBound())
                {
                    M.SetUpperBound(n-nMerged+cCurrent);
                }
            }
            else
            {
                for (TNode u=0;u<nCurrent;u++)
                {
                    TNode v = revMap[u];

                    if (G.NodeColour(u)==CONN_RIGHT_HAND) nodeColour[v] = cNext;
                }

                cNext++;

                if (cNext<=M.UpperBound())
                {
                    M.SetLowerBound(cNext);
                }
            }
        }
        else
        {
            nMerged += nCurrent;
            cCurrent++;

            if (n-nMerged+cCurrent>=M.LowerBound())
            {
                M.SetUpperBound(n-nMerged+cCurrent);
            }
        }

        #if defined(_PROGRESS_)

        M.SetProgressCounter(n-1+nMerged-cCurrent+cNext);

        #endif
    }

    delete[] map;
    delete[] revMap;

    sprintf(CT.logBuffer,
        "...Graph has %lu strong %g-edge connected components",
        static_cast<unsigned long>(cNext),k);
    M.Shutdown(LOG_RES,CT.logBuffer);

    return cNext==1;
}
