
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveTJoin.cpp
/// \brief  Computation of minimum T-Joins and application to the shortest path problem

#include "sparseGraph.h"
#include "denseGraph.h"
#include "moduleGuard.h"


void abstractGraph::ComputeTJoin(const indexSet<TNode>& Terminals) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (MaxLCap()>0)
        Error(ERR_REJECTED,"ComputeTJoin","Non-trivial lower bounds");

    #endif

    if (CT.traceLevel==2) Display();

    moduleGuard M(ModTJoin,*this,"Computing minimum cost T-join...");

    TNode* map = new TNode[n];

    TNode no = 0;

    for (TNode v=0;v<n;v++)
    {
        if (Terminals.IsMember(v)) map[v] = no++;
        else map[v] = NoNode;
    }

    if (CT.logMeth)
    {
        sprintf(CT.logBuffer,"Problem has %lu odd vertices",
            static_cast<unsigned long>(no));
        LogEntry(LOG_METH,CT.logBuffer);
    }

    if (no==0)
    {
        delete[] map;
        return;
    }

    if (no%1)
    {
        delete[] map;
        Error(ERR_REJECTED,"ComputeTJoin","Number of odd nodes must be even");
    }

    LogEntry(LOG_METH,"Transforming into matching problem...");
    OpenFold();

    #if defined(_PROGRESS_)

    M.InitProgressCounter(no+n+no/2,1);

    #endif

    denseGraph* G = new denseGraph(no,0,CT);
    G -> ImportLayoutData(*this);
    graphRepresentation* GR = G->Representation();

    TNode* revmap = new TNode[no];

    for (TNode u=0;u<n;u++)
    {
        if (map[u]!=NoNode)
        {
            if (Dim()>=2)
            {
                GR -> SetC(map[u],0,C(u,0));
                GR -> SetC(map[u],1,C(u,1));
            }

            revmap[map[u]] = u;

            ShortestPath(SPX_DIJKSTRA,SPX_ORIGINAL,nonBlockingArcs(*G),u);
            TFloat* dist = GetDistanceLabels();

            for (TNode v=0;v<=u;v++)
            {
                if (map[v]!=NoNode)
                {
                    TArc a = G->Adjacency(map[u],map[v]);

                    if (u==v)
                    {
                        // Do not bother the matching solver with
                        // attracting loops (relevant only when
                        // starting with a min-cost flow solution)
                        GR -> SetLength(a,InfFloat);
                    }
                    else
                    {
                        GR -> SetLength(a,dist[v]);
                    }
                }
            }

            #if defined(_PROGRESS_)

            M.ProgressStep(1);

            #endif
        }
    }

    CloseFold();

    M.Trace(*G,n);

    #if defined(_PROGRESS_)

    M.SetProgressNext(n);

    #endif

    G -> MinCMatching((TCap)1);

    M.Trace(*G,n);

    #if defined(_PROGRESS_)

    M.SetProgressNext(1);

    #endif

    LogEntry(LOG_METH,"Transforming matching into disjoint paths...");
    OpenFold();

    TNode pathCount = 0;

    for (TArc a=0;a<G->M();a++)
    {
        if (G->Sub(2*a)<=0) continue;

        if (G->Length(2*a)>=InfFloat) continue;

        pathCount++;

        TNode u = revmap[G->StartNode(2*a)];
        TNode v = revmap[G->EndNode(2*a)];

        ShortestPath(SPX_DIJKSTRA,SPX_ORIGINAL,nonBlockingArcs(*G),u);

        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            LogEntry(LOG_METH2,"Adding the path (in reverse order):");
            sprintf(CT.logBuffer,"  (%lu",static_cast<unsigned long>(v));
            LH = LogStart(LOG_METH2,CT.logBuffer);
        }

        #endif

        while (v!=u)
        {
            TArc a2 = Pred(v);

            SetSub(a2,(Sub(a2)<UCap(a2)) ? UCap(a2) : 0);

            v = StartNode(a2);

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

        #if defined(_PROGRESS_)

        M.ProgressStep(1);

        #endif
    }

    CloseFold();

    M.Trace();

    delete G;

    delete[] map;
    delete[] revmap;

    if (2*pathCount<no)
        Error(ERR_REJECTED,"ComputeTJoin","No T-join exists");
}


void abstractGraph::MinCTJoin(const indexSet<TNode>& Terminals) throw()
{
    moduleGuard M(ModTJoin,*this,"Eliminating negative length labels...");

    sparseGraph G(*this,OPT_CLONE);
    graphRepresentation* GR = G.Representation();

    for (TNode v=0;v<n;v++)
    {
        GR -> SetDemand(v,Terminals.IsMember(v) ? 1 : 0);
    }

    for (TArc a=0;a<2*m;a++)
    {
        if (Length(a)<0)
        {
            TNode v = StartNode(a);
            GR -> SetDemand(v,1-G.Demand(v));

            if (a&1) GR -> SetLength(a,-Length(a));
        }
    }

    G.ComputeTJoin(demandNodes(G));

    LogEntry(LOG_METH,"Flipping negative length arcs...");

    for (TArc a=0;a<m;a++)
    {
        TArc a2 = 2*a;

        if ((G.Sub(a2)>0 && Length(a2)>=0) || (G.Sub(a2)==0 && Length(a2)<0))
            SetSub(a2,UCap(a2));
        else SetSub(a2,LCap(a2));
    }
}


bool abstractGraph::SPX_TJoin(TNode s,TNode t) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("SPX_TJoin",s);

    if (t>=n) NoSuchNode("SPX_TJoin",t);

    #endif

    moduleGuard M(ModTJoin,*this,moduleGuard::SHOW_TITLE);

    LogEntry(LOG_METH,"Eliminating negative length labels...");

    sparseGraph G(*this,OPT_CLONE);
    graphRepresentation* GR = G.Representation();

    for (TNode v=0;v<n;v++)
    {
        GR -> SetDemand(v, (v==s || v==t) ? 1 : 0);
    }

    for (TArc a=0;a<2*m;a++)
    {
        if (Length(a)<0)
        {
            TNode v = StartNode(a);
            GR -> SetDemand(v,(G.Demand(v)>0) ? 0 : 1);

            if (a&1) GR -> SetLength(a,-Length(a));
        }
    }

    try
    {
        G.ComputeTJoin(demandNodes(G));

        LogEntry(LOG_METH,"Flipping negative length arcs...");

        for (TArc a=0;a<m;a++)
        {
            TArc a2 = 2*a;

            if ((G.Sub(a2)>0 && Length(a2)>=0) || (G.Sub(a2)==0 && Length(a2)<0))
                SetSub(a2,UCap(a2));
            else SetSub(a2,LCap(a2));
        }

        BFS(subgraphArcs(*this),singletonIndex<TNode>(s,n,CT),singletonIndex<TNode>(t,n,CT));
    }
    catch (ERRejected)
    {
        return false;
    }

    return true;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveTJoin.cpp
/// \brief  Computation of minimum T-Joins and application to the shortest path problem

#include "sparseGraph.h"
#include "denseGraph.h"
#include "moduleGuard.h"


void abstractGraph::ComputeTJoin(const indexSet<TNode>& Terminals) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (MaxLCap()>0)
        Error(ERR_REJECTED,"ComputeTJoin","Non-trivial lower bounds");

    #endif

    if (CT.traceLevel==2) Display();

    moduleGuard M(ModTJoin,*this,"Computing minimum cost T-join...");

    TNode* map = new TNode[n];

    TNode no = 0;

    for (TNode v=0;v<n;v++)
    {
        if (Terminals.IsMember(v)) map[v] = no++;
        else map[v] = NoNode;
    }

    if (CT.logMeth)
    {
        sprintf(CT.logBuffer,"Problem has %lu odd vertices",
            static_cast<unsigned long>(no));
        LogEntry(LOG_METH,CT.logBuffer);
    }

    if (no==0)
    {
        delete[] map;
        return;
    }

    if (no%1)
    {
        delete[] map;
        Error(ERR_REJECTED,"ComputeTJoin","Number of odd nodes must be even");
    }

    LogEntry(LOG_METH,"Transforming into matching problem...");
    OpenFold();

    #if defined(_PROGRESS_)

    M.InitProgressCounter(no+n+no/2,1);

    #endif

    denseGraph* G = new denseGraph(no,0,CT);
    G -> ImportLayoutData(*this);
    graphRepresentation* GR = G->Representation();

    TNode* revmap = new TNode[no];

    for (TNode u=0;u<n;u++)
    {
        if (map[u]!=NoNode)
        {
            if (Dim()>=2)
            {
                GR -> SetC(map[u],0,C(u,0));
                GR -> SetC(map[u],1,C(u,1));
            }

            revmap[map[u]] = u;

            ShortestPath(SPX_DIJKSTRA,SPX_ORIGINAL,nonBlockingArcs(*G),u);
            TFloat* dist = GetDistanceLabels();

            for (TNode v=0;v<=u;v++)
            {
                if (map[v]!=NoNode)
                {
                    TArc a = G->Adjacency(map[u],map[v]);

                    if (u==v)
                    {
                        // Do not bother the matching solver with
                        // attracting loops (relevant only when
                        // starting with a min-cost flow solution)
                        GR -> SetLength(a,InfFloat);
                    }
                    else
                    {
                        GR -> SetLength(a,dist[v]);
                    }
                }
            }

            #if defined(_PROGRESS_)

            M.ProgressStep(1);

            #endif
        }
    }

    CloseFold();

    M.Trace(*G,n);

    #if defined(_PROGRESS_)

    M.SetProgressNext(n);

    #endif

    G -> MinCMatching((TCap)1);

    M.Trace(*G,n);

    #if defined(_PROGRESS_)

    M.SetProgressNext(1);

    #endif

    LogEntry(LOG_METH,"Transforming matching into disjoint paths...");
    OpenFold();

    TNode pathCount = 0;

    for (TArc a=0;a<G->M();a++)
    {
        if (G->Sub(2*a)<=0) continue;

        if (G->Length(2*a)>=InfFloat) continue;

        pathCount++;

        TNode u = revmap[G->StartNode(2*a)];
        TNode v = revmap[G->EndNode(2*a)];

        ShortestPath(SPX_DIJKSTRA,SPX_ORIGINAL,nonBlockingArcs(*G),u);

        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            LogEntry(LOG_METH2,"Adding the path (in reverse order):");
            sprintf(CT.logBuffer,"  (%lu",static_cast<unsigned long>(v));
            LH = LogStart(LOG_METH2,CT.logBuffer);
        }

        #endif

        while (v!=u)
        {
            TArc a2 = Pred(v);

            SetSub(a2,(Sub(a2)<UCap(a2)) ? UCap(a2) : 0);

            v = StartNode(a2);

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

        #if defined(_PROGRESS_)

        M.ProgressStep(1);

        #endif
    }

    CloseFold();

    M.Trace();

    delete G;

    delete[] map;
    delete[] revmap;

    if (2*pathCount<no)
        Error(ERR_REJECTED,"ComputeTJoin","No T-join exists");
}


void abstractGraph::MinCTJoin(const indexSet<TNode>& Terminals) throw()
{
    moduleGuard M(ModTJoin,*this,"Eliminating negative length labels...");

    sparseGraph G(*this,OPT_CLONE);
    graphRepresentation* GR = G.Representation();

    for (TNode v=0;v<n;v++)
    {
        GR -> SetDemand(v,Terminals.IsMember(v) ? 1 : 0);
    }

    for (TArc a=0;a<2*m;a++)
    {
        if (Length(a)<0)
        {
            TNode v = StartNode(a);
            GR -> SetDemand(v,1-G.Demand(v));

            if (a&1) GR -> SetLength(a,-Length(a));
        }
    }

    G.ComputeTJoin(demandNodes(G));

    LogEntry(LOG_METH,"Flipping negative length arcs...");

    for (TArc a=0;a<m;a++)
    {
        TArc a2 = 2*a;

        if ((G.Sub(a2)>0 && Length(a2)>=0) || (G.Sub(a2)==0 && Length(a2)<0))
            SetSub(a2,UCap(a2));
        else SetSub(a2,LCap(a2));
    }
}


bool abstractGraph::SPX_TJoin(TNode s,TNode t) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (s>=n) NoSuchNode("SPX_TJoin",s);

    if (t>=n) NoSuchNode("SPX_TJoin",t);

    #endif

    moduleGuard M(ModTJoin,*this,moduleGuard::SHOW_TITLE);

    LogEntry(LOG_METH,"Eliminating negative length labels...");

    sparseGraph G(*this,OPT_CLONE);
    graphRepresentation* GR = G.Representation();

    for (TNode v=0;v<n;v++)
    {
        GR -> SetDemand(v, (v==s || v==t) ? 1 : 0);
    }

    for (TArc a=0;a<2*m;a++)
    {
        if (Length(a)<0)
        {
            TNode v = StartNode(a);
            GR -> SetDemand(v,(G.Demand(v)>0) ? 0 : 1);

            if (a&1) GR -> SetLength(a,-Length(a));
        }
    }

    try
    {
        G.ComputeTJoin(demandNodes(G));

        LogEntry(LOG_METH,"Flipping negative length arcs...");

        for (TArc a=0;a<m;a++)
        {
            TArc a2 = 2*a;

            if ((G.Sub(a2)>0 && Length(a2)>=0) || (G.Sub(a2)==0 && Length(a2)<0))
                SetSub(a2,UCap(a2));
            else SetSub(a2,LCap(a2));
        }

        BFS(subgraphArcs(*this),singletonIndex<TNode>(s,n,CT),singletonIndex<TNode>(t,n,CT));
    }
    catch (ERRejected)
    {
        return false;
    }

    return true;
}
