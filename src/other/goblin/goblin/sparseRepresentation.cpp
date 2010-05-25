
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   sparseRepresentation.cpp
/// \brief  #sparseRepresentation class implementation

#include "sparseRepresentation.h"
#include "binaryHeap.h"


sparseRepresentation::sparseRepresentation(const abstractMixedGraph& _G) throw() :
    managedObject(_G.Context()), graphRepresentation(_G)
{
    SN = new TNode[2*mMax];

    right = new TArc[2*mMax];
    first = new TArc[nMax];
    left = NULL;

    for (TNode v=0;v<nMax;v++) first[v] = NoArc;

    sub = NULL;

    LogEntry(LOG_MEM,"...Sparse graph structure instanciated");
}


void sparseRepresentation::ReadIncidences(goblinImport& F)
    throw(ERParse)
{
    SN = new TNode[2*mMax];

    right = new TArc[2*mMax];
    first = new TArc[nMax];

    for (TArc a=0;a<2*mMax;a++) SN[a] = NoNode;

    char* token = NULL;

    for (TNode v=0;v<nAct;v++)
    {
        token = F.Scan();   /* node v */
        TArc* AL = F.GetTArcTuple(0);
        TArc maxIndex = F.Length();

        for (TNode i=0;i<maxIndex;i++)
        {
            TArc a = AL[i];
            SN[a] = v;
            if (i<maxIndex-1) right[a] = AL[i+1];
        }

        if (maxIndex>0)
        {
            right[AL[maxIndex-1]] = AL[0];
            first[v] = AL[0];
        }
        else first[v] = NoArc;

        delete[] AL;
    }

    token = F.Scan();

    LogEntry(LOG_MEM,"...Incidence lists allocated");
}


void sparseRepresentation::SetCapacity(TNode _n,TArc _m,TNode _l)
    throw(ERRejected)
{
    if (_l==NoNode)
    {
        _l = (_n<lAct) ? lAct : _n;
    }

    #if defined(_FAILSAVE_)

    if (_n<nAct || _m<mAct || _l<lAct || _n>CT.MaxNode() || 2*_m>CT.MaxArc() || _l>CT.MaxNode())
        Error(ERR_REJECTED,"SetCapacity","Dimensions are out of range");

    #endif

    if (_m!=mMax)
    {
        if (sub)
        {
            sub = (TFloat *)GoblinRealloc(sub,_m*sizeof(TFloat));

            for (TArc a=mMax;a<_m;a++)
            {
                sub[a] = representation.DefaultValue(TokReprLCap,0);
            }
        }

        SN = (TNode *)GoblinRealloc(SN,2*_m*sizeof(TNode));
        right = (TArc *)GoblinRealloc(right,2*_m*sizeof(TArc));

        if (left)
        {
            left = (TArc *)GoblinRealloc(left,2*_m*sizeof(TArc));
        }

        mMax = _m;

        representation.ReserveItems(DIM_GRAPH_ARCS,mMax);
        representation.ReserveItems(DIM_ARCS_TWICE,2*mMax);
        G.registers.ReserveItems(DIM_GRAPH_ARCS,mMax);
        G.registers.ReserveItems(DIM_ARCS_TWICE,2*mMax);
        layoutData.ReserveItems(DIM_GRAPH_ARCS,mMax);
        layoutData.ReserveItems(DIM_ARCS_TWICE,2*mMax);
    }

    if (_n!=nMax)
    {
        first = (TArc *)GoblinRealloc(first,_n*sizeof(TArc));

        for (TNode v=nMax;v<_n;v++) first[v] = NoArc;

        nMax = _n;

        representation.ReserveItems(DIM_GRAPH_NODES,nMax);
        G.registers.ReserveItems(DIM_GRAPH_NODES,nMax);
        layoutData.ReserveItems(DIM_GRAPH_NODES,nMax);
    }

    if (_l!=lMax)
    {
        lMax = _l;
        geometry.ReserveItems(DIM_LAYOUT_NODES,lMax);
        layoutData.ReserveItems(DIM_LAYOUT_NODES,lMax);
    }
}


unsigned long sparseRepresentation::Size()
    const throw()
{
    return
          sizeof(sparseRepresentation)
        + managedObject::Allocated()
        + graphRepresentation::Allocated()
        + sparseRepresentation::Allocated();
}


unsigned long sparseRepresentation::Allocated()
    const throw()
{
    unsigned long tmpSize
        = 2*mMax*sizeof(TNode)             // SN[]
        + nMax*sizeof(TArc)                // first[]
        + 2*mMax*sizeof(TArc);             // right[]

    if (left)         tmpSize += 2*mMax*sizeof(TArc);
    if (sub)          tmpSize += mMax*sizeof(TFloat);

    return tmpSize;
}


TArc sparseRepresentation::InsertArc(TNode u,TNode v,TCap _u,TFloat _c,TCap _l)
    throw(ERRange,ERRejected)
{
    G.ReleaseInvestigators();

    #if defined(_FAILSAVE_)

    if (G.IsReferenced())
        Error(ERR_REJECTED,"InsertArc","Object is referenced");

    if (u>=nAct) NoSuchNode("InsertArc",u);

    if (v>=nAct) NoSuchNode("InsertArc",v);

    if (2*mMax>CT.MaxArc()-2)
        Error(ERR_REJECTED,"InsertArc","Number of arcs is out of range");

    #endif

    G.ReleaseEmbedding();

    mAct++;

    if (mMax+1==mAct)
    {
        SN = (TNode*)GoblinRealloc(SN,(2*mAct)*sizeof(TNode));
        right = (TArc*)GoblinRealloc(right,(2*mAct)*sizeof(TArc));

        if (left)
            left = (TArc*)GoblinRealloc(left,2*mAct*sizeof(TArc));

        if (sub)
        {
            sub = (TFloat*)GoblinRealloc(sub,mAct*sizeof(TFloat));
            sub[mMax] = 0;
        }

        Error(MSG_WARN,"InsertArc","Non-Buffered arc insertion");
        mMax++;
    }

    TArc a = mAct-1;
    TArc af = 2*a;

    SetRouting(af,u,v);

    if (sub)
    {
        sub[a] = _l;
        if (_l>0) G.ReleaseDegrees();
    }

    representation.AppendItems(DIM_GRAPH_ARCS,1);
    representation.AppendItems(DIM_ARCS_TWICE,2);
    G.registers.AppendItems(DIM_GRAPH_ARCS,1);
    G.registers.AppendItems(DIM_ARCS_TWICE,2);
    layoutData.AppendItems(DIM_GRAPH_ARCS,1);
    layoutData.AppendItems(DIM_ARCS_TWICE,2);

    SetLength(af,_c);
    SetUCap(af,_u);
    SetLCap(af,_l);

    G.MarkAdjacency(u,v,af);

    return a;
}


TNode sparseRepresentation::InsertNode()
    throw(ERRejected)
{
    G.ReleaseInvestigators();

    #if defined(_FAILSAVE_)

    if (G.IsReferenced())
        Error(ERR_REJECTED,"InsertNode","Object is referenced");

    if (nMax>CT.MaxNode()-1)
        Error(ERR_REJECTED,"InsertNode","Number of nodes is out of range");

    #endif

    nAct++;

    if (nMax+1==nAct)
    {
        first = (TArc *)GoblinRealloc(first,nAct*sizeof(TArc));
        first[nMax] = NoArc;

        Error(MSG_WARN,"InsertNode","Non-Buffered node insertion");
        nMax++;
    }

    lAct++;

    if (lMax+1==lAct)
    {
        Error(MSG_WARN,"InsertNode","Non-Buffered node insertion");
        lMax++;
    }

    representation.AppendItems(DIM_GRAPH_NODES,1);
    geometry.AppendItems(DIM_LAYOUT_NODES,1);
    layoutData.AppendItems(DIM_GRAPH_NODES,1);
    layoutData.AppendItems(DIM_LAYOUT_NODES,1);
    G.registers.AppendItems(DIM_GRAPH_NODES,1);

    if (lAct>nAct) SwapNodes(nAct-1,lAct-1);

    return nAct-1;
}


TNode sparseRepresentation::InsertLayoutPoint()
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (lMax>CT.MaxNode()-1)
        Error(ERR_REJECTED,
            "InsertLayoutPoint","Number of nodes is out of range");

    #endif

    lAct++;

    if (lMax+1==lAct)
    {
        lMax++;

        geometry.ReserveItems(DIM_LAYOUT_NODES,lMax);
        layoutData.ReserveItems(DIM_LAYOUT_NODES,lMax);

        Error(MSG_WARN,"InsertLayoutPoint","Non-Buffered node insertion");
    }

    geometry.AppendItems(DIM_LAYOUT_NODES,1);
    layoutData.AppendItems(DIM_LAYOUT_NODES,1);

    G.ni++;

    return lAct-1;
}


TNode sparseRepresentation::InsertThreadSuccessor(TNode v)
    throw(ERRejected,ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=lAct) NoSuchNode("InsertThreadSuccessor",v);

    #endif

    TNode u = InsertLayoutPoint();

    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!thread)
    {
        thread = layoutData.InitArray<TNode>(G,TokLayoutThread,NoNode);
        LogEntry(LOG_MEM,"...Layout thread points allocated");
    }

    thread[u] = thread[v];
    thread[v] = u;

    return u;
}


void sparseRepresentation::ExplicitParallels() throw()
{
    TArc mOld = mAct;

    for (TArc a=0;a<mOld;a++)
    {
        TNode u = StartNode(2*a);
        TNode v = EndNode(2*a);
        TCap multiplicity = ceil(UCap(2*a));
        TCap thisLower = LCap(2*a);
        TFloat thisSub = Sub(2*a);
        TFloat thisLength = Length(2*a);

        SetUCap(2*a,UCap(2*a)-multiplicity+1);

        for (TCap i=1;i<multiplicity;i++)
        {
            if (thisLower==0)
            {
                InsertArc(u,v,1,thisLength,0);
            }
            else if (thisLower<1)
            {
                InsertArc(u,v,1,thisLength,thisLower);
                thisLower = 0;
            }
            else
            {
                InsertArc(u,v,1,thisLength,1);
                thisLower -= 1;
            }

            if (!sub) continue;

            if (thisSub==0)
            {
                sub[mAct-1] = 0;
            }
            else if (thisSub<1)
            {
                sub[mAct-1] = thisSub;
                thisSub = 0;
            }
            else
            {
                sub[mAct-1] = 1;
                thisSub -= 1;
            }
        }

        if (sub) sub[a] = thisSub;

        SetLCap(2*a,thisLower);
    }

    G.m = mAct;

    if (Dim()>0) Layout_ArcRouting();
}


void sparseRepresentation::SwapArcs(TArc a1,TArc a2)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a1>=2*mAct) NoSuchArc("SwapArcs",a1);

    if (a2>=2*mAct) NoSuchArc("SwapArcs",a2);

    #endif

    if (a1==a2) return;

    TNode u1 = SN[a1];
    TNode u2 = SN[a2];
    TNode v1 = SN[a1^1];
    TNode v2 = SN[a2^1];

    SN[a1] = u2;
    SN[a2] = u1;
    SN[a1^1] = v2;
    SN[a2^1] = v1;

    if (u1!=NoNode)
    {
        if (first[u1]==a1) first[u1] = a2;
        if (first[v1]==(a1^1)) first[v1] = (a2^1);
    }

    if (u2!=NoNode)
    {
        if (first[u2]==a2 && u1!=u2) first[u2] = a1;
        if (first[v2]==(a2^1) && v1!=v2) first[v2] = (a1^1);
    }

    TArc a1l = Left(a1);
    TArc a2l = Left(a2);
    TArc a1r = Right(a1);
    TArc a2r = Right(a2);

    if (a1l!=a2 && a2l!=a1)
    {
        if (a2!=a2r)
        {
            left[a1] = a2l;
            right[a1] = a2r;

            if (a1r!=NoArc)
            {
                left[a1r] = a2;
                right[a1l] = a2;
            }
        }
        else
        {
            left[a1] = a1;
            right[a1] = a1;
        }

        if (a1!=a1r)
        {
            left[a2] = a1l;
            right[a2] = a1r;

            if (a2r!=NoArc)
            {
                left[a2r] = a1;
                right[a2l] = a1;
            }
        }
        else
        {
            left[a2] = a2;
            right[a2] = a2;
        }
    }

    a1 = (a1^1);
    a2 = (a2^1);
    a1l = Left(a1);
    a2l = Left(a2);
    a1r = Right(a1);
    a2r = Right(a2);

    if (a1l!=a2 && a2l!=a1 && a2!=(a1^1))
    {
        if (a2!=a2r)
        {
            left[a1] = a2l;
            right[a1] = a2r;

            if (a1r!=NoArc)
            {
                left[a1r] = a2;
                right[a1l] = a2;
            }
        }
        else
        {
            left[a1] = a1;
            right[a1] = a1;
        }

        if (a1!=a1r)
        {
            left[a2] = a1l;
            right[a2] = a1r;

            if (a2r!=NoArc)
            {
                left[a2r] = a1;
                right[a2l] = a1;
            }
        }
        else
        {
            left[a2] = a2;
            right[a2] = a2;
        }
    }

    if (sub)
    {
        TFloat swap = sub[a1>>1];
        sub[a1>>1] = sub[a2>>1];
        sub[a2>>1] = swap;
    }

    representation.SwapItems(DIM_GRAPH_ARCS,a1>>1,a2>>1);
    representation.SwapItems(DIM_ARCS_TWICE,a1,a2);
    representation.SwapItems(DIM_ARCS_TWICE,a1^1,a2^1);
    G.registers.SwapItems(DIM_GRAPH_ARCS,a1>>1,a2>>1);
    G.registers.SwapItems(DIM_ARCS_TWICE,a1,a2);
    G.registers.SwapItems(DIM_ARCS_TWICE,a1^1,a2^1);
    layoutData.SwapItems(DIM_GRAPH_ARCS,a1>>1,a2>>1);
    layoutData.SwapItems(DIM_ARCS_TWICE,a1,a2);
    layoutData.SwapItems(DIM_ARCS_TWICE,a1^1,a2^1);


    // Adjust the sequence of control points, but only if the
    // implicit orientations of a1 and a2 are different
    if (((a1^a2)&1) == 0) return;

    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);
    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);
    TNode* revThread = NULL;

    if (!align || !thread) return;

    TNode x = align[a1>>1];

    if (x!=NoNode && thread[x]!=NoNode)
    {
        // Revert the order of control points

        revThread = new TNode[lAct];
        TNode y = thread[x];
        revThread[y] = NoNode;

        while (thread[y]!=NoNode)
        {
            revThread[thread[y]] = y;
            y = thread[y];
        }

        thread[x] = y;

        while (y!=NoNode)
        {
            thread[y] = revThread[y];
            y = revThread[y];
        }
    }

    x = align[a2>>1];

    if (x!=NoNode && thread[x]!=NoNode && a1!=(a2^1))
    {
        // Revert the order of control points

        if (!revThread) revThread = new TNode[lAct];
        TNode y = thread[x];
        revThread[y] = NoNode;

        while (thread[y]!=NoNode)
        {
            revThread[thread[y]] = y;
            y = thread[y];
        }

        thread[x] = y;

        while (y!=NoNode)
        {
            thread[y] = revThread[y];
            y = revThread[y];
        }
    }

    if (revThread) delete[] revThread;
}


void sparseRepresentation::SwapNodes(TNode u,TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (u>=lAct) NoSuchNode("SwapNodes",u);

    if (v>=lAct) NoSuchNode("SwapNodes",v);

    #endif

    if (u==v) return;


    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);

    if (align)
    {
        for (TArc a=0;a<mAct;a++)
        {
            if (align[a]==u) align[a] = v;
            else if (align[a]==v) align[a] = u;
        }

        TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

        if (thread)
        {
            for (TNode w=0;w<lAct;w++)
            {
                if (w==u || w==v) continue;

                if (thread[w]==u)
                {
                    thread[w] = v;
                }
                else if (thread[w]==v)
                {
                    thread[w] = u;
                }
            }
        }
    }

    geometry.SwapItems(DIM_LAYOUT_NODES,u,v);
    layoutData.SwapItems(DIM_LAYOUT_NODES,u,v);


    if (u>=nAct || v>=nAct)
    {
        // Adjust the bound box indices
        TNode* pBound = geometry.GetArray<TNode>(TokGeoMinBound);

        if (pBound)
        {
            if (*pBound==u) *pBound = v;
            else if (*pBound==v) *pBound = u;
        }

        pBound = geometry.GetArray<TNode>(TokGeoMaxBound);

        if (pBound)
        {
            if (*pBound==u) *pBound = v;
            else if (*pBound==v) *pBound = u;
        }


        // Mixed case is only allowed for the deletion of graph nodes.
        // Here, nAct-1 and lAct-1 must be swapped
        return;
    }


    TArc  a =  first[u];

    if (a!=NoArc)
    {
        SN[a] = v;

        while (right[a]!=first[u])
        {
            a = right[a];
            SN[a] = v;
        }
    }

    a = first[v];

    if (a!=NoArc)
    {
        SN[a] = u;

        while (right[a]!=first[v])
        {
            a = right[a];
            SN[a] = u;
        }
    }

    TArc swap = first[u];
    first[u] = first[v];
    first[v] = swap;

    representation.SwapItems(DIM_GRAPH_NODES,u,v);
    geometry.SwapItems(DIM_GRAPH_NODES,u,v);
    layoutData.SwapItems(DIM_GRAPH_NODES,u,v);
    G.registers.SwapItems(DIM_GRAPH_NODES,u,v);
}


void sparseRepresentation::CancelArc(TArc a)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("CancelArc",a);

    if (SN[a]==NoNode) CancelledArc("CancelArc",a);

    #endif

    TArc ad = a;

    G.MarkAdjacency(SN[a],SN[a^1],NoArc);

    TArc aExtNew = right[ad^1];
    if (aExtNew!=(ad^1))
    {
        G.MarkExteriorFace(aExtNew);
    }
    else
    {
        G.SetExteriorArc(NoArc);
    }

    if (G.Pred(SN[a^1])==a) G.SetPred(SN[a^1],NoArc);
    if (G.Pred(SN[a])==(a^1)) G.SetPred(SN[a],NoArc);

    if (sub) G.AdjustDegrees(2*(ad>>1)+1,sub[ad]);

    TArc al = Left(ad);
    TArc ar = right[ad];

    if (ad!=ar)
    {
        right[al] = ar;
        left[ar] = al;

        if (first[SN[ad]]==ad) first[SN[ad]] = al;
    }
    else first[SN[ad]] = NoArc;

    SN[ad] = NoNode;
    right[ad] = NoArc;
    left[ad] = NoArc;

    ad = ad^1;

    al = left[ad];
    ar = right[ad];

    if (ad!=ar)
    {
        right[al] = ar;
        left[ar] = al;

        if (first[SN[ad]]==ad) first[SN[ad]] = al;
    }
    else first[SN[ad]] = NoArc;

    SN[ad] = NoNode;
    right[ad] = NoArc;
    left[ad] = NoArc;

    G.SetArcVisibility(ad,false);
}


void sparseRepresentation::CancelNode(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=nAct) NoSuchNode("CancelNode",v);

    #endif

    while (first[v]!=NoArc) CancelArc(first[v]);

    G.SetNodeVisibility(v,false);
}


bool sparseRepresentation::ReleaseEdgeControlPoints(TArc a)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("ReleaseEdgeControlPoints",a);

    #endif

    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);
    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!align) return false;

    // A priority queue to store the dispensable layout nodes.
    // These nodes must be deleted in the order of decreasing indices!
    binaryHeap<TNode,TFloat> Q(lAct,CT);

    TNode w = align[a>>1];

    if (w==NoNode) return false;

    align[a>>1] = NoNode;

    while (thread && w!=NoNode)
    {
        TNode x = thread[w];
        thread[w] = NoNode;

        Q.Insert(w,-w);

        w = x;
    }

    // Eventually delete the collected layout nodes
    while (!Q.Empty()) EraseLayoutNode(Q.Delete());

    G.ni = lAct-nAct;

    return true;
}


bool sparseRepresentation::ReleaseDoubleEdgeControlPoints(TPortMode portMode) throw()
{
    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);
    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!thread) return false;

    bool isModified = false;

    // A priority queue to store the dispensable layout nodes.
    // These nodes must be deleted in the order of decreasing indices!
    binaryHeap<TNode,TFloat> Q(lAct,CT);

    for (TArc a=0;a<mAct;a++)
    {
        TNode v = SN[2*a];
        TNode w = SN[2*a+1];

        TNode x = align[a];

        if (x==NoNode) continue;

        TNode y = v;
        x = thread[x];

        if (x==NoNode) continue;

        TNode u = NoNode;

        while (true)
        {
            bool covered = true;

            for (TDim i=0;i<Dim();i++)
            {
                if (fabs(C(x,i)-C(y,i))>CT.epsilon) covered = false;
            }

            if (covered && (portMode==PORTS_IMPLICIT || y!=v))
            {
                if (   (portMode==PORTS_IMPLICIT && x==w)
                    || (portMode==PORTS_EXPLICIT && thread[x]==NoNode)
                   )
                {
                    if (y==v) break;

                    // Delete y instead of x
                    x = y;
                    y = u;
                }

                TNode z = thread[x];

                if (y!=v) thread[y] = z;
                else thread[align[a]] = z;

                // Delete x
                Q.Insert(x,-x);

                x = z;
                if (x==NoNode) x = w;
                isModified = true;
            }
            else
            {
                if (   (portMode==PORTS_IMPLICIT && x==w)
                    || (portMode==PORTS_EXPLICIT && thread[x]==NoNode) )  break;

                u = y;
                y = x;
                x = thread[x];

                if (x==NoNode) x = w;
            }
        }
    }

    // Eventually delete the collected layout nodes
    while (!Q.Empty()) EraseLayoutNode(Q.Delete());

    G.ni = lAct-nAct;

    return isModified;
}


bool sparseRepresentation::ReleaseCoveredEdgeControlPoints(TPortMode portMode) throw()
{
    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);
    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);
    TDim dimLayout = Dim();

    if (!thread) return false;

    bool isModified = ReleaseDoubleEdgeControlPoints(portMode);

    TFloat bendSpacing = 0.0;
    G.GetLayoutParameter(TokLayoutBendSpacing,bendSpacing);

    // A priority queue to store the dispensable layout nodes.
    // These nodes must be deleted in the order of decreasing indices!
    binaryHeap<TNode,TFloat> Q(lAct,CT);

    for (TArc a=0;a<mAct;a++)
    {
        TNode v = SN[2*a];
        TNode w = SN[2*a+1];

        TNode x = align[a];

        if (x==NoNode) continue;

        TNode y = v;
        x = thread[x];

        if (x==NoNode) continue;

        if (portMode==PORTS_EXPLICIT)
        {
            y = x;
            x = thread[x];

            if (x==NoNode) continue;
        }

        TNode u = NoNode;

        do
        {
            u = y;
            y = x;
            x = thread[x];

            if (x==NoNode)
            {
                if (portMode==PORTS_EXPLICIT)
                {
                    break;
                }
                else
                {
                    x = w;
                }
            }

            TDim matchingCoordinates = 0;
            TDim vCoveredCoordinates = 0;
            TDim wCoveredCoordinates = 0;

            for (TDim i=0;i<Dim();i++)
            {
                TFloat cy = C(y,i);

                if (fabs(C(x,i)-cy)<bendSpacing/2.0 && fabs(C(u,i)-cy)<bendSpacing/2.0)
                {
                    ++matchingCoordinates;
                }

                TFloat vMin,vMax,wMin,wMax;

                Layout_GetNodeRange(v,i,vMin,vMax);
                Layout_GetNodeRange(w,i,wMin,wMax);

                if (cy>=vMin-bendSpacing/2.0 && cy<=vMax+bendSpacing/2.0) ++vCoveredCoordinates;
                if (cy>=wMin-bendSpacing/2.0 && cy<=wMax+bendSpacing/2.0) ++wCoveredCoordinates;
            }

            if (   matchingCoordinates+1>=dimLayout // y is a convex combination of x and u
                || vCoveredCoordinates>=dimLayout   // y is covered by the box representation of v
                || wCoveredCoordinates>=dimLayout   // y is covered by the box representation of w
               )
            {
                // Delete y
                Q.Insert(y,-y);

                if (portMode==PORTS_EXPLICIT)
                {
                    if (u!=v) thread[u] = x;
                    else thread[align[a]] = x;
                }
                else
                {
                    if (u!=v) thread[u] = NoNode;
                    else thread[align[a]] = NoNode;
                }
            }
        }
        while (x!=w);
    }

    // Eventually delete the collected layout nodes
    while (!Q.Empty()) EraseLayoutNode(Q.Delete());

    G.ni = lAct-nAct;

    return isModified;
}


bool sparseRepresentation::ReleaseNodeControlPoints(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=nAct) NoSuchNode("ReleaseNodeControlPoints",v);

    #endif

    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!thread) return false;

    // A priority queue to store the dispensable layout nodes.
    // These nodes must be deleted in the order of decreasing indices!
    binaryHeap<TNode,TFloat> Q(lAct,CT);

    TNode w = thread[v];

    if (w==NoNode) return false;

    thread[v] = NoNode;

    do
    {
        TNode x = thread[w];
        thread[w] = NoNode;

        Q.Insert(w,-w);

        w = x;
    }
    while (w!=NoNode);

    // Eventually delete the collected layout nodes
    while (!Q.Empty()) EraseLayoutNode(Q.Delete());

    G.ni = lAct-nAct;

    return true;
}


void sparseRepresentation::DeleteArc(TArc a)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("DeleteArc",a);

    #endif

    a &= (a^1);
    TArc ad = 2*mAct-2;

    if (SN[a]!=NoNode) CancelArc(a);

    ReleaseEdgeControlPoints(a);
    SwapArcs(a,ad);

    representation.EraseItems(DIM_GRAPH_ARCS,1);
    representation.EraseItems(DIM_ARCS_TWICE,2);
    G.registers.EraseItems(DIM_GRAPH_ARCS,1);
    G.registers.EraseItems(DIM_ARCS_TWICE,2);
    layoutData.EraseItems(DIM_GRAPH_ARCS,1);
    layoutData.EraseItems(DIM_ARCS_TWICE,2);

    mAct--;
    G.ni = lAct-nAct;
    G.m = mAct;
}


void sparseRepresentation::EraseLayoutNode(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=lAct || v<nAct) NoSuchNode("EraseLayoutNode",v);

    #endif

    SwapNodes(v,lAct-1);
    lAct--;
    geometry.EraseItems(DIM_LAYOUT_NODES,1);
    layoutData.EraseItems(DIM_LAYOUT_NODES,1);
}


void sparseRepresentation::DeleteNode(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=lAct) NoSuchNode("DeleteNode",v);

    #endif

    if (v<nAct)
    {
        G.ReleaseAdjacencies();
        G.ReleaseEmbedding();
        G.SetExteriorArc(NoArc);
        G.ReleaseDegrees();
        G.ReleasePartition();

        CancelNode(v);
        ReleaseNodeControlPoints(v);
        SwapNodes(v,nAct-1);
        G.registers.EraseItems(DIM_GRAPH_NODES,1);

        if (lAct>nAct) SwapNodes(nAct-1,lAct-1);

        representation.EraseItems(DIM_GRAPH_NODES,1);
        geometry.EraseItems(DIM_LAYOUT_NODES,1);
        layoutData.EraseItems(DIM_GRAPH_NODES,1);
        layoutData.EraseItems(DIM_LAYOUT_NODES,1);

        nAct--;
        lAct--;

        DeleteArcs();
    }
    else
    {
        TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);
        TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

        if (align)
        {
            TArc i = 0;

            for (;i<mAct && align[i]!=v;++i) {};

            if (i<mAct)
            {
                // v is an arc label anchor point
                // Mark the thread successor v as the first arc control point
                if (!thread) align[i] = NoNode;
                else  align[i] = thread[v];
            }
            else // if (i==mAct)
            {
                // v is not an arc label anchor point

                TNode j = nAct;

                for (;j<lAct && ThreadSuccessor(j)!=v;++j) {};

                if (j<lAct)
                {
                    // v is in a thread of layout point
                    // Relink the predecessor to the successor of v.
                    thread[j] = thread[v];
                }
                else // if (j==lAct)
                {
                    // Reaching this means that v is not related to any other
                    // layout point, and no relinking operations are required
                }
            }
        }

        EraseLayoutNode(v);
    }

    G.n = nAct;
    G.m = mAct;
    G.ni = lAct-nAct;
}


void sparseRepresentation::DeleteArcs()
    throw()
{
    TArc a = 0;

    while (a<2*mAct)
    {
        if (SN[a]==NoNode) DeleteArc(a);
        else a+=2;
    }
}


void sparseRepresentation::DeleteNodes()
    throw()
{
    TNode v = 0;

    while (v<nAct)
    {
        if (first[v]==NoArc) DeleteNode(v);
        else v++;
    }

    G.n = nAct;
    G.m = mAct;
    G.ni = lAct-nAct;
}


void sparseRepresentation::ContractArc(TArc ac)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (ac>=2*mAct) NoSuchArc("ContractArc",ac);

    if (SN[ac]==SN[ac^1])
    {
        sprintf(CT.logBuffer,"Arc is a loop: %lu",static_cast<unsigned long>(ac));
        Error(ERR_REJECTED,"ContractArc",CT.logBuffer);
    }

    #endif

    ReleaseEdgeControlPoints(ac);

    TNode u = SN[ac];
    TNode v = SN[ac^1];
    TArc a = (ac^1);

    while (right[a]!=(ac^1))
    {
        SN[a] = u;
        a = right[a];
    }

    SN[a] = u;

    right[a] = right[ac];

    if (left) left[right[ac]] = a;

    right[ac] = (ac^1);

    if (left) left[ac^1] = ac;

    first[v] = NoArc;

    CancelArc(ac);

    for (TDim i=0;i<Dim();i++) SetC(u,i,(C(u,i)+C(v,i))/2);

    G.SetNodeVisibility(v,false);

    G.n = nAct;
    G.m = mAct;
    G.ni = lAct-nAct;
}


void sparseRepresentation::IdentifyNodes(TNode u,TNode v)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=nAct) NoSuchNode("IdentifyNodes",u);

    if (v>=nAct) NoSuchNode("IdentifyNodes",v);

    #endif

    TArc a1 = first[u];
    TArc a2 = first[v];
    TArc a = a2;

    while (right[a]!=a2)
    {
        SN[a] = u;
        a = right[a];
    }

    SN[a] = u;

    right[a] = right[a1];

    if (left) left[right[a1]] = a;

    right[a1] = a2;

    if (left) left[a2] = a1;

    first[v] = NoArc;

    G.SetNodeVisibility(v,false);
}


void sparseRepresentation::ReorderNodeIndices(const TFloat* keyValue)
    throw()
{
    binaryHeap<TNode,TFloat> Q(nAct,CT);

    TNode* mapForward = new TNode[nAct];
    TNode* mapBackward = new TNode[nAct];

    for (TNode v=0;v<nAct;++v)
    {
        Q.Insert(v,keyValue[v]);
        mapForward[v] = mapBackward[v] = v;
    }

    for (TNode vNew=0;vNew<nAct;++vNew)
    {
        TNode wOld = Q.Delete();
        TNode wNew = mapForward[wOld];

        if (vNew==wNew) continue;

        SwapNodes(vNew,wNew);

        TNode vOld = mapBackward[vNew];

        mapForward[wOld] = vNew;
        mapForward[vOld] = wNew;
        mapBackward[wNew] = vOld;
        mapBackward[vNew] = wOld;
    }

    delete[] mapForward;
    delete[] mapBackward;
}


void sparseRepresentation::ReorderEdgeIndices(const TFloat* keyValue)
    throw()
{
    binaryHeap<TArc,TFloat> Q(mAct,CT);

    TNode* mapForward = new TNode[mAct];
    TNode* mapBackward = new TNode[mAct];

    for (TArc a=0;a<mAct;++a)
    {
        Q.Insert(a,keyValue[a]);
        mapForward[a] = mapBackward[a] = a;
    }

    for (TArc aNew=0;aNew<mAct;++aNew)
    {
        TArc bOld = Q.Delete();
        TArc bNew = mapForward[bOld];

        if (aNew==bNew) continue;

        SwapArcs(2*aNew,2*bNew);

        TArc aOld = mapBackward[aNew];

        mapForward[bOld] = aNew;
        mapForward[aOld] = bNew;
        mapBackward[bNew] = aOld;
        mapBackward[aNew] = bOld;
    }

    delete[] mapForward;
    delete[] mapBackward;
}


TNode sparseRepresentation::StartNode(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct)
    {
        NoSuchArc("StartNode",a);
    }

    #endif

    return SN[a];
}


TNode sparseRepresentation::EndNode(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct)
    {
        NoSuchArc("EndNode",a);
    }

    #endif

    return SN[a^1];
}


TArc sparseRepresentation::First(TNode v)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=nAct)
    {
        NoSuchNode("First",v);
    }

    #endif

    return first[v];
}


void sparseRepresentation::SetFirst(TNode v,TArc a)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=nAct) NoSuchNode("SetFirst",v);

    if (SN[a]!=v) Error(ERR_REJECTED,"SetFirst","Mismatching start node");

    #endif

    first[v] = a;
}


TArc sparseRepresentation::Right(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("Right",a);

    #endif

    return right[a];
}


void sparseRepresentation::SetRight(TArc a1,TArc a2,TArc a3)
    throw(ERRange,ERRejected)
{
    if (a1==a2) return;

    if (a3==NoArc) a3 = a2;

    #if defined(_FAILSAVE_)

    if (a1>=2*mAct) NoSuchArc("SetRight",a1);
    if (a2>=2*mAct) NoSuchArc("SetRight",a2);
    if (a3>=2*mAct) NoSuchArc("SetRight",a3);

    if (SN[a1]!=SN[a2] || SN[a1]!=SN[a3])
        Error(ERR_REJECTED,"SetRight","Mismatching start nodes");

    #endif

    TArc a2l = Left(a2);
    TArc a1r = Right(a1);
    TArc a3r = Right(a3);

    if (a1r==a2) return;

    right[a1]  = a2;
    left[a2]   = a1;
    right[a3]  = a1r;
    left[a1r]  = a3;
    right[a2l] = a3r;
    left[a3r]  = a2l;
}


void sparseRepresentation::RouteArc(TArc a,TNode u,TNode v)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("RouteArc",a);

    if (u>=nAct) NoSuchNode("RouteArc",u);

    if (v>=nAct) NoSuchNode("RouteArc",v);

    #endif

    TArc af = a;
    TArc ar = a^1;

    if (SN[af]==u && SN[ar]==v) return;

    if (SN[af]!=NoNode || SN[ar]!=NoNode)
    {
        G.ReleaseEmbedding();
        G.ReleaseInvestigators();
        CancelArc(af);
    }

    SetRouting(af,u,v);

    G.MarkAdjacency(u,v,af);
}


void sparseRepresentation::SetRouting(TArc af,TNode u,TNode v)
    throw()
{
    TArc ar = af^1;

    SN[af] = u;
    SN[ar] = v;

    if (first[u]==NoArc)
    {
        first[u] = af;
        right[af] = af;

        if (left) left[af] = af;
    }
    else
    {
        if (left)
        {
            left[right[first[u]]] = af;
            left[af] = first[u];
        }

        right[af] = right[first[u]];
        right[first[u]] = af;
        first[u] = af;
    }

    if (first[v]==NoArc)
    {
        first[v] = ar;
        right[ar] = ar;

        if (left) left[ar] = ar;
    }
    else
    {
        if (left)
        {
            left[right[first[v]]] = ar;
            left[ar] = first[v];
        }

        right[ar] = right[first[v]];
        right[first[v]] = ar;
        first[v] = ar;
    }
}


TArc sparseRepresentation::Left(TArc ac)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (ac>=2*mAct) NoSuchArc("Left",ac);

    #endif

    if (!left)
    {
        left = new TArc[2*mMax];
        LogEntry(LOG_MEM,"Generating reverse incidence lists...");

        for (TArc a=0;a<2*mAct;a++) left[a] = NoArc;

        for (TArc a=0;a<2*mAct;a++)
        {
            if (right[a]!=NoArc)
            {
                if (left[right[a]]==NoArc) left[right[a]] = a;
                else
                {
                    InternalError("Left","Inconsistent incidence lists");
                }
            }
        }
    }

    return left[ac];
}


void sparseRepresentation::ReorderIncidences(const TArc* next,bool nodeOriented)
    throw()
{
    for (TArc a=0;a<2*mAct;a++)
    {
        TArc a2 = next[a];

        if (!nodeOriented) a2 ^= 1;

        right[a] = a2;

        if (left) left[a2] = a;
    }
}


TNode sparseRepresentation::ArcLabelAnchor(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("ArcLabelAnchor",a);

    #endif

    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);

    if (!align) return NoNode;

    return align[a>>1];
}


bool sparseRepresentation::NoArcLabelAnchors()
    const throw()
{
    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);

    if (!align) return true;

    for (TArc a=0;a<mAct;a++)
    {
        if (align[a]!=NoNode) return false;
    }

    // layoutData.ReleaseAttribute(TokLayoutArcLabel);

    return true;
}


TNode sparseRepresentation::ThreadSuccessor(TNode x)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (x>=lAct) NoSuchNode("ThreadSuccessor",x);

    #endif

    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!thread) return NoNode;

    return thread[x];
}


bool sparseRepresentation::NoThreadSuccessors()
    const throw()
{
    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!thread) return true;

    for (TNode v=0;v<lAct;v++)
    {
        if (thread[v]!=NoNode) return false;
    }

    // layoutData.ReleaseAttribute(TokLayoutThread);

    return true;
}


void sparseRepresentation::ReleaseReverseIncidences()
    throw()
{
    if (left)
    {
        delete[] left;
        left = NULL;
        LogEntry(LOG_MEM,"...Reverse incidences disallocated");
    }
}


void sparseRepresentation::NewSubgraph()
    throw(ERRejected)
{
    if (!sub)
    {
        sub = new TFloat[mMax];
        LogEntry(LOG_MEM,"...Subgraph allocated");

        for (TArc a=0;a<mAct;a++)
        {
            TCap ll = LCap(2*a);
            sub[a] = ll;

            if (ll>0) G.AdjustDegrees(2*a,ll);
        }
    }
    else Error(ERR_REJECTED,"NewSubgraph","A subgraph is already present");
}


void sparseRepresentation::ReleaseSubgraph()
    throw()
{
    if (sub)
    {
        delete[] sub;
        LogEntry(LOG_MEM,"...Subgraph disallocated");
        sub = NULL;
    }
}


void sparseRepresentation::SetSub(TArc a,TFloat multiplicity)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetSub",a);

    #endif

    TCap thisCap = UCap(a);
    TFloat thisSub = Sub(a);

    #if defined(_FAILSAVE_)

    if (   (thisSub>0 && fabs(multiplicity)<LCap(a))
        || (thisCap<InfCap && fabs(multiplicity)>thisCap)
       )
    {
        AmountOutOfRange("SetSub",multiplicity);
    }

    #endif

    if (!sub) NewSubgraph();

    G.AdjustDegrees(a,multiplicity-sub[a>>1]);
    sub[a>>1] = multiplicity;
}


void sparseRepresentation::SetSubRelative(TArc a,TFloat lambda)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetSubRelative",a);

    #endif

    TCap thisCap = UCap(a);
    TFloat thisSub = Sub(a);

    #if defined(_FAILSAVE_)

    if (   (thisSub>0 && fabs(thisSub+lambda)<LCap(a))
        || (thisCap<InfCap && fabs(thisSub+lambda)>thisCap)
       )
    {
        AmountOutOfRange("SetSubRelative",lambda);
    }

    #endif

    if (!sub) NewSubgraph();

    sub[a>>1] += lambda;
    G.AdjustDegrees(a,lambda);
}


TFloat sparseRepresentation::Sub(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("Sub",a);

    #endif

    if (!sub) return LCap(a);
    else return sub[a>>1];
}


sparseRepresentation::~sparseRepresentation()
    throw()
{
    ReleaseReverseIncidences();
    ReleaseSubgraph();

    delete[] SN;
    delete[] right;
    delete[] first;

    LogEntry(LOG_MEM,"...Sparse graph structure disallocated");
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1999
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   sparseRepresentation.cpp
/// \brief  #sparseRepresentation class implementation

#include "sparseRepresentation.h"
#include "binaryHeap.h"


sparseRepresentation::sparseRepresentation(const abstractMixedGraph& _G) throw() :
    managedObject(_G.Context()), graphRepresentation(_G)
{
    SN = new TNode[2*mMax];

    right = new TArc[2*mMax];
    first = new TArc[nMax];
    left = NULL;

    for (TNode v=0;v<nMax;v++) first[v] = NoArc;

    sub = NULL;

    LogEntry(LOG_MEM,"...Sparse graph structure instanciated");
}


void sparseRepresentation::ReadIncidences(goblinImport& F)
    throw(ERParse)
{
    SN = new TNode[2*mMax];

    right = new TArc[2*mMax];
    first = new TArc[nMax];

    for (TArc a=0;a<2*mMax;a++) SN[a] = NoNode;

    char* token = NULL;

    for (TNode v=0;v<nAct;v++)
    {
        token = F.Scan();   /* node v */
        TArc* AL = F.GetTArcTuple(0);
        TArc maxIndex = F.Length();

        for (TNode i=0;i<maxIndex;i++)
        {
            TArc a = AL[i];
            SN[a] = v;
            if (i<maxIndex-1) right[a] = AL[i+1];
        }

        if (maxIndex>0)
        {
            right[AL[maxIndex-1]] = AL[0];
            first[v] = AL[0];
        }
        else first[v] = NoArc;

        delete[] AL;
    }

    token = F.Scan();

    LogEntry(LOG_MEM,"...Incidence lists allocated");
}


void sparseRepresentation::SetCapacity(TNode _n,TArc _m,TNode _l)
    throw(ERRejected)
{
    if (_l==NoNode)
    {
        _l = (_n<lAct) ? lAct : _n;
    }

    #if defined(_FAILSAVE_)

    if (_n<nAct || _m<mAct || _l<lAct || _n>CT.MaxNode() || 2*_m>CT.MaxArc() || _l>CT.MaxNode())
        Error(ERR_REJECTED,"SetCapacity","Dimensions are out of range");

    #endif

    if (_m!=mMax)
    {
        if (sub)
        {
            sub = (TFloat *)GoblinRealloc(sub,_m*sizeof(TFloat));

            for (TArc a=mMax;a<_m;a++)
            {
                sub[a] = representation.DefaultValue(TokReprLCap,0);
            }
        }

        SN = (TNode *)GoblinRealloc(SN,2*_m*sizeof(TNode));
        right = (TArc *)GoblinRealloc(right,2*_m*sizeof(TArc));

        if (left)
        {
            left = (TArc *)GoblinRealloc(left,2*_m*sizeof(TArc));
        }

        mMax = _m;

        representation.ReserveItems(DIM_GRAPH_ARCS,mMax);
        representation.ReserveItems(DIM_ARCS_TWICE,2*mMax);
        G.registers.ReserveItems(DIM_GRAPH_ARCS,mMax);
        G.registers.ReserveItems(DIM_ARCS_TWICE,2*mMax);
        layoutData.ReserveItems(DIM_GRAPH_ARCS,mMax);
        layoutData.ReserveItems(DIM_ARCS_TWICE,2*mMax);
    }

    if (_n!=nMax)
    {
        first = (TArc *)GoblinRealloc(first,_n*sizeof(TArc));

        for (TNode v=nMax;v<_n;v++) first[v] = NoArc;

        nMax = _n;

        representation.ReserveItems(DIM_GRAPH_NODES,nMax);
        G.registers.ReserveItems(DIM_GRAPH_NODES,nMax);
        layoutData.ReserveItems(DIM_GRAPH_NODES,nMax);
    }

    if (_l!=lMax)
    {
        lMax = _l;
        geometry.ReserveItems(DIM_LAYOUT_NODES,lMax);
        layoutData.ReserveItems(DIM_LAYOUT_NODES,lMax);
    }
}


unsigned long sparseRepresentation::Size()
    const throw()
{
    return
          sizeof(sparseRepresentation)
        + managedObject::Allocated()
        + graphRepresentation::Allocated()
        + sparseRepresentation::Allocated();
}


unsigned long sparseRepresentation::Allocated()
    const throw()
{
    unsigned long tmpSize
        = 2*mMax*sizeof(TNode)             // SN[]
        + nMax*sizeof(TArc)                // first[]
        + 2*mMax*sizeof(TArc);             // right[]

    if (left)         tmpSize += 2*mMax*sizeof(TArc);
    if (sub)          tmpSize += mMax*sizeof(TFloat);

    return tmpSize;
}


TArc sparseRepresentation::InsertArc(TNode u,TNode v,TCap _u,TFloat _c,TCap _l)
    throw(ERRange,ERRejected)
{
    G.ReleaseInvestigators();

    #if defined(_FAILSAVE_)

    if (G.IsReferenced())
        Error(ERR_REJECTED,"InsertArc","Object is referenced");

    if (u>=nAct) NoSuchNode("InsertArc",u);

    if (v>=nAct) NoSuchNode("InsertArc",v);

    if (2*mMax>CT.MaxArc()-2)
        Error(ERR_REJECTED,"InsertArc","Number of arcs is out of range");

    #endif

    G.ReleaseEmbedding();

    mAct++;

    if (mMax+1==mAct)
    {
        SN = (TNode*)GoblinRealloc(SN,(2*mAct)*sizeof(TNode));
        right = (TArc*)GoblinRealloc(right,(2*mAct)*sizeof(TArc));

        if (left)
            left = (TArc*)GoblinRealloc(left,2*mAct*sizeof(TArc));

        if (sub)
        {
            sub = (TFloat*)GoblinRealloc(sub,mAct*sizeof(TFloat));
            sub[mMax] = 0;
        }

        Error(MSG_WARN,"InsertArc","Non-Buffered arc insertion");
        mMax++;
    }

    TArc a = mAct-1;
    TArc af = 2*a;

    SetRouting(af,u,v);

    if (sub)
    {
        sub[a] = _l;
        if (_l>0) G.ReleaseDegrees();
    }

    representation.AppendItems(DIM_GRAPH_ARCS,1);
    representation.AppendItems(DIM_ARCS_TWICE,2);
    G.registers.AppendItems(DIM_GRAPH_ARCS,1);
    G.registers.AppendItems(DIM_ARCS_TWICE,2);
    layoutData.AppendItems(DIM_GRAPH_ARCS,1);
    layoutData.AppendItems(DIM_ARCS_TWICE,2);

    SetLength(af,_c);
    SetUCap(af,_u);
    SetLCap(af,_l);

    G.MarkAdjacency(u,v,af);

    return a;
}


TNode sparseRepresentation::InsertNode()
    throw(ERRejected)
{
    G.ReleaseInvestigators();

    #if defined(_FAILSAVE_)

    if (G.IsReferenced())
        Error(ERR_REJECTED,"InsertNode","Object is referenced");

    if (nMax>CT.MaxNode()-1)
        Error(ERR_REJECTED,"InsertNode","Number of nodes is out of range");

    #endif

    nAct++;

    if (nMax+1==nAct)
    {
        first = (TArc *)GoblinRealloc(first,nAct*sizeof(TArc));
        first[nMax] = NoArc;

        Error(MSG_WARN,"InsertNode","Non-Buffered node insertion");
        nMax++;
    }

    lAct++;

    if (lMax+1==lAct)
    {
        Error(MSG_WARN,"InsertNode","Non-Buffered node insertion");
        lMax++;
    }

    representation.AppendItems(DIM_GRAPH_NODES,1);
    geometry.AppendItems(DIM_LAYOUT_NODES,1);
    layoutData.AppendItems(DIM_GRAPH_NODES,1);
    layoutData.AppendItems(DIM_LAYOUT_NODES,1);
    G.registers.AppendItems(DIM_GRAPH_NODES,1);

    if (lAct>nAct) SwapNodes(nAct-1,lAct-1);

    return nAct-1;
}


TNode sparseRepresentation::InsertLayoutPoint()
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (lMax>CT.MaxNode()-1)
        Error(ERR_REJECTED,
            "InsertLayoutPoint","Number of nodes is out of range");

    #endif

    lAct++;

    if (lMax+1==lAct)
    {
        lMax++;

        geometry.ReserveItems(DIM_LAYOUT_NODES,lMax);
        layoutData.ReserveItems(DIM_LAYOUT_NODES,lMax);

        Error(MSG_WARN,"InsertLayoutPoint","Non-Buffered node insertion");
    }

    geometry.AppendItems(DIM_LAYOUT_NODES,1);
    layoutData.AppendItems(DIM_LAYOUT_NODES,1);

    G.ni++;

    return lAct-1;
}


TNode sparseRepresentation::InsertThreadSuccessor(TNode v)
    throw(ERRejected,ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=lAct) NoSuchNode("InsertThreadSuccessor",v);

    #endif

    TNode u = InsertLayoutPoint();

    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!thread)
    {
        thread = layoutData.InitArray<TNode>(G,TokLayoutThread,NoNode);
        LogEntry(LOG_MEM,"...Layout thread points allocated");
    }

    thread[u] = thread[v];
    thread[v] = u;

    return u;
}


void sparseRepresentation::ExplicitParallels() throw()
{
    TArc mOld = mAct;

    for (TArc a=0;a<mOld;a++)
    {
        TNode u = StartNode(2*a);
        TNode v = EndNode(2*a);
        TCap multiplicity = ceil(UCap(2*a));
        TCap thisLower = LCap(2*a);
        TFloat thisSub = Sub(2*a);
        TFloat thisLength = Length(2*a);

        SetUCap(2*a,UCap(2*a)-multiplicity+1);

        for (TCap i=1;i<multiplicity;i++)
        {
            if (thisLower==0)
            {
                InsertArc(u,v,1,thisLength,0);
            }
            else if (thisLower<1)
            {
                InsertArc(u,v,1,thisLength,thisLower);
                thisLower = 0;
            }
            else
            {
                InsertArc(u,v,1,thisLength,1);
                thisLower -= 1;
            }

            if (!sub) continue;

            if (thisSub==0)
            {
                sub[mAct-1] = 0;
            }
            else if (thisSub<1)
            {
                sub[mAct-1] = thisSub;
                thisSub = 0;
            }
            else
            {
                sub[mAct-1] = 1;
                thisSub -= 1;
            }
        }

        if (sub) sub[a] = thisSub;

        SetLCap(2*a,thisLower);
    }

    G.m = mAct;

    if (Dim()>0) Layout_ArcRouting();
}


void sparseRepresentation::SwapArcs(TArc a1,TArc a2)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a1>=2*mAct) NoSuchArc("SwapArcs",a1);

    if (a2>=2*mAct) NoSuchArc("SwapArcs",a2);

    #endif

    if (a1==a2) return;

    TNode u1 = SN[a1];
    TNode u2 = SN[a2];
    TNode v1 = SN[a1^1];
    TNode v2 = SN[a2^1];

    SN[a1] = u2;
    SN[a2] = u1;
    SN[a1^1] = v2;
    SN[a2^1] = v1;

    if (u1!=NoNode)
    {
        if (first[u1]==a1) first[u1] = a2;
        if (first[v1]==(a1^1)) first[v1] = (a2^1);
    }

    if (u2!=NoNode)
    {
        if (first[u2]==a2 && u1!=u2) first[u2] = a1;
        if (first[v2]==(a2^1) && v1!=v2) first[v2] = (a1^1);
    }

    TArc a1l = Left(a1);
    TArc a2l = Left(a2);
    TArc a1r = Right(a1);
    TArc a2r = Right(a2);

    if (a1l!=a2 && a2l!=a1)
    {
        if (a2!=a2r)
        {
            left[a1] = a2l;
            right[a1] = a2r;

            if (a1r!=NoArc)
            {
                left[a1r] = a2;
                right[a1l] = a2;
            }
        }
        else
        {
            left[a1] = a1;
            right[a1] = a1;
        }

        if (a1!=a1r)
        {
            left[a2] = a1l;
            right[a2] = a1r;

            if (a2r!=NoArc)
            {
                left[a2r] = a1;
                right[a2l] = a1;
            }
        }
        else
        {
            left[a2] = a2;
            right[a2] = a2;
        }
    }

    a1 = (a1^1);
    a2 = (a2^1);
    a1l = Left(a1);
    a2l = Left(a2);
    a1r = Right(a1);
    a2r = Right(a2);

    if (a1l!=a2 && a2l!=a1 && a2!=(a1^1))
    {
        if (a2!=a2r)
        {
            left[a1] = a2l;
            right[a1] = a2r;

            if (a1r!=NoArc)
            {
                left[a1r] = a2;
                right[a1l] = a2;
            }
        }
        else
        {
            left[a1] = a1;
            right[a1] = a1;
        }

        if (a1!=a1r)
        {
            left[a2] = a1l;
            right[a2] = a1r;

            if (a2r!=NoArc)
            {
                left[a2r] = a1;
                right[a2l] = a1;
            }
        }
        else
        {
            left[a2] = a2;
            right[a2] = a2;
        }
    }

    if (sub)
    {
        TFloat swap = sub[a1>>1];
        sub[a1>>1] = sub[a2>>1];
        sub[a2>>1] = swap;
    }

    representation.SwapItems(DIM_GRAPH_ARCS,a1>>1,a2>>1);
    representation.SwapItems(DIM_ARCS_TWICE,a1,a2);
    representation.SwapItems(DIM_ARCS_TWICE,a1^1,a2^1);
    G.registers.SwapItems(DIM_GRAPH_ARCS,a1>>1,a2>>1);
    G.registers.SwapItems(DIM_ARCS_TWICE,a1,a2);
    G.registers.SwapItems(DIM_ARCS_TWICE,a1^1,a2^1);
    layoutData.SwapItems(DIM_GRAPH_ARCS,a1>>1,a2>>1);
    layoutData.SwapItems(DIM_ARCS_TWICE,a1,a2);
    layoutData.SwapItems(DIM_ARCS_TWICE,a1^1,a2^1);


    // Adjust the sequence of control points, but only if the
    // implicit orientations of a1 and a2 are different
    if (((a1^a2)&1) == 0) return;

    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);
    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);
    TNode* revThread = NULL;

    if (!align || !thread) return;

    TNode x = align[a1>>1];

    if (x!=NoNode && thread[x]!=NoNode)
    {
        // Revert the order of control points

        revThread = new TNode[lAct];
        TNode y = thread[x];
        revThread[y] = NoNode;

        while (thread[y]!=NoNode)
        {
            revThread[thread[y]] = y;
            y = thread[y];
        }

        thread[x] = y;

        while (y!=NoNode)
        {
            thread[y] = revThread[y];
            y = revThread[y];
        }
    }

    x = align[a2>>1];

    if (x!=NoNode && thread[x]!=NoNode && a1!=(a2^1))
    {
        // Revert the order of control points

        if (!revThread) revThread = new TNode[lAct];
        TNode y = thread[x];
        revThread[y] = NoNode;

        while (thread[y]!=NoNode)
        {
            revThread[thread[y]] = y;
            y = thread[y];
        }

        thread[x] = y;

        while (y!=NoNode)
        {
            thread[y] = revThread[y];
            y = revThread[y];
        }
    }

    if (revThread) delete[] revThread;
}


void sparseRepresentation::SwapNodes(TNode u,TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (u>=lAct) NoSuchNode("SwapNodes",u);

    if (v>=lAct) NoSuchNode("SwapNodes",v);

    #endif

    if (u==v) return;


    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);

    if (align)
    {
        for (TArc a=0;a<mAct;a++)
        {
            if (align[a]==u) align[a] = v;
            else if (align[a]==v) align[a] = u;
        }

        TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

        if (thread)
        {
            for (TNode w=0;w<lAct;w++)
            {
                if (w==u || w==v) continue;

                if (thread[w]==u)
                {
                    thread[w] = v;
                }
                else if (thread[w]==v)
                {
                    thread[w] = u;
                }
            }
        }
    }

    geometry.SwapItems(DIM_LAYOUT_NODES,u,v);
    layoutData.SwapItems(DIM_LAYOUT_NODES,u,v);


    if (u>=nAct || v>=nAct)
    {
        // Adjust the bound box indices
        TNode* pBound = geometry.GetArray<TNode>(TokGeoMinBound);

        if (pBound)
        {
            if (*pBound==u) *pBound = v;
            else if (*pBound==v) *pBound = u;
        }

        pBound = geometry.GetArray<TNode>(TokGeoMaxBound);

        if (pBound)
        {
            if (*pBound==u) *pBound = v;
            else if (*pBound==v) *pBound = u;
        }


        // Mixed case is only allowed for the deletion of graph nodes.
        // Here, nAct-1 and lAct-1 must be swapped
        return;
    }


    TArc  a =  first[u];

    if (a!=NoArc)
    {
        SN[a] = v;

        while (right[a]!=first[u])
        {
            a = right[a];
            SN[a] = v;
        }
    }

    a = first[v];

    if (a!=NoArc)
    {
        SN[a] = u;

        while (right[a]!=first[v])
        {
            a = right[a];
            SN[a] = u;
        }
    }

    TArc swap = first[u];
    first[u] = first[v];
    first[v] = swap;

    representation.SwapItems(DIM_GRAPH_NODES,u,v);
    geometry.SwapItems(DIM_GRAPH_NODES,u,v);
    layoutData.SwapItems(DIM_GRAPH_NODES,u,v);
    G.registers.SwapItems(DIM_GRAPH_NODES,u,v);
}


void sparseRepresentation::CancelArc(TArc a)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("CancelArc",a);

    if (SN[a]==NoNode) CancelledArc("CancelArc",a);

    #endif

    TArc ad = a;

    G.MarkAdjacency(SN[a],SN[a^1],NoArc);

    TArc aExtNew = right[ad^1];
    if (aExtNew!=(ad^1))
    {
        G.MarkExteriorFace(aExtNew);
    }
    else
    {
        G.SetExteriorArc(NoArc);
    }

    if (G.Pred(SN[a^1])==a) G.SetPred(SN[a^1],NoArc);
    if (G.Pred(SN[a])==(a^1)) G.SetPred(SN[a],NoArc);

    if (sub) G.AdjustDegrees(2*(ad>>1)+1,sub[ad]);

    TArc al = Left(ad);
    TArc ar = right[ad];

    if (ad!=ar)
    {
        right[al] = ar;
        left[ar] = al;

        if (first[SN[ad]]==ad) first[SN[ad]] = al;
    }
    else first[SN[ad]] = NoArc;

    SN[ad] = NoNode;
    right[ad] = NoArc;
    left[ad] = NoArc;

    ad = ad^1;

    al = left[ad];
    ar = right[ad];

    if (ad!=ar)
    {
        right[al] = ar;
        left[ar] = al;

        if (first[SN[ad]]==ad) first[SN[ad]] = al;
    }
    else first[SN[ad]] = NoArc;

    SN[ad] = NoNode;
    right[ad] = NoArc;
    left[ad] = NoArc;

    G.SetArcVisibility(ad,false);
}


void sparseRepresentation::CancelNode(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=nAct) NoSuchNode("CancelNode",v);

    #endif

    while (first[v]!=NoArc) CancelArc(first[v]);

    G.SetNodeVisibility(v,false);
}


bool sparseRepresentation::ReleaseEdgeControlPoints(TArc a)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("ReleaseEdgeControlPoints",a);

    #endif

    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);
    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!align) return false;

    // A priority queue to store the dispensable layout nodes.
    // These nodes must be deleted in the order of decreasing indices!
    binaryHeap<TNode,TFloat> Q(lAct,CT);

    TNode w = align[a>>1];

    if (w==NoNode) return false;

    align[a>>1] = NoNode;

    while (thread && w!=NoNode)
    {
        TNode x = thread[w];
        thread[w] = NoNode;

        Q.Insert(w,-w);

        w = x;
    }

    // Eventually delete the collected layout nodes
    while (!Q.Empty()) EraseLayoutNode(Q.Delete());

    G.ni = lAct-nAct;

    return true;
}


bool sparseRepresentation::ReleaseDoubleEdgeControlPoints(TPortMode portMode) throw()
{
    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);
    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!thread) return false;

    bool isModified = false;

    // A priority queue to store the dispensable layout nodes.
    // These nodes must be deleted in the order of decreasing indices!
    binaryHeap<TNode,TFloat> Q(lAct,CT);

    for (TArc a=0;a<mAct;a++)
    {
        TNode v = SN[2*a];
        TNode w = SN[2*a+1];

        TNode x = align[a];

        if (x==NoNode) continue;

        TNode y = v;
        x = thread[x];

        if (x==NoNode) continue;

        TNode u = NoNode;

        while (true)
        {
            bool covered = true;

            for (TDim i=0;i<Dim();i++)
            {
                if (fabs(C(x,i)-C(y,i))>CT.epsilon) covered = false;
            }

            if (covered && (portMode==PORTS_IMPLICIT || y!=v))
            {
                if (   (portMode==PORTS_IMPLICIT && x==w)
                    || (portMode==PORTS_EXPLICIT && thread[x]==NoNode)
                   )
                {
                    if (y==v) break;

                    // Delete y instead of x
                    x = y;
                    y = u;
                }

                TNode z = thread[x];

                if (y!=v) thread[y] = z;
                else thread[align[a]] = z;

                // Delete x
                Q.Insert(x,-x);

                x = z;
                if (x==NoNode) x = w;
                isModified = true;
            }
            else
            {
                if (   (portMode==PORTS_IMPLICIT && x==w)
                    || (portMode==PORTS_EXPLICIT && thread[x]==NoNode) )  break;

                u = y;
                y = x;
                x = thread[x];

                if (x==NoNode) x = w;
            }
        }
    }

    // Eventually delete the collected layout nodes
    while (!Q.Empty()) EraseLayoutNode(Q.Delete());

    G.ni = lAct-nAct;

    return isModified;
}


bool sparseRepresentation::ReleaseCoveredEdgeControlPoints(TPortMode portMode) throw()
{
    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);
    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);
    TDim dimLayout = Dim();

    if (!thread) return false;

    bool isModified = ReleaseDoubleEdgeControlPoints(portMode);

    TFloat bendSpacing = 0.0;
    G.GetLayoutParameter(TokLayoutBendSpacing,bendSpacing);

    // A priority queue to store the dispensable layout nodes.
    // These nodes must be deleted in the order of decreasing indices!
    binaryHeap<TNode,TFloat> Q(lAct,CT);

    for (TArc a=0;a<mAct;a++)
    {
        TNode v = SN[2*a];
        TNode w = SN[2*a+1];

        TNode x = align[a];

        if (x==NoNode) continue;

        TNode y = v;
        x = thread[x];

        if (x==NoNode) continue;

        if (portMode==PORTS_EXPLICIT)
        {
            y = x;
            x = thread[x];

            if (x==NoNode) continue;
        }

        TNode u = NoNode;

        do
        {
            u = y;
            y = x;
            x = thread[x];

            if (x==NoNode)
            {
                if (portMode==PORTS_EXPLICIT)
                {
                    break;
                }
                else
                {
                    x = w;
                }
            }

            TDim matchingCoordinates = 0;
            TDim vCoveredCoordinates = 0;
            TDim wCoveredCoordinates = 0;

            for (TDim i=0;i<Dim();i++)
            {
                TFloat cy = C(y,i);

                if (fabs(C(x,i)-cy)<bendSpacing/2.0 && fabs(C(u,i)-cy)<bendSpacing/2.0)
                {
                    ++matchingCoordinates;
                }

                TFloat vMin,vMax,wMin,wMax;

                Layout_GetNodeRange(v,i,vMin,vMax);
                Layout_GetNodeRange(w,i,wMin,wMax);

                if (cy>=vMin-bendSpacing/2.0 && cy<=vMax+bendSpacing/2.0) ++vCoveredCoordinates;
                if (cy>=wMin-bendSpacing/2.0 && cy<=wMax+bendSpacing/2.0) ++wCoveredCoordinates;
            }

            if (   matchingCoordinates+1>=dimLayout // y is a convex combination of x and u
                || vCoveredCoordinates>=dimLayout   // y is covered by the box representation of v
                || wCoveredCoordinates>=dimLayout   // y is covered by the box representation of w
               )
            {
                // Delete y
                Q.Insert(y,-y);

                if (portMode==PORTS_EXPLICIT)
                {
                    if (u!=v) thread[u] = x;
                    else thread[align[a]] = x;
                }
                else
                {
                    if (u!=v) thread[u] = NoNode;
                    else thread[align[a]] = NoNode;
                }
            }
        }
        while (x!=w);
    }

    // Eventually delete the collected layout nodes
    while (!Q.Empty()) EraseLayoutNode(Q.Delete());

    G.ni = lAct-nAct;

    return isModified;
}


bool sparseRepresentation::ReleaseNodeControlPoints(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=nAct) NoSuchNode("ReleaseNodeControlPoints",v);

    #endif

    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!thread) return false;

    // A priority queue to store the dispensable layout nodes.
    // These nodes must be deleted in the order of decreasing indices!
    binaryHeap<TNode,TFloat> Q(lAct,CT);

    TNode w = thread[v];

    if (w==NoNode) return false;

    thread[v] = NoNode;

    do
    {
        TNode x = thread[w];
        thread[w] = NoNode;

        Q.Insert(w,-w);

        w = x;
    }
    while (w!=NoNode);

    // Eventually delete the collected layout nodes
    while (!Q.Empty()) EraseLayoutNode(Q.Delete());

    G.ni = lAct-nAct;

    return true;
}


void sparseRepresentation::DeleteArc(TArc a)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("DeleteArc",a);

    #endif

    a &= (a^1);
    TArc ad = 2*mAct-2;

    if (SN[a]!=NoNode) CancelArc(a);

    ReleaseEdgeControlPoints(a);
    SwapArcs(a,ad);

    representation.EraseItems(DIM_GRAPH_ARCS,1);
    representation.EraseItems(DIM_ARCS_TWICE,2);
    G.registers.EraseItems(DIM_GRAPH_ARCS,1);
    G.registers.EraseItems(DIM_ARCS_TWICE,2);
    layoutData.EraseItems(DIM_GRAPH_ARCS,1);
    layoutData.EraseItems(DIM_ARCS_TWICE,2);

    mAct--;
    G.ni = lAct-nAct;
    G.m = mAct;
}


void sparseRepresentation::EraseLayoutNode(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=lAct || v<nAct) NoSuchNode("EraseLayoutNode",v);

    #endif

    SwapNodes(v,lAct-1);
    lAct--;
    geometry.EraseItems(DIM_LAYOUT_NODES,1);
    layoutData.EraseItems(DIM_LAYOUT_NODES,1);
}


void sparseRepresentation::DeleteNode(TNode v)
    throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=lAct) NoSuchNode("DeleteNode",v);

    #endif

    if (v<nAct)
    {
        G.ReleaseAdjacencies();
        G.ReleaseEmbedding();
        G.SetExteriorArc(NoArc);
        G.ReleaseDegrees();
        G.ReleasePartition();

        CancelNode(v);
        ReleaseNodeControlPoints(v);
        SwapNodes(v,nAct-1);
        G.registers.EraseItems(DIM_GRAPH_NODES,1);

        if (lAct>nAct) SwapNodes(nAct-1,lAct-1);

        representation.EraseItems(DIM_GRAPH_NODES,1);
        geometry.EraseItems(DIM_LAYOUT_NODES,1);
        layoutData.EraseItems(DIM_GRAPH_NODES,1);
        layoutData.EraseItems(DIM_LAYOUT_NODES,1);

        nAct--;
        lAct--;

        DeleteArcs();
    }
    else
    {
        TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);
        TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

        if (align)
        {
            TArc i = 0;

            for (;i<mAct && align[i]!=v;++i) {};

            if (i<mAct)
            {
                // v is an arc label anchor point
                // Mark the thread successor v as the first arc control point
                if (!thread) align[i] = NoNode;
                else  align[i] = thread[v];
            }
            else // if (i==mAct)
            {
                // v is not an arc label anchor point

                TNode j = nAct;

                for (;j<lAct && ThreadSuccessor(j)!=v;++j) {};

                if (j<lAct)
                {
                    // v is in a thread of layout point
                    // Relink the predecessor to the successor of v.
                    thread[j] = thread[v];
                }
                else // if (j==lAct)
                {
                    // Reaching this means that v is not related to any other
                    // layout point, and no relinking operations are required
                }
            }
        }

        EraseLayoutNode(v);
    }

    G.n = nAct;
    G.m = mAct;
    G.ni = lAct-nAct;
}


void sparseRepresentation::DeleteArcs()
    throw()
{
    TArc a = 0;

    while (a<2*mAct)
    {
        if (SN[a]==NoNode) DeleteArc(a);
        else a+=2;
    }
}


void sparseRepresentation::DeleteNodes()
    throw()
{
    TNode v = 0;

    while (v<nAct)
    {
        if (first[v]==NoArc) DeleteNode(v);
        else v++;
    }

    G.n = nAct;
    G.m = mAct;
    G.ni = lAct-nAct;
}


void sparseRepresentation::ContractArc(TArc ac)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (ac>=2*mAct) NoSuchArc("ContractArc",ac);

    if (SN[ac]==SN[ac^1])
    {
        sprintf(CT.logBuffer,"Arc is a loop: %lu",static_cast<unsigned long>(ac));
        Error(ERR_REJECTED,"ContractArc",CT.logBuffer);
    }

    #endif

    ReleaseEdgeControlPoints(ac);

    TNode u = SN[ac];
    TNode v = SN[ac^1];
    TArc a = (ac^1);

    while (right[a]!=(ac^1))
    {
        SN[a] = u;
        a = right[a];
    }

    SN[a] = u;

    right[a] = right[ac];

    if (left) left[right[ac]] = a;

    right[ac] = (ac^1);

    if (left) left[ac^1] = ac;

    first[v] = NoArc;

    CancelArc(ac);

    for (TDim i=0;i<Dim();i++) SetC(u,i,(C(u,i)+C(v,i))/2);

    G.SetNodeVisibility(v,false);

    G.n = nAct;
    G.m = mAct;
    G.ni = lAct-nAct;
}


void sparseRepresentation::IdentifyNodes(TNode u,TNode v)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (u>=nAct) NoSuchNode("IdentifyNodes",u);

    if (v>=nAct) NoSuchNode("IdentifyNodes",v);

    #endif

    TArc a1 = first[u];
    TArc a2 = first[v];
    TArc a = a2;

    while (right[a]!=a2)
    {
        SN[a] = u;
        a = right[a];
    }

    SN[a] = u;

    right[a] = right[a1];

    if (left) left[right[a1]] = a;

    right[a1] = a2;

    if (left) left[a2] = a1;

    first[v] = NoArc;

    G.SetNodeVisibility(v,false);
}


void sparseRepresentation::ReorderNodeIndices(const TFloat* keyValue)
    throw()
{
    binaryHeap<TNode,TFloat> Q(nAct,CT);

    TNode* mapForward = new TNode[nAct];
    TNode* mapBackward = new TNode[nAct];

    for (TNode v=0;v<nAct;++v)
    {
        Q.Insert(v,keyValue[v]);
        mapForward[v] = mapBackward[v] = v;
    }

    for (TNode vNew=0;vNew<nAct;++vNew)
    {
        TNode wOld = Q.Delete();
        TNode wNew = mapForward[wOld];

        if (vNew==wNew) continue;

        SwapNodes(vNew,wNew);

        TNode vOld = mapBackward[vNew];

        mapForward[wOld] = vNew;
        mapForward[vOld] = wNew;
        mapBackward[wNew] = vOld;
        mapBackward[vNew] = wOld;
    }

    delete[] mapForward;
    delete[] mapBackward;
}


void sparseRepresentation::ReorderEdgeIndices(const TFloat* keyValue)
    throw()
{
    binaryHeap<TArc,TFloat> Q(mAct,CT);

    TNode* mapForward = new TNode[mAct];
    TNode* mapBackward = new TNode[mAct];

    for (TArc a=0;a<mAct;++a)
    {
        Q.Insert(a,keyValue[a]);
        mapForward[a] = mapBackward[a] = a;
    }

    for (TArc aNew=0;aNew<mAct;++aNew)
    {
        TArc bOld = Q.Delete();
        TArc bNew = mapForward[bOld];

        if (aNew==bNew) continue;

        SwapArcs(2*aNew,2*bNew);

        TArc aOld = mapBackward[aNew];

        mapForward[bOld] = aNew;
        mapForward[aOld] = bNew;
        mapBackward[bNew] = aOld;
        mapBackward[aNew] = bOld;
    }

    delete[] mapForward;
    delete[] mapBackward;
}


TNode sparseRepresentation::StartNode(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct)
    {
        NoSuchArc("StartNode",a);
    }

    #endif

    return SN[a];
}


TNode sparseRepresentation::EndNode(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct)
    {
        NoSuchArc("EndNode",a);
    }

    #endif

    return SN[a^1];
}


TArc sparseRepresentation::First(TNode v)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (v>=nAct)
    {
        NoSuchNode("First",v);
    }

    #endif

    return first[v];
}


void sparseRepresentation::SetFirst(TNode v,TArc a)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (v>=nAct) NoSuchNode("SetFirst",v);

    if (SN[a]!=v) Error(ERR_REJECTED,"SetFirst","Mismatching start node");

    #endif

    first[v] = a;
}


TArc sparseRepresentation::Right(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("Right",a);

    #endif

    return right[a];
}


void sparseRepresentation::SetRight(TArc a1,TArc a2,TArc a3)
    throw(ERRange,ERRejected)
{
    if (a1==a2) return;

    if (a3==NoArc) a3 = a2;

    #if defined(_FAILSAVE_)

    if (a1>=2*mAct) NoSuchArc("SetRight",a1);
    if (a2>=2*mAct) NoSuchArc("SetRight",a2);
    if (a3>=2*mAct) NoSuchArc("SetRight",a3);

    if (SN[a1]!=SN[a2] || SN[a1]!=SN[a3])
        Error(ERR_REJECTED,"SetRight","Mismatching start nodes");

    #endif

    TArc a2l = Left(a2);
    TArc a1r = Right(a1);
    TArc a3r = Right(a3);

    if (a1r==a2) return;

    right[a1]  = a2;
    left[a2]   = a1;
    right[a3]  = a1r;
    left[a1r]  = a3;
    right[a2l] = a3r;
    left[a3r]  = a2l;
}


void sparseRepresentation::RouteArc(TArc a,TNode u,TNode v)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("RouteArc",a);

    if (u>=nAct) NoSuchNode("RouteArc",u);

    if (v>=nAct) NoSuchNode("RouteArc",v);

    #endif

    TArc af = a;
    TArc ar = a^1;

    if (SN[af]==u && SN[ar]==v) return;

    if (SN[af]!=NoNode || SN[ar]!=NoNode)
    {
        G.ReleaseEmbedding();
        G.ReleaseInvestigators();
        CancelArc(af);
    }

    SetRouting(af,u,v);

    G.MarkAdjacency(u,v,af);
}


void sparseRepresentation::SetRouting(TArc af,TNode u,TNode v)
    throw()
{
    TArc ar = af^1;

    SN[af] = u;
    SN[ar] = v;

    if (first[u]==NoArc)
    {
        first[u] = af;
        right[af] = af;

        if (left) left[af] = af;
    }
    else
    {
        if (left)
        {
            left[right[first[u]]] = af;
            left[af] = first[u];
        }

        right[af] = right[first[u]];
        right[first[u]] = af;
        first[u] = af;
    }

    if (first[v]==NoArc)
    {
        first[v] = ar;
        right[ar] = ar;

        if (left) left[ar] = ar;
    }
    else
    {
        if (left)
        {
            left[right[first[v]]] = ar;
            left[ar] = first[v];
        }

        right[ar] = right[first[v]];
        right[first[v]] = ar;
        first[v] = ar;
    }
}


TArc sparseRepresentation::Left(TArc ac)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (ac>=2*mAct) NoSuchArc("Left",ac);

    #endif

    if (!left)
    {
        left = new TArc[2*mMax];
        LogEntry(LOG_MEM,"Generating reverse incidence lists...");

        for (TArc a=0;a<2*mAct;a++) left[a] = NoArc;

        for (TArc a=0;a<2*mAct;a++)
        {
            if (right[a]!=NoArc)
            {
                if (left[right[a]]==NoArc) left[right[a]] = a;
                else
                {
                    InternalError("Left","Inconsistent incidence lists");
                }
            }
        }
    }

    return left[ac];
}


void sparseRepresentation::ReorderIncidences(const TArc* next,bool nodeOriented)
    throw()
{
    for (TArc a=0;a<2*mAct;a++)
    {
        TArc a2 = next[a];

        if (!nodeOriented) a2 ^= 1;

        right[a] = a2;

        if (left) left[a2] = a;
    }
}


TNode sparseRepresentation::ArcLabelAnchor(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("ArcLabelAnchor",a);

    #endif

    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);

    if (!align) return NoNode;

    return align[a>>1];
}


bool sparseRepresentation::NoArcLabelAnchors()
    const throw()
{
    TNode* align = layoutData.GetArray<TNode>(TokLayoutArcLabel);

    if (!align) return true;

    for (TArc a=0;a<mAct;a++)
    {
        if (align[a]!=NoNode) return false;
    }

    // layoutData.ReleaseAttribute(TokLayoutArcLabel);

    return true;
}


TNode sparseRepresentation::ThreadSuccessor(TNode x)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (x>=lAct) NoSuchNode("ThreadSuccessor",x);

    #endif

    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!thread) return NoNode;

    return thread[x];
}


bool sparseRepresentation::NoThreadSuccessors()
    const throw()
{
    TNode* thread = layoutData.GetArray<TNode>(TokLayoutThread);

    if (!thread) return true;

    for (TNode v=0;v<lAct;v++)
    {
        if (thread[v]!=NoNode) return false;
    }

    // layoutData.ReleaseAttribute(TokLayoutThread);

    return true;
}


void sparseRepresentation::ReleaseReverseIncidences()
    throw()
{
    if (left)
    {
        delete[] left;
        left = NULL;
        LogEntry(LOG_MEM,"...Reverse incidences disallocated");
    }
}


void sparseRepresentation::NewSubgraph()
    throw(ERRejected)
{
    if (!sub)
    {
        sub = new TFloat[mMax];
        LogEntry(LOG_MEM,"...Subgraph allocated");

        for (TArc a=0;a<mAct;a++)
        {
            TCap ll = LCap(2*a);
            sub[a] = ll;

            if (ll>0) G.AdjustDegrees(2*a,ll);
        }
    }
    else Error(ERR_REJECTED,"NewSubgraph","A subgraph is already present");
}


void sparseRepresentation::ReleaseSubgraph()
    throw()
{
    if (sub)
    {
        delete[] sub;
        LogEntry(LOG_MEM,"...Subgraph disallocated");
        sub = NULL;
    }
}


void sparseRepresentation::SetSub(TArc a,TFloat multiplicity)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetSub",a);

    #endif

    TCap thisCap = UCap(a);
    TFloat thisSub = Sub(a);

    #if defined(_FAILSAVE_)

    if (   (thisSub>0 && fabs(multiplicity)<LCap(a))
        || (thisCap<InfCap && fabs(multiplicity)>thisCap)
       )
    {
        AmountOutOfRange("SetSub",multiplicity);
    }

    #endif

    if (!sub) NewSubgraph();

    G.AdjustDegrees(a,multiplicity-sub[a>>1]);
    sub[a>>1] = multiplicity;
}


void sparseRepresentation::SetSubRelative(TArc a,TFloat lambda)
    throw(ERRange,ERRejected)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("SetSubRelative",a);

    #endif

    TCap thisCap = UCap(a);
    TFloat thisSub = Sub(a);

    #if defined(_FAILSAVE_)

    if (   (thisSub>0 && fabs(thisSub+lambda)<LCap(a))
        || (thisCap<InfCap && fabs(thisSub+lambda)>thisCap)
       )
    {
        AmountOutOfRange("SetSubRelative",lambda);
    }

    #endif

    if (!sub) NewSubgraph();

    sub[a>>1] += lambda;
    G.AdjustDegrees(a,lambda);
}


TFloat sparseRepresentation::Sub(TArc a)
    const throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (a>=2*mAct) NoSuchArc("Sub",a);

    #endif

    if (!sub) return LCap(a);
    else return sub[a>>1];
}


sparseRepresentation::~sparseRepresentation()
    throw()
{
    ReleaseReverseIncidences();
    ReleaseSubgraph();

    delete[] SN;
    delete[] right;
    delete[] first;

    LogEntry(LOG_MEM,"...Sparse graph structure disallocated");
}
