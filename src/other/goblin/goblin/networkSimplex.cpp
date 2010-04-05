
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, March 2004
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   networkSimplex.cpp
/// \brief  #networkSimplex class implementation

#include "abstractDigraph.h"
#include "networkSimplex.h"


networkSimplex::networkSimplex(abstractDiGraph& GC) throw() :
    managedObject(GC.Context()), G(GC), n(G.N()), m(G.M()),
    piG(G.GetPotentials()), pred(G.GetPredecessors())
{
    l = 0;

    // Initialize data structures for pricing

    k = 30;
    j = 5;

    if (m>60000)
    {
        k = 200;
        j = 20;
    }
    else if (m>10000)
    {
        k = 50;
        j = 10;
    }

    r = m/k;
    if (m!=r*k) r++;

    nextList = 0;

    hotList  = new TArc[j+k];
    swapList = new TArc[j+k];

    // Initialize tree indices

    thread = new TNode[n];
    depth  = new TNode[n];
}


networkSimplex::~networkSimplex() throw()
{
    delete[] hotList;
    delete[] swapList;

    delete[] thread;
    delete[] depth;
}


TArc networkSimplex::PivotArc() throw()
{
    #if defined(_TIMERS_)

    CT.globalTimer[TimerPricing] -> Enable();

    #endif

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Selecting incoming arc...");

    #endif

    TArc pivotArc = NoArc;

    switch (CT.methLPPricing)
    {
        case 0:
        {
            pivotArc = PartialPricing();
            break;
        }
        case 1:
        {
            pivotArc = MultiplePartialPricing();
            break;
        }
        case 2:
        {
            pivotArc = DantzigPricing();
            break;
        }
        case 3:
        {
            pivotArc = FirstEligiblePricing();
            break;
        }
        default:
        {
            UnknownOption("PivotArc",CT.methLPPricing);
        }
    }

    if (CT.logRes>1)
    {
        if (pivotArc!=NoArc)
        {
            sprintf(CT.logBuffer,"...Pivot arc is %lu = (%lu,%lu)",
                static_cast<unsigned long>(pivotArc),
                static_cast<unsigned long>(G.StartNode(pivotArc)),
                static_cast<unsigned long>(G.EndNode(pivotArc)));
            LogEntry(LOG_RES2,CT.logBuffer);
        }
        else LogEntry(LOG_RES2,"...Reached optimality");
    }

    #if defined(_TIMERS_)

    CT.globalTimer[TimerPricing] -> Disable();

    #endif

    return pivotArc;
}


TArc networkSimplex::DantzigPricing() throw()
{
    TArc pivotArc = NoArc;

    for (TArc a=0;a<2*m;a++)
    {
        if (G.ResCap(a)>0 && G.RedLength(piG,a)<0 &&
            (pivotArc==NoArc || G.RedLength(piG,a)<G.RedLength(piG,pivotArc))
           )
        {
            pivotArc = a;
        }
    }

    return pivotArc;
}


TArc networkSimplex::FirstEligiblePricing() throw()
{
    for (TArc i=0;i<2*m;i++)
    {
        TArc a = (nextList+i)%(2*m);

        if (G.ResCap(a)>0 && G.RedLength(piG,a)<0)
        {
            nextList = (nextList+i+1)%(2*m);

            return a;
        }
    }

    return NoArc;
}


TArc networkSimplex::PartialPricing() throw()
{
    TArc pivotArc = NoArc;

    // Delete all arcs from hotList which are not admissible any longer

    TArc lNext = 0;

    for (TArc i=0;i<l;i++)
    {
        TArc a = hotList[i];

        if (G.ResCap(a)>0 && G.RedLength(piG,a)<0)
        {
            swapList[lNext] = hotList[i];
            lNext++;

            if (pivotArc==NoArc || G.RedLength(piG,a)<G.RedLength(piG,pivotArc))
                pivotArc = a;
        }
    }

    TArc* swap2 = swapList;
    swapList = hotList;
    hotList = swap2;
    l = lNext;

    // Fill up candidate list

    if (l<=j)
    {
        LogEntry(LOG_MEM,"Constructing candidate list...");

        TArc a = nextList;
        TArc i = 0;

        for (;l<j+k && i<2*m;i++)
        {
            a = (nextList+i)%(2*m);

            if (G.ResCap(a)>0 && G.RedLength(piG,a)<0)
            {
                hotList[l++] = a;

                if (pivotArc==NoArc || G.RedLength(piG,a)<G.RedLength(piG,pivotArc))
                    pivotArc = a;
            }
        }

        if (l<j+k)
            sprintf(CT.logBuffer,"...%lu candidates found",static_cast<unsigned long>(l));
        else
            sprintf(CT.logBuffer,"...%lu arcs inspected",static_cast<unsigned long>(i));

        LogEntry(LOG_MEM,CT.logBuffer);

        nextList = (a+1)%(2*m);
    }

    return pivotArc;
}


TArc networkSimplex::MultiplePartialPricing() throw()
{
    TArc pivotArc = NoArc;

    // Delete all arcs from hotList which are not admissible any longer

    TArc lNext = 0;

    for (TArc i=0;i<l;i++)
    {
        TArc a = 2*hotList[i];

        if (G.RedLength(piG,a)>0) a++;

        if (G.ResCap(a)>0 && G.RedLength(piG,a)<0)
        {
            swapList[lNext] = hotList[i];
            lNext++;

            if (pivotArc==NoArc || G.RedLength(piG,a)<G.RedLength(piG,pivotArc))
                pivotArc = a;
        }
    }

    TArc* swap2 = swapList;
    swapList = hotList;
    hotList = swap2;
    l = lNext;

    // Extend hotList as long as a complete candidate list fits into

    TArc savedList = nextList;

    while (l<=j)
    {
        for (TArc i=0;i<k;i++)
        {
            TArc a = 2*(i*r+nextList);

            if (a>=2*m) continue;

            if (G.RedLength(piG,a)>0) a++;

            if (G.ResCap(a)>0 && G.RedLength(piG,a)<0)
            {
                hotList[l] = i*r+nextList;
                l++;

                if (pivotArc==NoArc || G.RedLength(piG,a)<G.RedLength(piG,pivotArc))
                    pivotArc = a;
            }
        }

        nextList = (nextList+1)%r;

        if (nextList==savedList) break;
    }

    return pivotArc;
}


void networkSimplex::InitThreadIndex() throw()
{
    LogEntry(LOG_METH2,"Computing thread index...");
    OpenFold();

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Traversed nodes:");

    #endif

    TNode thisDepth = 0;

    THandle H = G.Investigate();
    investigator &I = G.Investigator(H);

    for (TNode r=0;r<n;r++)
    {
        if (pred[r]!=NoArc) continue;

        depth[r] = thisDepth;
        TNode previous = r;
        TNode v = r;
        thread[r] = NoNode;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"  %lu",static_cast<unsigned long>(r));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        while (v!=r || I.Active(v))
        {
            if (I.Active(v))
            {
                TArc a = I.Read(v);
                TNode u = G.EndNode(a);

                if (pred[u]==a)
                {
                    thread[previous] = u;
                    thread[u] = NoNode;
                    depth[u] = ++thisDepth;
                    v = u;
                    previous = u;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(v));
                        LogEntry(MSG_APPEND,CT.logBuffer);
                    }

                    #endif
                }
            }
            else
            {
                v = G.StartNode(pred[v]);
                thisDepth--;
            }
        }
    }

    G.Close(H);

    CloseFold();
}


void networkSimplex::ComputePotentials() throw()
{
    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Computing node potentials...");

    #endif

    OpenFold();
    G.InitPotentials();

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"");

    #endif

    for (TNode r=0;r<n;r++)
    {
        if (pred[r]!=NoArc) continue;

        piG[r] = 0;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"%lu[0] ",static_cast<unsigned long>(r));
            LogEntry(MSG_APPEND,CT.logBuffer);
        }

        #endif

        TNode v = thread[r];

        while (v!=NoNode)
        {
            TArc a = pred[v];
            piG[v] = piG[G.StartNode(a)]+G.RedLength(NULL,a);

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"%lu[%g] ",
                    static_cast<unsigned long>(v),static_cast<double>(piG[v]));
                LogEntry(MSG_APPEND,CT.logBuffer);
            }

            #endif

            v = thread[v];
        }
    }

    CloseFold();
}


TNode networkSimplex::UpdateThread(TNode r,TNode pr,TNode fs) throw()
{
    TNode x = r;
    TNode y = thread[x];

    if (y!=NoNode && y==pr)
    {
        y = thread[fs];
        thread[x] = y;
    }

    while (y!=NoNode && depth[y]>depth[r])
    {
        x = y;
        y = thread[x];

        if (y!=NoNode && y==pr)
        {
            y = thread[fs];
            thread[x] = y;
        }
    }

    if (fs!=NoNode) thread[fs] = r;

    return x;
}


bool networkSimplex::PivotOperation(TArc pivot) throw()
{
    OpenFold();

    #if defined(_LOGGING_)

    LogEntry(LOG_METH2,"Identify pivot cycle...");

    #endif

    TNode u = G.StartNode(pivot);
    TNode v = G.EndNode(pivot);
    TFloat delta = G.ResCap(pivot);
    while (v!=u && (depth[u]>0 || depth[v]>0))
    {
        if (depth[u]>depth[v])
        {
            TArc a = pred[u];
            u = G.StartNode(a);

            if (G.ResCap(a)<delta) delta = G.ResCap(a);
        }
        else
        {
            TArc a = pred[v];
            v = G.StartNode(a);

            if (G.ResCap(a^1)<delta) delta = G.ResCap(a^1);
        }
    }

    if (delta==InfCap)
    {
        LogEntry(LOG_RES,"...Problem is unbounded");
        CloseFold();
        return true;
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Augmenting by %g units of flow...",delta);
        LogEntry(LOG_METH2,CT.logBuffer);
    }

    #endif

    u = G.StartNode(pivot);
    v = G.EndNode(pivot);
    TArc leftCandidate = NoArc;
    TArc rightCandidate = NoArc;
    TNode nBlocking = 0;
    G.Push(pivot,delta);

    if (G.ResCap(pivot)==0)
    {
        nBlocking++;
        rightCandidate = pivot;
    }

    while (v!=u && (depth[u]>0 || depth[v]>0))
    {
        if (depth[u]>depth[v])
        {
            TArc a = pred[u];
            u = G.StartNode(a);
            G.Push(a,delta);

            if (G.ResCap(a)==0)
            {
                nBlocking++;

                if (leftCandidate==NoArc) leftCandidate = a;
            }
        }
        else
        {
            TArc a = pred[v];
            v = G.StartNode(a);
            G.Push(a^1,delta);

            if (G.ResCap(a^1)==0)
            {
                nBlocking++;
                rightCandidate = (a^1);
            }
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"...%lu blocking arcs found",
            static_cast<unsigned long>(nBlocking));
        LogEntry(LOG_METH2,CT.logBuffer);
    }

    #endif

    if (nBlocking==0) G.Display();

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
        LogEntry(LOG_METH2,"Correcting tree indices...");

    #endif

    TArc leaving = rightCandidate; // Arc omitted in the new base tree
    TNode r = NoNode;  // root node of the partial tree hanging from the pivot
    TNode fs = NoNode; // final node in the thread index of this partial tree
    TFloat mu = G.RedLength(piG,pivot); // Change of potentials on this partial tree

    if (leaving!=NoArc)
    {
        // Only reached when delta>0

        if (leaving!=pivot)
        {
            TArc a = (pivot^1);
            r = v = G.EndNode(pivot);
            fs = UpdateThread(v,NoNode,NoNode);

            while (pred[v]!=(leaving^1))
            {
                TArc a2 = pred[v];
                pred[v] = (a^1);
                v = G.StartNode(a2);
                fs = UpdateThread(v,G.EndNode(a2),fs);
                a = a2;
            }

            pred[v] = (a^1);

            TNode v = G.EndNode(leaving);

            while (thread[v]!=G.StartNode(leaving)) v = thread[v];

            thread[v] = thread[fs];

            thread[fs] = thread[G.StartNode(pivot)];
            thread[G.StartNode(pivot)] = G.EndNode(pivot);
        }
    }
    else
    {
        leaving = leftCandidate;

        TArc a = pivot;
        r = u = G.StartNode(pivot);
        fs = UpdateThread(u,NoNode,NoNode);

        while (pred[u]!=leaving)
        {
            TArc a2 = pred[u];
            pred[u] = (a^1);
            u = G.StartNode(a2);
            fs = UpdateThread(u,G.EndNode(a2),fs);
            a = a2;
        }

        pred[u] = (a^1);
        mu *= -1;

        TNode v = G.StartNode(leaving);

        while (thread[v]!=G.EndNode(leaving)) v = thread[v];

        thread[v] = thread[fs];

        thread[fs] = thread[G.EndNode(pivot)];
        thread[G.EndNode(pivot)] = G.StartNode(pivot);
    }

    #if defined(_LOGGING_)

    if (CT.logRes>1 && leaving!=NoArc)
    {
        sprintf(CT.logBuffer,"...Leaving arc is %lu",
            static_cast<unsigned long>(leaving));
        LogEntry(LOG_RES2,CT.logBuffer);
        sprintf(CT.logBuffer,"...Primal improvement to %g",
            static_cast<double>(G.Weight()));
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    #endif

    if (r!=NoNode)
    {
        #if defined(_LOGGING_)

        LogEntry(LOG_METH2,"Correcting node potentials...");

        #endif

        piG[r] += mu;
        depth[r] = depth[G.StartNode(pred[r])]+1;

        v = thread[r];

        while (v!=thread[fs])
        {
            piG[v] += mu;
            depth[v] = depth[G.StartNode(pred[v])]+1;

            v = thread[v];
        }
    }

    CloseFold();

    return false;
}
