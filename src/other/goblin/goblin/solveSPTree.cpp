
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveSPTree.cpp
/// \brief  A collection of shortest path methods in the directed setting

#include "abstractGraph.h"
#include "abstractDigraph.h"
#include "staticQueue.h"
#include "moduleGuard.h"


bool abstractMixedGraph::ShortestPath(TMethSPX method,TOptSPX characteristic,
    const indexSet<TArc>& EligibleArcs,TNode s,TNode t) throw(ERRange,ERRejected)
{
    if (s>=n && t>=n)
    {
        s = DefaultSourceNode();
        t = DefaultTargetNode();
    }

    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("ShortestPath",s);

    if (t>=n && t!=NoNode) NoSuchNode("ShortestPath",t);

    #endif

    if (method==SPX_DEFAULT) method = TMethSPX(CT.methSPX);

    moduleGuard M(ModSPTree,*this,moduleGuard::NO_INDENT);

    LogEntry(LOG_METH,"Computing shortest path tree...");

    bool ret = false;

    switch (method)
    {
        case SPX_FIFO:
        {
            ret = SPX_FIFOLabelCorrecting(characteristic,EligibleArcs,s,t);
            break;
        }
        case SPX_DIJKSTRA:
        {
            if (t==NoNode)
            {
                ret = (SPX_Dijkstra(characteristic,EligibleArcs,
                                    singletonIndex<TNode>(s,n,CT),
                                    voidIndex<TNode>(n,CT)) != NoNode);
            }
            else
            {
                ret = (SPX_Dijkstra(characteristic,EligibleArcs,
                                    singletonIndex<TNode>(s,n,CT),
                                    singletonIndex<TNode>(t,n,CT)) != NoNode);
            }

            break;
        }
        case SPX_BELLMAN:
        {
            ret = SPX_BellmanFord(characteristic,EligibleArcs,s,t);
            break;
        }
        case SPX_BFS:
        {
            #if defined(_FAILSAVE_)

            if (!CLength() || MaxLength()<0)
            {
                Error(ERR_REJECTED,"ShortestPath","Non-trivial length labels");
            }

            #endif

            if (t==NoNode)
            {
                ret = (BFS(EligibleArcs,
                           singletonIndex<TNode>(s,n,CT),
                           voidIndex<TNode>(n,CT)) != NoNode);
            }
            else
            {
                ret = (BFS(EligibleArcs,
                           singletonIndex<TNode>(s,n,CT),
                           singletonIndex<TNode>(t,n,CT)) != NoNode);
            }

            TNode* dist = GetNodeColours();

            for (TNode i=0;i<n;i++)
            {
                SetDist(i,(dist[i]==NoNode) ? InfFloat : dist[i]*MaxLength());
            }

            break;
        }
        case SPX_DAG:
        {
            TNode x = DAGSearch(DAG_SPTREE,EligibleArcs,s,t);

            if (x!=NoNode)
            {
                #if defined(_FAILSAVE_)

                Error(ERR_REJECTED,"ShortestPath","Graph is recurrent");

                #endif
            }

            ret = (t==NoNode || Dist(t)<InfFloat);

            break;
        }
        case SPX_TJOIN:
        {
            if (t==NoNode)
            {
                NoSuchNode("ShortestPath",t);
            }
            else if (IsUndirected())
            {
                abstractGraph* thisGraph =
                    static_cast<abstractGraph*>(this);

                ret = thisGraph -> SPX_TJoin(s,t);

                break;
            }
            else
            {
                Error(ERR_REJECTED,"ShortestPath","Method applies to undirected graphs only");
            }
        }
        default:
        {
            UnknownOption("ShortestPath",method);
        }
    }

    return ret;
}


TNode abstractMixedGraph::BFS(const indexSet<TArc>& EligibleArcs,
    const indexSet<TNode>& S,const indexSet<TNode>& T) throw()
{
    moduleGuard M(ModSPTree,*this,moduleGuard::NO_INDENT);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n);

    #endif

    TNode* dist = InitNodeColours(NoNode);
    TArc* pred = InitPredecessors();

    staticQueue<TNode> Q(n,CT);

    for (TNode s=S.First();s<n;s=S.Successor(s))
    {
        Q.Insert(s);
        dist[s] = 0;
    }

    TNode t = NoNode;

    LogEntry(LOG_METH,"Breadth first graph search...");

    OpenFold();

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Expanded nodes: ");

    #endif

    THandle H = Investigate();
    investigator &I = Investigator(H);

    while (!(Q.Empty()))
    {
        M.Trace(Q);
        TNode u = Q.Delete();

        if (T.IsMember(u))
        {
            t = u;
            break;
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(u));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        while (I.Active(u))
        {
            TArc a = I.Read(u);
            TNode v = EndNode(a);

            if (dist[v]==NoNode && EligibleArcs.IsMember(a))
            {
                dist[v] = dist[u]+1;
                M.Trace(Q);
                Q.Insert(v);
                pred[v] = a;
            }
        }

        M.Trace(1);
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    Close(H);

    CloseFold();

    return t;
}


TNode abstractMixedGraph::SPX_Dijkstra(TOptSPX characteristic,const indexSet<TArc>& EligibleArcs,
    const indexSet<TNode>& S,const indexSet<TNode>& T) throw(ERRange,ERRejected)
{
    moduleGuard M(ModDikjstra,*this,moduleGuard::SHOW_TITLE);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n);

    #endif

    TFloat* dist = InitDistanceLabels();
    TFloat* potential = GetPotentials();
    TArc* pred = InitPredecessors();

    goblinQueue<TNode,TFloat> *Q = NULL;

    if (nHeap)
    {
        Q = nHeap;
        Q -> Init();
    }
    else Q = NewNodeHeap();

    for (TNode s=S.First();s<n;s=S.Successor(s))
    {
        Q -> Insert(s,0);
        dist[s] = 0;
    }

    TNode t = NoNode;

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Expanded nodes:");

    #endif

    THandle H = Investigate();
    investigator &I = Investigator(H);

    while (!(Q->Empty()))
    {
        TNode u = Q->Delete();

        if (T.IsMember(u))
        {
            t = u;
            break;
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1 && I.Active(u))
        {
            sprintf(CT.logBuffer," %lu[%g]",
                static_cast<unsigned long>(u),static_cast<double>(dist[u]));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        while (I.Active(u))
        {
            TArc a = I.Read(u);
            TFloat l = (characteristic==SPX_ORIGINAL) ? Length(a) : RedLength(potential,a);

            TNode v = EndNode(a);
            TFloat dd = dist[u]+l;

            if (dist[v]>dd && EligibleArcs.IsMember(a))
            {
                #if defined(_FAILSAVE_)

                if (l<-CT.epsilon)
                {
                    Error(ERR_REJECTED,"SPX_Dijkstra","Negative arc length");
                }

                #endif

                if (dist[v]==InfFloat) Q->Insert(v,dd);
                else Q->ChangeKey(v,dd);

                dist[v] = dd;
                pred[v] = a;
            }
        }

        M.Trace(1);
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    Close(H);

    if (!nHeap) delete Q;

    return t;
}


TNode abstractMixedGraph::VoronoiRegions(const indexSet<TNode>& Terminals)
    throw(ERRejected)
{
    LogEntry(LOG_METH,"Computing Voronoi regions...");

    SPX_Dijkstra(SPX_ORIGINAL,nonBlockingArcs(*this),Terminals,singletonIndex<TNode>(NoNode,n,CT));

    InitPartition();

    TNode nTerminals = 0;

    for (TNode i=0;i<n;++i)
    {
        Bud(i);

        if (Terminals.IsMember(i)) ++nTerminals;
    }

    for (TNode j=0;j<n;++j)
    {
        if (Pred(j)==NoArc) continue;

        TNode u = Find(StartNode(Pred(j)));
        TNode v = Find(j);

        if (u!=v) Merge(u,v);
    }

    return nTerminals;
}


bool abstractMixedGraph::SPX_BellmanFord(TOptSPX characteristic,const indexSet<TArc>& EligibleArcs,TNode s,TNode t)
    throw(ERRange,ERCheck)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("SPX_BellmanFord",s);

    if (t>=n && t!=NoNode) NoSuchNode("SPX_BellmanFord",t);

    #endif

    moduleGuard M(ModBellmanFord,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(m*(2.0*n-1));

    #endif

    TFloat* dist = InitDistanceLabels();
    TArc* pred = InitPredecessors();
    TFloat* potential = GetPotentials();

    dist[s] = 0;
    TNode i = 1;
    bool Updates = true;

    while (Updates && i<2*n)
    {
        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Iteration %lu:",static_cast<unsigned long>(i));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        OpenFold();
        Updates = 0;

        for (TArc a=0;a<2*m;a++)
        {
            TNode u = StartNode(a);
            TNode v = EndNode(a);
            TFloat l = (characteristic==SPX_ORIGINAL) ? Length(a) : RedLength(potential,a);
            TFloat thisLabel = dist[u] + l;

            if (dist[v]>thisLabel && dist[u]!=InfFloat && pred[u]!=(a^1) && EligibleArcs.IsMember(a))
            {
                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Label d[%lu] updated to %g",
                        static_cast<unsigned long>(v),static_cast<double>(thisLabel));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif

                dist[v] = thisLabel;
                pred[v] = a;
                Updates = 1;
            }
        }

        i++;
        CloseFold();
        M.Trace(m);
    }

    if (i==2*n)
    {
        Error(ERR_CHECK,"SPX_BellmanFord","Negative length cycles");
    }

    if (t==NoNode)
    {
        return false;
    }

    if (dist[t]!=InfFloat)
    {
        M.SetBounds(dist[t],dist[t]);
    }
    else
    {
        M.SetBounds(-InfFloat,-InfFloat);
    }

    return (dist[t]!=InfFloat);
}


bool abstractMixedGraph::SPX_FIFOLabelCorrecting(TOptSPX characteristic,const indexSet<TArc>& EligibleArcs,TNode s,TNode t)
    throw(ERRange,ERCheck)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("SPX_FIFOLabelCorrecting",s);

    if (t>=n && t!=NoNode) NoSuchNode("SPX_FIFOLabelCorrecting",t);

    #endif

    moduleGuard M(ModFIFOLabelCorrect,*this,
        moduleGuard::NO_INDENT | moduleGuard::SYNC_BOUNDS);

    TNode v = NegativeCycle(characteristic,EligibleArcs,s);

    if (v!=NoNode)
    {
        Error(ERR_CHECK,"SPX_FIFOLabelCorrecting","Negative length cycles");
    }

    if (t==NoNode)
    {
        return false;
    }

    TFloat dt = Dist(t);

    if (dt!=InfFloat)
    {
        M.SetBounds(dt,dt);
    }
    else
    {
        M.SetBounds(-InfFloat,-InfFloat);
    }

    return (dt!=InfFloat);
}


TNode abstractMixedGraph::NegativeCycle(TOptSPX characteristic,const indexSet<TArc>& EligibleArcs,
    TNode source,TFloat epsilon) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (source>=n && source!=NoNode) NoSuchNode("NegativeCycle",source);

    #endif

    moduleGuard M(ModFIFOLabelCorrect,*this,moduleGuard::SHOW_TITLE);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(m*2.0*n);

    #endif

    TFloat* dist = InitDistanceLabels();
    TArc* pred = InitPredecessors();
    TFloat* potential = GetPotentials();

    bool* active = new bool[n];

    if (source!=NoNode)
    {
        dist[source] = 0;
        for (TNode v=0;v<n;v++) active[v] = false;
        active[source] = true;
    }
    else
    {
        for (TNode v=0;v<n;v++)
        {
            active[v] = true;
            dist[v] = 0;
        }
    }

    TNode itCount = 1;
    TNode root = NoNode;

    bool Updates = true;
    THandle H = Investigate();
    investigator &I = Investigator(H);

    while (Updates && itCount<=2*n)
    {
        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Iteration %lu:",static_cast<unsigned long>(itCount));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        OpenFold();
        Updates = 0;

        for (TNode v=0;v<n;v++)
        {
            if (active[v])
            {
                active[v] = false;

                while (I.Active(v))
                {
                    TArc a = I.Read(v);
                    TNode w = EndNode(a);
                    TFloat l = (characteristic==SPX_ORIGINAL) ? Length(a) : RedLength(potential,a);

                    // Active nodes have dist <InfFloat
                    TFloat thisLabel = dist[v] + l + epsilon;

                    if (   dist[w]>thisLabel
                        && pred[v]!=(a^1)
                        && EligibleArcs.IsMember(a) )
                    {
                        #if defined(_LOGGING_)

                        if (CT.logMeth>1)
                        {
                            sprintf(CT.logBuffer,"Label d[%lu] updated to %g",
                                static_cast<unsigned long>(w),static_cast<double>(thisLabel));
                            LogEntry(LOG_METH2,CT.logBuffer);
                        }

                        #endif

                        dist[w] = thisLabel;
                        pred[w] = a;

                        if (itCount<2*n) Updates = 1;
                        else root = v;

                        active[w] = true;
                    }
                }

                I.Reset(v);
            }
        }

        itCount++;

        CloseFold();
        M.Trace(m);
    }

    Close(H);
    delete[] active;

    if (root!=NoNode)
    {
        for (TNode i=0;i<n;i++) root = StartNode(pred[root]);

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"...Node on a negative cycle: %lu",
                static_cast<unsigned long>(root));
            LogEntry(LOG_RES,CT.logBuffer);
        }

        #endif
    }

    return root;
}


TNode abstractDiGraph::MinimumMeanCycle(const indexSet<TArc>& EligibleArcs,TFloat* meanValue) throw()
{
    moduleGuard M(ModKarpMeanCycle,*this,moduleGuard::SHOW_TITLE);

    InitPotentials();
    TFloat *dist = new TFloat[(n+1)*n];
    TArc* pred = new TArc[(n+1)*n];

    for (TNode v=0;v<n;v++) dist[0*n+v] = 0;

    for (TNode k=1;k<=n;k++)
    {
        for (TNode v=0;v<n;v++) dist[k*n+v] = InfFloat;

        for (TArc a=0;a<2*m;a++)
        {
            TNode u = StartNode(a);
            TNode v = EndNode(a);
            TFloat l = (a&1) ? -Length(a) : Length(a);

            if (EligibleArcs.IsMember(a) && dist[(k-1)*n+u]!=InfFloat && (dist[k*n+v]>dist[(k-1)*n+u]+l))
            {
                dist[k*n+v] = dist[(k-1)*n+u]+l;
                pred[k*n+v] = a;
            }
        }
    }

    TNode minNode = NoNode;
    TFloat muLocal = InfFloat;

    for (TNode v=0;v<n;v++)
    {
        TFloat thisMaxMean = -InfFloat;

        if ((dist[n*n+v])<InfFloat)
        {
            for (TNode k=0;k<n;k++)
            {
                if ((dist[k*n+v])<InfFloat)
                {
                    TFloat thisMean = (dist[n*n+v]-dist[k*n+v])/(n-k);
                    if (thisMean>thisMaxMean) thisMaxMean = thisMean;
                }
            }
        }

        if (thisMaxMean<muLocal && thisMaxMean>-InfFloat)
        {
            muLocal = thisMaxMean;
            minNode = v;
        }
    }

    if (muLocal<InfFloat)
    {
        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"...Minimum ratio: %g",static_cast<double>(muLocal));
            LogEntry(LOG_RES,CT.logBuffer);
            LogEntry(LOG_METH2,"Extracting minimum mean cycle...");
        }

        #endif

        TArc* predExport = InitPredecessors();

        TNode k = n;

        while (predExport[minNode]==NoArc)
        {
            predExport[minNode] = pred[k*n+minNode];
            minNode = StartNode(predExport[minNode]);
            k--;
        }
    }
    else
    {
        minNode = NoNode;
        M.Shutdown(LOG_RES,"...Graph is acyclic");
    }

    delete[] dist;
    delete[] pred;

    if (meanValue) *meanValue = muLocal;

    return minNode;
}


TNode abstractDiGraph::TopSort() throw()
{
    moduleGuard M(ModSPTree,*this,"Computing topological ordering...");

    TNode ret = DAGSearch(DAG_TOPSORT,nonBlockingArcs(*this));

    return ret;
}


TNode abstractDiGraph::CriticalPath() throw()
{
    moduleGuard M(ModSPTree,*this,"Computing critical path...");

    TNode ret = DAGSearch(DAG_CRITICAL,nonBlockingArcs(*this));

    return ret;
}


TNode abstractMixedGraph::DAGSearch(TOptDAGSearch opt,const indexSet<TArc>& EligibleArcs,TNode s,TNode t)
    throw(ERRange)
{
    moduleGuard M(ModDAGSearch,*this, (opt==DAG_TOPSORT) ? 0 : moduleGuard::SHOW_TITLE);

    staticQueue<TNode> Q(n,CT);
    TArc* idg = new TArc[n];

    for (TNode v=0;v<n;v++) idg[v] = 0;

    for (TArc a=0;a<2*m;a++)
    {
        if (EligibleArcs.IsMember(a)) 
        {
            TNode w = EndNode(a);
            idg[w]++;
        }
    }

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    #endif

    TFloat* dist = NULL;
    TArc* pred = NULL;
    TNode* nodeColour = NULL;

    switch (opt)
    {
        case DAG_TOPSORT:
        {
            nodeColour = InitNodeColours(NoNode);
            break;
        }
        case DAG_CRITICAL:
        {
            pred = InitPredecessors();
            dist = InitDistanceLabels(-InfFloat);

            #if defined(_LOGGING_)

            LH = LogStart(LOG_METH2,"Expanded nodes:");

            #endif

            break;
        }
        case DAG_SPTREE:
        {
            nodeColour = InitNodeColours(NoNode);
            dist   = InitDistanceLabels(InfFloat);
            pred   = InitPredecessors();

            if (s!=NoNode) dist[s] = 0;

            #if defined(_LOGGING_)

            LH = LogStart(LOG_METH2,"Expanded nodes:");

            #endif

            break;
        }
    }

    for (TNode v=0;v<n;v++)
    {
        if (idg[v]==0)
        {
            Q.Insert(v);

            if (opt==DAG_CRITICAL || (opt==DAG_SPTREE && s==NoNode)) dist[v] = 0;
        }
    }

    TNode nr = 0;
    THandle H = Investigate();
    investigator &I = Investigator(H);

    while (!Q.Empty())
    {
        TNode v = Q.Delete();

        if (opt!=DAG_CRITICAL) nodeColour[v] = nr;

        nr++;

        #if defined(_LOGGING_)

        if (opt!=DAG_TOPSORT && CT.logMeth>1 && dist[v]<InfFloat)
        {
            sprintf(CT.logBuffer," %lu[%g]",
                static_cast<unsigned long>(v),static_cast<double>(dist[v]));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        while (I.Active(v))
        {
            TArc a = I.Read(v);

            if (!EligibleArcs.IsMember(a)) continue;

            TNode w = EndNode(a);
            idg[w]--;

            if ((opt==DAG_SPTREE && dist[v]<InfFloat && dist[w]>dist[v]+Length(a)) ||
                (opt==DAG_CRITICAL && dist[w]<dist[v]+Length(a))
               )
            {
                dist[w] = dist[v]+Length(a);
                pred[w] = a;
            }

            if (idg[w]==0) Q.Insert(w);
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    Close(H);
    delete[] idg;

    M.Trace(m);

    if (CT.logRes && nr<n) LogEntry(LOG_RES,"...Graph contains cycles");

    switch (opt)
    {
        case DAG_TOPSORT:
        case DAG_SPTREE:
        {
            if (nr<n)
            {
                for (TNode v=0;v<n;v++)
                    if (nodeColour[v]==NoNode) return v;
            }

            if (opt==DAG_SPTREE)
            {
                if (t!=NoNode) M.SetBounds(dist[t],dist[t]);
            }
            else
            {
                M.Shutdown(LOG_RES,"...Graph is acyclic");
            }

            return NoNode;
        }
        case DAG_CRITICAL:
        {
            if (nr<n) return NoNode;

            TNode maxNode = NoNode;
            for (TNode v=0;v<n;v++)
            {
                if ((maxNode==NoNode && dist[v]>-InfFloat) ||
                    (maxNode!=NoNode && dist[v]>dist[maxNode])
                   )
                {
                    maxNode = v;
                }
            }

            if (CT.logRes)
            {
                sprintf(CT.logBuffer,"...Critical path length is: %g",
                    static_cast<double>(dist[maxNode]));
                M.Shutdown(LOG_RES,CT.logBuffer);
            }

            return maxNode;
        }
    }

    return NoNode;
}
