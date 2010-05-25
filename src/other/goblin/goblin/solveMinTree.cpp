
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveMinTree.cpp
/// \brief  Minimum spanning tree methods for both the directed and the undirected setting

#include "mixedGraph.h"
#include "nestedFamily.h"
#include "abstractGraph.h"
#include "staticQueue.h"
#include "moduleGuard.h"


TFloat abstractMixedGraph::MinTree(TMethMST method,TOptMST characteristic,TNode root)
    throw(ERRange,ERRejected)
{
    if (root>=n) root = DefaultRootNode();

    #if defined(_FAILSAVE_)

    if (root>=n && root!=NoNode) NoSuchNode("MinTree",root);

    #endif

    if (method==MST_DEFAULT) method = TMethMST(CT.methMST);

    if (   method==MST_EDMONDS
        && ((characteristic & MST_ONE_CYCLE) || !CLCap() || MaxLCap()>0)
        && !IsDirected()
       )
    {
        method = MST_KRUSKAL;
    }

    const char* objSense = (characteristic & MST_MAX) ? "maximum" : "minimum";

    if (characteristic & MST_ONE_CYCLE)
    {
        sprintf(CT.logBuffer,"Computing %s one cycle tree...",objSense);
    }
    else
    {
        sprintf(CT.logBuffer,"Computing %s spanning tree...",objSense);
    }

    moduleGuard M(ModMinTree,*this,CT.logBuffer,moduleGuard::NO_INDENT);

    TFloat totalWeight = InfFloat;

    switch (method)
    {
        case MST_PRIM:
        case MST_PRIM2:
        {
            totalWeight = MST_Prim(method,characteristic,root);

            if (root!=NoNode)
            {
                InitSubgraph();
                AddToSubgraph();
            }

            break;
        }
        case MST_KRUSKAL:
        {
            if (root!=NoNode)
            {
                totalWeight = MST_Kruskal(characteristic,root);

                if (fabs(totalWeight)<InfFloat)
                {
                    ExtractTree(root,characteristic);
                }
            }
            else totalWeight = MST_Kruskal(characteristic);

            break;
        }
        case MST_EDMONDS:
        {
            totalWeight = MST_Edmonds(characteristic,root);

            if (root!=NoNode)
            {
                InitSubgraph();
                AddToSubgraph();
            }

            break;
        }
        default:
        {
            UnknownOption("MinTree",method);
        }
    }

    return totalWeight;
}


TFloat abstractMixedGraph::MST_Length(TOptMST characteristic,
    TFloat* potential,TArc a) throw(ERRange)
{
    if (characteristic & MST_REDUCED) return RedLength(potential,a);

    return Length(a);
}


TFloat abstractMixedGraph::MST_Prim(TMethMST method,
    TOptMST characteristic,TNode r) throw(ERRange,ERRejected)
{
    if (r==NoNode) r = 0;

    #if defined(_FAILSAVE_)

    if (r>=n) NoSuchNode("MST_Prim",r);

    #endif

    TModule moduleID = ModPrim2;

    if (!CLCap() || MaxLCap()>0) method = MST_PRIM2;

    if (method==MST_PRIM) moduleID = ModPrim;

    moduleGuard M(moduleID,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n);

    #endif

    TFloat sign = (characteristic & MST_MAX) ? -1 : 1;

    TFloat* dist = InitDistanceLabels(sign*InfFloat);
    TArc*   pred = InitPredecessors();
    TFloat* potential = GetPotentials();

    TFloat sum = 0;
    THandle H = Investigate();
    investigator &I = Investigator(H);

    TNode nExplored = 0;
    TNode r2 = r;

    if (characteristic & MST_ONE_CYCLE)
    {
        // Compute 1-cycle tree with r and some other node r2 on the cycle.
        // Compute  arcs a1,a2 which are incident with r on this cycle

        TArc a1 = NoArc;
        TArc a2 = NoArc;

        while (I.Active(r))
        {
            TArc a = I.Read(r);

            if (EndNode(a)!=r && UCap(a)>0)
            {
                if (   a1==NoArc || LCap(a)>0
                    || (   sign*MST_Length(characteristic,potential,a)
                               < sign*MST_Length(characteristic,potential,a1)
                        && LCap(a1)==0
                       )
                   )
                {
                    a2 = a1;
                    a1 = a;
                }
                else if (   a2==NoArc
                         || (   sign*MST_Length(characteristic,potential,a)
                                    < sign*MST_Length(characteristic,potential,a2)
                             && LCap(a2)==0
                            )
                        )
                {
                    a2 = a;
                }
            }
        }

        if (a2==NoArc)
        {
            Close(H);

            M.SetBounds(sign*InfFloat,sign*InfFloat);
            sprintf(CT.logBuffer,"...There is no cycle through node %lu",
                static_cast<unsigned long>(r));
            M.Shutdown(LOG_RES,CT.logBuffer);

            return sign*InfFloat;
        }

        r2 = EndNode(a1);
        pred[r2] = a1;
        pred[r] = a2^1;
        dist[r] = -sign*InfFloat;
        sum += MST_Length(characteristic,potential,a1)
             + MST_Length(characteristic,potential,a2);
        nExplored = 1;

        M.Trace();
    }

    TNode u = r2;
    dist[u] = 0;

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Expanded nodes: ");

    if (CT.logMeth>1 && r!=r2)
    {
        sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(r));
        LogAppend(LH,CT.logBuffer);
    }

    #endif

    if (method==MST_PRIM)
    {
        // Basic version of the algorithm

        while (u!=NoNode && dist[u]!=sign*InfFloat)
        {
            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"%lu ",
                    static_cast<unsigned long>(u));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            sum += dist[u];
            dist[u] = -sign*InfFloat;
            nExplored++;

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                TNode v = EndNode(a);
                TFloat l = MST_Length(characteristic,potential,a);

                if (sign*dist[v]>sign*l && UCap(a)>0)
                {
                    dist[v] = l;
                    pred[v] = a;
                }
            }

            u = NoNode;
            for (TNode v=0;v<n;v++)
                if (   dist[v]!=-sign*InfFloat
                    && (u==NoNode || sign*dist[v]<sign*dist[u]) )
                {
                    u = v;
                }

            M.Trace(1);
        }
    }
    else
    {
        // Enhanced version of the algorithm

        goblinQueue<TNode,TFloat>* Q = nHeap;

        if (Q!=NULL) Q -> Init();
        else Q = NewNodeHeap();

        Q -> Insert(u,0);

        while (!(Q->Empty()))
        {
            u = Q->Delete();
            dist[u] = -sign*InfFloat;
            nExplored++;

            if (u!=r2) sum += MST_Length(characteristic,potential,pred[u]);

            #if defined(_LOGGING_)

            if (CT.logMeth>=2)
            {
                sprintf(CT.logBuffer,"%lu ",
                    static_cast<unsigned long>(u));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                TNode v = EndNode(a);
                TFloat l = -sign*InfFloat;

                if (LCap(a)==0)
                {
                    l = MST_Length(characteristic,potential,a);
                }

                if (sign*dist[v]>sign*l && UCap(a)>0)
                {
                    if (dist[v]==sign*InfFloat)
                    {
                        Q->Insert(v,sign*l);
                    }
                    else Q->ChangeKey(v,sign*l);

                    dist[v] = l;
                    pred[v] = a;
                }
            }

            M.Trace(1);
        }

        if (nHeap==NULL) delete Q;
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    Close(H);

    M.SetBounds(sum,sum);

    if (nExplored<n)
    {
        sum = sign*InfFloat;
        M.Shutdown(LOG_RES,"...Graph is disconnected");
    }
    else
    {
        sprintf(CT.logBuffer,"...Final spanning tree has weight %g",sum);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return sum;
}


TFloat abstractMixedGraph::MST_Kruskal(TOptMST characteristic,TNode r)
    throw(ERRange,ERRejected)
{
    if (r==NoNode) r = 0;

    #if defined(_FAILSAVE_)

    if (r>=n) NoSuchNode("MST_Kruskal",r);

    #endif

    moduleGuard M(ModKruskal,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(m+n-1);

    #endif

    TFloat sign = 1;
    if (characteristic & MST_MAX) sign = -1;

    InitPartition();
    InitSubgraph();
    TFloat* potential = GetPotentials();

    for (TNode i=0;i<n;i++) Bud(i);

    TFloat sum = 0;
    TNode rank = 0;

    if (!CLCap() || MaxLCap()>0)
    {
        for (TArc a=0;a<m;a++)
        {
            TNode u = StartNode(2*a);
            TNode v = EndNode(2*a);

            if (   LCap(2*a)>0
                && (   !(characteristic & MST_ONE_CYCLE)
                    || (u!=r && v!=r)
                   )
               )
            {
                if (Find(u)==Find(v))
                {
                    M.SetBounds(sign*InfFloat,sign*InfFloat);
                    M.Shutdown(LOG_RES,"...Fixed subtour detected");

                    return sign*InfFloat;
                }

                Merge(u,v);
                sum += MST_Length(characteristic,potential,2*a);
                rank++;

                #if defined(_LOGGING_)

                sprintf(CT.logBuffer,
                    "Mandatory arc %lu=(%lu,%lu), length: %g",
                    static_cast<unsigned long>(2*a),
                    static_cast<unsigned long>(u),
                    static_cast<unsigned long>(v),
                    static_cast<double>(MST_Length(characteristic,potential,2*a)));
                LogEntry(LOG_METH2,CT.logBuffer);

                #endif
            }
        }
    }

    goblinQueue<TArc,TFloat>* Q = NewArcHeap();

    for (TArc a=0;a<m;a++)
    {
        if (Length(2*a)!=sign*InfFloat && UCap(2*a)>0)
        {
            Q->Insert(a,sign*MST_Length(characteristic,potential,2*a));
        }

        #if defined(_PROGRESS_)

        M.ProgressStep(1);

        #endif
    }

    while (!Q->Empty() && (rank<n-1))
    {
        TArc a = 2*(Q->Delete());

        TNode u = StartNode(a);
        TNode v = EndNode(a);

        TNode Bu = Find(u);
        TNode Bv = Find(v);

        if ((!(characteristic & MST_ONE_CYCLE) && Bu!=Bv) ||
            ((characteristic & MST_ONE_CYCLE) && Bu!=Bv && Bu!=r && Bv!=r && UCap(a)>0))
        {
            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,
                    "Contracted arc %lu=(%lu,%lu), length: %g",
                    static_cast<unsigned long>(a),
                    static_cast<unsigned long>(Bu),
                    static_cast<unsigned long>(Bv),
                    static_cast<double>(MST_Length(characteristic,potential,a)));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            Merge(u,v);
            SetSubRelative(a,1);
            sum += MST_Length(characteristic,potential,a);
            rank++;

            M.Trace(1);
        }
    }

    delete Q;

    if (characteristic & MST_ONE_CYCLE)   // Compute 1-tree with node r on the cycle.
    {
        if (rank<n-2)
        {
            M.SetBounds(sign*InfFloat,sign*InfFloat);
            sprintf(CT.logBuffer,"...%lu is a cut node",static_cast<unsigned long>(r));
            M.Shutdown(LOG_RES,CT.logBuffer);

            return sign*InfFloat;
        }

        TArc a1 = NoArc;
        TArc a2 = NoArc;

        THandle H = Investigate();
        investigator &I = Investigator(H);
        while (I.Active(r))
        {
            TArc a = I.Read(r);

            if (EndNode(a)!=r && UCap(a)>0)
            {
                if (   a1==NoArc
                    || sign*MST_Length(characteristic,potential,a)
                           <sign*MST_Length(characteristic,potential,a1)
                   )
                {
                    a2 = a1;
                    a1 = a;
                }
                else if (   a2==NoArc
                         || sign*MST_Length(characteristic,potential,a)
                                < sign*MST_Length(characteristic,potential,a2)
                        )
                {
                    a2 = a;
                }
            }
        }

        TArc fixedArcs = 0;
        I.Reset(r);
        while (I.Active(r))
        {
            TArc a = I.Read(r);
            if (LCap(a)>0 && a!=a1 && a!=a2)
            {
                a2 = a1;
                a1 = a;
                fixedArcs++;
            }
        }

        Close(H);

        if (fixedArcs>2)
        {
            M.SetBounds(sign*InfFloat,sign*InfFloat);
            sprintf(CT.logBuffer,"...Too much fixed arcs adjacent with %lu",
                static_cast<unsigned long>(r));
            M.Shutdown(LOG_RES,CT.logBuffer);

            return sign*InfFloat;
        }

        if (a2==NoArc)
        {
            M.SetBounds(sign*InfFloat,sign*InfFloat);
            sprintf(CT.logBuffer,"...There is no cycle through node %lu",
                static_cast<unsigned long>(r));
            M.Shutdown(LOG_RES,CT.logBuffer);

            return sign*InfFloat;
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>=2)
        {
            sprintf(CT.logBuffer,
                "Adding arc %lu=(%lu,%lu), arc length: %g",
                static_cast<unsigned long>(a1),
                static_cast<unsigned long>(r),
                static_cast<unsigned long>(EndNode(a1)),
                static_cast<double>(MST_Length(characteristic,potential,a1)));
            LogEntry(LOG_METH2,CT.logBuffer);

            sprintf(CT.logBuffer,
                "Adding arc %lu=(%lu,%lu), arc length: %g",
                static_cast<unsigned long>(a2),
                static_cast<unsigned long>(r),
                static_cast<unsigned long>(EndNode(a2)),
                static_cast<double>(MST_Length(characteristic,potential,a2)));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        if (Sub(a1)==0) SetSub(a1,1);
        if (Sub(a2)==0) SetSub(a2,1);

        sum += MST_Length(characteristic,potential,a1)
             + MST_Length(characteristic,potential,a2);
        rank += 2;

        M.Trace(1);
    }

    if (!(characteristic & MST_ONE_CYCLE) && rank<n-1)
    {
        sprintf(CT.logBuffer,"...Graph rank is %lu",static_cast<unsigned long>(rank));
        LogEntry(LOG_RES,CT.logBuffer);

        sum = sign*InfFloat;
    }

    M.SetBounds(sum,sum);

    if (sum!=sign*InfFloat)
    {
        sprintf(CT.logBuffer,"...Final spanning tree has weight %g",sum);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }
    else
    {
        M.Shutdown(LOG_RES,"...No spanning tree exists");
    }

    return sum;
}


TFloat abstractMixedGraph::MST_Edmonds(TOptMST characteristic,TNode r)
    throw(ERRange,ERRejected)
{
    if (r==NoNode) r = 0;

    #if defined(_FAILSAVE_)

    if (r>=n) NoSuchNode("MST_Edmonds",r);

    if ((characteristic & MST_ONE_CYCLE) && !IsDirected())
    {
        Error(MSG_WARN,"MST_Edmonds","Method does not apply to undirected graphs");
    }

    #endif

    moduleGuard M(ModEdmondsArb,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter((3.0*n-3.0)*m);

    #endif

    TNode nTotal = 2*n;
    nestedFamily<TNode> S(n,nTotal,CT);

    TArc* inArc = new TArc[nTotal];
    bool* support = new bool[nTotal];
    for (TNode v=0;v<nTotal;v++)
    {
        inArc[v] = NoArc;
        support[v] = false;
    }

    staticQueue<TArc>** Q = new staticQueue<TArc>*[2*n];
    Q[0] = new staticQueue<TArc>(2*m,CT);

    for (TNode i=1;i<2*n;i++) Q[i] = new staticQueue<TArc>(*Q[0]);


    TFloat sign = 1;
    if (characteristic & MST_MAX) sign = -1;

    TFloat* potential = GetPotentials();
    TFloat* modLength = new TFloat[2*m];
    for (TArc a=0;a<2*m;a++)
    {
        if (LCap(a)==0)
        {
            modLength[a] = MST_Length(characteristic,potential,a);
        }
        else modLength[a] = -sign*InfFloat;

        TNode u = StartNode(a);
        TNode v = EndNode(a);

        if (   !Blocking(a)
            && u!=v
            && v!=r
            && UCap(a)>0
        )
        {
            Q[v] -> Insert(a);
        }
    }

    LogEntry(LOG_METH,"Shrinking cycles...");
    OpenFold();

    TNode rank = 0;
    TNode nCycles = 0;
    TNode scanNode = 0;

    while (true)
    {
        while (scanNode<n+nCycles && (scanNode==r || S.Set(scanNode)!=scanNode)) scanNode++;

        if (scanNode>=n+nCycles) break;

        // Choose an entering arc for scanNode

        TArc minArc = NoArc;
        TArc a = Q[scanNode]->First();

        while (a<2*m)
        {
            if ( minArc==NoArc || sign*modLength[minArc]>sign*modLength[a] )
            {
                minArc = a;
            }

            if (Q[scanNode]->Successor(a)==a) break;

            a = Q[scanNode]->Successor(a);
        }

        if (minArc==NoArc || modLength[minArc]==sign*InfFloat) break;

        scanNode++;


        TNode u = S.Set(StartNode(minArc));
        TNode v = S.Set(EndNode(minArc));

        inArc[v] = minArc;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"inArc[%lu] = %lu",
                static_cast<unsigned long>(v),static_cast<unsigned long>(minArc));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        #if defined(_TRACING_)

        if (CT.traceLevel==3 && IsDirected())
        {
            explicitSurfaceGraph G(*this,S,modLength,inArc);
            M.Trace(G,m);
        }

        #endif

        TNode w = u;
        while (w!=v && inArc[w]!=NoArc) w = S.Set(StartNode(inArc[w]));

        if (w!=v)
        {
            rank++;
            continue;
        }


        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Merging cycle (%lu",
                static_cast<unsigned long>(u));
            LH = LogStart(LOG_METH2,CT.logBuffer);
        }

        #endif

        support[u] = true;
        w = u;
        while (w!=v)
        {
            w = S.Set(StartNode(inArc[w]));
            support[w] = true;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(w));
                LogAppend(LH,CT.logBuffer);
            }

            #endif
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,",%lu)",static_cast<unsigned long>(u));
            LogEnd(LH,CT.logBuffer);
        }

        #endif


        TNode cycle = S.MakeSet();

        // Collect the incoming arcs for the super node cycle
        // and update the modified length

        w = NoNode;

        while (w!=v)
        {
            if (w==NoNode)
            {
                w = u;
            }
            else
            {
                w = S.Set(StartNode(inArc[w]));
            }

            while (!(Q[w]->Empty()))
            {
                TArc a = Q[w]->Delete();
                TNode u = S.Set(StartNode(a));

                if (support[u]) continue;

                if (modLength[a]!=-sign*InfFloat)
                {
                    if (modLength[inArc[w]]==-sign*InfFloat)
                    {
                        modLength[a] = sign*InfFloat;
                    }
                    else modLength[a] -= sign*modLength[inArc[w]];
                }

                Q[cycle] -> Insert(a);
            }
        }

        // Merge the support[] set into the super node cycle

        support[u] = false;
        S.Merge(cycle,u);
        w = u;
        while (w!=v)
        {
            w = S.Set(StartNode(inArc[w]));
            support[w] = false;
            S.Merge(cycle,w);
        }
        S.FixSet(cycle);

        nCycles++;

        #if defined(_TRACING_)

        if (CT.traceLevel==3 && IsDirected())
        {
            explicitSurfaceGraph G(*this,S,modLength,inArc);
            M.Trace(G,m);
        }

        #endif
    }

    for (TNode i=0;i<2*n;i++) delete Q[2*n-i-1];
    delete[] Q;



    #if defined(_PROGRESS_)

    M.SetProgressCounter((3.0*n-3.0-nCycles)*m);

    #endif

    CloseFold();
    LogEntry(LOG_METH,"Expanding cycles...");
    OpenFold();

    for (TNode v=n+nCycles-1;v>=n;v--)
    {
        TArc a = inArc[v];

        if (a==NoArc)
        {
            TNode u = S.First(v);
            S.Split(v);

            TNode thisRoot = u;
            TNode w = S.Set(StartNode(inArc[u]));

            while (w!=u)
            {
                if (sign*modLength[inArc[w]]>sign*modLength[inArc[thisRoot]])
                {
                    thisRoot = w;
                }
                w = S.Set(StartNode(inArc[w]));
            }

            inArc[thisRoot] = NoArc;

            #if defined(_LOGGING_)

            if (CT.logMeth>1 && IsDirected())
            {
                sprintf(CT.logBuffer,"inArc[%lu] = *",
                    static_cast<unsigned long>(thisRoot));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
        else
        {
            S.Split(v);

            TNode u = S.Set(EndNode(a));
            inArc[u] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"inArc[%lu] = %lu",
                    static_cast<unsigned long>(u),
                    static_cast<unsigned long>(a));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }

        #if defined(_TRACING_)

        if (CT.traceLevel==3)
        {
            explicitSurfaceGraph G(*this,S,modLength,inArc);
            M.Trace(G,m);
        }

        #endif
    }

    if ((characteristic & MST_ONE_CYCLE) && r!=NoNode)
    {
        TArc minArc = NoArc;
        THandle H = Investigate();
        investigator &I = Investigator(H);
        while (I.Active(r))
        {
            TArc a = I.Read(r);

            if (   EndNode(a)!=r
                && !Blocking(a^1)
                && UCap(a)>0
                && (   minArc==NoArc
                    || sign*modLength[minArc]>sign*modLength[a^1]
                   )
               )
            {
                minArc = a^1;
            }
        }
        Close(H);

        if (minArc!=NoArc)
        {
            inArc[r] = minArc;
            rank++;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"inArc[%lu] = %lu",
                    static_cast<unsigned long>(r),
                    static_cast<unsigned long>(minArc));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
    }

    TFloat sum = 0;
    TArc* pred = RawPredecessors();

    for (TNode v=0;v<n;v++)
    {
        pred[v] = inArc[v];

        if (inArc[v]!=NoArc) sum += MST_Length(characteristic,potential,inArc[v]);
    }

    delete[] inArc;
    delete[] support;
    delete[] modLength;

    CloseFold();

    if ((!(characteristic & MST_ONE_CYCLE) && rank!=n-1) ||
        ((characteristic & MST_ONE_CYCLE) && rank!=n)
       )
    {
        sum = sign*InfFloat;

        if (CT.logRes)
        {
            sprintf(CT.logBuffer,"...Graph rank is %lu",
                static_cast<unsigned long>(rank));
            LogEntry(LOG_RES,CT.logBuffer);
        }
    }

    M.SetBounds(sum,sum);

    if (sum!=sign*InfFloat)
    {
        sprintf(CT.logBuffer,"...Final spanning tree has weight %g",static_cast<double>(sum));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }
    else
    {
        M.Shutdown(LOG_RES,"...No spanning tree exists");
    }

    return sum;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2003
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   solveMinTree.cpp
/// \brief  Minimum spanning tree methods for both the directed and the undirected setting

#include "mixedGraph.h"
#include "nestedFamily.h"
#include "abstractGraph.h"
#include "staticQueue.h"
#include "moduleGuard.h"


TFloat abstractMixedGraph::MinTree(TMethMST method,TOptMST characteristic,TNode root)
    throw(ERRange,ERRejected)
{
    if (root>=n) root = DefaultRootNode();

    #if defined(_FAILSAVE_)

    if (root>=n && root!=NoNode) NoSuchNode("MinTree",root);

    #endif

    if (method==MST_DEFAULT) method = TMethMST(CT.methMST);

    if (   method==MST_EDMONDS
        && ((characteristic & MST_ONE_CYCLE) || !CLCap() || MaxLCap()>0)
        && !IsDirected()
       )
    {
        method = MST_KRUSKAL;
    }

    const char* objSense = (characteristic & MST_MAX) ? "maximum" : "minimum";

    if (characteristic & MST_ONE_CYCLE)
    {
        sprintf(CT.logBuffer,"Computing %s one cycle tree...",objSense);
    }
    else
    {
        sprintf(CT.logBuffer,"Computing %s spanning tree...",objSense);
    }

    moduleGuard M(ModMinTree,*this,CT.logBuffer,moduleGuard::NO_INDENT);

    TFloat totalWeight = InfFloat;

    switch (method)
    {
        case MST_PRIM:
        case MST_PRIM2:
        {
            totalWeight = MST_Prim(method,characteristic,root);

            if (root!=NoNode)
            {
                InitSubgraph();
                AddToSubgraph();
            }

            break;
        }
        case MST_KRUSKAL:
        {
            if (root!=NoNode)
            {
                totalWeight = MST_Kruskal(characteristic,root);

                if (fabs(totalWeight)<InfFloat)
                {
                    ExtractTree(root,characteristic);
                }
            }
            else totalWeight = MST_Kruskal(characteristic);

            break;
        }
        case MST_EDMONDS:
        {
            totalWeight = MST_Edmonds(characteristic,root);

            if (root!=NoNode)
            {
                InitSubgraph();
                AddToSubgraph();
            }

            break;
        }
        default:
        {
            UnknownOption("MinTree",method);
        }
    }

    return totalWeight;
}


TFloat abstractMixedGraph::MST_Length(TOptMST characteristic,
    TFloat* potential,TArc a) throw(ERRange)
{
    if (characteristic & MST_REDUCED) return RedLength(potential,a);

    return Length(a);
}


TFloat abstractMixedGraph::MST_Prim(TMethMST method,
    TOptMST characteristic,TNode r) throw(ERRange,ERRejected)
{
    if (r==NoNode) r = 0;

    #if defined(_FAILSAVE_)

    if (r>=n) NoSuchNode("MST_Prim",r);

    #endif

    TModule moduleID = ModPrim2;

    if (!CLCap() || MaxLCap()>0) method = MST_PRIM2;

    if (method==MST_PRIM) moduleID = ModPrim;

    moduleGuard M(moduleID,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(n);

    #endif

    TFloat sign = (characteristic & MST_MAX) ? -1 : 1;

    TFloat* dist = InitDistanceLabels(sign*InfFloat);
    TArc*   pred = InitPredecessors();
    TFloat* potential = GetPotentials();

    TFloat sum = 0;
    THandle H = Investigate();
    investigator &I = Investigator(H);

    TNode nExplored = 0;
    TNode r2 = r;

    if (characteristic & MST_ONE_CYCLE)
    {
        // Compute 1-cycle tree with r and some other node r2 on the cycle.
        // Compute  arcs a1,a2 which are incident with r on this cycle

        TArc a1 = NoArc;
        TArc a2 = NoArc;

        while (I.Active(r))
        {
            TArc a = I.Read(r);

            if (EndNode(a)!=r && UCap(a)>0)
            {
                if (   a1==NoArc || LCap(a)>0
                    || (   sign*MST_Length(characteristic,potential,a)
                               < sign*MST_Length(characteristic,potential,a1)
                        && LCap(a1)==0
                       )
                   )
                {
                    a2 = a1;
                    a1 = a;
                }
                else if (   a2==NoArc
                         || (   sign*MST_Length(characteristic,potential,a)
                                    < sign*MST_Length(characteristic,potential,a2)
                             && LCap(a2)==0
                            )
                        )
                {
                    a2 = a;
                }
            }
        }

        if (a2==NoArc)
        {
            Close(H);

            M.SetBounds(sign*InfFloat,sign*InfFloat);
            sprintf(CT.logBuffer,"...There is no cycle through node %lu",
                static_cast<unsigned long>(r));
            M.Shutdown(LOG_RES,CT.logBuffer);

            return sign*InfFloat;
        }

        r2 = EndNode(a1);
        pred[r2] = a1;
        pred[r] = a2^1;
        dist[r] = -sign*InfFloat;
        sum += MST_Length(characteristic,potential,a1)
             + MST_Length(characteristic,potential,a2);
        nExplored = 1;

        M.Trace();
    }

    TNode u = r2;
    dist[u] = 0;

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Expanded nodes: ");

    if (CT.logMeth>1 && r!=r2)
    {
        sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(r));
        LogAppend(LH,CT.logBuffer);
    }

    #endif

    if (method==MST_PRIM)
    {
        // Basic version of the algorithm

        while (u!=NoNode && dist[u]!=sign*InfFloat)
        {
            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"%lu ",
                    static_cast<unsigned long>(u));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            sum += dist[u];
            dist[u] = -sign*InfFloat;
            nExplored++;

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                TNode v = EndNode(a);
                TFloat l = MST_Length(characteristic,potential,a);

                if (sign*dist[v]>sign*l && UCap(a)>0)
                {
                    dist[v] = l;
                    pred[v] = a;
                }
            }

            u = NoNode;
            for (TNode v=0;v<n;v++)
                if (   dist[v]!=-sign*InfFloat
                    && (u==NoNode || sign*dist[v]<sign*dist[u]) )
                {
                    u = v;
                }

            M.Trace(1);
        }
    }
    else
    {
        // Enhanced version of the algorithm

        goblinQueue<TNode,TFloat>* Q = nHeap;

        if (Q!=NULL) Q -> Init();
        else Q = NewNodeHeap();

        Q -> Insert(u,0);

        while (!(Q->Empty()))
        {
            u = Q->Delete();
            dist[u] = -sign*InfFloat;
            nExplored++;

            if (u!=r2) sum += MST_Length(characteristic,potential,pred[u]);

            #if defined(_LOGGING_)

            if (CT.logMeth>=2)
            {
                sprintf(CT.logBuffer,"%lu ",
                    static_cast<unsigned long>(u));
                LogAppend(LH,CT.logBuffer);
            }

            #endif

            while (I.Active(u))
            {
                TArc a = I.Read(u);
                TNode v = EndNode(a);
                TFloat l = -sign*InfFloat;

                if (LCap(a)==0)
                {
                    l = MST_Length(characteristic,potential,a);
                }

                if (sign*dist[v]>sign*l && UCap(a)>0)
                {
                    if (dist[v]==sign*InfFloat)
                    {
                        Q->Insert(v,sign*l);
                    }
                    else Q->ChangeKey(v,sign*l);

                    dist[v] = l;
                    pred[v] = a;
                }
            }

            M.Trace(1);
        }

        if (nHeap==NULL) delete Q;
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    Close(H);

    M.SetBounds(sum,sum);

    if (nExplored<n)
    {
        sum = sign*InfFloat;
        M.Shutdown(LOG_RES,"...Graph is disconnected");
    }
    else
    {
        sprintf(CT.logBuffer,"...Final spanning tree has weight %g",sum);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return sum;
}


TFloat abstractMixedGraph::MST_Kruskal(TOptMST characteristic,TNode r)
    throw(ERRange,ERRejected)
{
    if (r==NoNode) r = 0;

    #if defined(_FAILSAVE_)

    if (r>=n) NoSuchNode("MST_Kruskal",r);

    #endif

    moduleGuard M(ModKruskal,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(m+n-1);

    #endif

    TFloat sign = 1;
    if (characteristic & MST_MAX) sign = -1;

    InitPartition();
    InitSubgraph();
    TFloat* potential = GetPotentials();

    for (TNode i=0;i<n;i++) Bud(i);

    TFloat sum = 0;
    TNode rank = 0;

    if (!CLCap() || MaxLCap()>0)
    {
        for (TArc a=0;a<m;a++)
        {
            TNode u = StartNode(2*a);
            TNode v = EndNode(2*a);

            if (   LCap(2*a)>0
                && (   !(characteristic & MST_ONE_CYCLE)
                    || (u!=r && v!=r)
                   )
               )
            {
                if (Find(u)==Find(v))
                {
                    M.SetBounds(sign*InfFloat,sign*InfFloat);
                    M.Shutdown(LOG_RES,"...Fixed subtour detected");

                    return sign*InfFloat;
                }

                Merge(u,v);
                sum += MST_Length(characteristic,potential,2*a);
                rank++;

                #if defined(_LOGGING_)

                sprintf(CT.logBuffer,
                    "Mandatory arc %lu=(%lu,%lu), length: %g",
                    static_cast<unsigned long>(2*a),
                    static_cast<unsigned long>(u),
                    static_cast<unsigned long>(v),
                    static_cast<double>(MST_Length(characteristic,potential,2*a)));
                LogEntry(LOG_METH2,CT.logBuffer);

                #endif
            }
        }
    }

    goblinQueue<TArc,TFloat>* Q = NewArcHeap();

    for (TArc a=0;a<m;a++)
    {
        if (Length(2*a)!=sign*InfFloat && UCap(2*a)>0)
        {
            Q->Insert(a,sign*MST_Length(characteristic,potential,2*a));
        }

        #if defined(_PROGRESS_)

        M.ProgressStep(1);

        #endif
    }

    while (!Q->Empty() && (rank<n-1))
    {
        TArc a = 2*(Q->Delete());

        TNode u = StartNode(a);
        TNode v = EndNode(a);

        TNode Bu = Find(u);
        TNode Bv = Find(v);

        if ((!(characteristic & MST_ONE_CYCLE) && Bu!=Bv) ||
            ((characteristic & MST_ONE_CYCLE) && Bu!=Bv && Bu!=r && Bv!=r && UCap(a)>0))
        {
            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,
                    "Contracted arc %lu=(%lu,%lu), length: %g",
                    static_cast<unsigned long>(a),
                    static_cast<unsigned long>(Bu),
                    static_cast<unsigned long>(Bv),
                    static_cast<double>(MST_Length(characteristic,potential,a)));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            Merge(u,v);
            SetSubRelative(a,1);
            sum += MST_Length(characteristic,potential,a);
            rank++;

            M.Trace(1);
        }
    }

    delete Q;

    if (characteristic & MST_ONE_CYCLE)   // Compute 1-tree with node r on the cycle.
    {
        if (rank<n-2)
        {
            M.SetBounds(sign*InfFloat,sign*InfFloat);
            sprintf(CT.logBuffer,"...%lu is a cut node",static_cast<unsigned long>(r));
            M.Shutdown(LOG_RES,CT.logBuffer);

            return sign*InfFloat;
        }

        TArc a1 = NoArc;
        TArc a2 = NoArc;

        THandle H = Investigate();
        investigator &I = Investigator(H);
        while (I.Active(r))
        {
            TArc a = I.Read(r);

            if (EndNode(a)!=r && UCap(a)>0)
            {
                if (   a1==NoArc
                    || sign*MST_Length(characteristic,potential,a)
                           <sign*MST_Length(characteristic,potential,a1)
                   )
                {
                    a2 = a1;
                    a1 = a;
                }
                else if (   a2==NoArc
                         || sign*MST_Length(characteristic,potential,a)
                                < sign*MST_Length(characteristic,potential,a2)
                        )
                {
                    a2 = a;
                }
            }
        }

        TArc fixedArcs = 0;
        I.Reset(r);
        while (I.Active(r))
        {
            TArc a = I.Read(r);
            if (LCap(a)>0 && a!=a1 && a!=a2)
            {
                a2 = a1;
                a1 = a;
                fixedArcs++;
            }
        }

        Close(H);

        if (fixedArcs>2)
        {
            M.SetBounds(sign*InfFloat,sign*InfFloat);
            sprintf(CT.logBuffer,"...Too much fixed arcs adjacent with %lu",
                static_cast<unsigned long>(r));
            M.Shutdown(LOG_RES,CT.logBuffer);

            return sign*InfFloat;
        }

        if (a2==NoArc)
        {
            M.SetBounds(sign*InfFloat,sign*InfFloat);
            sprintf(CT.logBuffer,"...There is no cycle through node %lu",
                static_cast<unsigned long>(r));
            M.Shutdown(LOG_RES,CT.logBuffer);

            return sign*InfFloat;
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>=2)
        {
            sprintf(CT.logBuffer,
                "Adding arc %lu=(%lu,%lu), arc length: %g",
                static_cast<unsigned long>(a1),
                static_cast<unsigned long>(r),
                static_cast<unsigned long>(EndNode(a1)),
                static_cast<double>(MST_Length(characteristic,potential,a1)));
            LogEntry(LOG_METH2,CT.logBuffer);

            sprintf(CT.logBuffer,
                "Adding arc %lu=(%lu,%lu), arc length: %g",
                static_cast<unsigned long>(a2),
                static_cast<unsigned long>(r),
                static_cast<unsigned long>(EndNode(a2)),
                static_cast<double>(MST_Length(characteristic,potential,a2)));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        if (Sub(a1)==0) SetSub(a1,1);
        if (Sub(a2)==0) SetSub(a2,1);

        sum += MST_Length(characteristic,potential,a1)
             + MST_Length(characteristic,potential,a2);
        rank += 2;

        M.Trace(1);
    }

    if (!(characteristic & MST_ONE_CYCLE) && rank<n-1)
    {
        sprintf(CT.logBuffer,"...Graph rank is %lu",static_cast<unsigned long>(rank));
        LogEntry(LOG_RES,CT.logBuffer);

        sum = sign*InfFloat;
    }

    M.SetBounds(sum,sum);

    if (sum!=sign*InfFloat)
    {
        sprintf(CT.logBuffer,"...Final spanning tree has weight %g",sum);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }
    else
    {
        M.Shutdown(LOG_RES,"...No spanning tree exists");
    }

    return sum;
}


TFloat abstractMixedGraph::MST_Edmonds(TOptMST characteristic,TNode r)
    throw(ERRange,ERRejected)
{
    if (r==NoNode) r = 0;

    #if defined(_FAILSAVE_)

    if (r>=n) NoSuchNode("MST_Edmonds",r);

    if ((characteristic & MST_ONE_CYCLE) && !IsDirected())
    {
        Error(MSG_WARN,"MST_Edmonds","Method does not apply to undirected graphs");
    }

    #endif

    moduleGuard M(ModEdmondsArb,*this,
        moduleGuard::SHOW_TITLE | moduleGuard::SYNC_BOUNDS);

    #if defined(_PROGRESS_)

    M.InitProgressCounter((3.0*n-3.0)*m);

    #endif

    TNode nTotal = 2*n;
    nestedFamily<TNode> S(n,nTotal,CT);

    TArc* inArc = new TArc[nTotal];
    bool* support = new bool[nTotal];
    for (TNode v=0;v<nTotal;v++)
    {
        inArc[v] = NoArc;
        support[v] = false;
    }

    staticQueue<TArc>** Q = new staticQueue<TArc>*[2*n];
    Q[0] = new staticQueue<TArc>(2*m,CT);

    for (TNode i=1;i<2*n;i++) Q[i] = new staticQueue<TArc>(*Q[0]);


    TFloat sign = 1;
    if (characteristic & MST_MAX) sign = -1;

    TFloat* potential = GetPotentials();
    TFloat* modLength = new TFloat[2*m];
    for (TArc a=0;a<2*m;a++)
    {
        if (LCap(a)==0)
        {
            modLength[a] = MST_Length(characteristic,potential,a);
        }
        else modLength[a] = -sign*InfFloat;

        TNode u = StartNode(a);
        TNode v = EndNode(a);

        if (   !Blocking(a)
            && u!=v
            && v!=r
            && UCap(a)>0
        )
        {
            Q[v] -> Insert(a);
        }
    }

    LogEntry(LOG_METH,"Shrinking cycles...");
    OpenFold();

    TNode rank = 0;
    TNode nCycles = 0;
    TNode scanNode = 0;

    while (true)
    {
        while (scanNode<n+nCycles && (scanNode==r || S.Set(scanNode)!=scanNode)) scanNode++;

        if (scanNode>=n+nCycles) break;

        // Choose an entering arc for scanNode

        TArc minArc = NoArc;
        TArc a = Q[scanNode]->First();

        while (a<2*m)
        {
            if ( minArc==NoArc || sign*modLength[minArc]>sign*modLength[a] )
            {
                minArc = a;
            }

            if (Q[scanNode]->Successor(a)==a) break;

            a = Q[scanNode]->Successor(a);
        }

        if (minArc==NoArc || modLength[minArc]==sign*InfFloat) break;

        scanNode++;


        TNode u = S.Set(StartNode(minArc));
        TNode v = S.Set(EndNode(minArc));

        inArc[v] = minArc;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"inArc[%lu] = %lu",
                static_cast<unsigned long>(v),static_cast<unsigned long>(minArc));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        #if defined(_TRACING_)

        if (CT.traceLevel==3 && IsDirected())
        {
            explicitSurfaceGraph G(*this,S,modLength,inArc);
            M.Trace(G,m);
        }

        #endif

        TNode w = u;
        while (w!=v && inArc[w]!=NoArc) w = S.Set(StartNode(inArc[w]));

        if (w!=v)
        {
            rank++;
            continue;
        }


        #if defined(_LOGGING_)

        THandle LH = NoHandle;

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"Merging cycle (%lu",
                static_cast<unsigned long>(u));
            LH = LogStart(LOG_METH2,CT.logBuffer);
        }

        #endif

        support[u] = true;
        w = u;
        while (w!=v)
        {
            w = S.Set(StartNode(inArc[w]));
            support[w] = true;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(w));
                LogAppend(LH,CT.logBuffer);
            }

            #endif
        }

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,",%lu)",static_cast<unsigned long>(u));
            LogEnd(LH,CT.logBuffer);
        }

        #endif


        TNode cycle = S.MakeSet();

        // Collect the incoming arcs for the super node cycle
        // and update the modified length

        w = NoNode;

        while (w!=v)
        {
            if (w==NoNode)
            {
                w = u;
            }
            else
            {
                w = S.Set(StartNode(inArc[w]));
            }

            while (!(Q[w]->Empty()))
            {
                TArc a = Q[w]->Delete();
                TNode u = S.Set(StartNode(a));

                if (support[u]) continue;

                if (modLength[a]!=-sign*InfFloat)
                {
                    if (modLength[inArc[w]]==-sign*InfFloat)
                    {
                        modLength[a] = sign*InfFloat;
                    }
                    else modLength[a] -= sign*modLength[inArc[w]];
                }

                Q[cycle] -> Insert(a);
            }
        }

        // Merge the support[] set into the super node cycle

        support[u] = false;
        S.Merge(cycle,u);
        w = u;
        while (w!=v)
        {
            w = S.Set(StartNode(inArc[w]));
            support[w] = false;
            S.Merge(cycle,w);
        }
        S.FixSet(cycle);

        nCycles++;

        #if defined(_TRACING_)

        if (CT.traceLevel==3 && IsDirected())
        {
            explicitSurfaceGraph G(*this,S,modLength,inArc);
            M.Trace(G,m);
        }

        #endif
    }

    for (TNode i=0;i<2*n;i++) delete Q[2*n-i-1];
    delete[] Q;



    #if defined(_PROGRESS_)

    M.SetProgressCounter((3.0*n-3.0-nCycles)*m);

    #endif

    CloseFold();
    LogEntry(LOG_METH,"Expanding cycles...");
    OpenFold();

    for (TNode v=n+nCycles-1;v>=n;v--)
    {
        TArc a = inArc[v];

        if (a==NoArc)
        {
            TNode u = S.First(v);
            S.Split(v);

            TNode thisRoot = u;
            TNode w = S.Set(StartNode(inArc[u]));

            while (w!=u)
            {
                if (sign*modLength[inArc[w]]>sign*modLength[inArc[thisRoot]])
                {
                    thisRoot = w;
                }
                w = S.Set(StartNode(inArc[w]));
            }

            inArc[thisRoot] = NoArc;

            #if defined(_LOGGING_)

            if (CT.logMeth>1 && IsDirected())
            {
                sprintf(CT.logBuffer,"inArc[%lu] = *",
                    static_cast<unsigned long>(thisRoot));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
        else
        {
            S.Split(v);

            TNode u = S.Set(EndNode(a));
            inArc[u] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"inArc[%lu] = %lu",
                    static_cast<unsigned long>(u),
                    static_cast<unsigned long>(a));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }

        #if defined(_TRACING_)

        if (CT.traceLevel==3)
        {
            explicitSurfaceGraph G(*this,S,modLength,inArc);
            M.Trace(G,m);
        }

        #endif
    }

    if ((characteristic & MST_ONE_CYCLE) && r!=NoNode)
    {
        TArc minArc = NoArc;
        THandle H = Investigate();
        investigator &I = Investigator(H);
        while (I.Active(r))
        {
            TArc a = I.Read(r);

            if (   EndNode(a)!=r
                && !Blocking(a^1)
                && UCap(a)>0
                && (   minArc==NoArc
                    || sign*modLength[minArc]>sign*modLength[a^1]
                   )
               )
            {
                minArc = a^1;
            }
        }
        Close(H);

        if (minArc!=NoArc)
        {
            inArc[r] = minArc;
            rank++;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"inArc[%lu] = %lu",
                    static_cast<unsigned long>(r),
                    static_cast<unsigned long>(minArc));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
    }

    TFloat sum = 0;
    TArc* pred = RawPredecessors();

    for (TNode v=0;v<n;v++)
    {
        pred[v] = inArc[v];

        if (inArc[v]!=NoArc) sum += MST_Length(characteristic,potential,inArc[v]);
    }

    delete[] inArc;
    delete[] support;
    delete[] modLength;

    CloseFold();

    if ((!(characteristic & MST_ONE_CYCLE) && rank!=n-1) ||
        ((characteristic & MST_ONE_CYCLE) && rank!=n)
       )
    {
        sum = sign*InfFloat;

        if (CT.logRes)
        {
            sprintf(CT.logBuffer,"...Graph rank is %lu",
                static_cast<unsigned long>(rank));
            LogEntry(LOG_RES,CT.logBuffer);
        }
    }

    M.SetBounds(sum,sum);

    if (sum!=sign*InfFloat)
    {
        sprintf(CT.logBuffer,"...Final spanning tree has weight %g",static_cast<double>(sum));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }
    else
    {
        M.Shutdown(LOG_RES,"...No spanning tree exists");
    }

    return sum;
}
