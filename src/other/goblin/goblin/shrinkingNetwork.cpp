
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   shrinkingNetwork.cpp
/// \brief  #layeredShrNetwork class implementation

#include "shrinkingNetwork.h"


layeredShrNetwork::layeredShrNetwork(
                        abstractBalancedFNW& _H,TNode _s,
                        staticQueue<TNode>** _Q,
                        staticQueue<TArc>** _Anomalies,
                        staticQueue<TArc>** _Bridges) throw() :
    managedObject(_H.Context()),
    layeredAuxNetwork(_H,_s),
    H(_H), dist(_H.GetNodeColours()),
    Q(_Q), Anomalies(_Anomalies), Bridges( _Bridges)
{
    LeftSupport = new dynamicStack<TNode>(n,CT);
    RightSupport = new dynamicStack<TNode>(n,CT);

    leftProp = new TArc[n];
    rightProp = new TArc[n];
    petal = new TArc[n];
    firstProp = new TArc[2*m];
    breakPoint = new TNode[2*m];

    for (TNode i=0;i<n;i++) leftProp[i] = rightProp[i] = petal[i] = NoArc;

    LI = new iLayeredAuxNetwork(*this);
    RI = new iLayeredAuxNetwork(*this);

    if (!dist) dist = H.InitNodeColours(NoNode);

    align = "  ";

    LogEntry(LOG_MEM,"Layered shrinking network instanciated...");
}


void layeredShrNetwork::Init() throw()
{
    #if defined(_FAILSAVE_)

    if (Phase!=1) Error(ERR_REJECTED,"Init","Inapplicable in phase 2");

    #endif

    layeredAuxNetwork::Init();

    for (TNode i=0;i<n;i++) leftProp[i] = rightProp[i] = NoArc;
}


layeredShrNetwork::~layeredShrNetwork() throw()
{
    delete[] leftProp;
    delete[] rightProp;
    delete[] firstProp;
    delete[] petal;
    delete[] breakPoint;
    delete LeftSupport;
    delete RightSupport;
    delete LI;
    delete RI;

    LogEntry(LOG_MEM,"...Layered shrinking network disallocated");
}


unsigned long layeredShrNetwork::Size() const throw()
{
    return
         sizeof(layeredShrNetwork)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractDiGraph::Allocated()
        + layeredAuxNetwork::Allocated()
        +layeredShrNetwork::Allocated();
}


unsigned long layeredShrNetwork::Allocated() const throw()
{
    unsigned long tmpSize
        = 3*n*sizeof(TArc)      // leftProp[],rightProp[],petal[]
        + 4*m*sizeof(TArc*);    // firstProp[],breakPoint[]

    return tmpSize;
}


TNode layeredShrNetwork::StartNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("StartNode",a);

    #endif

    return H.Base(H.StartNode(a));
}


TCap layeredShrNetwork::UCap(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("UCap",a);

    #endif

    return (TCap)H.BalCap(a);
}


TNode layeredShrNetwork::DDFS(TArc thisPetal) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (thisPetal>=2*m) NoSuchArc("DDFS",thisPetal);

    #endif

    TNode left,right,root,barrier;
    left = root = StartNode(thisPetal);
    right = barrier = StartNode(thisPetal^2);

    if (left==right && dist[left^1]!=NoNode) return left;

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Starting DDFS(%lu), rooted at nodes %lu and %lu",
            static_cast<unsigned long>(thisPetal),
            static_cast<unsigned long>(left),
            static_cast<unsigned long>(right));
        LogEntry(LOG_METH2,CT.logBuffer);
        OpenFold();
    }

    #endif

    LeftSupport -> Insert(left);
    RightSupport -> Insert(right);

    LI -> Reset(left);
    RI -> Reset(right);
    TNode b = NoNode;

    TNode next = NoNode;

    while (b==NoNode)
    {
        if (dist[left]>=dist[right])
        {
            if (LI->Active(left))
            {
                TArc a = LI->Read(left);
                next = StartNode(a);

                if (H.BalCap(a)>0 && leftProp[next]==NoArc && !Blocked(next)
                             && (rightProp[next]!=a || H.BalCap(a)>1))
                {
                    leftProp[next] = a;
                    LeftSupport->Insert(next);
                    LI -> Reset(next);
                    left = next;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Node %lu explored by the left DFS",
                            static_cast<unsigned long>(left));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
            }
            else
            {
                // Backtracking

                if (left!=root && left!=s)
                {
                    left = EndNode(leftProp[left]);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Left backtracking to node %lu",
                            static_cast<unsigned long>(left));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
                else
                {
                    if (left==s) b = s;
                    else
                    {
                        rightProp[right] = NoArc;
                        RightSupport -> Delete();
                        b = barrier;
                    }

                    firstProp[thisPetal] = leftProp[b];
                    firstProp[thisPetal^2] = rightProp[b];
                    breakPoint[thisPetal] = b;
                    breakPoint[thisPetal^2] = NoNode;
                }
            }
        }
        else
        {
            if (RI->Active(right))
            {
                TArc a = RI->Read(right);
                next = StartNode(a);

                if (H.BalCap(a)>0 && rightProp[next]==NoArc && !Blocked(next)
                             && (leftProp[next]!=a || H.BalCap(a)>1))
                {
                    rightProp[next] = a;
                    RightSupport->Insert(next);
                    RI -> Reset(next);
                    right = next;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Node %lu explored by the right DFS",
                            static_cast<unsigned long>(right));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
            }
            else
            {
                // Backtracking

                if (right!=barrier)
                {
                    right = EndNode(rightProp[right]);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Right backtracking to node %lu",
                            static_cast<unsigned long>(right));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
                else
                {
                    right = left;
                    barrier = EndNode(leftProp[left]);
                    rightProp[right] = leftProp[right];
                    RightSupport->Insert(next);
                    leftProp[left] = NoArc;
                    LeftSupport -> Delete();
                    left = barrier;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Node %lu moved to the right DFS",
                            static_cast<unsigned long>(right));
                        LogEntry(LOG_METH2,CT.logBuffer);
                        sprintf(CT.logBuffer,
                            "Left backtracking to node %lu",
                            static_cast<unsigned long>(left));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
            }
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) CloseFold();

    #endif

    return b;
}


void layeredShrNetwork::ShrinkBlossom (TNode b,TArc thisPetal,TNode tenacity)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (b>=n) NoSuchNode("ShrinkBlossom",b);

    if (thisPetal>=2*m) NoSuchArc("ShrinkBlossom",thisPetal);

    #endif

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    if (CT.logMeth>1 && !LeftSupport->Empty())
    {
        sprintf(CT.logBuffer,"Shrinking %lu",static_cast<unsigned long>(b));
        LH = LogStart(LOG_METH2,CT.logBuffer);
    }

    #endif

    leftProp[b] = NoArc;
    rightProp[b] = NoArc;

    while (!LeftSupport->Empty())
    {
        TNode w = LeftSupport->Delete();
        H.Shrink(b,w);

        #if defined(_LOGGING_)

        if (CT.logMeth>1 && w!=b)
        {
            sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(w));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        TNode cw = w^1;

        if (dist[cw]==NoNode)
        {
            dist[cw] = tenacity-dist[w];
            TNode thisDist = dist[cw];

            if (Q[thisDist]==NULL) Q[thisDist] = new staticQueue<TNode>(*Q[0]);

            Q[thisDist] -> Insert(cw);

            if (Anomalies[cw]!=NULL)
            {
                #if defined(_LOGGING_)

                if (CT.logMeth>1) LogAppend(LH," (anomalies are resolved)");

                #endif

                while (!Anomalies[cw]->Empty())
                {
                    TArc a = Anomalies[cw]->Delete();
                    TNode x = H.StartNode(a);
                    TNode index = dist[x]+dist[cw]+1;

                    if (Bridges[index]==NULL)
                        Bridges[index] = new staticQueue<TArc>(*Bridges[0]);

                    if (index>=tenacity) Bridges[index] -> Insert(a);
                }

                if (w!=s)
                {
                    delete Anomalies[cw];
                    Anomalies[cw] = NULL;
                }
            }

            petal[cw] = thisPetal^2;
        }
    }

    while (!RightSupport->Empty())
    {
        TNode w = RightSupport->Delete();
        H.Shrink(b,w);

        #if defined(_LOGGING_)

        if (CT.logMeth>1 && w!=b)
        {
            sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(w));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        TNode cw = w^1;

        if (dist[cw]==NoNode)
        {
            dist[cw] = tenacity-dist[w];
            TNode thisDist = dist[cw];

            if (Q[thisDist]==NULL) Q[thisDist] = new staticQueue<TNode>(*Q[0]);

            Q[thisDist] -> Insert(cw);

            while (Anomalies[cw]!=NULL && !Anomalies[cw]->Empty())
            {
                #if defined(_LOGGING_)

                if (CT.logMeth>1) LogAppend(LH," (anomalies are resolved)");

                #endif

                while (!Anomalies[cw]->Empty())
                {
                    TArc a = Anomalies[cw]->Delete();
                    TNode x = H.StartNode(a);
                    TNode index = dist[x]+dist[cw]+1;

                    if (Bridges[index]==NULL)
                        Bridges[index] = new staticQueue<TArc>(*Bridges[0]);

                    if (index>=tenacity) Bridges[index] -> Insert(a);
                }

                if (w!=s)
                {
                    delete Anomalies[cw];
                    Anomalies[cw] = NULL;
                }
            }

            petal[cw] = thisPetal;
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif
}


TFloat layeredShrNetwork::FindPath(TNode t) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (t>=n) NoSuchNode("FindPath",t);

    if (Phase==3) Error(ERR_REJECTED,"FindPath","Inapplicable in phase 1");

    #endif

    Expand(s,t);

    return H.FindBalCap(pred,s,t);
}


void layeredShrNetwork::Expand(TNode x,TNode y) throw()
{
    #if defined(_FAILSAVE_)

    if (dist[y]<dist[x])
    {
        sprintf(CT.logBuffer,"Missing start node: %lu",static_cast<unsigned long>(x));
        InternalError1("Expand");
    }

    #endif

    if (x!=y)
    {
        TNode cy = y^1;

        if (dist[y]<dist[cy])
        {
            TArc a = I->Peek(y);
            TNode u = StartNode(a);

            while (Blocked(u) || H.BalCap(a)==0)
            {
                I -> Read(y);
                a = I->Peek(y);
                u = StartNode(a);
            }

            pred[y] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,
                    "Expand(%lu,%lu) puts pred[%lu] = %lu (prop)",
                    static_cast<unsigned long>(x),
                    static_cast<unsigned long>(y),
                    static_cast<unsigned long>(y),
                    static_cast<unsigned long>(a));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            Expand(x,H.StartNode(a));
        }
        else
        {
            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Expand(%lu,%lu) started...",
                    static_cast<unsigned long>(x),static_cast<unsigned long>(y));
                LogEntry(LOG_METH2,CT.logBuffer);
                OpenFold();
            }

            #endif

            TArc thisPetal = petal[y];
            TNode b = breakPoint[thisPetal];

            if (b!=NoNode)
            {
                Expand(x,b);
                Traverse(b,b,y,thisPetal,leftProp,rightProp);
            }
            else
            {
                b = breakPoint[thisPetal^2];
                Expand(x,b);
                Traverse(b,b,y,thisPetal,rightProp,leftProp);
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                CloseFold();
                sprintf(CT.logBuffer,"...Expand(%lu,%lu) complete",
                    static_cast<unsigned long>(x),static_cast<unsigned long>(y));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
    }
}


void layeredShrNetwork::CoExpand(TNode x,TNode y) throw()
{
    TNode cx = x^1;

    #if defined(_FAILSAVE_)

    TNode cy = y^1;

    if (dist[cx]<dist[cy])
    {
        sprintf(CT.logBuffer,"Missing end node: %lu",static_cast<unsigned long>(y));
        InternalError1("CoExpand");
    }

    #endif

    if (y!=x)
    {
        if (dist[cx]<dist[x])
        {
            TArc a = I->Peek(cx);
            TNode u = StartNode(a);

            while (Blocked(u) || H.BalCap(a)==0)
            {
                I -> Read(cx);
                a = I->Peek(cx);
                u = StartNode(a);
            }

            a = a^2;
            u = EndNode(a);
            pred[u] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,
                    "CoExpand(%lu,%lu) puts pred[%lu] = %lu (co-prop)",
                    static_cast<unsigned long>(x),
                    static_cast<unsigned long>(y),
                    static_cast<unsigned long>(u),
                    static_cast<unsigned long>(a));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            CoExpand(u,y);
        }
        else
        {
            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"CoExpand(%lu,%lu) started...",
                    static_cast<unsigned long>(x),static_cast<unsigned long>(y));
                LogEntry(LOG_METH2,CT.logBuffer);
                OpenFold();
            }

            #endif

            TArc thisPetal = petal[cx]^2;
            TNode b = breakPoint[thisPetal^2];

            if (b!=NoNode)
            {
                TNode cb = b^1;
                Traverse(b,x,cb,thisPetal,rightProp,leftProp);
                CoExpand(cb,y);
            }
            else
            {
                b = breakPoint[thisPetal];
                TNode cb = b^1;
                Traverse(b,x,cb,thisPetal,leftProp,rightProp);
                CoExpand(cb,y);
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                CloseFold();
                sprintf(CT.logBuffer,"...CoExpand(%lu,%lu) complete",
                    static_cast<unsigned long>(x),static_cast<unsigned long>(y));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
    }
}


void layeredShrNetwork::Traverse(TNode b,TNode x,TNode y,TArc thisPetal,
    TArc *thisProp,TArc *otherProp) throw()
{
    TNode u = H.StartNode(thisPetal);
    TNode v = EndNode(thisPetal);
    TNode w = x;
    TArc a = thisProp[x];

    if (x==b) a = firstProp[thisPetal];

    while (!(a==NoArc))
    {
        Expand(w,H.StartNode(a));
        w = EndNode(a);
        pred[w] = a;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"pred[%lu] = %lu (prop)",
                static_cast<unsigned long>(w),static_cast<unsigned long>(a));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        a = thisProp[w];
    }

    Expand(w,u);

    pred[v] = thisPetal;

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"pred[%lu] = %lu (petal)",
            static_cast<unsigned long>(v),static_cast<unsigned long>(thisPetal));
        LogEntry(LOG_METH2,CT.logBuffer);
    }

    #endif

    w = y;

    if (y==(b^1)) a = firstProp[thisPetal^2]; else a = otherProp[y^1];

    while (!(a==NoArc))
    {
        a = a^2;
        TNode z = EndNode(a);
        CoExpand(z,w);
        w = H.StartNode(a);
        pred[z] = a;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"pred[%lu] = %lu (co-prop)",
                static_cast<unsigned long>(z),static_cast<unsigned long>(a));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        a = otherProp[w^1];
    }

    CoExpand(v,w);
}


TFloat layeredShrNetwork::Augment(TArc thisPetal) throw()
{
    TNode t = s^1;
    petal[t] = thisPetal;
    TFloat Lambda = FindPath(t);

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        LogEntry(LOG_METH2,"Topological erasure...");
        OpenFold();
    }

    #endif

    TArc a = leftProp[s];

    while (!(a==NoArc))
    {
        if (H.BalCap(a)==Lambda && !Blocked(StartNode(a))) TopErasure(a);
        a = leftProp[EndNode(a)];    
    }

    a = rightProp[s];

    while (!(a==NoArc))
    {
        if (H.BalCap(a)==Lambda && !Blocked(StartNode(a)) && leftProp[StartNode(a)]!=a) TopErasure(a);
        if (H.BalCap(a)==2*Lambda && !Blocked(StartNode(a)) && leftProp[StartNode(a)]==a) TopErasure(a);
        a = rightProp[EndNode(a)];    
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) CloseFold();

    #endif

    TNode w = t;
    LogEntry(LOG_METH,"Augmenting...");

    while (w!=s)
    {
        a = pred[w];
        H.BalPush(a,Lambda);
        pred[w] = NoArc;
        w = H.StartNode(a);
    }

    while (!LeftSupport->Empty()) leftProp[LeftSupport->Delete()] = NoArc;

    while (!RightSupport->Empty()) rightProp[RightSupport->Delete()] = NoArc;

    return Lambda;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, February 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   shrinkingNetwork.cpp
/// \brief  #layeredShrNetwork class implementation

#include "shrinkingNetwork.h"


layeredShrNetwork::layeredShrNetwork(
                        abstractBalancedFNW& _H,TNode _s,
                        staticQueue<TNode>** _Q,
                        staticQueue<TArc>** _Anomalies,
                        staticQueue<TArc>** _Bridges) throw() :
    managedObject(_H.Context()),
    layeredAuxNetwork(_H,_s),
    H(_H), dist(_H.GetNodeColours()),
    Q(_Q), Anomalies(_Anomalies), Bridges( _Bridges)
{
    LeftSupport = new dynamicStack<TNode>(n,CT);
    RightSupport = new dynamicStack<TNode>(n,CT);

    leftProp = new TArc[n];
    rightProp = new TArc[n];
    petal = new TArc[n];
    firstProp = new TArc[2*m];
    breakPoint = new TNode[2*m];

    for (TNode i=0;i<n;i++) leftProp[i] = rightProp[i] = petal[i] = NoArc;

    LI = new iLayeredAuxNetwork(*this);
    RI = new iLayeredAuxNetwork(*this);

    if (!dist) dist = H.InitNodeColours(NoNode);

    align = "  ";

    LogEntry(LOG_MEM,"Layered shrinking network instanciated...");
}


void layeredShrNetwork::Init() throw()
{
    #if defined(_FAILSAVE_)

    if (Phase!=1) Error(ERR_REJECTED,"Init","Inapplicable in phase 2");

    #endif

    layeredAuxNetwork::Init();

    for (TNode i=0;i<n;i++) leftProp[i] = rightProp[i] = NoArc;
}


layeredShrNetwork::~layeredShrNetwork() throw()
{
    delete[] leftProp;
    delete[] rightProp;
    delete[] firstProp;
    delete[] petal;
    delete[] breakPoint;
    delete LeftSupport;
    delete RightSupport;
    delete LI;
    delete RI;

    LogEntry(LOG_MEM,"...Layered shrinking network disallocated");
}


unsigned long layeredShrNetwork::Size() const throw()
{
    return
         sizeof(layeredShrNetwork)
        + managedObject::Allocated()
        + abstractMixedGraph::Allocated()
        + abstractDiGraph::Allocated()
        + layeredAuxNetwork::Allocated()
        +layeredShrNetwork::Allocated();
}


unsigned long layeredShrNetwork::Allocated() const throw()
{
    unsigned long tmpSize
        = 3*n*sizeof(TArc)      // leftProp[],rightProp[],petal[]
        + 4*m*sizeof(TArc*);    // firstProp[],breakPoint[]

    return tmpSize;
}


TNode layeredShrNetwork::StartNode(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("StartNode",a);

    #endif

    return H.Base(H.StartNode(a));
}


TCap layeredShrNetwork::UCap(TArc a) const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*m) NoSuchArc("UCap",a);

    #endif

    return (TCap)H.BalCap(a);
}


TNode layeredShrNetwork::DDFS(TArc thisPetal) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (thisPetal>=2*m) NoSuchArc("DDFS",thisPetal);

    #endif

    TNode left,right,root,barrier;
    left = root = StartNode(thisPetal);
    right = barrier = StartNode(thisPetal^2);

    if (left==right && dist[left^1]!=NoNode) return left;

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"Starting DDFS(%lu), rooted at nodes %lu and %lu",
            static_cast<unsigned long>(thisPetal),
            static_cast<unsigned long>(left),
            static_cast<unsigned long>(right));
        LogEntry(LOG_METH2,CT.logBuffer);
        OpenFold();
    }

    #endif

    LeftSupport -> Insert(left);
    RightSupport -> Insert(right);

    LI -> Reset(left);
    RI -> Reset(right);
    TNode b = NoNode;

    TNode next = NoNode;

    while (b==NoNode)
    {
        if (dist[left]>=dist[right])
        {
            if (LI->Active(left))
            {
                TArc a = LI->Read(left);
                next = StartNode(a);

                if (H.BalCap(a)>0 && leftProp[next]==NoArc && !Blocked(next)
                             && (rightProp[next]!=a || H.BalCap(a)>1))
                {
                    leftProp[next] = a;
                    LeftSupport->Insert(next);
                    LI -> Reset(next);
                    left = next;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Node %lu explored by the left DFS",
                            static_cast<unsigned long>(left));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
            }
            else
            {
                // Backtracking

                if (left!=root && left!=s)
                {
                    left = EndNode(leftProp[left]);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Left backtracking to node %lu",
                            static_cast<unsigned long>(left));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
                else
                {
                    if (left==s) b = s;
                    else
                    {
                        rightProp[right] = NoArc;
                        RightSupport -> Delete();
                        b = barrier;
                    }

                    firstProp[thisPetal] = leftProp[b];
                    firstProp[thisPetal^2] = rightProp[b];
                    breakPoint[thisPetal] = b;
                    breakPoint[thisPetal^2] = NoNode;
                }
            }
        }
        else
        {
            if (RI->Active(right))
            {
                TArc a = RI->Read(right);
                next = StartNode(a);

                if (H.BalCap(a)>0 && rightProp[next]==NoArc && !Blocked(next)
                             && (leftProp[next]!=a || H.BalCap(a)>1))
                {
                    rightProp[next] = a;
                    RightSupport->Insert(next);
                    RI -> Reset(next);
                    right = next;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Node %lu explored by the right DFS",
                            static_cast<unsigned long>(right));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
            }
            else
            {
                // Backtracking

                if (right!=barrier)
                {
                    right = EndNode(rightProp[right]);

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Right backtracking to node %lu",
                            static_cast<unsigned long>(right));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
                else
                {
                    right = left;
                    barrier = EndNode(leftProp[left]);
                    rightProp[right] = leftProp[right];
                    RightSupport->Insert(next);
                    leftProp[left] = NoArc;
                    LeftSupport -> Delete();
                    left = barrier;

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        sprintf(CT.logBuffer,
                            "Node %lu moved to the right DFS",
                            static_cast<unsigned long>(right));
                        LogEntry(LOG_METH2,CT.logBuffer);
                        sprintf(CT.logBuffer,
                            "Left backtracking to node %lu",
                            static_cast<unsigned long>(left));
                        LogEntry(LOG_METH2,CT.logBuffer);
                    }

                    #endif
                }
            }
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) CloseFold();

    #endif

    return b;
}


void layeredShrNetwork::ShrinkBlossom (TNode b,TArc thisPetal,TNode tenacity)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (b>=n) NoSuchNode("ShrinkBlossom",b);

    if (thisPetal>=2*m) NoSuchArc("ShrinkBlossom",thisPetal);

    #endif

    #if defined(_LOGGING_)

    THandle LH = NoHandle;

    if (CT.logMeth>1 && !LeftSupport->Empty())
    {
        sprintf(CT.logBuffer,"Shrinking %lu",static_cast<unsigned long>(b));
        LH = LogStart(LOG_METH2,CT.logBuffer);
    }

    #endif

    leftProp[b] = NoArc;
    rightProp[b] = NoArc;

    while (!LeftSupport->Empty())
    {
        TNode w = LeftSupport->Delete();
        H.Shrink(b,w);

        #if defined(_LOGGING_)

        if (CT.logMeth>1 && w!=b)
        {
            sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(w));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        TNode cw = w^1;

        if (dist[cw]==NoNode)
        {
            dist[cw] = tenacity-dist[w];
            TNode thisDist = dist[cw];

            if (Q[thisDist]==NULL) Q[thisDist] = new staticQueue<TNode>(*Q[0]);

            Q[thisDist] -> Insert(cw);

            if (Anomalies[cw]!=NULL)
            {
                #if defined(_LOGGING_)

                if (CT.logMeth>1) LogAppend(LH," (anomalies are resolved)");

                #endif

                while (!Anomalies[cw]->Empty())
                {
                    TArc a = Anomalies[cw]->Delete();
                    TNode x = H.StartNode(a);
                    TNode index = dist[x]+dist[cw]+1;

                    if (Bridges[index]==NULL)
                        Bridges[index] = new staticQueue<TArc>(*Bridges[0]);

                    if (index>=tenacity) Bridges[index] -> Insert(a);
                }

                if (w!=s)
                {
                    delete Anomalies[cw];
                    Anomalies[cw] = NULL;
                }
            }

            petal[cw] = thisPetal^2;
        }
    }

    while (!RightSupport->Empty())
    {
        TNode w = RightSupport->Delete();
        H.Shrink(b,w);

        #if defined(_LOGGING_)

        if (CT.logMeth>1 && w!=b)
        {
            sprintf(CT.logBuffer,",%lu",static_cast<unsigned long>(w));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        TNode cw = w^1;

        if (dist[cw]==NoNode)
        {
            dist[cw] = tenacity-dist[w];
            TNode thisDist = dist[cw];

            if (Q[thisDist]==NULL) Q[thisDist] = new staticQueue<TNode>(*Q[0]);

            Q[thisDist] -> Insert(cw);

            while (Anomalies[cw]!=NULL && !Anomalies[cw]->Empty())
            {
                #if defined(_LOGGING_)

                if (CT.logMeth>1) LogAppend(LH," (anomalies are resolved)");

                #endif

                while (!Anomalies[cw]->Empty())
                {
                    TArc a = Anomalies[cw]->Delete();
                    TNode x = H.StartNode(a);
                    TNode index = dist[x]+dist[cw]+1;

                    if (Bridges[index]==NULL)
                        Bridges[index] = new staticQueue<TArc>(*Bridges[0]);

                    if (index>=tenacity) Bridges[index] -> Insert(a);
                }

                if (w!=s)
                {
                    delete Anomalies[cw];
                    Anomalies[cw] = NULL;
                }
            }

            petal[cw] = thisPetal;
        }
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif
}


TFloat layeredShrNetwork::FindPath(TNode t) throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (t>=n) NoSuchNode("FindPath",t);

    if (Phase==3) Error(ERR_REJECTED,"FindPath","Inapplicable in phase 1");

    #endif

    Expand(s,t);

    return H.FindBalCap(pred,s,t);
}


void layeredShrNetwork::Expand(TNode x,TNode y) throw()
{
    #if defined(_FAILSAVE_)

    if (dist[y]<dist[x])
    {
        sprintf(CT.logBuffer,"Missing start node: %lu",static_cast<unsigned long>(x));
        InternalError1("Expand");
    }

    #endif

    if (x!=y)
    {
        TNode cy = y^1;

        if (dist[y]<dist[cy])
        {
            TArc a = I->Peek(y);
            TNode u = StartNode(a);

            while (Blocked(u) || H.BalCap(a)==0)
            {
                I -> Read(y);
                a = I->Peek(y);
                u = StartNode(a);
            }

            pred[y] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,
                    "Expand(%lu,%lu) puts pred[%lu] = %lu (prop)",
                    static_cast<unsigned long>(x),
                    static_cast<unsigned long>(y),
                    static_cast<unsigned long>(y),
                    static_cast<unsigned long>(a));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            Expand(x,H.StartNode(a));
        }
        else
        {
            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Expand(%lu,%lu) started...",
                    static_cast<unsigned long>(x),static_cast<unsigned long>(y));
                LogEntry(LOG_METH2,CT.logBuffer);
                OpenFold();
            }

            #endif

            TArc thisPetal = petal[y];
            TNode b = breakPoint[thisPetal];

            if (b!=NoNode)
            {
                Expand(x,b);
                Traverse(b,b,y,thisPetal,leftProp,rightProp);
            }
            else
            {
                b = breakPoint[thisPetal^2];
                Expand(x,b);
                Traverse(b,b,y,thisPetal,rightProp,leftProp);
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                CloseFold();
                sprintf(CT.logBuffer,"...Expand(%lu,%lu) complete",
                    static_cast<unsigned long>(x),static_cast<unsigned long>(y));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
    }
}


void layeredShrNetwork::CoExpand(TNode x,TNode y) throw()
{
    TNode cx = x^1;

    #if defined(_FAILSAVE_)

    TNode cy = y^1;

    if (dist[cx]<dist[cy])
    {
        sprintf(CT.logBuffer,"Missing end node: %lu",static_cast<unsigned long>(y));
        InternalError1("CoExpand");
    }

    #endif

    if (y!=x)
    {
        if (dist[cx]<dist[x])
        {
            TArc a = I->Peek(cx);
            TNode u = StartNode(a);

            while (Blocked(u) || H.BalCap(a)==0)
            {
                I -> Read(cx);
                a = I->Peek(cx);
                u = StartNode(a);
            }

            a = a^2;
            u = EndNode(a);
            pred[u] = a;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,
                    "CoExpand(%lu,%lu) puts pred[%lu] = %lu (co-prop)",
                    static_cast<unsigned long>(x),
                    static_cast<unsigned long>(y),
                    static_cast<unsigned long>(u),
                    static_cast<unsigned long>(a));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif

            CoExpand(u,y);
        }
        else
        {
            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"CoExpand(%lu,%lu) started...",
                    static_cast<unsigned long>(x),static_cast<unsigned long>(y));
                LogEntry(LOG_METH2,CT.logBuffer);
                OpenFold();
            }

            #endif

            TArc thisPetal = petal[cx]^2;
            TNode b = breakPoint[thisPetal^2];

            if (b!=NoNode)
            {
                TNode cb = b^1;
                Traverse(b,x,cb,thisPetal,rightProp,leftProp);
                CoExpand(cb,y);
            }
            else
            {
                b = breakPoint[thisPetal];
                TNode cb = b^1;
                Traverse(b,x,cb,thisPetal,leftProp,rightProp);
                CoExpand(cb,y);
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                CloseFold();
                sprintf(CT.logBuffer,"...CoExpand(%lu,%lu) complete",
                    static_cast<unsigned long>(x),static_cast<unsigned long>(y));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
    }
}


void layeredShrNetwork::Traverse(TNode b,TNode x,TNode y,TArc thisPetal,
    TArc *thisProp,TArc *otherProp) throw()
{
    TNode u = H.StartNode(thisPetal);
    TNode v = EndNode(thisPetal);
    TNode w = x;
    TArc a = thisProp[x];

    if (x==b) a = firstProp[thisPetal];

    while (!(a==NoArc))
    {
        Expand(w,H.StartNode(a));
        w = EndNode(a);
        pred[w] = a;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"pred[%lu] = %lu (prop)",
                static_cast<unsigned long>(w),static_cast<unsigned long>(a));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        a = thisProp[w];
    }

    Expand(w,u);

    pred[v] = thisPetal;

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        sprintf(CT.logBuffer,"pred[%lu] = %lu (petal)",
            static_cast<unsigned long>(v),static_cast<unsigned long>(thisPetal));
        LogEntry(LOG_METH2,CT.logBuffer);
    }

    #endif

    w = y;

    if (y==(b^1)) a = firstProp[thisPetal^2]; else a = otherProp[y^1];

    while (!(a==NoArc))
    {
        a = a^2;
        TNode z = EndNode(a);
        CoExpand(z,w);
        w = H.StartNode(a);
        pred[z] = a;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"pred[%lu] = %lu (co-prop)",
                static_cast<unsigned long>(z),static_cast<unsigned long>(a));
            LogEntry(LOG_METH2,CT.logBuffer);
        }

        #endif

        a = otherProp[w^1];
    }

    CoExpand(v,w);
}


TFloat layeredShrNetwork::Augment(TArc thisPetal) throw()
{
    TNode t = s^1;
    petal[t] = thisPetal;
    TFloat Lambda = FindPath(t);

    #if defined(_LOGGING_)

    if (CT.logMeth>1)
    {
        LogEntry(LOG_METH2,"Topological erasure...");
        OpenFold();
    }

    #endif

    TArc a = leftProp[s];

    while (!(a==NoArc))
    {
        if (H.BalCap(a)==Lambda && !Blocked(StartNode(a))) TopErasure(a);
        a = leftProp[EndNode(a)];    
    }

    a = rightProp[s];

    while (!(a==NoArc))
    {
        if (H.BalCap(a)==Lambda && !Blocked(StartNode(a)) && leftProp[StartNode(a)]!=a) TopErasure(a);
        if (H.BalCap(a)==2*Lambda && !Blocked(StartNode(a)) && leftProp[StartNode(a)]==a) TopErasure(a);
        a = rightProp[EndNode(a)];    
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) CloseFold();

    #endif

    TNode w = t;
    LogEntry(LOG_METH,"Augmenting...");

    while (w!=s)
    {
        a = pred[w];
        H.BalPush(a,Lambda);
        pred[w] = NoArc;
        w = H.StartNode(a);
    }

    while (!LeftSupport->Empty()) leftProp[LeftSupport->Delete()] = NoArc;

    while (!RightSupport->Empty()) rightProp[RightSupport->Delete()] = NoArc;

    return Lambda;
}
