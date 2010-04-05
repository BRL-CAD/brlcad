
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, June 2006
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   seriesParallel.cpp
/// \brief  Methods for the recognition and embedding of series-parallel graphs

#include "sparseGraph.h"
#include "sparseDigraph.h"
#include "mixedGraph.h"
#include "staticStack.h"
#include "moduleGuard.h"


bool abstractMixedGraph::EdgeSeriesParallelMethod(
    TOptSeriesParallel options,TNode source,TNode target,abstractMixedGraph* _T) throw (ERRejected)
{
    // If not provided by the calling context, compute a binary decomposition tree
    abstractMixedGraph* T = _T;

    if (!T) T = ESP_DecompositionTree(options,source,target);

    if (!T)
    {
        // Graph is not series-parallel
        return false;
    }

    if (options<=ESP_DIRECTED)
    {
        // Graph is series-parallel. No further actions have been requested
        return true;
    }

    sparseRepresentation* X = NULL;

    if (Representation() && IsSparse())
    {
        X = static_cast<sparseRepresentation*>(Representation());
    }
    else if (options & (ESP_EMBEDDING | ESP_VISIBILITY | ESP_ORIENT))
    {
        NoSparseRepresentation("EdgeSeriesParallelMethod");
    }


    moduleGuard M(ModSeriesParallel,*this);

    // The nodes of the decomposition tree T are either arcs in the original graph or define
    // subgraphs split into smaller ones by either serial or parallel decomposition

    // Node colours encode the type of nodes in the decomposition tree:
    // Bit 0: Orientation of the arcs / sugraphs in the parent subgraph
    // Values 0/1: Original arcs (Forward/Backward)
    // Values 2/3: Serial composition of its childs
    // Values 4/5: Parallel composition of its childs
    TNode* nodeType = T->GetNodeColours();

    // Memorize the upper-leftmost and the lower rightmost arc of each subgraph.
    // The corresponding start nodes also encode the source and target node of each subgraph
    TArc* sourceLeftMost = new TArc[m-1];
    TArc* targetRightMost  = new TArc[m-1];

    // Occasionally memorize the topologic embedding implicitly defined by the decomposition tree
    TArc* right = NULL;

    if (options & ESP_EMBEDDING)
    {
        right = new TArc[2*m];
        for (TArc i=0;i<2*m;i++) right[i] = i;
    }

    // Occasionally memorize the required dimensions of each subgraph
    TArc* width  = NULL;
    TArc* height = NULL;

    if (options & ESP_VISIBILITY)
    {
        width  = new TArc[m-1];
        height = new TArc[m-1];
    }

    // First pass determines the dimensions of recursive subgraphs
    // and reorder the node incidence lists
    for (TArc i=m;i<2*m-1;i++)
    {
        TArc child1 = TArc(T->EndNode(T->First(i)));
        TArc child2 = TArc(T->EndNode(T->Right(T->First(i),i)));

        int childWidth1 = 0;
        int childWidth2 = 0;
        int childHeight1 = 1;
        int childHeight2 = 1;
        TArc sourceLeftMost1  = NoArc;
        TArc targetRightMost1 = NoArc;
        TArc sourceLeftMost2  = NoArc;
        TArc targetRightMost2 = NoArc;

        if (child1>=m)
        {
            sourceLeftMost1  = sourceLeftMost[child1-m];
            targetRightMost1 = targetRightMost[child1-m];

            if (width)
            {
                childWidth1  = width[child1-m];
                childHeight1 = height[child1-m];
            }
        }
        else
        {
            sourceLeftMost1  = 2*child1;
            targetRightMost1 = sourceLeftMost1^1;
        }

        if (nodeType[child1] & 1)
        {
            TArc swap = sourceLeftMost1;
            sourceLeftMost1 = targetRightMost1;
            targetRightMost1 = swap;
        }

        if (child2>=m)
        {
            sourceLeftMost2  = sourceLeftMost[child2-m];
            targetRightMost2 = targetRightMost[child2-m];

            if (width)
            {
                childWidth2  = width[child2-m];
                childHeight2 = height[child2-m];
            }
        }
        else
        {
            sourceLeftMost2  = 2*child2;
            targetRightMost2 = sourceLeftMost2^1;
        }

        if (nodeType[child2] & 1)
        {
            TArc swap = sourceLeftMost2;
            sourceLeftMost2 = targetRightMost2;
            targetRightMost2 = swap;
        }

        if (nodeType[i] & 2)
        {
            // Serial production case

            if (width)
            {
                height[i-m] = childHeight1+childHeight2;
                width[i-m] = childWidth1;
                if (childWidth1<childWidth2) width[i-m] = childWidth2;
            }

            if (right)
            {
                TArc targetLeftMost2 = right[targetRightMost2];
                TArc sourceRightMost1 = right[sourceLeftMost1];

                right[targetRightMost2] = sourceRightMost1;
                right[sourceLeftMost1] = targetLeftMost2;
            }
        }
        else
        {
            // Parallel production case

            if (width)
            {
                width[i-m] = childWidth1+childWidth2+1;
                height[i-m] = childHeight1;
                if (childHeight1<childHeight2) height[i-m] = childHeight2;
            }

            if (right)
            {
                TArc targetLeftMost1 = right[targetRightMost1];
                TArc sourceRightMost1 = right[sourceLeftMost1];
                TArc targetLeftMost2 = right[targetRightMost2];
                TArc sourceRightMost2 = right[sourceLeftMost2];

                right[targetRightMost1] = targetLeftMost2;
                right[targetRightMost2] = targetLeftMost1;
                right[sourceLeftMost1] = sourceRightMost2;
                right[sourceLeftMost2] = sourceRightMost1;
            }
        }

        sourceLeftMost[i-m]  = sourceLeftMost2;
        targetRightMost[i-m] = targetRightMost1;
    }

    if (right)
    {
        X -> ReorderIncidences(right,true);
        MarkExteriorFace(sourceLeftMost[m-2]^1);
        delete[] right;
    }

    if (!(options & (ESP_VISIBILITY | ESP_ORIENT)))
    {
        delete[] sourceLeftMost;
        delete[] targetRightMost;
        delete T;

        M.Shutdown(LOG_RES,"...Edge series-parallel embedding found");

        return true;
    }

    // Second pass determines upper left corner of each subgraph
    bool* upsideDown = new bool[2*m-1];
    upsideDown[2*m-2] = false;

    int* xOffset  = NULL;
    int* yOffset = NULL;

    if (options & ESP_VISIBILITY)
    {
        xOffset = new int[2*m-1];
        yOffset = new int[2*m-1];
        xOffset[2*m-2] = yOffset[2*m-2] = 0;
    }

    for (TArc i=2*m-2;i>=m;i--)
    {
        TArc child1 = TArc(T->EndNode(T->First(i)));
        TArc child2 = TArc(T->EndNode(T->Right(T->First(i),i)));

        int thisWidth = 0;
        int thisHeight = 1;

        upsideDown[i] ^= (nodeType[i] & 1);
        upsideDown[child1] = upsideDown[child2] = upsideDown[i];

        if (!xOffset) continue;

        if (upsideDown[i])
        {
            if (child1>=m)
            {
                thisWidth = width[child1-m];
                thisHeight = height[child1-m];
            }
        }
        else
        {
            if (child2>=m)
            {
                thisWidth = width[child2-m];
                thisHeight = height[child2-m];
            }
        }

        if (nodeType[i] & 2)
        {
            // Serial production case
            xOffset[child1] = xOffset[i];
            xOffset[child2] = xOffset[i];

            if (upsideDown[i])
            {
                yOffset[child1] = yOffset[i];
                yOffset[child2] = yOffset[i]+thisHeight;
            }
            else
            {
                yOffset[child1] = yOffset[i]+thisHeight;
                yOffset[child2] = yOffset[i];
            }
        }
        else
        {
            // Parallel production case
            yOffset[child1] = yOffset[i];
            yOffset[child2] = yOffset[i];

            if (upsideDown[i])
            {
                xOffset[child1] = xOffset[i];
                xOffset[child2] = xOffset[i]+thisWidth+1;
            }
            else
            {
                xOffset[child1] = xOffset[i]+thisWidth+1;
                xOffset[child2] = xOffset[i];
            }
        }
    }


    // Third pass assigns actual coordinates and orientations

    if (options & ESP_VISIBILITY)
    {
        Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
        Layout_ConvertModel(LAYOUT_VISIBILITY);

        for (TNode v=0;v<n;v++) X -> SetC(v,0,-1);
    }

    TFloat fineSpacing = 1.0;
    GetLayoutParameter(TokLayoutFineSpacing,fineSpacing);
    TFloat bendSpacing = 1.0;
    GetLayoutParameter(TokLayoutBendSpacing,bendSpacing);
    TFloat nodeSpacing = 1.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    X -> SetCapacity(n,m,2*n+3*m+2);

    for (TArc i=2*m-2;i<NoArc;i--)
    {
        TNode s = NoNode;
        TNode t = NoNode;
        int thisWidth = 0;
        int thisHeight = 1;

        if (i<m)
        {
            s = StartNode(2*i);
            t = EndNode(2*i);

            upsideDown[i] ^= (nodeType[i] & 1);
        }
        else if (width)
        {
            s = StartNode(sourceLeftMost[i-m]);
            t = StartNode(targetRightMost[i-m]);
            thisWidth = width[i-m];
            thisHeight = height[i-m];
        }

        if (!(options & ESP_VISIBILITY)) continue;

        if (upsideDown[i])
        {
            TNode swap = s;
            s = t;
            t = swap;
        }

        if (C(s,0)<0)
        {
            X -> SetC(s,0,(xOffset[i]+thisWidth/2.0)*nodeSpacing);
            X -> SetC(s,1,yOffset[i]*nodeSpacing);

            TNode w = X->InsertThreadSuccessor(s);
            X -> SetC(w,0,thisWidth*nodeSpacing*0.5);
            X -> SetC(w,1,0);
        }

        if (C(t,0)<0)
        {
            X -> SetC(t,0,(xOffset[i]+thisWidth/2.0)*nodeSpacing);
            X -> SetC(t,1,(yOffset[i]+thisHeight)*nodeSpacing);

            TNode w = X->InsertThreadSuccessor(t);
            X -> SetC(w,0,thisWidth*nodeSpacing*0.5);
            X -> SetC(w,1,0);
        }

        if (i<m)
        {
            s = StartNode(2*i);
            t = EndNode(2*i);

            TNode w = X->ProvideArcLabelAnchor(2*i);
            TNode x = X->ProvidePortNode(2*i);
            TNode y = X->ProvidePortNode(2*i+1);

            TFloat cx = xOffset[i]*nodeSpacing;
            TFloat signy = (C(t,1)>C(s,1)) ? 1 : -1;

            X -> SetC(w,0,cx+fineSpacing);
            X -> SetC(x,0,cx);
            X -> SetC(y,0,cx);

            X -> SetC(w,1,(C(t,1)+C(s,1))/2.0);
            X -> SetC(x,1,C(s,1) + signy*bendSpacing);
            X -> SetC(y,1,C(t,1) - signy*bendSpacing);
        }
    }

    if (options & ESP_ORIENT)
    {
        for (TArc i=0;i<m;i++)
        {
            if (upsideDown[i]) X -> FlipArc(2*i);
        }

        M.Shutdown(LOG_RES,"...Edge series parallel orientation found");
    }

    if (options & ESP_VISIBILITY)
    {
        X -> Layout_SetBoundingInterval(0,-nodeSpacing,(width[m-2]+1)*nodeSpacing);
        X -> Layout_SetBoundingInterval(1,-nodeSpacing,(height[m-2]+1)*nodeSpacing);

        sprintf(CT.logBuffer,"...Grid size: %lux%lu",
            static_cast<unsigned long>(width[m-2]),
            static_cast<unsigned long>(height[m-2]));
        LogEntry(LOG_RES,CT.logBuffer);
        M.Shutdown(LOG_RES,"...Edge series parallel drawing found");
    }

    if (options & ESP_EMBEDDING)
    {
        M.Shutdown(LOG_RES,"...Edge series-parallel embedding found");
    }

    delete[] sourceLeftMost;
    delete[] targetRightMost;
    delete[] width;
    delete[] height;
    delete[] xOffset;
    delete[] yOffset;
    delete[] upsideDown;
    delete T;

    return true;
}


abstractMixedGraph* abstractMixedGraph::ESP_DecompositionTree(
    TOptSeriesParallel options,TNode source,TNode target) throw ()
{
    moduleGuard M(ModSeriesParallel,*this,
        "Computing edge series parallel decomposition tree...");

    bool isSeriesParallel = true;

    if (source>=n) source = DefaultSourceNode();
    if (target>=n) target = DefaultTargetNode();

    // Work on a copy of this graph object
    mixedGraph G(*this,OPT_CLONE);
    sparseRepresentation* GR = static_cast<sparseRepresentation*>(G.Representation());

    GR -> SetCapacity(n,2*m-1);

    // Generate a binary decomposition tree
    sparseGraph* T = new sparseGraph(2*m-1,CT);
    sparseRepresentation* TR = static_cast<sparseRepresentation*>(T->Representation());
    TR -> SetCapacity(2*m-1,2*m-2);
    TNode* nodeType = T->InitNodeColours(0);
    T -> SetLayoutParameter(TokLayoutNodeColourMode,graphDisplayProxy::NODES_FIXED_COLOURS,LAYOUT_VISIBILITY);

    // The list of nodes to be inspected
    staticStack<TNode> Q(n,CT);
    for (TNode v=0;v<n;v++) Q.Insert(v);

    while (!Q.Empty())
    {
        TNode v = Q.Delete();
        TArc a1 = G.First(v);

        if (a1==NoArc)
        {
            LogEntry(LOG_RES,"...Graph contains isolated nodes");
            isSeriesParallel = false;
            break;
        }

        TArc a2 = G.Right(a1,v);

        if (a2==a1)
        {
            // Node degree = 1 should only happen for the source and the target node
            continue;
        }

        TArc a3 = G.Right(a2,v);

        if (G.EndNode(a1)==v || G.EndNode(a2)==v || G.EndNode(a3)==v)
        {
            LogEntry(LOG_RES,"...Graph contains loops");
            isSeriesParallel = false;
            break;
        }

        TArc parallelArc1 = NoArc;
        TArc parallelArc2 = NoArc;
        TNode w1 = G.EndNode(a1);
        TNode w2 = G.EndNode(a2);

        if (w1==w2)
        {
            parallelArc1 = a1;
            parallelArc2 = a2;
        }
        else if (a3==a1)
        {
            // Serial production case

            if (v==source || v==target) continue;

            if (options & ESP_DIRECTED)
            {
                if (G.Blocking(a2) && G.Blocking(a1))
                {
                    // v is a target node
                    continue;
                }
                else if (G.Blocking(a1^1) && G.Blocking(a2^1))
                {
                    // v is a source node
                    continue;
                }
                else if (G.Blocking(a2) || G.Blocking(a1^1))
                {
                    // a2^1 does not block, so swap the arcs a1 and a2
                    TArc aSwap = a1;
                    a1 = a2;
                    a2 = aSwap;
                    w1 = w2;
                    w2 = G.EndNode(a2);
                }
            }

            TArc aNew = G.InsertArc(w1,w2);

            if (G.Blocking(a1) || G.Blocking(a2^1))
            {
                // Mark new arc as directed
                GR -> SetOrientation(2*aNew,1);
            }
            else
            {
                // Mark new arc as undirected
                GR -> SetOrientation(2*aNew,0);
            }

            sprintf(CT.logBuffer,
                "Serial reduction of %lu[%lu]%lu[%lu]%lu to %lu[%lu]%lu",
                static_cast<unsigned long>(w1),
                static_cast<unsigned long>(a1^1),
                static_cast<unsigned long>(v),
                static_cast<unsigned long>(a2),
                static_cast<unsigned long>(w2),
                static_cast<unsigned long>(w1),
                static_cast<unsigned long>(2*aNew),
                static_cast<unsigned long>(w2));
            LogEntry(LOG_METH2,CT.logBuffer);

            Q.Insert(w1,Q.INSERT_NO_THROW);
            Q.Insert(w2,Q.INSERT_NO_THROW);

            GR -> CancelArc(a1);
            GR -> CancelArc(a2);

            T -> InsertArc(aNew,a2>>1);
            T -> InsertArc(aNew,a1>>1);
            nodeType[aNew] = 2;
            nodeType[a1>>1] |= (a1&1)^1;
            nodeType[a2>>1] |= (a2&1);

            // The node v is now isolated but will not inspected any more

            continue;
        }

        if (G.EndNode(a3)==G.EndNode(a1))
        {
            parallelArc1 = a1;
            parallelArc2 = a3;
        }

        if (G.EndNode(a2)==G.EndNode(a3))
        {
            parallelArc1 = a2;
            parallelArc2 = a3;
        }

        if (parallelArc1!=NoArc)
        {
            // Parallel production case

            Q.Insert(v);

            if (options & ESP_DIRECTED)
            {
                if (   (G.Blocking(parallelArc1) && G.Blocking(parallelArc2^1))
                    || (G.Blocking(parallelArc1^1) && G.Blocking(parallelArc2))
                   )
                {
                    LogEntry(LOG_RES,"...Graph contains contains directed cycles");
                    isSeriesParallel = false;
                    break;
                }
                else if (G.Blocking(parallelArc1) || G.Blocking(parallelArc2))
                {
                    // Use the opposite direction
                    v = G.EndNode(parallelArc1);
                    parallelArc1 ^= 1;
                    parallelArc2 ^= 1;
                }
            }

            TArc aNew = G.InsertArc(v,G.EndNode(parallelArc2));

            if (G.Blocking(parallelArc1^1) || G.Blocking(parallelArc2^1))
            {
                // Mark new arc as directed
                GR -> SetOrientation(2*aNew,1);
            }
            else
            {
                // Mark new arc as undirected
                GR -> SetOrientation(2*aNew,0);
            }

            sprintf(CT.logBuffer,
                "Parallel reduction of %lu[%lu,%lu]%lu to %lu[%lu]%lu",
                static_cast<unsigned long>(v),
                static_cast<unsigned long>(parallelArc1),
                static_cast<unsigned long>(parallelArc2),
                static_cast<unsigned long>(G.EndNode(parallelArc1)),
                static_cast<unsigned long>(v),
                static_cast<unsigned long>(2*aNew),
                static_cast<unsigned long>(G.EndNode(parallelArc1)));
            LogEntry(LOG_METH2,CT.logBuffer);

            GR -> CancelArc(parallelArc1);
            GR -> CancelArc(parallelArc2);

            T -> InsertArc(aNew,parallelArc2>>1);
            T -> InsertArc(aNew,parallelArc1>>1);
            nodeType[aNew] = 4;
            nodeType[parallelArc1>>1] |= (parallelArc1&1);
            nodeType[parallelArc2>>1] |= (parallelArc2&1);
        }
    }

    // Let the first incidence of a non-leave node in T point to a child node
    for (TArc i=m;i<G.M();i++)
    {
        TR -> SetFirst(i,T->Right(T->First(i),i));
    }

    if (G.M()>m && CT.traceLevel>0)
    {
        for (TArc i=0;i<T->M();i++)
        {
            T -> SetPred(T->EndNode(2*i),2*i);
        }

        CT.SuppressLogging();
        T -> Layout_PredecessorTree();
        CT.RestoreLogging();

        T -> ReleasePredecessors();
        T -> Display();
    }

    if (isSeriesParallel)
    {
        if (G.M()<2*m-1)
        {
            M.Shutdown(LOG_RES,"...Graph could not be entirely decomposed");

            if (options & ESP_MINOR) ESP_ConstructK4Minor(options,G,T);

            delete T;

            return NULL;
        }
        else
        {
            if (G.StartNode(4*(m-1))==target) nodeType[2*(m-1)] ^= 1;

            M.Shutdown(LOG_RES,"...Graph is edge series-parallel");
            return T;
        }
    }

    return NULL;
}


void abstractMixedGraph::ESP_ConstructK4Minor(TOptSeriesParallel options,
    abstractMixedGraph& G,abstractMixedGraph* T) throw ()
{
    moduleGuard M(ModSeriesParallel,*this,"Extracting forbidden subgraph...");

    // This is either a plain copy or an orientation of G
    sparseDiGraph* H = NULL;
    mixedGraph* H2 = NULL;

    // In the undirected case, s and t form the poles of a bipolar orientation
    TArc retArc = NoArc;
    TNode s = NoNode;
    TNode t = NoNode;

    if (options & ESP_DIRECTED)
    {
        H = new sparseDiGraph(G,OPT_MAPPINGS);
        H->Representation() -> SetCLength(1);
    }
    else
    {
        // Reduce to the directed case by the following technique:
        // Choose a non-bridge arc retArc = (s,t) and an st-orientation of the
        // respective 2-block. Any forbidden sub(di)graph X in this block can
        // be extended to a K_4 subdivision by adding retArc and some critical
        // paths between X and retArc.

        G.CutNodes();
        TArc* edgeColour = G.GetEdgeColours();

        bool* foundColour = new bool[G.M()];

        for (TArc a=0;a<G.M();a++) foundColour[a] = false;

        for (TArc a=0;a<G.M();a++)
        {
            if (G.StartNode(2*a)==NoNode || G.EndNode(2*a)==NoNode) continue; // Cancelled arc

            if (foundColour[edgeColour[a]])
            {
                retArc = 2*a;
                s = G.EndNode(retArc);
                t = G.StartNode(retArc);
                break;
            }

            foundColour[edgeColour[a]] = true;
        }

        if (retArc==NoArc)
        {
            M.Shutdown(LOG_RES,"...Reduced graph forms a tree");
            return;
        }

        H2 = new inducedSubgraph(G,fullIndex<TNode>(G.N(),G.Context()),
                                        colouredArcs(G,edgeColour[retArc>>1]),OPT_MAPPINGS);
        H2->Representation() -> SetCLength(1);

        TArc retArcH2 = NoArc;
        for (TArc a=0;a<G.M();a++)
        {
            if ((H2->OriginalOfArc(2*a)>>1)==(retArc>>1))
            {
                retArcH2 = 2*a;
                break;
            }
        }

        if (!H2->STNumbering(retArcH2))
        {
            M.Shutdown(LOG_RES,"...No rule to construct minors of non-biconnected graphs");
            return;
        }

        H = new inducedOrientation(*H2,OPT_MAPPINGS);
        H->Representation() -> SetCLength(1);
    }

    if (H->CriticalPath()==NoNode)
    {
        M.Shutdown(LOG_RES,"...Graph is recurrent");
        delete H;
        return;
    }

    TFloat* depth = H->GetDistanceLabels();


    LogEntry(LOG_METH,"Generating subgraph edges...");

    // Find a minimal node v1 with at least two predecessors
    TNode v1 = NoNode;

    for (TNode v=0;v<H->N();++v)
    {
        if (v1!=NoNode && depth[v]>=depth[v1]) continue;

        TArc a = H->First(v);
        TArc degIn = 0;

        do
        {
            if (a==NoArc) break;

            if (a&1) ++degIn;

            a = H->Right(a,v);
        }
        while (a!=H->First(v) && degIn<2);

        if (degIn>1) v1 = v;
    }

    if (v1==NoNode)
    {
        M.Shutdown(LOG_RES,"...Graph has multiple target nodes");
        delete H;
        return;
    }

    // Determine the maximal predecessors u1, u2 of vMin
    TNode u1 = NoNode;
    TNode u2 = NoNode;
    TArc  a1 = NoArc;
    TArc  a2 = NoArc;
    TArc a = H->First(v1);

    do
    {
        if (a&1)
        {
            TNode uCurrent = H->EndNode(a);

            if (u1==NoNode)
            {
                a1 = a^1;
                u1 = H->EndNode(a);
            }
            else if (u2==NoNode)
            {
                if (depth[uCurrent]>depth[u1])
                {
                    a2 = a^1;
                    u2 = uCurrent;
                }
                else
                {
                    a2 = a1;
                    u2 = u1;
                    a1 = a^1;
                    u1 = uCurrent;
                }
            }
            else if (depth[uCurrent]>depth[u1])
            {
                if (depth[uCurrent]>depth[u2])
                {
                    a1 = a2;
                    u1 = u2;
                    a2 = a^1;
                    u2 = uCurrent;
                }
                else
                {
                    a1 = a^1;
                    u1 = uCurrent;
                }
            }
        }

        a = H->Right(a,v1);
    }
    while (a!=H->First(v1));

    // Both predecessor arcs form part of the forbidden minor
    H->SetEdgeColour(a1,1);
    H->SetEdgeColour(a2,0);

    // Determine a successor v2 of u2 other than v1. This works
    // since - by choice of v1 - u2 has exactly one predecessor
    // and hence - by construction of G - must have two successors
    a = H->First(u2);
    TNode v2 = NoNode;

    do
    {
        if ((a&1)==0)
        {
            TNode vCurrent = H->EndNode(a);

            if (vCurrent!=v1)
            {
                v2 = vCurrent;
                break;
            }
        }

        a = H->Right(a,u2);
    }
    while (a!=H->First(u2) && v2==NoNode);

    // Let the arc a form part of the forbidden minor
    H->SetEdgeColour(a,2);


    // Grow two paths from v1 and v2 towards the target node and colour them
    while (v1!=v2)
    {
        TNode vMin = (depth[v1]>depth[v2]) ? v2 : v1;
        TNode vNext = NoNode;
        a = H->First(vMin);

        while (true)
        {
            if ((a&1)==0)
            {
                vNext = H->EndNode(a);

                if (depth[vNext]>depth[vMin]) break;
            }

            a = H->Right(a,vMin);

            if (a==H->First(vMin))
            {
                M.Shutdown(LOG_RES,"...Graph has multiple target nodes");
                delete H;
                return;
            }
        }

        if (vMin==v1)
        {
            H->SetEdgeColour(a,4);
            v1 = vNext;
        }
        else
        {
            H->SetEdgeColour(a,2);
            v2 = vNext;
        }
    }

    if (!(options & ESP_DIRECTED))
    {
        // Grow a path from v1 towards the target node and colour it
        while (true)
        {
            TNode vNext = NoNode;
            a = H->First(v1);

            while (true)
            {
                if ((a&1)==0)
                {
                    vNext = H->EndNode(a);

                    if (depth[vNext]>depth[v1]) break;
                }

                a = H->Right(a,v1);

                if (a==H->First(v1))
                {
                    vNext = NoNode;
                    break;
                }
            }

            if (vNext==NoNode) break;

            H->SetEdgeColour(a,5);
            v1 = vNext;
        }
    }

    // Grow two paths from u1 and u2 back to the source node and colour them
    while (u1!=u2)
    {
        TNode uMax = (depth[u1]<depth[u2]) ? u2 : u1;
        TNode uNext = NoNode;
        a = H->First(uMax);

        while (true)
        {
            if (a&1)
            {
                uNext = H->EndNode(a);

                if (depth[uNext]<depth[uMax]) break;
            }

            a = H->Right(a,uMax);

            if (a==H->First(uMax))
            {
                M.Shutdown(LOG_RES,"...Graph has multiple source nodes");
                delete H;
                return;
            }
        }

        if (uMax==u1)
        {
            H->SetEdgeColour(a,1);
            u1 = uNext;
        }
        else
        {
            H->SetEdgeColour(a,3);
            u2 = uNext;
        }
    }

    if (!(options & ESP_DIRECTED))
    {
        // Grow a path from u1 back to the source node and colour it
        while (true)
        {
            TNode uNext = NoNode;
            a = H->First(u1);

            while (true)
            {
                if (a&1)
                {
                    uNext = H->EndNode(a);

                    if (depth[uNext]<depth[u1]) break;
                }

                a = H->Right(a,u1);

                if (a==H->First(u1))
                {
                    uNext = NoNode;
                    break;
                }
            }

            if (uNext==NoNode) break;

            H->SetEdgeColour(a,5);
            u1 = uNext;
        }
    }


    G.InitEdgeColours();

    if (options & ESP_DIRECTED)
    {
        for (TArc a=0;a<H->M();++a)
        {
            G.SetEdgeColour(H->OriginalOfArc(2*a),H->EdgeColour(2*a));
        }

        if (retArc!=NoArc) G.SetEdgeColour(retArc,5);
    }
    else
    {
        for (TArc a=0;a<H->M();++a)
        {
            TArc a2 = H2->OriginalOfArc(H->OriginalOfArc(2*a));

            if ((a2>>1)==(retArc>>1))
            {
                G.SetEdgeColour(retArc,5);
            }
            else
            {
                G.SetEdgeColour(a2,H->EdgeColour(2*a));
            }
        }

        delete H2;
    }

    delete H;


    // Translate the found minor to the original graph. This means tracing down
    // the partial decomposition tree from its root nodes to the leafs which are
    // the original arcs
    staticStack<TNode> Q(T->N(),CT);
    TArc* mapColour = new TArc[T->N()];

    for (TNode a=0;a<T->N();++a)
    {
        TArc aT = T->First(a);

        if (a<m)
        {
            if (aT==NoArc) SetEdgeColour(2*a,G.EdgeColour(2*a));
        }
        else if (aT!=NoArc && T->Right(T->Right(aT,a),a)==aT)
        {
            Q.Insert(a);
            mapColour[a] = G.EdgeColour(2*a);
        }
    }

    while (!Q.Empty())
    {
        TArc r = Q.Delete();

        // The two descendants of r are available by the first two incident arcs
        TArc a1 = T->First(r);
        TArc a2 = T->Right(a1,r);
        TArc u1 = T->EndNode(a1);
        TArc u2 = T->EndNode(a2);
        TNode nodeType = T->NodeColour(r);

        if (nodeType==2 || nodeType==3)
        {
            // Serial production case

            if (u1>=m)
            {
                Q.Insert(u1);
                mapColour[u1] = mapColour[r];
            }
            else
            {
                SetEdgeColour(2*u1,mapColour[r]);
            }

            if (u2>=m)
            {
                Q.Insert(u2);
                mapColour[u2] = mapColour[r];
            }
            else
            {
                SetEdgeColour(2*u2,mapColour[r]);
            }
        }
        else
        {
            // Parallel production case

            if (u1>=m)
            {
                Q.Insert(u1);
                mapColour[u1] = mapColour[r];
            }
            else
            {
                SetEdgeColour(2*u1,mapColour[r]);
            }

            if (u2>=m)
            {
                Q.Insert(u2);
                mapColour[u2] = NoArc;
            }
            else
            {
                SetEdgeColour(2*u2,NoArc);
            }
        }
    }

    delete[] mapColour;

    M.Shutdown(LOG_RES,"...Forbidden subgraph found");
}
