
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solvePostman.cpp
/// \brief  All codes for Euler cycles and Chinese postman problems

#include "sparseGraph.h"
#include "sparseDigraph.h"
#include "denseBigraph.h"
#include "moduleGuard.h"


void abstractMixedGraph::ChinesePostman(bool adjustUCap) throw(ERRejected)
{
    Error(ERR_REJECTED,"ChinesePostman",
        "Method applies to sparse undirected and sparse directed graphs only");

    throw ERRejected();
}


void abstractGraph::ChinesePostman(bool adjustUCap) throw(ERRejected)
{
    graphRepresentation* X = NULL;

    if (adjustUCap)
    {
        X = Representation();

        if (!X) NoRepresentation("ChinesePostman");
    }

    moduleGuard M(ModPostman,*this,adjustUCap ?
        "Computing minimum Eulerian supergraph..." :
        "Computing maximum Eulerian subgraph..." );

    LogEntry(LOG_METH,"Eliminating negative length labels...");

    sparseGraph G(*this,OPT_CLONE);
    graphRepresentation* GR = G.Representation();

    for (TArc a=0;a<m;a++) SetSub(2*a,UCap(2*a));

    for (TNode v=0;v<n;v++)
    {
        GR -> SetDemand(v,(((TArc)Deg(v))%2) ? 1 : 0);
    }

    for (TArc a=0;a<2*m;a++)
    {
        if (Length(a)<0)
        {
            TNode v = StartNode(a);
            GR -> SetDemand(v,(G.Demand(v)==1) ? 0 : 1);

            if (a&1) GR -> SetLength(a,-Length(a));
        }
    }

    G.ComputeTJoin(demandNodes(G));

    if (X)
    {
        LogEntry(LOG_METH,"Adjusting arc capacities...");
    }
    else
    {
        LogEntry(LOG_METH,"Constructing maximal Eulerian subgraph...");
    }

    for (TArc a=0;a<m;a++)
    {
        TArc a2 = 2*a;

        if ((G.Sub(a2)>0 && Length(a2)>=0) || (G.Sub(a2)==0 && Length(a2)<0))
        {
            SetSub(a2,UCap(a2)-1);

            if (X) X -> SetUCap(a2,UCap(a2)+1);
        }
        else
        {
            SetSub(a2,UCap(a2));
        }
    }
}


void abstractDiGraph::ChinesePostman(bool adjustUCap) throw(ERRejected)
{
    graphRepresentation* X = NULL;

    if (adjustUCap)
    {
        X = Representation();

        if (!X) NoRepresentation("ChinesePostman");
    }

    moduleGuard M(ModPostman,*this,adjustUCap ?
        "Computing minimum Eulerian supergraph..." :
        "Computing maximum Eulerian subgraph..." );

    TNode* map1 = new TNode[n];
    TNode* map2 = new TNode[n];

    TNode n1 = 0;
    TNode n2 = 0;
    TFloat deficiency = 0;

    for (TArc a=0;a<m;a++) SetSub(2*a,UCap(2*a));

    LogEntry(LOG_METH,"Computing unbalanced nodes...");
    OpenFold();

    for (TNode v=0;v<n;v++)
    {
        if (Divergence(v)>0)
        {
            map1[v] = n1++;
            deficiency += Divergence(v);

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Divergence(%lu) = %g",static_cast<unsigned long>(v),Divergence(v));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
        else map1[v] = NoNode;
    }

    for (TNode v=0;v<n;v++)
    {
        if (Divergence(v)<0)
        {
            map2[v] = n1+(n2++);

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Divergence(%lu) = %g",static_cast<unsigned long>(v),Divergence(v));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
        else map2[v] = NoNode;
    }

    TNode n0 = n1+n2;

    CloseFold();

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Problem has %lu unbalanced nodes",static_cast<unsigned long>(n0));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    if (n0==0)
    {
        delete[] map1;
        delete[] map2;
        CloseFold();
        return;
    }

    LogEntry(LOG_METH,"Transforming into matching problem...");
    OpenFold();

    denseBiGraph *G = new denseBiGraph(n1,n2,0,CT);
    G -> ImportLayoutData(*this);
    graphRepresentation* GR = G->Representation();

    TNode *revmap = new TNode[n0];
    TCap *dgl = new TCap[n0];

    for (TNode u=0;u<n;u++)
    {
        if (map2[u]!=NoNode)
        {
            if (Dim()>=2)
            {
                GR -> SetC(map2[u],0,C(u,0));
                GR -> SetC(map2[u],1,C(u,1));
            }

            revmap[map2[u]] = u;
            dgl[map2[u]] = TCap(-Divergence(u));
        }

        if (map1[u]!=NoNode)
        {
            if (Dim()>=2)
            {
                GR -> SetC(map1[u],0,C(u,0));
                GR -> SetC(map1[u],1,C(u,1));
            }

            revmap[map1[u]] = u;
            dgl[map1[u]] = TCap(Divergence(u));
        }
    }

    for (TArc a=0;a<m;a++) SetSub(2*a,0);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n1+n+deficiency, 1);

    #endif

    for (TNode u=0;u<n;u++)
    {
        if (map1[u]!=NoNode)
        {
            ShortestPath(SPX_DIJKSTRA,SPX_ORIGINAL,nonBlockingArcs(*G),u);

            TFloat* dist = GetDistanceLabels();

            for (TNode v=0;v<n;v++)
            {
                if (map2[v]!=NoNode && dist[v]<InfFloat)
                {
                    TArc a = G->Adjacency(map1[u],map2[v]);
                    GR -> SetLength(a,dist[v]);
                }
            }

            #if defined(_PROGRESS_)

            M.ProgressStep(1);

            #endif
        }
    }

    GR -> SetCUCap(deficiency);

    CloseFold();

    M.Trace(*G);

    #if defined(_PROGRESS_)

    M.SetProgressNext(n);

    #endif

    G -> MinCAssignment(dgl);

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(1);

    #endif

    if (X)
    {
        LogEntry(LOG_METH,"Adjusting arc capacities...");
    }
    else
    {
        LogEntry(LOG_METH,"Constructing maximal Eulerian subgraph...");
    }

    OpenFold();

    for (TArc a=0;a<m;a++) SetSub(2*a,UCap(2*a));

    TArc* pred = InitPredecessors();

    for (TArc a=0;a<G->M();a++)
    {
        if (G->Sub(2*a)>0)
        {
            TNode u = revmap[G->StartNode(2*a)];
            TNode v = revmap[G->EndNode(2*a)];

            ShortestPath(SPX_DIJKSTRA,SPX_REDUCED,nonBlockingArcs(*G),u);

            if (pred[v]==NoArc)
            {
                delete G;
                delete[] map1;
                delete[] map2;
                delete[] revmap;

                Error(ERR_REJECTED,"ChinesePostman","Digraph is not strongly connected");
            }

            #if defined(_LOGGING_)

            THandle LH = NoHandle;

            if (CT.logMeth>1)
            {
                LogEntry(LOG_METH2,"Adding the path (in reverse order):");
                sprintf(CT.logBuffer,"(%lu",static_cast<unsigned long>(v));
                LH = LogStart(LOG_METH2,CT.logBuffer);
            }

            #endif

            TCap delta = TCap(G->Sub(2*a));

            while (v!=u)
            {
                TArc a2 = pred[v];
                SetSubRelative(a2,-delta);
                v = StartNode(a2);

                if (X) X -> SetUCap(a2,UCap(a2)+delta);

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

            M.ProgressStep(delta);
            M.SetProgressNext(1);

            #endif
        }
    }

    CloseFold();

    M.Trace(0);

    delete G;

    delete[] map1;
    delete[] map2;
    delete[] revmap;
}


bool abstractMixedGraph::EulerianCycle(TArc* pred) throw(ERRejected)
{
    if (MaxUCap()>1.0)
        Error(ERR_REJECTED,"EulerianCycle","Capacity bounds must be all one");

    TArc* predLocal = pred;
    bool isEulerian = true;

    if (!pred) predLocal = new TArc[m];

    LogEntry(LOG_METH,"Computing an Eulerian cycle...");

    moduleGuard M(ModHierholzer,*this,moduleGuard::SHOW_TITLE);

    TArc count = 0;

    for (TArc a=0;a<m;a++) predLocal[a] = NoArc;

    THandle H = Investigate();
    investigator &I = Investigator(H);

    // Start with an arbitrary arc
    TArc a = 0;

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Initial cycle: (%lu[%lu]",
            static_cast<unsigned long>(StartNode(a)),static_cast<unsigned long>(a));
        LH = LogStart(LOG_METH2,CT.logBuffer);
    }

    #endif

    // Find a closed walk through arc 0
    while (true)
    {
        TNode u = EndNode(a);

        if (I.Active(u))
        {
            TArc a2 = I.Read(u);

            if (predLocal[a2>>1]!=NoArc || (a2>>1)==0 || Blocking(a2)) continue;

            predLocal[a2>>1] = a;
            a = a2;
            count++;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"%lu[%lu]",
                    static_cast<unsigned long>(u),static_cast<unsigned long>(a));
                LogAppend(LH,CT.logBuffer);
            }

            #endif
        }
        else break;
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"%lu)",static_cast<unsigned long>(EndNode(a)));
        LogEnd(LH,CT.logBuffer);
    }

    #endif

    if (EndNode(a)!=StartNode(0)) isEulerian = false;

    predLocal[0] = a;
    count++;

    // Track back on the cycle found so far.
    while (isEulerian && a!=0)
    {
        TNode u = StartNode(a);

        if (I.Active(u))
        {
            TArc a2 = I.Read(u);

            if (predLocal[a2>>1]!=NoArc || a2==0 || Blocking(a2)) continue;

            // Find a closed walk through arc a2

            #if defined(_LOGGING_)

            THandle LH = NoHandle;

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Inserting before %lu: (%lu[%lu]",
                    static_cast<unsigned long>(a),
                    static_cast<unsigned long>(StartNode(a2)),
                    static_cast<unsigned long>(a2));
                LH = LogStart(LOG_METH2,CT.logBuffer);
            }

            #endif

            predLocal[a2>>1] = predLocal[a>>1];
            count++;

            while (true)
            {
                TNode u = EndNode(a2);

                if (I.Active(u))
                {
                    TArc a3 = I.Read(u);

                    if (predLocal[a3>>1]!=NoArc || Blocking(a3)) continue;

                    predLocal[a3>>1] = a2;
                    a2 = a3;
                    count++;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,"%lu[%lu]",
                            static_cast<unsigned long>(u),static_cast<unsigned long>(a2));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif
                }
                else break;
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"%lu)",static_cast<unsigned long>(EndNode(a2)));
                LogEnd(LH,CT.logBuffer);
            }

            #endif

            if (EndNode(a2)!=StartNode(a))
            {
                isEulerian = false;
            }
            else
            {
                predLocal[a>>1] = a2;
            }
        }
        else a = predLocal[a>>1];
    }

    Close(H);

    if (count<m) isEulerian = false;

    if (!pred)
    {
        if (isEulerian)
        {
            TArc* edgeColour = RawEdgeColours();
            TArc a = predLocal[0];

            for (TArc i=1;i<=m;i++)
            {
                edgeColour[a>>1] = m-i;
                a = predLocal[a>>1];
            }
        }

        delete[] predLocal;
    }

    if (isEulerian)
    {
        M.Shutdown(LOG_RES,"...Graph is Eulerian");
    }
    else
    {
        M.Shutdown(LOG_RES,"...Graph is not Eulerian");
    }

    return isEulerian;
}
