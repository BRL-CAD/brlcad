
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2006
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   layoutLayered.cpp
/// \brief  Implementations of layered drawing methods

#include "mixedGraph.h"
#include "staticStack.h"
#include "staticQueue.h"
#include "lpSolver.h"
#include "moduleGuard.h"


TCap abstractMixedGraph::FeedbackArcSet() throw()
{
    moduleGuard M(ModFeedbackArcSet,*this,"Computing minimum feedback arc set...");

    TArc* edgeColour = InitEdgeColours();
    TCap cancelledArcs = 0;
    TCap* degIn = new TCap[n];
    TCap* degOut = new TCap[n];
    staticStack<TNode> SourceNodes(n,CT);
    staticStack<TNode> TargetNodes(n,CT);

    for (TNode i=0;i<n;i++) degIn[i] = degOut[i] = 0;

    for (TArc a=0;a<m;a++)
    {
        TNode u = StartNode(2*a);
        TNode v = EndNode(2*a);

        if (Orientation(2*a)==1)
        {
            degIn[v] += UCap(2*a);
            degOut[u] += UCap(2*a);
        }
        else if (u==v)
        {
            edgeColour[a] = 1;
        }
    }

    for (TNode i=0;i<n;i++)
    {
        if (degOut[i]==0) TargetNodes.Insert(i);
        else if (degIn[i]==0) SourceNodes.Insert(i);
    }

    for (TNode i=0;i<n;i++)
    {
        TNode vDelete = NoNode;
        bool addEntering = false;

        if (!TargetNodes.Empty())
        {
            vDelete = TargetNodes.Delete();
            addEntering = true;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Target node %lu",
                    static_cast<unsigned long>(vDelete));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
        else if (!SourceNodes.Empty())
        {
            vDelete = SourceNodes.Delete();

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Source node %lu",
                    static_cast<unsigned long>(vDelete));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
        else
        {
            TCap bestDelta = -InfCap;

            for (TNode w=0;w<n;w++)
            {
                if (degIn[w]==0 || degOut[w]==0) continue;

                TCap thisDelta = fabs(degOut[w])-fabs(degIn[w]);

                if (thisDelta>bestDelta)
                {
                    vDelete = w;
                    bestDelta = thisDelta;
                }
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Indefinite node %lu",
                    static_cast<unsigned long>(vDelete));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }

        if (vDelete==NoNode) continue;

        TArc a = First(vDelete);

        if (a==NoArc) continue;

        do
        {
            TNode v = EndNode(a);

            if (edgeColour[a>>1]!=NoArc)
            {
                a = Right(a,vDelete);
                continue;
            }

            if (Orientation(a)==0)
            {
                edgeColour[a>>1] = 3 - (!addEntering ^ (a&1));
                a = Right(a,vDelete);
                continue;
            }

            if (((a&1) && addEntering) || (!(a&1) && !addEntering))
            {
                edgeColour[a>>1] = 0;
            }
            else
            {
                cancelledArcs += UCap(a);
                edgeColour[a>>1] = 1;
            }

            if (a&1)
            {
                degOut[v] -= UCap(a);

                if (degOut[v]==0 && UCap(a)>0 && degIn[v]>0) TargetNodes.Insert(v);
            }
            else
            {
                degIn[v] -= UCap(a);

                if (degIn[v]==0 && UCap(a)>0 && degOut[v]>0) SourceNodes.Insert(v);
            }

            a = Right(a,vDelete);
        }
        while (a!=First(vDelete));

        degIn[vDelete] = degOut[vDelete] = 0;
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...%g capacity units eliminated", cancelledArcs);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    CT.Trace(Handle(),0);

    return cancelledArcs;
}


void abstractMixedGraph::ImplicitOrientationFromDrawing() throw()
{
    moduleGuard M(ModFeedbackArcSet,*this,"Extracting orientation from drawing...");

    TArc* edgeColour = InitEdgeColours();
    TArc cancelledArcs = 0;

    for (TArc a=0;a<m;a++)
    {
        TNode u = StartNode(2*a);
        TNode v = EndNode(2*a);

        if (   C(u,1)<C(v,1)-CT.epsilon
            || (fabs(C(u,1)-C(v,1))<CT.epsilon && u<v)
            )
        {
            edgeColour[a] = 0;
        }
        else
        {
            edgeColour[a] = 1;
            cancelledArcs++;
        }

        if (Orientation(2*a)==0) edgeColour[a] |= 2;
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...%lu arcs eliminated",
            static_cast<unsigned long>(cancelledArcs));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    CT.Trace(Handle(),0);
}


bool abstractMixedGraph::ImplicitSTOrientation(TNode s,TNode t) throw(ERRejected)
{
    if (s<n && t<n)
    {
        sprintf(CT.logBuffer,"Computing (%lu,%lu)-orientation...",
            static_cast<unsigned long>(s),static_cast<unsigned long>(t));
    }
    else
    {
        sprintf(CT.logBuffer,"Computing bipolar orientation...");
    }

    moduleGuard M(ModFeedbackArcSet,*this,CT.logBuffer);

    if (!STNumbering(NoArc,s,t))
    {
        return false;
    }

    TNode* nodeColour = GetNodeColours();
    TArc* edgeColour = InitEdgeColours();
    TArc cancelledArcs = 0;

    for (TArc a=0;a<m;a++)
    {
        TNode u = StartNode(2*a);
        TNode v = EndNode(2*a);

        if (nodeColour[u]<nodeColour[v])
        {
            edgeColour[a] = 0;
        }
        else
        {
            edgeColour[a] = 1;
            cancelledArcs++;
        }

        if (Orientation(2*a)==0) edgeColour[a] |= 2;
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...%lu arcs eliminated",
            static_cast<unsigned long>(cancelledArcs));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    CT.Trace(Handle(),0);

    return true;
}


void abstractMixedGraph::Layout_PredecessorTree(TOptAlign mode,TFloat dx,TFloat dy)
    throw(ERRejected)
{
    TArc* pred = GetPredecessors();
    TNode* nodeColour = GetNodeColours();

    #if defined(_FAILSAVE_)

    if (pred==NULL)
        Error(ERR_REJECTED,"Layout_PredecessorTree","Missing predecessors");

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_PredecessorTree","Coordinates are fixed");

    #endif

    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
    Layout_ConvertModel(LAYOUT_FREESTYLE_POLYGONES);

    SyncSpacingParameters(TokLayoutNodeSpacing,dx);
    SyncSpacingParameters(TokLayoutNodeSpacing,dy);

    THandle H = Investigate();
    investigator &I = Investigator(H);
    staticStack<TNode> S(n,CT);

    TNode* tmpAlign = new TNode[n];
        // For every node, its ordinate
    TNode* leftAlign = new TNode[n];
        // For every node, the ordinate of the leftmost child ot NoNode
    TNode* rightmost = new TNode[n];
        // The ordinate of the rightmost node at a specified depth
    TNode* depth = new TNode[n];
        // For every node, its depth in the search tree
    TNode* traversed = new TNode[n];
        // All nodes listed in the order of first entering them
    TNode* nr = new TNode[n];
        // For every node, the index of occurence in traverse[]

    TNode maxDepth = 0;
    rightmost[0] = 0;
    TNode cNr = 0;

    for (TNode v=0;v<n;v++)
        tmpAlign[v] = leftAlign[v] = rightmost[v] = NoNode;

    for (TNode r=0;r<n;r++)
    {
        if (pred[r]==NoArc) SetNodeVisibility(r,false);

        if (pred[r]==NoArc && (nodeColour==NULL || nodeColour[r]!=NoNode))
        {
            // Start a new tree rooted at r

            depth[r] = 0;
            traversed[cNr] = r;
            cNr++;

            TNode u = r;
            TNode cDepth = 0;

            while (true)
            {
                if (I.Active(u))
                {
                    TArc a = I.Read(u);
                    TNode v = EndNode(a);

                    if (pred[v]==a)
                    {
                        S.Insert(u);
                        cDepth++;
                        depth[v] = cDepth;

                        if (cDepth>maxDepth) maxDepth = cDepth;

                        traversed[cNr] = v;
                        nr[v] = cNr;
                        cNr++;
                        u = v;
                    }

                    continue;
                }

                // Backtracking

                TNode pos = rightmost[depth[u]];

                if (leftAlign[u]==NoNode)
                {
                    // Node u is a leave

                    if (pos==NoNode) tmpAlign[u] = 0;
                    else tmpAlign[u] = pos+2;
                }
                else if (pos!=NoNode && pos+2>tmpAlign[u])
                {
                    // Shift the partial tree rooted at u

                    TNode shift = pos-tmpAlign[u]+2;

                    for (TNode i=nr[u];i<cNr;i++)
                    {
                        TNode x = traversed[i];

                        if (tmpAlign[x]==rightmost[depth[x]])
                            rightmost[depth[x]] += shift;

                        tmpAlign[x] += shift;
                    }
                }

                rightmost[depth[u]] = tmpAlign[u];

                if (S.Empty()) break;

                TNode v = S.Delete();

                if (leftAlign[v] == NoNode)
                {
                    // u is the leftmost child of v

                    leftAlign[v] = tmpAlign[v] = tmpAlign[u];
                }
                else if (mode==ALIGN_CENTER || mode==ALIGN_FDP || mode==ALIGN_OPTIMIZE)
                {
                    // Proceed as if u was the rightmost child of v:
                    // Center v in top of the child nodes embedded so far

                    tmpAlign[v] = (leftAlign[v]+tmpAlign[u])/2;
                }
                else if (mode==ALIGN_RIGHT)
                {
                    // Proceed as if u was the rightmost child of v:
                    // Place v in top of u

                    tmpAlign[v] = tmpAlign[u];
                }

                u = v;
                cDepth --;
            }
        }
    }

    for (TNode i=0;i<cNr;i++)
    {
        TNode u = traversed[i];
        SetC(u,0,tmpAlign[u]*dx);
        SetC(u,1,depth[u]*dy);
    }

    delete[] tmpAlign;
    delete[] leftAlign;
    delete[] rightmost;
    delete[] depth;
    delete[] traversed;
    delete[] nr;

    Close(H);

    Layout_DefaultBoundingBox();

    if (mode==ALIGN_FDP)
    {
        Layout_ForceDirected(FDP_LAYERED_RESTR);
    }
    else if (IsSparse() && Representation())
    {
        sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

        if (mode==ALIGN_OPTIMIZE)
        {
            X -> Layout_SubdivideArcs();
            Layout_SetHorizontalCoordinates(dx);
        }
        else
        {
            X -> Layout_ArcRouting((dx+dy)/4);
        }
    }
}


void abstractMixedGraph::Layout_Layered(int method,TFloat dx,TFloat dy) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || !Representation()) NoSparseRepresentation("Layout_Layered");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    moduleGuard M(ModLayering,*this,"Layered drawing...");

    #if defined(_PROGRESS_)

    M.InitProgressCounter(5,1);

    #endif

    if (method==LAYERED_DEFAULT) method = (LAYERED_VERTICAL|LAYERED_HORIZONTAL);

    if (dx<dy || dx<CT.epsilon)
    {
        SyncSpacingParameters(TokLayoutBendSpacing,dx);
        SyncSpacingParameters(TokLayoutNodeSpacing,dx);
    }

    if (dx>dy || dy<CT.epsilon)
    {
        SyncSpacingParameters(TokLayoutBendSpacing,dy);
        SyncSpacingParameters(TokLayoutNodeSpacing,dy);
    }


    // Vertical methods
    if (method & LAYERED_FEEDBACK)
    {
        if (IsUndirected())
        {
            ImplicitSTOrientation();
        }
        else
        {
            FeedbackArcSet();
        }

        M.Trace(0);
    }

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif


    if (method & (LAYERED_COLOURS|LAYERED_EDGE_SPAN))
    {
        for (TArc a=0;a<m;a++) X -> ReleaseEdgeControlPoints(2*a);
    }

    if (method & LAYERED_COLOURS)
    {
        TNode* nodeColour = GetNodeColours();

        bool completeColouring = true;

        for (TNode v=0;v<n;v++)
        {
            if (!nodeColour || nodeColour[v]>=n)
            {
                completeColouring = false;
                break;
            }
        }

        if (completeColouring)
        {
            Layout_LayeringByColours(dx,dy);
        }
        else
        {
            Layout_EdgeSpanMinimalLayering(dy,nodeColour);
        }
    }
    else if (method & LAYERED_EDGE_SPAN)
    {
        if (!(method & LAYERED_FEEDBACK))
        {
            TArc* edgeColour = GetEdgeColours();
            bool oriented = (edgeColour!=NULL);

            for (TArc a=0;a<m && oriented;a++) oriented = (edgeColour[a]<=3);

            if (!oriented) ImplicitOrientationFromDrawing();
        }

        Layout_EdgeSpanMinimalLayering(dy);
    }

    Layout_DefaultBoundingBox();

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif


    X -> Layout_SubdivideArcs();

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif


    // Horizontal methods
    if (method & LAYERED_SWEEP)
    {
        explicitSubdivision G(*this,OPT_MAPPINGS);
        G.Layout_SweepLayerByLayer(dx,dy);

        TFloat fineSpacing = 0.0;
        GetLayoutParameter(TokLayoutFineSpacing,fineSpacing);

        for (TNode v=0;v<G.N();v++)
        {
            for (TDim i=0;i<G.Dim();i++)
            {
                SetC(G.OriginalOfNode(v),i,G.C(v,i));
            }
        }

        for (TArc a=0;a<m;a++)
        {
            TNode x = ArcLabelAnchor(2*a);
            if (x==NoNode) continue;

            TNode y = ThreadSuccessor(x);
            if (y==NoNode) continue;

            for (TDim i=0;i<G.Dim();i++)
            {
                SetC(x,i,C(y,i) + ((i==0) ? fineSpacing : 0.0));
            }
        }

        if (IsSparse())
        {
            static_cast<sparseRepresentation*>(Representation()) -> Layout_AdoptBoundingBox(G);
        }
//        Layout_SweepLayerByLayer(dx,dy);
    }

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif


    if (method & LAYERED_FDP)
    {
        if (method & LAYERED_SWEEP)
        {
            Layout_ForceDirected(FDP_LAYERED_RESTR,dx);
        }
        else
        {
            Layout_ForceDirected(FDP_LAYERED,dx);
        }
    }
    else if (method & LAYERED_ALIGN)
    {
        Layout_SetHorizontalCoordinates(dx);
    }
}


bool abstractMixedGraph::Layout_EdgeSpanMinimalLayering(TFloat dy,TNode* nodeColour)
    throw()
{
    if (m==0)
    {
        return true;
    }

    moduleGuard M(ModLayering,*this,"Assigning layers for the given orientation...");

    if (nodeColour) LogEntry(LOG_METH,"...restricted by the node colours");

    mipInstance *XLP  =
        static_cast<mipInstance*>(VerticalCoordinatesModel(nodeColour));

    if (!XLP)
    {
        M.Shutdown(LOG_RES,"...No appropriate orientation given");

        return false;
    }

    XLP -> SolveLP();

    for (TNode v=0;v<n;v++) SetC(v,1,dy*XLP->X(TVar(v)));

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Total edge span is %g (%g control points required)",
            XLP -> ObjVal(), XLP -> ObjVal()-m);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    delete XLP;

    CT.Trace(Handle(),0);

    return true;
}


void abstractMixedGraph::Layout_LayeringByColours(TFloat dx,TFloat dy) throw(ERRejected)
{
    TNode* nodeColour = GetNodeColours();

    #if defined(_FAILSAVE_)

    if (!nodeColour)
        Error(ERR_REJECTED,"Layout_LayeringByColours","Missing colours");

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_LayeringByColours","Coordinates are fixed");

    #endif

    moduleGuard M(ModLayering,*this,"Assigning layers from node colours...");

    TNode maxCardinality = 0;

    for (TNode k=0;k<=n;k++)
    {
        TNode cardinality = 0;

        for (TNode v=0;v<n;v++)
            if ((k<n && nodeColour[v]==k) || nodeColour[v]>=n) cardinality++;

        if (cardinality>maxCardinality) maxCardinality = cardinality;
    }

    for (TNode k=0;k<=n;k++)
    {
        TNode cardinality = 0;

        for (TNode v=0;v<n;v++)
            if ((k<n && nodeColour[v]==k) || nodeColour[v]>=n) cardinality++;

        TNode i = 0;

        for (TNode v=0;v<n;v++)
        {
            if ((k<n && nodeColour[v]==k) || nodeColour[v]>=n)
            {
                if (cardinality==1) SetC(v,0,0);
                else SetC(v,0,dy*i*(maxCardinality-1)/(cardinality-1));

                SetC(v,1,dx*k);

                i++;
            }
        }
    }

    CT.Trace(Handle(),0);
}


void abstractMixedGraph::Layout_SetHorizontalCoordinates(TFloat dx) throw()
{
    if (m==0)
    {
        return;
    }

    moduleGuard M(ModLayering,*this,"Assigning horizontal coordinates...");

    explicitSubdivision G(*this,OPT_MAPPINGS);
    graphRepresentation* GR = G.Representation();

    // Set arc length
    for (TArc a=0;a<G.M();a++)
    {
        TNode u = G.OriginalOfNode(G.StartNode(2*a));
        TNode v = G.OriginalOfNode(G.EndNode(2*a));

        if (u<n && v<n)
        {
            GR -> SetLength(2*a,1);
        }
        else if (u>=n && v>=n)
        {
            GR -> SetLength(2*a,4);
        }
        else
        {
            GR -> SetLength(2*a,2);
        }
    }

    mipInstance *XLP  =
        static_cast<mipInstance*>(G.HorizontalCoordinatesModel());

    XLP -> SolveLP();

    TFloat minX = InfFloat;
    TFloat maxX = -InfFloat;

    for (TNode v=0;v<G.N();v++)
    {
        TFloat thisX = XLP->X(TVar(v));

        if (thisX<minX) minX = thisX;
        if (thisX>maxX) maxX = thisX;
    }

    for (TNode v=0;v<G.N();v++)
    {
        SetC(G.OriginalOfNode(v),0,dx*(XLP->X(TVar(v)))-minX);
    }

    TFloat fineSpacing = 0.0;
    GetLayoutParameter(TokLayoutFineSpacing,fineSpacing);

    if (fineSpacing<CT.epsilon || fineSpacing>dx)
    {
        fineSpacing = dx * 0.5;
    }

    for (TArc a=0;a<m;a++)
    {
        TNode x = ArcLabelAnchor(2*a);
        if (x==NoNode) continue;

        TNode y = ThreadSuccessor(x);
        if (y==NoNode) continue;

        SetC(x,0,C(y,0)+fineSpacing);
    }

    static_cast<sparseRepresentation*>(Representation()) ->
        Layout_SetBoundingInterval(0,(minX-1)*dx,(maxX+1)*dx);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Achieved width is %g", maxX-minX);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    delete XLP;

    M.Trace(0);
}


void abstractMixedGraph::Layout_SweepLayerByLayer(TFloat dx,TFloat dy)
    throw(ERRejected)
{
    LogEntry(LOG_METH,"Sweeping layer-by-layer...");
    OpenFold();

    TFloat minY = InfFloat;
    TFloat maxY = -InfFloat;

    for (TNode v=0;v<n;v++)
    {
        TFloat thisY = C(v,1);

        if (minY>thisY) minY = thisY;
        if (maxY<thisY) maxY = thisY;
    }

    if (minY>maxY-dy+CT.epsilon || fabs(maxY-minY)>n*dy)
        Error(ERR_REJECTED,"Layout_SweepLayerByLayer","Inappropriate layers");

    TNode nLayers = TNode(floor((maxY-minY)/dy+0.5)+1);
    staticQueue<TNode>** nodesInLayer = new staticQueue<TNode>*[nLayers];
    nodesInLayer[0] = new staticQueue<TNode>(n,CT);

    for (TNode l=1;l<nLayers;l++)
        nodesInLayer[l] = new staticQueue<TNode>(*nodesInLayer[0]);

    for (TNode v=0;v<n;v++)
    {
        TNode thisLayer = TNode(floor((C(v,1)-minY)/dy+0.5));
        nodesInLayer[thisLayer] -> Insert(v);
    }

    for (TNode l=0;l<nLayers;l++)
    {
        if (nodesInLayer[l]->Empty())
        {
            for (TNode k=1;k<nLayers;k++)
                delete nodesInLayer[k];

            delete nodesInLayer[0];
            delete[] nodesInLayer;

            Error(ERR_REJECTED,"Layout_SweepLayerByLayer","Missing layers");
        }
    }


    bool shuffled = true;
    unsigned iterations = 0;

    while (shuffled && CT.SolverRunning() && iterations<nLayers)
    {
        shuffled = false;
        iterations++;

        for (TNode l=0;l<2*nLayers-2;l++)
        {
            TNode thisLayer = l;
            TNode nextLayer = l+1;

            if (l>nLayers-2)
            {
                thisLayer = 2*nLayers-l-2;
                nextLayer = 2*nLayers-l-3;
            }

            bool shuffledThisTime = true;

            if (iterations==1)
            {
                Layout_CrossingMinimization(
                            *nodesInLayer[thisLayer],*nodesInLayer[nextLayer]);
            }
            else
            {
                shuffledThisTime = Layout_CrossingLocalSearch(
                            *nodesInLayer[thisLayer],*nodesInLayer[nextLayer]);
            }

            if (shuffledThisTime)
            {
                shuffled = true;

                Layout_DefaultBoundingBox();

                CT.Trace(Handle(),0);
            }
        }
    }

    for (TNode l=1;l<nLayers;l++)
        delete nodesInLayer[l];

    delete nodesInLayer[0];
    delete[] nodesInLayer;

    CloseFold();

    CT.Trace(Handle(),0);
}


void abstractMixedGraph::Layout_CrossingMinimization(
    indexSet<TNode>& fixedLayer,indexSet<TNode>& floatingLayer) throw()
{
    LogEntry(LOG_METH,"1-sided crossing minimization...");
    OpenFold();

    // Update the relative order of the nodes in floatingLayer

    for (TNode u=floatingLayer.First();u<n;u=floatingLayer.Successor(u))
    {
        TArc neighbours = 0;
        TFloat sumX = 0;
        TArc a = First(u);

        do
        {
            if (a==NoArc) break;

            TNode v = EndNode(a);

            if (fixedLayer.IsMember(v))
            {
                sumX += C(v,0);
                neighbours++;
            }

            a = Right(a,u);
        }
        while (a!=First(u));

        if (neighbours>0) SetC(u,0,sumX/neighbours);
        else SetC(u,0,0);
    }

    Layout_CrossingLocalSearch(fixedLayer,floatingLayer);

    CloseFold();
}


void abstractMixedGraph::Layout_ComputeCrossingNumbers(
    indexSet<TNode>& fixedLayer,indexSet<TNode>& floatingLayer,
    goblinHashTable<TArc,TFloat>* cross) throw()
{
    LogEntry(LOG_METH,"Computing crossing numbers...");

    TNode nFloating = 0;
    TArc* neighbours = new TArc[n];
    TArc* adjacent = new TArc[n];

    for (TNode u=floatingLayer.First();u<n;u=floatingLayer.Successor(u))
    {
        nFloating++;
        neighbours[u] = 0;
    }

    // Compute the initial ordering of nodes in fixedLayer

    TNode* nodesInFixedLayer = new TNode[n];
    TNode nFixed = 0;
    goblinQueue<TNode,TFloat> *Q = NULL;

    if (nHeap!=NULL)
    {
        Q = nHeap;
        Q -> Init();
    }
    else Q = NewNodeHeap();

    for (TNode u=fixedLayer.First();u<n;u=fixedLayer.Successor(u))
    {
        Q->Insert(u,C(u,0));
    }

    while (!Q->Empty())
    {
        TNode v = Q->Delete();
        nodesInFixedLayer[nFixed++] = v;
    }


    for (TNode i=0;i<nFixed;i++)
    {
        TNode u = nodesInFixedLayer[i];
        TArc a = First(u);

        // 
        for (TNode v=floatingLayer.First();v<n;v=floatingLayer.Successor(v))
        {
            adjacent[v] = 0;
        }

        do
        {
            if (a==NoArc) break;

            TNode v = EndNode(a);

            if (floatingLayer.IsMember(v))
            {
                adjacent[v]++;

                for (TNode w=floatingLayer.First();w<n;w=floatingLayer.Successor(w))
                {
                    if (w!=v) cross->ChangeKey(v+w*n,cross->Key(v+w*n)+neighbours[w]);
                }
            }

            a = Right(a,u);
        }
        while (a!=First(u));

        // 
        for (TNode v=floatingLayer.First();v<n;v=floatingLayer.Successor(v))
        {
            neighbours[v] += adjacent[v];
        }
    }

    delete[] neighbours;
    delete[] adjacent;
    delete[] nodesInFixedLayer;
}


bool abstractMixedGraph::Layout_CrossingLocalSearch(
    indexSet<TNode>& fixedLayer,indexSet<TNode>& floatingLayer) throw()
{
    LogEntry(LOG_METH,"Greedy switch heuristic...");
    OpenFold();


    // Compute the initial ordering of nodes in floatingLayer

    goblinQueue<TNode,TFloat> *Q = NULL;

    if (nHeap!=NULL)
    {
        Q = nHeap;
        Q -> Init();
    }
    else Q = NewNodeHeap();

    for (TNode u=floatingLayer.First();u<n;u=floatingLayer.Successor(u))
    {
        Q->Insert(u,C(u,0));
    }

    TNode* nodeInFloatingLayer = new TNode[n];
    TNode nFloating = 0;

    while (!Q->Empty())
    {
        TNode v = Q->Delete();
        nodeInFloatingLayer[nFloating++] = v;
    }

    if (nHeap==NULL) delete Q;


    // Compute the crossing numbers between all node pairs in floatingLayer

    goblinHashTable<TArc,TFloat>* cross = new goblinHashTable<TArc,TFloat>(n*n,2*n,0,CT);
    Layout_ComputeCrossingNumbers(fixedLayer,floatingLayer,cross);


    // Flip adjacent nodes in floatingLayer as long as the number of crossings can be reduced

    bool shuffled = false;
    bool searching = true;

    while (searching && CT.SolverRunning())
    {
        searching = false;

        for (TNode i=1;i<nFloating;i++)
        {
            TNode u = nodeInFloatingLayer[i-1];
            TNode v = nodeInFloatingLayer[i];

            if (cross->Key(u+n*v)>cross->Key(v+n*u))
            {
                searching = true;
                nodeInFloatingLayer[i-1] = v;
                nodeInFloatingLayer[i] = u;

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Flipping to ... %lu %lu ...",
                        static_cast<unsigned long>(v),
                        static_cast<unsigned long>(u));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif
            }
        }

        shuffled ^= searching;
    }

    CloseFold();


    // Update ordinates of the nodes in floatingLayer

    TFloat thisX = 0;

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"...Final order is ");

    #endif


    TFloat spacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,spacing);

    for (TNode i=0;i<nFloating;i++)
    {
        SetC(nodeInFloatingLayer[i],0,thisX);
        thisX += spacing;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"%lu ",
                static_cast<unsigned long>(nodeInFloatingLayer[i]));
            LogAppend(LH,CT.logBuffer);
        }

        #endif
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    delete cross;
    delete[] nodeInFloatingLayer;

    return shuffled;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2006
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   layoutLayered.cpp
/// \brief  Implementations of layered drawing methods

#include "mixedGraph.h"
#include "staticStack.h"
#include "staticQueue.h"
#include "lpSolver.h"
#include "moduleGuard.h"


TCap abstractMixedGraph::FeedbackArcSet() throw()
{
    moduleGuard M(ModFeedbackArcSet,*this,"Computing minimum feedback arc set...");

    TArc* edgeColour = InitEdgeColours();
    TCap cancelledArcs = 0;
    TCap* degIn = new TCap[n];
    TCap* degOut = new TCap[n];
    staticStack<TNode> SourceNodes(n,CT);
    staticStack<TNode> TargetNodes(n,CT);

    for (TNode i=0;i<n;i++) degIn[i] = degOut[i] = 0;

    for (TArc a=0;a<m;a++)
    {
        TNode u = StartNode(2*a);
        TNode v = EndNode(2*a);

        if (Orientation(2*a)==1)
        {
            degIn[v] += UCap(2*a);
            degOut[u] += UCap(2*a);
        }
        else if (u==v)
        {
            edgeColour[a] = 1;
        }
    }

    for (TNode i=0;i<n;i++)
    {
        if (degOut[i]==0) TargetNodes.Insert(i);
        else if (degIn[i]==0) SourceNodes.Insert(i);
    }

    for (TNode i=0;i<n;i++)
    {
        TNode vDelete = NoNode;
        bool addEntering = false;

        if (!TargetNodes.Empty())
        {
            vDelete = TargetNodes.Delete();
            addEntering = true;

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Target node %lu",
                    static_cast<unsigned long>(vDelete));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
        else if (!SourceNodes.Empty())
        {
            vDelete = SourceNodes.Delete();

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Source node %lu",
                    static_cast<unsigned long>(vDelete));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }
        else
        {
            TCap bestDelta = -InfCap;

            for (TNode w=0;w<n;w++)
            {
                if (degIn[w]==0 || degOut[w]==0) continue;

                TCap thisDelta = fabs(degOut[w])-fabs(degIn[w]);

                if (thisDelta>bestDelta)
                {
                    vDelete = w;
                    bestDelta = thisDelta;
                }
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1)
            {
                sprintf(CT.logBuffer,"Indefinite node %lu",
                    static_cast<unsigned long>(vDelete));
                LogEntry(LOG_METH2,CT.logBuffer);
            }

            #endif
        }

        if (vDelete==NoNode) continue;

        TArc a = First(vDelete);

        if (a==NoArc) continue;

        do
        {
            TNode v = EndNode(a);

            if (edgeColour[a>>1]!=NoArc)
            {
                a = Right(a,vDelete);
                continue;
            }

            if (Orientation(a)==0)
            {
                edgeColour[a>>1] = 3 - (!addEntering ^ (a&1));
                a = Right(a,vDelete);
                continue;
            }

            if (((a&1) && addEntering) || (!(a&1) && !addEntering))
            {
                edgeColour[a>>1] = 0;
            }
            else
            {
                cancelledArcs += UCap(a);
                edgeColour[a>>1] = 1;
            }

            if (a&1)
            {
                degOut[v] -= UCap(a);

                if (degOut[v]==0 && UCap(a)>0 && degIn[v]>0) TargetNodes.Insert(v);
            }
            else
            {
                degIn[v] -= UCap(a);

                if (degIn[v]==0 && UCap(a)>0 && degOut[v]>0) SourceNodes.Insert(v);
            }

            a = Right(a,vDelete);
        }
        while (a!=First(vDelete));

        degIn[vDelete] = degOut[vDelete] = 0;
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...%g capacity units eliminated", cancelledArcs);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    CT.Trace(Handle(),0);

    return cancelledArcs;
}


void abstractMixedGraph::ImplicitOrientationFromDrawing() throw()
{
    moduleGuard M(ModFeedbackArcSet,*this,"Extracting orientation from drawing...");

    TArc* edgeColour = InitEdgeColours();
    TArc cancelledArcs = 0;

    for (TArc a=0;a<m;a++)
    {
        TNode u = StartNode(2*a);
        TNode v = EndNode(2*a);

        if (   C(u,1)<C(v,1)-CT.epsilon
            || (fabs(C(u,1)-C(v,1))<CT.epsilon && u<v)
            )
        {
            edgeColour[a] = 0;
        }
        else
        {
            edgeColour[a] = 1;
            cancelledArcs++;
        }

        if (Orientation(2*a)==0) edgeColour[a] |= 2;
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...%lu arcs eliminated",
            static_cast<unsigned long>(cancelledArcs));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    CT.Trace(Handle(),0);
}


bool abstractMixedGraph::ImplicitSTOrientation(TNode s,TNode t) throw(ERRejected)
{
    if (s<n && t<n)
    {
        sprintf(CT.logBuffer,"Computing (%lu,%lu)-orientation...",
            static_cast<unsigned long>(s),static_cast<unsigned long>(t));
    }
    else
    {
        sprintf(CT.logBuffer,"Computing bipolar orientation...");
    }

    moduleGuard M(ModFeedbackArcSet,*this,CT.logBuffer);

    if (!STNumbering(NoArc,s,t))
    {
        return false;
    }

    TNode* nodeColour = GetNodeColours();
    TArc* edgeColour = InitEdgeColours();
    TArc cancelledArcs = 0;

    for (TArc a=0;a<m;a++)
    {
        TNode u = StartNode(2*a);
        TNode v = EndNode(2*a);

        if (nodeColour[u]<nodeColour[v])
        {
            edgeColour[a] = 0;
        }
        else
        {
            edgeColour[a] = 1;
            cancelledArcs++;
        }

        if (Orientation(2*a)==0) edgeColour[a] |= 2;
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...%lu arcs eliminated",
            static_cast<unsigned long>(cancelledArcs));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    CT.Trace(Handle(),0);

    return true;
}


void abstractMixedGraph::Layout_PredecessorTree(TOptAlign mode,TFloat dx,TFloat dy)
    throw(ERRejected)
{
    TArc* pred = GetPredecessors();
    TNode* nodeColour = GetNodeColours();

    #if defined(_FAILSAVE_)

    if (pred==NULL)
        Error(ERR_REJECTED,"Layout_PredecessorTree","Missing predecessors");

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_PredecessorTree","Coordinates are fixed");

    #endif

    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
    Layout_ConvertModel(LAYOUT_FREESTYLE_POLYGONES);

    SyncSpacingParameters(TokLayoutNodeSpacing,dx);
    SyncSpacingParameters(TokLayoutNodeSpacing,dy);

    THandle H = Investigate();
    investigator &I = Investigator(H);
    staticStack<TNode> S(n,CT);

    TNode* tmpAlign = new TNode[n];
        // For every node, its ordinate
    TNode* leftAlign = new TNode[n];
        // For every node, the ordinate of the leftmost child ot NoNode
    TNode* rightmost = new TNode[n];
        // The ordinate of the rightmost node at a specified depth
    TNode* depth = new TNode[n];
        // For every node, its depth in the search tree
    TNode* traversed = new TNode[n];
        // All nodes listed in the order of first entering them
    TNode* nr = new TNode[n];
        // For every node, the index of occurence in traverse[]

    TNode maxDepth = 0;
    rightmost[0] = 0;
    TNode cNr = 0;

    for (TNode v=0;v<n;v++)
        tmpAlign[v] = leftAlign[v] = rightmost[v] = NoNode;

    for (TNode r=0;r<n;r++)
    {
        if (pred[r]==NoArc) SetNodeVisibility(r,false);

        if (pred[r]==NoArc && (nodeColour==NULL || nodeColour[r]!=NoNode))
        {
            // Start a new tree rooted at r

            depth[r] = 0;
            traversed[cNr] = r;
            cNr++;

            TNode u = r;
            TNode cDepth = 0;

            while (true)
            {
                if (I.Active(u))
                {
                    TArc a = I.Read(u);
                    TNode v = EndNode(a);

                    if (pred[v]==a)
                    {
                        S.Insert(u);
                        cDepth++;
                        depth[v] = cDepth;

                        if (cDepth>maxDepth) maxDepth = cDepth;

                        traversed[cNr] = v;
                        nr[v] = cNr;
                        cNr++;
                        u = v;
                    }

                    continue;
                }

                // Backtracking

                TNode pos = rightmost[depth[u]];

                if (leftAlign[u]==NoNode)
                {
                    // Node u is a leave

                    if (pos==NoNode) tmpAlign[u] = 0;
                    else tmpAlign[u] = pos+2;
                }
                else if (pos!=NoNode && pos+2>tmpAlign[u])
                {
                    // Shift the partial tree rooted at u

                    TNode shift = pos-tmpAlign[u]+2;

                    for (TNode i=nr[u];i<cNr;i++)
                    {
                        TNode x = traversed[i];

                        if (tmpAlign[x]==rightmost[depth[x]])
                            rightmost[depth[x]] += shift;

                        tmpAlign[x] += shift;
                    }
                }

                rightmost[depth[u]] = tmpAlign[u];

                if (S.Empty()) break;

                TNode v = S.Delete();

                if (leftAlign[v] == NoNode)
                {
                    // u is the leftmost child of v

                    leftAlign[v] = tmpAlign[v] = tmpAlign[u];
                }
                else if (mode==ALIGN_CENTER || mode==ALIGN_FDP || mode==ALIGN_OPTIMIZE)
                {
                    // Proceed as if u was the rightmost child of v:
                    // Center v in top of the child nodes embedded so far

                    tmpAlign[v] = (leftAlign[v]+tmpAlign[u])/2;
                }
                else if (mode==ALIGN_RIGHT)
                {
                    // Proceed as if u was the rightmost child of v:
                    // Place v in top of u

                    tmpAlign[v] = tmpAlign[u];
                }

                u = v;
                cDepth --;
            }
        }
    }

    for (TNode i=0;i<cNr;i++)
    {
        TNode u = traversed[i];
        SetC(u,0,tmpAlign[u]*dx);
        SetC(u,1,depth[u]*dy);
    }

    delete[] tmpAlign;
    delete[] leftAlign;
    delete[] rightmost;
    delete[] depth;
    delete[] traversed;
    delete[] nr;

    Close(H);

    Layout_DefaultBoundingBox();

    if (mode==ALIGN_FDP)
    {
        Layout_ForceDirected(FDP_LAYERED_RESTR);
    }
    else if (IsSparse() && Representation())
    {
        sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

        if (mode==ALIGN_OPTIMIZE)
        {
            X -> Layout_SubdivideArcs();
            Layout_SetHorizontalCoordinates(dx);
        }
        else
        {
            X -> Layout_ArcRouting((dx+dy)/4);
        }
    }
}


void abstractMixedGraph::Layout_Layered(int method,TFloat dx,TFloat dy) throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || !Representation()) NoSparseRepresentation("Layout_Layered");

    #endif

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    moduleGuard M(ModLayering,*this,"Layered drawing...");

    #if defined(_PROGRESS_)

    M.InitProgressCounter(5,1);

    #endif

    if (method==LAYERED_DEFAULT) method = (LAYERED_VERTICAL|LAYERED_HORIZONTAL);

    if (dx<dy || dx<CT.epsilon)
    {
        SyncSpacingParameters(TokLayoutBendSpacing,dx);
        SyncSpacingParameters(TokLayoutNodeSpacing,dx);
    }

    if (dx>dy || dy<CT.epsilon)
    {
        SyncSpacingParameters(TokLayoutBendSpacing,dy);
        SyncSpacingParameters(TokLayoutNodeSpacing,dy);
    }


    // Vertical methods
    if (method & LAYERED_FEEDBACK)
    {
        if (IsUndirected())
        {
            ImplicitSTOrientation();
        }
        else
        {
            FeedbackArcSet();
        }

        M.Trace(0);
    }

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif


    if (method & (LAYERED_COLOURS|LAYERED_EDGE_SPAN))
    {
        for (TArc a=0;a<m;a++) X -> ReleaseEdgeControlPoints(2*a);
    }

    if (method & LAYERED_COLOURS)
    {
        TNode* nodeColour = GetNodeColours();

        bool completeColouring = true;

        for (TNode v=0;v<n;v++)
        {
            if (!nodeColour || nodeColour[v]>=n)
            {
                completeColouring = false;
                break;
            }
        }

        if (completeColouring)
        {
            Layout_LayeringByColours(dx,dy);
        }
        else
        {
            Layout_EdgeSpanMinimalLayering(dy,nodeColour);
        }
    }
    else if (method & LAYERED_EDGE_SPAN)
    {
        if (!(method & LAYERED_FEEDBACK))
        {
            TArc* edgeColour = GetEdgeColours();
            bool oriented = (edgeColour!=NULL);

            for (TArc a=0;a<m && oriented;a++) oriented = (edgeColour[a]<=3);

            if (!oriented) ImplicitOrientationFromDrawing();
        }

        Layout_EdgeSpanMinimalLayering(dy);
    }

    Layout_DefaultBoundingBox();

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif


    X -> Layout_SubdivideArcs();

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif


    // Horizontal methods
    if (method & LAYERED_SWEEP)
    {
        explicitSubdivision G(*this,OPT_MAPPINGS);
        G.Layout_SweepLayerByLayer(dx,dy);

        TFloat fineSpacing = 0.0;
        GetLayoutParameter(TokLayoutFineSpacing,fineSpacing);

        for (TNode v=0;v<G.N();v++)
        {
            for (TDim i=0;i<G.Dim();i++)
            {
                SetC(G.OriginalOfNode(v),i,G.C(v,i));
            }
        }

        for (TArc a=0;a<m;a++)
        {
            TNode x = ArcLabelAnchor(2*a);
            if (x==NoNode) continue;

            TNode y = ThreadSuccessor(x);
            if (y==NoNode) continue;

            for (TDim i=0;i<G.Dim();i++)
            {
                SetC(x,i,C(y,i) + ((i==0) ? fineSpacing : 0.0));
            }
        }

        if (IsSparse())
        {
            static_cast<sparseRepresentation*>(Representation()) -> Layout_AdoptBoundingBox(G);
        }
//        Layout_SweepLayerByLayer(dx,dy);
    }

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif


    if (method & LAYERED_FDP)
    {
        if (method & LAYERED_SWEEP)
        {
            Layout_ForceDirected(FDP_LAYERED_RESTR,dx);
        }
        else
        {
            Layout_ForceDirected(FDP_LAYERED,dx);
        }
    }
    else if (method & LAYERED_ALIGN)
    {
        Layout_SetHorizontalCoordinates(dx);
    }
}


bool abstractMixedGraph::Layout_EdgeSpanMinimalLayering(TFloat dy,TNode* nodeColour)
    throw()
{
    if (m==0)
    {
        return true;
    }

    moduleGuard M(ModLayering,*this,"Assigning layers for the given orientation...");

    if (nodeColour) LogEntry(LOG_METH,"...restricted by the node colours");

    mipInstance *XLP  =
        static_cast<mipInstance*>(VerticalCoordinatesModel(nodeColour));

    if (!XLP)
    {
        M.Shutdown(LOG_RES,"...No appropriate orientation given");

        return false;
    }

    XLP -> SolveLP();

    for (TNode v=0;v<n;v++) SetC(v,1,dy*XLP->X(TVar(v)));

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Total edge span is %g (%g control points required)",
            XLP -> ObjVal(), XLP -> ObjVal()-m);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    delete XLP;

    CT.Trace(Handle(),0);

    return true;
}


void abstractMixedGraph::Layout_LayeringByColours(TFloat dx,TFloat dy) throw(ERRejected)
{
    TNode* nodeColour = GetNodeColours();

    #if defined(_FAILSAVE_)

    if (!nodeColour)
        Error(ERR_REJECTED,"Layout_LayeringByColours","Missing colours");

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_LayeringByColours","Coordinates are fixed");

    #endif

    moduleGuard M(ModLayering,*this,"Assigning layers from node colours...");

    TNode maxCardinality = 0;

    for (TNode k=0;k<=n;k++)
    {
        TNode cardinality = 0;

        for (TNode v=0;v<n;v++)
            if ((k<n && nodeColour[v]==k) || nodeColour[v]>=n) cardinality++;

        if (cardinality>maxCardinality) maxCardinality = cardinality;
    }

    for (TNode k=0;k<=n;k++)
    {
        TNode cardinality = 0;

        for (TNode v=0;v<n;v++)
            if ((k<n && nodeColour[v]==k) || nodeColour[v]>=n) cardinality++;

        TNode i = 0;

        for (TNode v=0;v<n;v++)
        {
            if ((k<n && nodeColour[v]==k) || nodeColour[v]>=n)
            {
                if (cardinality==1) SetC(v,0,0);
                else SetC(v,0,dy*i*(maxCardinality-1)/(cardinality-1));

                SetC(v,1,dx*k);

                i++;
            }
        }
    }

    CT.Trace(Handle(),0);
}


void abstractMixedGraph::Layout_SetHorizontalCoordinates(TFloat dx) throw()
{
    if (m==0)
    {
        return;
    }

    moduleGuard M(ModLayering,*this,"Assigning horizontal coordinates...");

    explicitSubdivision G(*this,OPT_MAPPINGS);
    graphRepresentation* GR = G.Representation();

    // Set arc length
    for (TArc a=0;a<G.M();a++)
    {
        TNode u = G.OriginalOfNode(G.StartNode(2*a));
        TNode v = G.OriginalOfNode(G.EndNode(2*a));

        if (u<n && v<n)
        {
            GR -> SetLength(2*a,1);
        }
        else if (u>=n && v>=n)
        {
            GR -> SetLength(2*a,4);
        }
        else
        {
            GR -> SetLength(2*a,2);
        }
    }

    mipInstance *XLP  =
        static_cast<mipInstance*>(G.HorizontalCoordinatesModel());

    XLP -> SolveLP();

    TFloat minX = InfFloat;
    TFloat maxX = -InfFloat;

    for (TNode v=0;v<G.N();v++)
    {
        TFloat thisX = XLP->X(TVar(v));

        if (thisX<minX) minX = thisX;
        if (thisX>maxX) maxX = thisX;
    }

    for (TNode v=0;v<G.N();v++)
    {
        SetC(G.OriginalOfNode(v),0,dx*(XLP->X(TVar(v)))-minX);
    }

    TFloat fineSpacing = 0.0;
    GetLayoutParameter(TokLayoutFineSpacing,fineSpacing);

    if (fineSpacing<CT.epsilon || fineSpacing>dx)
    {
        fineSpacing = dx * 0.5;
    }

    for (TArc a=0;a<m;a++)
    {
        TNode x = ArcLabelAnchor(2*a);
        if (x==NoNode) continue;

        TNode y = ThreadSuccessor(x);
        if (y==NoNode) continue;

        SetC(x,0,C(y,0)+fineSpacing);
    }

    static_cast<sparseRepresentation*>(Representation()) ->
        Layout_SetBoundingInterval(0,(minX-1)*dx,(maxX+1)*dx);

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Achieved width is %g", maxX-minX);
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    delete XLP;

    M.Trace(0);
}


void abstractMixedGraph::Layout_SweepLayerByLayer(TFloat dx,TFloat dy)
    throw(ERRejected)
{
    LogEntry(LOG_METH,"Sweeping layer-by-layer...");
    OpenFold();

    TFloat minY = InfFloat;
    TFloat maxY = -InfFloat;

    for (TNode v=0;v<n;v++)
    {
        TFloat thisY = C(v,1);

        if (minY>thisY) minY = thisY;
        if (maxY<thisY) maxY = thisY;
    }

    if (minY>maxY-dy+CT.epsilon || fabs(maxY-minY)>n*dy)
        Error(ERR_REJECTED,"Layout_SweepLayerByLayer","Inappropriate layers");

    TNode nLayers = TNode(floor((maxY-minY)/dy+0.5)+1);
    staticQueue<TNode>** nodesInLayer = new staticQueue<TNode>*[nLayers];
    nodesInLayer[0] = new staticQueue<TNode>(n,CT);

    for (TNode l=1;l<nLayers;l++)
        nodesInLayer[l] = new staticQueue<TNode>(*nodesInLayer[0]);

    for (TNode v=0;v<n;v++)
    {
        TNode thisLayer = TNode(floor((C(v,1)-minY)/dy+0.5));
        nodesInLayer[thisLayer] -> Insert(v);
    }

    for (TNode l=0;l<nLayers;l++)
    {
        if (nodesInLayer[l]->Empty())
        {
            for (TNode k=1;k<nLayers;k++)
                delete nodesInLayer[k];

            delete nodesInLayer[0];
            delete[] nodesInLayer;

            Error(ERR_REJECTED,"Layout_SweepLayerByLayer","Missing layers");
        }
    }


    bool shuffled = true;
    unsigned iterations = 0;

    while (shuffled && CT.SolverRunning() && iterations<nLayers)
    {
        shuffled = false;
        iterations++;

        for (TNode l=0;l<2*nLayers-2;l++)
        {
            TNode thisLayer = l;
            TNode nextLayer = l+1;

            if (l>nLayers-2)
            {
                thisLayer = 2*nLayers-l-2;
                nextLayer = 2*nLayers-l-3;
            }

            bool shuffledThisTime = true;

            if (iterations==1)
            {
                Layout_CrossingMinimization(
                            *nodesInLayer[thisLayer],*nodesInLayer[nextLayer]);
            }
            else
            {
                shuffledThisTime = Layout_CrossingLocalSearch(
                            *nodesInLayer[thisLayer],*nodesInLayer[nextLayer]);
            }

            if (shuffledThisTime)
            {
                shuffled = true;

                Layout_DefaultBoundingBox();

                CT.Trace(Handle(),0);
            }
        }
    }

    for (TNode l=1;l<nLayers;l++)
        delete nodesInLayer[l];

    delete nodesInLayer[0];
    delete[] nodesInLayer;

    CloseFold();

    CT.Trace(Handle(),0);
}


void abstractMixedGraph::Layout_CrossingMinimization(
    indexSet<TNode>& fixedLayer,indexSet<TNode>& floatingLayer) throw()
{
    LogEntry(LOG_METH,"1-sided crossing minimization...");
    OpenFold();

    // Update the relative order of the nodes in floatingLayer

    for (TNode u=floatingLayer.First();u<n;u=floatingLayer.Successor(u))
    {
        TArc neighbours = 0;
        TFloat sumX = 0;
        TArc a = First(u);

        do
        {
            if (a==NoArc) break;

            TNode v = EndNode(a);

            if (fixedLayer.IsMember(v))
            {
                sumX += C(v,0);
                neighbours++;
            }

            a = Right(a,u);
        }
        while (a!=First(u));

        if (neighbours>0) SetC(u,0,sumX/neighbours);
        else SetC(u,0,0);
    }

    Layout_CrossingLocalSearch(fixedLayer,floatingLayer);

    CloseFold();
}


void abstractMixedGraph::Layout_ComputeCrossingNumbers(
    indexSet<TNode>& fixedLayer,indexSet<TNode>& floatingLayer,
    goblinHashTable<TArc,TFloat>* cross) throw()
{
    LogEntry(LOG_METH,"Computing crossing numbers...");

    TNode nFloating = 0;
    TArc* neighbours = new TArc[n];
    TArc* adjacent = new TArc[n];

    for (TNode u=floatingLayer.First();u<n;u=floatingLayer.Successor(u))
    {
        nFloating++;
        neighbours[u] = 0;
    }

    // Compute the initial ordering of nodes in fixedLayer

    TNode* nodesInFixedLayer = new TNode[n];
    TNode nFixed = 0;
    goblinQueue<TNode,TFloat> *Q = NULL;

    if (nHeap!=NULL)
    {
        Q = nHeap;
        Q -> Init();
    }
    else Q = NewNodeHeap();

    for (TNode u=fixedLayer.First();u<n;u=fixedLayer.Successor(u))
    {
        Q->Insert(u,C(u,0));
    }

    while (!Q->Empty())
    {
        TNode v = Q->Delete();
        nodesInFixedLayer[nFixed++] = v;
    }


    for (TNode i=0;i<nFixed;i++)
    {
        TNode u = nodesInFixedLayer[i];
        TArc a = First(u);

        // 
        for (TNode v=floatingLayer.First();v<n;v=floatingLayer.Successor(v))
        {
            adjacent[v] = 0;
        }

        do
        {
            if (a==NoArc) break;

            TNode v = EndNode(a);

            if (floatingLayer.IsMember(v))
            {
                adjacent[v]++;

                for (TNode w=floatingLayer.First();w<n;w=floatingLayer.Successor(w))
                {
                    if (w!=v) cross->ChangeKey(v+w*n,cross->Key(v+w*n)+neighbours[w]);
                }
            }

            a = Right(a,u);
        }
        while (a!=First(u));

        // 
        for (TNode v=floatingLayer.First();v<n;v=floatingLayer.Successor(v))
        {
            neighbours[v] += adjacent[v];
        }
    }

    delete[] neighbours;
    delete[] adjacent;
    delete[] nodesInFixedLayer;
}


bool abstractMixedGraph::Layout_CrossingLocalSearch(
    indexSet<TNode>& fixedLayer,indexSet<TNode>& floatingLayer) throw()
{
    LogEntry(LOG_METH,"Greedy switch heuristic...");
    OpenFold();


    // Compute the initial ordering of nodes in floatingLayer

    goblinQueue<TNode,TFloat> *Q = NULL;

    if (nHeap!=NULL)
    {
        Q = nHeap;
        Q -> Init();
    }
    else Q = NewNodeHeap();

    for (TNode u=floatingLayer.First();u<n;u=floatingLayer.Successor(u))
    {
        Q->Insert(u,C(u,0));
    }

    TNode* nodeInFloatingLayer = new TNode[n];
    TNode nFloating = 0;

    while (!Q->Empty())
    {
        TNode v = Q->Delete();
        nodeInFloatingLayer[nFloating++] = v;
    }

    if (nHeap==NULL) delete Q;


    // Compute the crossing numbers between all node pairs in floatingLayer

    goblinHashTable<TArc,TFloat>* cross = new goblinHashTable<TArc,TFloat>(n*n,2*n,0,CT);
    Layout_ComputeCrossingNumbers(fixedLayer,floatingLayer,cross);


    // Flip adjacent nodes in floatingLayer as long as the number of crossings can be reduced

    bool shuffled = false;
    bool searching = true;

    while (searching && CT.SolverRunning())
    {
        searching = false;

        for (TNode i=1;i<nFloating;i++)
        {
            TNode u = nodeInFloatingLayer[i-1];
            TNode v = nodeInFloatingLayer[i];

            if (cross->Key(u+n*v)>cross->Key(v+n*u))
            {
                searching = true;
                nodeInFloatingLayer[i-1] = v;
                nodeInFloatingLayer[i] = u;

                #if defined(_LOGGING_)

                if (CT.logMeth>1)
                {
                    sprintf(CT.logBuffer,"Flipping to ... %lu %lu ...",
                        static_cast<unsigned long>(v),
                        static_cast<unsigned long>(u));
                    LogEntry(LOG_METH2,CT.logBuffer);
                }

                #endif
            }
        }

        shuffled ^= searching;
    }

    CloseFold();


    // Update ordinates of the nodes in floatingLayer

    TFloat thisX = 0;

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"...Final order is ");

    #endif


    TFloat spacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,spacing);

    for (TNode i=0;i<nFloating;i++)
    {
        SetC(nodeInFloatingLayer[i],0,thisX);
        thisX += spacing;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"%lu ",
                static_cast<unsigned long>(nodeInFloatingLayer[i]));
            LogAppend(LH,CT.logBuffer);
        }

        #endif
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    delete cross;
    delete[] nodeInFloatingLayer;

    return shuffled;
}
