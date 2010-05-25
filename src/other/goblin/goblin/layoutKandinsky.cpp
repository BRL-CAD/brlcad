
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2006
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file layoutKandinsky.cpp
/// \brief Methods for drawing graphs in the Kandinsky layout model

#include "sparseGraph.h"
#include "staticStack.h"
#include "staticQueue.h"
#include "incrementalGeometry.h"
#include "moduleGuard.h"


void abstractMixedGraph::Layout_Kandinsky(TMethOrthogonal method,TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_Kandinsky","Coordinates are fixed");

    #endif


    LogEntry(LOG_METH,"Computing orthogonal drawing...");

    moduleGuard M(ModKandinsky,*this,moduleGuard::SHOW_TITLE);

    if (method==ORTHO_DEFAULT) method=TMethOrthogonal(CT.methOrthogonal);


    // This stores the virtual arc orientations
    char* orientation = new char[m];


    if (method==ORTHO_EXPLICIT)
    {
        // Orient the arcs so that the original edge routing of a 1-bent
        // Kandinsky drawing with node in general position is preserved

        LogEntry(LOG_METH,"Choose arc orientations...");

        for (TArc a=0;a<m;a++)
        {
            orientation[a] = 0;

            TNode x1 = PortNode(2*a);

            if (x1!=NoNode)
            {
                TNode x2 = ThreadSuccessor(x1);
                TNode x3 = NoNode;

                if (x2!=NoNode) x3 = ThreadSuccessor(x2);

                if (x3!=NoNode)
                {
                    TNode v = StartNode(2*a);
                    TFloat dx = C(v,0)-C(x2,0);
                    TFloat dy = C(v,1)-C(x2,1);

                    if (fabs(dx)<fabs(dy)) orientation[a] = 1;
                }
            }
        }
    }


    // Clean up the current drawing and set some default display parameters
    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);
    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
    Layout_ConvertModel(LAYOUT_KANDINSKI);


    LogEntry(LOG_METH,"Place nodes...");

    // Disturb the current positions to break the ties for nodes on common grid lines

    for (TArc a=0;a<m;a++)
    {
        TNode v = StartNode(2*a);
        TNode w = EndNode(2*a);
        TFloat dx = C(v,0)-C(w,0);
        TFloat dy = C(v,1)-C(w,1);
        TFloat signX = (dx>0) ? +1 : ( (dx<0) ? -1 : 0 );
        TFloat signY = (dx>0) ? +1 : ( (dy<0) ? -1 : 0 );

        SetC(w,0,C(w,0)+0.5*signX/m);
        SetC(v,0,C(v,0)-0.5*signX/m);
        SetC(w,1,C(w,1)+0.5*signY/m);
        SetC(v,1,C(v,1)-0.5*signY/m);
    }


    // Place nodes in general position based on the current order of X and Y coordinates

    goblinQueue<TNode,TFloat> *Q = NULL;

    if (nHeap!=NULL)
    {
        Q = nHeap;
        Q -> Init();
    }
    else Q = NewNodeHeap();

    for (TNode v=0;v<n;v++) Q->Insert(v,C(v,0));

    for (TNode i=0;i<n;i++)
    {
        TNode u = Q->Delete();
        SetC(u,0,i);
    }

    for (TNode v=0;v<n;v++) Q->Insert(v,C(v,1));

    for (TNode i=0;i<n;i++)
    {
        TNode u = Q->Delete();
        SetC(u,1,i);
    }

    if (nHeap==NULL) delete Q;


    if (method!=ORTHO_EXPLICIT)
    {
        // Orient the arcs so that for every node, the number of entering
        // arcs and the number of leaving arcs are (almost) equal.
        // The procedure basically computes an Eulerian cycle.

        LogEntry(LOG_METH,"Choose arc orientations...");

        for (TArc a=0;a<m;a++) orientation[a] = 2;

        THandle H = Investigate();
        investigator &I = Investigator(H);

        TNode u = 0;

        while (First(u)==NoArc) u++;

        TArc a = I.Read(u);

        for (TArc i=0;i<m;i++)
        {
            if (a&1) orientation[a>>1] = 1;
            else orientation[a>>1] = 0;

            u = EndNode(a);
            a = NoArc;

            // If the degree of u is odd, and all incident arcs are comsumed,
            // jump to another node from which the search can be continued
            while (i<m-1 && a==NoArc)
            {
                while (!I.Active(u)) u = (u+1)%n;

                a = I.Read(u);

                if (orientation[a>>1]!=2) a = NoArc;
            }

        }

        Close(H);
    }
    else
    {
        //
        Layout_KandinskyCrossingMinimization(orientation);
    }


    // Postoptimization of the node placement and arc orientations
    Layout_KandinskyCompaction(orientation,false);


    // Determine the effective node size CT.nodeSep and assign final node positions
    Layout_KandinskyScaleNodes(orientation);


    // Grow the nodes and assign ports to every arc. Place the control points based
    // on the orientation computed before: Every arc starts with a horizontal segment
    Layout_KandinskyRouteArcs(orientation);


    delete[] orientation;
}


abstractMixedGraph::TPortSide abstractMixedGraph::Layout_KandinskyPortSide(TArc a,const char* orientation)
    throw(ERRange)
{
    TNode v = StartNode(a);
    TNode w = EndNode(a);

    if (v==w)
    {
        if (a&1) return PORT_EAST;
        else return PORT_SOUTH;
    }

    TFloat dx = C(w,0)-C(v,0);
    TFloat dy = C(w,1)-C(v,1);

    if (fabs(dx)<0.5)
    {
        if (dy<0) return PORT_NORTH;
        else return PORT_SOUTH;
    }

    if (fabs(dy)<0.5)
    {
        if (dx<0) return PORT_WEST;
        else return PORT_EAST;
    }

    if ((orientation[a>>1]^(a&1))==0)
    {
        if (dx<0) return PORT_WEST;
        else return PORT_EAST;
    }
    else
    {
        if (dy<0) return PORT_NORTH;
        else return PORT_SOUTH;
    }
}


TArc abstractMixedGraph::Layout_KandinskyCrossingMinimization(char* orientation)
    throw()
{
    moduleGuard M(ModKandinsky,*this,"Reducing the number of edge crossings...");

    TArc a1 = 0;
    TArc lapsToGo = m;

    while (lapsToGo>0  && CT.SolverRunning())
    {
        TNode v1 = StartNode(2*a1);
        TNode w1 = EndNode(2*a1);
        TFloat v1x = C(v1,0);
        TFloat v1y = C(v1,1);
        TFloat w1x = C(w1,0);
        TFloat w1y = C(w1,1);
        TFloat max1x = (w1x<v1x) ? v1x : w1x;
        TFloat min1x = (w1x<v1x) ? w1x : v1x;
        TFloat max1y = (w1y<v1y) ? v1y : w1y;
        TFloat min1y = (w1y<v1y) ? w1y : v1y;
        long profit = 0;

        for (TArc a2=0;a2<m;a2++)
        {
            TNode v2 = StartNode(2*a2);
            TNode w2 = EndNode(2*a2);
            TFloat v2x = C(v2,0);
            TFloat v2y = C(v2,1);
            TFloat w2x = C(w2,0);
            TFloat w2y = C(w2,1);
            TFloat max2x = (w2x<v2x) ? v2x : w2x;
            TFloat min2x = (w2x<v2x) ? w2x : v2x;
            TFloat max2y = (w2y<v2y) ? v2y : w2y;
            TFloat min2y = (w2y<v2y) ? w2y : v2y;

            // The two possible routings of a1 and a2 form each a rectangle.
            // Inspect edge crossing properties in terms of these rectangles

            if (    min1x>=max2x || min2x>=max1x || min1y>=max2y || min2y>=max1y
                || (min1x>=min2x && max1x<=max2x && min1y>=min2y && max1y<=max2y)
                || (min1x<=min2x && max1x>=max2x && min1y<=min2y && max1y>=max2y)
                || (min1x>min2x && max1x<max2x && min1y<min2y && max1y>max2y)
                || (min1x<min2x && max1x>max2x && min1y>min2y && max1y<max2y)
               )
            {
                // Both rectangles are disjoint, nested or crossing each other.
                // That is, crossing does not depending on the a1 and a2 orientations
// cout << "(5) a1 = " << a1 << ", a2 = " << a2 << endl;
                continue;
            }

            // By now, only one of v1 and w1 can be enclosed by the a2 rectangle,
            // and only one of v2 and w2 can be enclosed by the a1 rectangle

            short sign1 = 0;
            short sign2 = 0;

            if (v1x>min2x && v1x<max2x && v1y>min2y && v1y<max2y)
            {
                // v1 is enclosed by the a2 rectangle but w1 is not
                sign1 = 1;
            }
            else if (w1x>min2x && w1x<max2x && w1y>min2y && w1y<max2y)
            {
                // w1 is enclosed by the a2 rectangle but v1 is not
                sign1 = -1;
            }

            if (v2x>min1x && v2x<max1x && v2y>min1y && v2y<max1y)
            {
                // v2 is enclosed by the a1 rectangle but w2 is not
                sign2 = 1;
            }
            else if (w2x>min1x && w2x<max1x && w2y>min1y && w2y<max1y)
            {
                // w2 is enclosed by the a1 rectangle but v2 is not
                sign2 = -1;
            }

            if (sign1!=0 && sign2==0)
            {
                // The number of edge crossings does not depend on the a1 orientation
// cout << "(4) a1 = " << a1 << ", a2 = " << a2 << endl;
                continue;
            }

            if (orientation[a1]) sign1 *= -1;
            if (orientation[a2]) sign2 *= -1;

            // By now, the sign1 and sign2 values encode the following:
            //   0: Neither the start nor the end node of an edge is enclosed by the other rectangle
            //   1: The virtual start node of an edge is enclosed by the other rectangle
            //  -1: The virtual end node of an edge is enclosed by the other rectangle

            if (sign1==0 && sign2==0)
            {
                // There is only one orientation of a1 and a2 which can produce crossings
                // (and, in that case, will cause double crossings)
                char conflictingOri1 =
                    (w1y<min2y || w1y>max2y || v1x<min2x || v1x>max2x) ?
                    orientation[a1]^1 : orientation[a1];
                char conflictingOri2 =
                    (w2y<min1y || w2y>max1y || v2x<min1x || v2x>max1x) ?
                    orientation[a2]^1 : orientation[a2];
                long nCross = (v1==v2 || v1==w2 || w1==v2 || w1==w2) ? 1 : 2;

                if (conflictingOri1 && conflictingOri2)
                {
                    profit += nCross;
// cout << "(1a) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                }
                else if (conflictingOri2)
                {
                    profit -= nCross;
// cout << "(1b) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                }
                else
                {
// cout << "(1c) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                }
            }
            else if (sign1==0 && sign2!=0)
            {
                TFloat bend1x = (orientation[a1]) ? w1x : v1x;
                TFloat bend1y = (orientation[a1]) ? v1y : w1y;
                TFloat bend2x = (orientation[a1]) ? v1x : w1x;
                TFloat bend2y = (orientation[a1]) ? w1y : v1y;

                if (bend1x>min2x && bend1x<max2x && bend1y>min2y && bend1y<max2y)
                {
                    profit -= 1;

// cout << "(2a) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                }
                else if (bend2x>min2x && bend2x<max2x && bend2y>min2y && bend2y<max2y)
                {
                    profit += 1;

// cout << "(2b) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                }
                else
                {
                    // The a2 rectangle is crossing exactly one side of the a1
                    // rectangle (twice). Determine this side
                    char min1yOri = (w1y>v1y) ? 0 : 1;
                    char max1yOri = min1yOri^1;
                    char min1xOri = (w1x<v1x) ? 0 : 1;
                    char max1xOri = min1xOri^1;

                    if (max2x>max1x)
                    {
                        profit += (max1xOri==orientation[a1]) ? +1 : -1;
// cout << "(2c) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                    }
                    else if (min2x<min1x)
                    {
                        profit += (min1xOri==orientation[a1]) ? +1 : -1;
// cout << "(2d) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                    }
                    else if (max2y>max1y)
                    {
                        profit += (max1yOri==orientation[a1]) ? +1 : -1;
// cout << "(2e) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                    }
                    else if (min2y<min1y)
                    {
                        profit += (min1yOri==orientation[a1]) ? +1 : -1;
// cout << "(2f) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                    }
                    else
                    {

// cout << "(2g) a1 = " << a1 << ", a2 = " << a2 << endl;
                    }
                }
            }
            else
            {
                // For every orientation of a2, there is one a1 orientation
                // which results in an edge crossing
                profit -= sign1*sign2;
// cout << "(3) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
            }
        }

        --lapsToGo;

// cout << "profit = " << profit << endl;
        // Flipping the virtual orientation of a1 decreases the number of edge crossings
        if (profit>0)
        {
            orientation[a1] ^= 1;
            lapsToGo = m;
        }

        a1 = (a1+1)%m;
    }

    return 0;
}


bool abstractMixedGraph::Layout_KandinskyCompaction(char* orientation,bool planar)
    throw()
{
    if (int(CT.methOrthoRefine)<int(ORTHO_REFINE_SWEEP)) return false;

    moduleGuard M(ModKandinsky,*this,"Reducing sketch grid size...");

    goblinQueue<TNode,TFloat> *Q = NULL;

    if (nHeap!=NULL)
    {
        Q = nHeap;
        Q -> Init();
    }
    else Q = NewNodeHeap();

    TNode* line[2];
    line[0] = new TNode[n+1];
    line[1] = new TNode[n+1];

    unsigned short firstLine = 0;
    short lapsToGo = 2;
    bool improvedAtLeastOnce = false;
    TNode maxGridLine[2] = {0,0};

    for (TDim i=0;lapsToGo>0 && CT.SolverRunning();i^=1)
    {
        --lapsToGo;

        if (i==0) LogEntry(LOG_METH,"Sweeping horizontally for line merges...");
        else      LogEntry(LOG_METH,"Sweeping vertically for line merges...");

        OpenFold();

        TNode targetLine = 0;
        TNode sourceLine = 0;
        TNode fixedNodes = 0;
        bool improvedThisTime = false;

        while (fixedNodes<n)
        {
            #if defined(_LOGGING_)

            THandle LH = NoHandle;

            #endif

            fixedNodes = 0;

            for (TNode v=0;v<n;v++)
            {
                if (C(v,i)<sourceLine+0.5) ++fixedNodes;
               // else
                {
                    // cout << "C(" << v << ")  == " << C(v,i) << endl;
                }

                if (fabs(C(v,i)-sourceLine)<0.5)
                {
                    Q->Insert(v,C(v,i^1));

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        if (LH==NoHandle)
                        {
                            LH = LogStart(LOG_METH2,"Nodes in line: ");
                        }

                        sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(v));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif
                }
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1 && LH!=NoHandle) LogEnd(LH);

            #endif

            if (sourceLine==targetLine)
            {
                if (Q->Empty())
                {
                    ++targetLine;
                    ++sourceLine;
                    continue;
                }

                for (TNode k=0;;k++)
                {
                    if (Q->Empty())
                    {
                        line[firstLine][k] = NoNode;
                        break;
                    }

                    line[firstLine][k] = Q->Delete();
                }

                ++sourceLine;
                continue;
            }

            for (TNode k=0;k<n;k++)
            {
                if (Q->Empty())
                {
                    line[firstLine^1][k] = NoNode;
                    break;
                }

                line[firstLine^1][k] = Q->Delete();
            }

            if (line[firstLine^1][0]==NoNode)
            {
                ++sourceLine;
                continue;
            }

            unsigned short lineAhead = firstLine;

            if (C(line[lineAhead^1][0],i^1)>C(line[lineAhead][0],i^1))
            {
                lineAhead = firstLine^1;
            }

// Deactivated buggy code:
/*
            if (Layout_KandinskyRefineRouting(orientation,i,line,firstLine))
            {
                improvedThisTime = true;

                #if defined(_LOGGING_)

                LogEntry(LOG_METH2,"...Arc orientations have been changed");

                #endif
            }
*/
            if (Layout_KandinskyShiftChain(orientation,i,line,firstLine,true))
            {
                // This results in a rescan of the target line:
                sourceLine = targetLine;

                #if defined(_LOGGING_)

                LogEntry(LOG_METH2,"...Lines have been merged");

                #endif
            }
            else if (Layout_KandinskyShiftChain(orientation,i,line,firstLine,false))
            {
                // This results in a rescan of the target line:
                sourceLine = targetLine;

                improvedThisTime = true;

                #if defined(_LOGGING_)

                LogEntry(LOG_METH2,"...A chain of collinear edge segments has been shifted");

                #endif
            }
            else
            {
                if (sourceLine>targetLine+1)
                {
                    for (TNode k=0;line[firstLine^1][k]!=NoNode;k++)
                    {
                        SetC(line[firstLine^1][k],i,targetLine+1);
                    }

                    improvedThisTime = true;

                    #if defined(_LOGGING_)

                    LogEntry(LOG_METH2,"...Line has been moved");

                    #endif
                }

                // Rotate the line buffers and skip the current target line
                firstLine ^= 1;
                targetLine = sourceLine;
                ++sourceLine;
            }
        }

        if (planar && Layout_KandinskyShiftNodes(orientation))
        {
            improvedThisTime = true;

            #if defined(_LOGGING_)

            LogEntry(LOG_METH2,"...Nodes have been shifted");

            #endif
        }

        CloseFold();

        maxGridLine[i] = TNode(C(line[firstLine][0],i)+0.5);
        improvedAtLeastOnce |= improvedThisTime;

        if (improvedThisTime)
        {
            lapsToGo = 2;

            #if defined(_PROGRESS_)

            if (maxGridLine[0]*maxGridLine[1]>0)
            {
                // In the very first iteration, maxGridLine[1] is still zero
                M.SetProgressCounter(1.0 - ((maxGridLine[0]+1.0)*(maxGridLine[1]+1.0)-n) / (n*n));
            }

            #endif

            if (CT.traceLevel>2)
            {
                sparseGraph G(*this,OPT_CLONE);
                G.Layout_ConvertModel(LAYOUT_KANDINSKI);
                G.Layout_KandinskyScaleNodes(orientation);
                G.Layout_KandinskyRouteArcs(orientation);
                G.Layout_DefaultBoundingBox();
                M.Trace(G,0);
            }
        }
    }

    if (nHeap==NULL) delete Q;

    delete[] line[0];
    delete[] line[1];

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Final coarse grid size is (%lu,%lu)",
            static_cast<unsigned long>(maxGridLine[0]),
            static_cast<unsigned long>(maxGridLine[1]));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return improvedAtLeastOnce;
}


bool abstractMixedGraph::Layout_KandinskyShiftNodes(char* orientation)
    throw()
{
    bool improved = false;
    goblinQueue<TArc,TFloat> *R = NewArcHeap();

    for (TNode v=0;v<n;v++)
    {
        TArc a = First(v);

        if (a==NoArc) continue;

        TNode maxNode[4]   = {NoNode,NoNode,NoNode,NoNode};
        TNode maxArc[4]    = {NoArc,NoArc,NoArc,NoArc};
        TNode unbentArc[4] = {NoArc,NoArc,NoArc,NoArc};
        short sign[4]      = {-1,+1,+1,-1};
        short coord[4]     = {1,0,1,0};

        do
        {
            TNode u = EndNode(a);
            TPortSide side = Layout_KandinskyPortSide(a,orientation);

            if (fabs(C(u,0)-C(v,0))<0.5 || fabs(C(u,1)-C(v,1))<0.5)
            {
                // Unbent arcs do not prevent from shifting in a perpendicular direction.
                // Memorize such arcs which must be bent after shifting v
                unbentArc[side]  = a;
            }
            else if (   maxNode[side]==NoNode
                     || sign[side]*C(u,coord[side])<sign[side]*C(maxNode[side],coord[side])
                    )
            {
                maxArc[side]  = a;
                maxNode[side] = u;
            }

            a = Right(a,v);
        }
        while (a!=First(v));

        short freeCoord = 2;

        if (maxArc[PORT_NORTH]==NoArc && maxArc[PORT_SOUTH]==NoArc)
        {
            freeCoord = 0;
        }
        else if (maxArc[PORT_WEST]==NoArc && maxArc[PORT_EAST]==NoArc)
        {
            freeCoord = 1;
        }
        else
        {
            continue;
        }

        // Reaching this means that v can be shifted basically.Now determine
        // the new position targetPos. This means hifting v to an adjacent bend
        // node. It takes some care not to introduce node-edge crossings

        a = First(v);

        do
        {
            TNode u = EndNode(a);
            R -> Insert(a>>1,C(u,freeCoord));
            a = Right(a,v);
        }
        while (a!=First(v));

        TArc medianIndex = R->Cardinality()/2;
        TFloat minRange = InfFloat;
        TFloat maxRange = -InfFloat;
        TFloat prevPos = -InfFloat;
        TFloat medianPos = -InfFloat;
        TFloat currentPos = C(v,freeCoord);
        TFloat targetPos = currentPos;

        for (TArc i=0;!R->Empty();++i)
        {
            TArc a = R->Delete();
            TNode u = EndNode(2*a);
            if (u==v) u = StartNode(2*a);
            TFloat thisPos = C(u,freeCoord);

            if (i==0)
            {
                minRange = prevPos = maxRange = thisPos;
            }
            else if (thisPos>maxRange)
            {
                prevPos = maxRange;
                maxRange = thisPos;
            }

            if (i==medianIndex) medianPos = thisPos;
        }

        if (medianPos==maxRange || medianPos==minRange) medianPos = prevPos;

        if (currentPos>=minRange && currentPos<=maxRange)
        {
            if (medianPos>minRange && medianPos<maxRange) targetPos = medianPos;
        }
        else if (currentPos<minRange)
        {
            if (medianPos<maxRange)
            {
                targetPos = medianPos;
            }
            else if (minRange<maxRange)
            {
                targetPos = minRange;
            }
        }
        else // if (currentPos>maxRange)
        {
            if (medianPos>minRange)
            {
                targetPos = medianPos;
            }
            else if (minRange<maxRange)
            {
                targetPos = maxRange;
            }
        }

        if (currentPos!=targetPos)
        {
            SetC(v,freeCoord,targetPos);
            improved = true;

            // Assign virtual orientations to arcs which become bent by this
            // node movement. The port side must change for those arcs!
            if (unbentArc[0^freeCoord]!=NoArc)
            {
                orientation[unbentArc[0^freeCoord]>>1] = (unbentArc[0^freeCoord]&1)^freeCoord;
            }

            if (unbentArc[2^freeCoord]!=NoArc)
            {
                orientation[unbentArc[2^freeCoord]>>1] = (unbentArc[2^freeCoord]&1)^freeCoord;
            }
        }
    }

    delete R;

    return improved;
}


bool abstractMixedGraph::Layout_KandinskySeparableNodes(const char* orientation,TDim i,TNode v,TNode w)
    throw()
{
    if (v==NoNode || w==NoNode) return true;

    TArc nAdj = 0; // The number of arcs connecting v and w
    TArc vAdj = 0; // The number of arcs connecting v with other nodes
    TArc wAdj = 0; // The number of arcs connecting w with other nodes

    TNode minNeighbor = v;
    TArc a = First(v);

    do
    {
        if (a==NoArc) break;

        TNode u = EndNode(a);
        TPortSide side = Layout_KandinskyPortSide(a,orientation);

        if (   (side==PORT_NORTH && i==0)
            || (side==PORT_WEST  && i==1)
            )
        {
            if (u==w)
            {
                ++nAdj;
            }
            else
            {
                ++vAdj;
            }

            if (   C(u,i^1)<C(minNeighbor,i^1)
                || (u!=w && C(u,i^1)<C(minNeighbor,i^1)+0.5)
               )
            {
                minNeighbor = u;
            }
        }

        a = Right(a,v);
    }
    while (a!=First(v));

    TNode maxNeighbor = w;
    a = First(w);

    do
    {
        if (a==NoArc) break;

        TNode u = EndNode(a);
        TPortSide side = Layout_KandinskyPortSide(a,orientation);

        if (   (side==PORT_SOUTH && i==0)
            || (side==PORT_EAST  && i==1)
            )
        {
            if (u==v)
            {
                ++nAdj;
            }
            else
            {
                ++wAdj;
            }

            if (   C(u,i^1)>C(maxNeighbor,i^1)+0.5
                || (u!=v && C(u,i^1)>C(maxNeighbor,i^1)-0.5)
               )
            {
                maxNeighbor = u;
            }
        }

        a = Right(a,w);
    }
    while (a!=First(w));


    if (C(maxNeighbor,i^1)>C(minNeighbor,i^1)+0.5)
    {
        // Merging the lines possibly introduces edge crossing
        return false;
    }

    if (   C(maxNeighbor,i^1)>C(minNeighbor,i^1)-0.5
        && maxNeighbor!=v
        && minNeighbor!=w
        && maxNeighbor!=minNeighbor
       )
    {
        // Merging the lines possibly introduces edge/edge or even node/edge crossings
        return false;
    }

    if (nAdj>1 && vAdj>0 && wAdj>0)
    {
        // This is only to avoid complications when assigning ports
        // to the arcs connecting v and w. The case (nAdj>0 && vAdj+wAdj>0)
        // requires some care for the routing of unbent edges.
        return false;
    }

    return true;
}


bool abstractMixedGraph::Layout_KandinskyShiftChain(const char* orientation,TDim i,TNode** line,TNode firstLine,bool mergeWholeLine)
    throw()
{
    TArc* pred = GetPredecessors();

    bool wholeLineMergable = true;

    TNode indexInLine[2] = {0,0};
    unsigned short lineAhead = firstLine;

    if (C(line[lineAhead^1][0],i^1)>C(line[lineAhead][0],i^1))
    {
        lineAhead = firstLine^1;
    }

    TFloat linePos[2];
    linePos[0] = C(line[0][0],i);
    linePos[1] = C(line[1][0],i);

    // In what follows, chains of line segments in the two grid lines are
    // considered. It is checked if one of these chains can be shifted to
    // the other grid line. For the latter purpose, the index of the first
    // chain (bend) node in the line[] buffer is maintained.
    TNode firstOfChain[2] = {NoNode,NoNode};

    // This tells how the total edge length changes if the current chain
    // is shifted to the other grid line.
    int profit[2] = {0,0};

    while (line[lineAhead^1][indexInLine[lineAhead^1]]!=NoNode)
    {
        // Verify that the arcs pointing back from line[indexInLine[lineAhead]]
        // and the arcs pointing forward from line[indexInLine[lineAhead^1]]
        // do not have any orthogonal grid line in common

        TNode v = line[lineAhead][indexInLine[lineAhead]];
        TNode w = line[lineAhead^1][indexInLine[lineAhead^1]];
        TNode x = line[lineAhead^1][indexInLine[lineAhead^1]+1];
        TNode y = NoNode;
        if (indexInLine[lineAhead^1]>0) y = line[lineAhead^1][indexInLine[lineAhead^1]-1];
        TNode z = NoNode;
        if (indexInLine[lineAhead]>0) z = line[lineAhead][indexInLine[lineAhead]-1];

        // The shifting direction
        TFloat sign = (lineAhead==firstLine) ? -1 : 1;

        TArc xAdj = 0; // The number of arcs connecting w and x (including the case x==NoNode)
        TArc yAdj = 0; // The number of arcs connecting w and y (including the case y==NoNode)

        TArc a = First(w);
        int thisProfit = 0;

        do
        {
            if (a==NoArc) break;

            TNode u = EndNode(a);

            if (u==x) ++xAdj;
            if (u==y) ++yAdj;

            if (sign*(C(u,i)-C(w,i))>0.5)
            {
                thisProfit++;

                if (fabs(C(u,i)-linePos[lineAhead])<0.5)
                {
                    // Shifting w to the grid line of u saves a control point
                    ++thisProfit;
                }

                if (pred && pred[w]==(a^1))
                {
                    // Shifting towards a fixed root node might decrease the
                    // total edge length in a later step
                    ++thisProfit;
                }

                if (pred && pred[u]==a)
                {
                    // Avoid to shift parental nodes towards the descendants
                    --thisProfit;
                }
            }
            else if (sign*(C(u,i)-C(w,i))<-0.5)
            {
                --thisProfit;
            }

            a = Right(a,w);
        }
        while (a!=First(w));


        // When shifting chains of nodes rather than full grid lines,
        // keep track of the current chain and if shifting is profitable
        if (yAdj==0 && firstOfChain[lineAhead^1]==NoNode)
        {
            firstOfChain[lineAhead^1] = indexInLine[lineAhead^1];
            profit[lineAhead^1] = 0;
        }

        profit[lineAhead^1] += thisProfit;

        bool mergable = Layout_KandinskySeparableNodes(orientation,i,v,w);

        if (mergable) mergable = Layout_KandinskySeparableNodes(orientation,i,w,z);

        if (mergeWholeLine)
        {
            wholeLineMergable &= mergable;

            if (!wholeLineMergable) return false;
        }
        else if (!mergable)
        {
            // No way to shift the current chain (if there is one)
            firstOfChain[lineAhead] = NoNode;
            firstOfChain[lineAhead^1] = NoNode;
        }
        else if (xAdj==0 && firstOfChain[lineAhead^1]!=NoNode)
        {
            // A chain has been completed and shifting is possible

            if (profit[lineAhead^1]>0)
            {
                // Shift the current chain to the other grid line since this will
                // reduce the total edge length and, possibly, the number of control points
                for (TNode k=firstOfChain[lineAhead^1];k<=indexInLine[lineAhead^1];k++)
                {
                    SetC(line[lineAhead^1][k],i,linePos[lineAhead]);
                }

                return true;
            }

            // Invalidate this chain
            firstOfChain[lineAhead^1] = NoNode;
        }

        ++indexInLine[lineAhead^1];

        if (   line[lineAhead^1][indexInLine[lineAhead^1]]==NoNode
            || line[lineAhead][indexInLine[lineAhead]]!=NoNode
                && (  C(line[lineAhead^1][indexInLine[lineAhead^1]],i^1)
                        > C(line[lineAhead][indexInLine[lineAhead]],i^1) )
            )
        {
            lineAhead ^= 1;
        }
    }

    if (mergeWholeLine)
    {
        for (TNode k=0;line[firstLine^1][k]!=NoNode;k++)
        {
            SetC(line[firstLine^1][k],i,linePos[firstLine]);
        }

        return true;
    }

    return false;
}


bool abstractMixedGraph::Layout_KandinskyRefineRouting(char* orientation,TDim i,TNode** line,TNode firstLine)
    throw()
{
    bool improved = false;
    TNode indexInLine[2] = {0,0};
    unsigned short lineAhead = firstLine;

    TFloat eps = CT.epsilon;

    while (line[lineAhead][indexInLine[lineAhead]]!=NoNode)
    {
        TNode w = line[lineAhead^1][indexInLine[lineAhead^1]];
        TNode v = line[lineAhead][indexInLine[lineAhead]];
        TNode x = line[lineAhead^1][indexInLine[lineAhead^1]+1];

        if (C(w,i^1)>C(v,i^1)+eps)
        {
            lineAhead ^= 1;
            continue;
        }

        while (x!=NoNode && C(x,i^1)<C(v,i^1)+eps)
        {
            ++indexInLine[lineAhead^1];
            w = x;
            x = line[lineAhead^1][indexInLine[lineAhead^1]+1];
        }

        if (C(w,i^1)>C(v,i^1)-eps)
        {
            ++indexInLine[lineAhead^1];
            ++indexInLine[lineAhead];
            continue;
        }

        // Now, C(x,i^1) > C(v,i^1) > C(w,i^1) and the potential arc a connecting v and
        // w can be flipped without introducing or deleting node-edge crossings.

        // Unfortunately, this does not exclude edge-edge crossings of a and arcs incident
        // with x and with arcs incident with the prededecessor of v

        // Next determine this arc a and if flipping a reduces the port side imbalances

        TPortSide sideForward = (i==0) ? PORT_SOUTH : PORT_EAST;
        TPortSide sideBackward = TPortSide(sideForward^2);
        TPortSide sideToLineBack = (C(w,i)-C(v,i)>0)
            ? ( (i==0) ? PORT_EAST : PORT_SOUTH )
            : ( (i==0) ? PORT_WEST : PORT_NORTH );
        TPortSide sideToLineAhead = TPortSide(sideToLineBack^2);

        TArc attachedArcs[4] = {0,0,0,0};
        TArc foundUnbent[4] = {false,false,false,false};
        TFloat maxPos = -InfFloat;
        TFloat minPos = InfFloat;

        TArc a = First(v);
        TArc aConnect = NoArc;

        do
        {
            if (a==NoArc) break;

            TNode u = EndNode(a);

            if (u==w) aConnect = a;

            TPortSide side = Layout_KandinskyPortSide(a,orientation);

            if (side==sideBackward || side==sideToLineBack)
            {
                attachedArcs[side]++;

                if (fabs(C(u,0)-C(v,0))<eps || fabs(C(u,1)-C(v,1))<eps) foundUnbent[side] = true;

                if (   side==sideBackward
                    && C(u,i^1)>maxPos
                    && (C(u,i)-C(v,i))*(C(w,i)-C(v,i))>0
                   )
                {
                    maxPos = C(u,i^1);
                }
            }

            a = Right(a,v);
        }
        while (a!=First(v));

        if (aConnect!=NoArc)
        {
            a = First(w);

            do
            {
                TPortSide side = Layout_KandinskyPortSide(a,orientation);

                if (side==sideForward || side==sideToLineAhead)
                {
                    attachedArcs[side]++;
                    TNode u = EndNode(a);

                    if (fabs(C(u,0)-C(w,0))<0.5 || fabs(C(u,1)-C(w,1))<0.5) foundUnbent[side] = true;

                    if (   side==sideForward
                        && C(u,i^1)<minPos
                        && (C(u,i)-C(w,i))*(C(v,i)-C(w,i))>0
                       )
                    {
                        minPos = C(u,i^1);
                    }
                }

                a = Right(a,w);
            }
            while (a!=First(w));

            TPortSide currentSideV = Layout_KandinskyPortSide(aConnect,orientation);
            TPortSide currentSideW = (currentSideV==sideBackward) ? sideToLineAhead : sideForward;
            TPortSide alternativeSideV = (currentSideV==sideBackward) ? sideToLineBack : sideBackward;
            TPortSide alternativeSideW = (currentSideV==sideBackward) ? sideForward : sideToLineAhead;

            long currentMax = (attachedArcs[currentSideV]>attachedArcs[currentSideW])
                ? attachedArcs[currentSideV] : attachedArcs[currentSideW];
            long alternativeMax = (attachedArcs[alternativeSideV]>attachedArcs[alternativeSideW])
                ? attachedArcs[alternativeSideV] : attachedArcs[alternativeSideW];


            if (   (currentSideV==sideBackward   && C(w,i^1)==maxPos)
                || (currentSideV==sideToLineBack && C(v,i^1)==minPos)
                )
            {
                if (   (!foundUnbent[alternativeSideV] || attachedArcs[alternativeSideV]!=1)
                    && (!foundUnbent[alternativeSideW] || attachedArcs[alternativeSideW]!=1)
                    && currentMax>alternativeMax+1
                   )
                {
                    orientation[aConnect>>1] ^= 1;
                    improved = true;
                }
            }
        }


        ++indexInLine[lineAhead^1];
        lineAhead ^= 1;
    }

    return improved;
}


TArc abstractMixedGraph::Layout_KandinskyScaleNodes(const char* orientation)
    throw()
{
    LogEntry(LOG_METH,"Determine node size...");

    TArc nodeSize = 1;

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=0;v<n;v++)
    {
        TArc nPorts[4] = {0,0,0,0};

        while (I.Active(v))
        {
            TArc a = I.Read(v);
            TPortSide side = Layout_KandinskyPortSide(a,orientation);
            nPorts[side]++;
        }

        if (nPorts[PORT_NORTH]>nodeSize) nodeSize = nPorts[PORT_NORTH];
        if (nPorts[PORT_EAST]>nodeSize)  nodeSize = nPorts[PORT_EAST];
        if (nPorts[PORT_SOUTH]>nodeSize) nodeSize = nPorts[PORT_SOUTH];
        if (nPorts[PORT_WEST]>nodeSize)  nodeSize = nPorts[PORT_WEST];
    }

    Close(H);

    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);
    TFloat bendSpacing = 0.5*nodeSpacing/(nodeSize+1);
    SetLayoutParameter(TokLayoutBendSpacing,bendSpacing,LAYOUT_KANDINSKI);
    SetLayoutParameter(TokLayoutNodeSize,500.0*nodeSize,LAYOUT_KANDINSKI);

    for (TNode v=0;v<n;v++)
    {
        SetC(v,0,C(v,0)*nodeSpacing);
        SetC(v,1,C(v,1)*nodeSpacing);
    }

    Layout_DefaultBoundingBox();

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...At most %lu ports are attached to each side",
            static_cast<unsigned long>(nodeSize));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    return nodeSize;
}


void abstractMixedGraph::Layout_KandinskyRouteArcs(const char* orientation)
    throw()
{
    LogEntry(LOG_METH,"Draw edges...");

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    // Allocate 3 control points for every arc (4 control points in the case of loops)

    TNode* port1 = new TNode[m];
    TNode* port2 = new TNode[m];
    TNode* bend = new TNode[m];
    TNode* align = new TNode[m];
    TNode* nPorts = new TNode[4*n];

    TFloat bendSpacing = 0.0;
    GetLayoutParameter(TokLayoutBendSpacing,bendSpacing);
    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    TNode controlPoint[4];

    for (TArc a=0;a<m;a++)
    {
        align[a] = X->ProvideArcLabelAnchor(2*a);

        TNode nControlPoints = (StartNode(2*a)==EndNode(2*a)) ? 4 : 3;
        X -> ProvideEdgeControlPoints(a,controlPoint,nControlPoints,PORTS_EXPLICIT);
        port1[a] = controlPoint[0];
        bend[a]  = controlPoint[nControlPoints-2];
        port2[a] = controlPoint[nControlPoints-1];
    }

    goblinQueue<TArc,TFloat> *R = NewArcHeap();

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=0;v<n;v++)
    {
        for (char i=0;i<4;i++)
        {
            TPortSide side = TPortSide(i);
            TFloat sgnx = (side==PORT_EAST)  ? (+1) : ((side==PORT_WEST)  ? (-1) : 0);
            TFloat sgny = (side==PORT_SOUTH) ? (+1) : ((side==PORT_NORTH) ? (-1) : 0);

            I.Reset(v);

            while (I.Active(v))
            {
                TArc a = I.Read(v);
                TNode w = EndNode(a);
                TFloat key = 0;
                TFloat dx = C(w,0)-C(v,0);
                TFloat dy = C(w,1)-C(v,1);

                if (v==w)
                {
                    if (i==char((a>>1)%4) && a%2==0)
                    {
                        // Add loops twice, just to obtain the
                        // correct number of ports
                        R -> Insert(a>>1,TFloat(n*nodeSpacing+a));
                        R -> Insert(a>>1,TFloat(n*nodeSpacing+a));
                    }

                    continue;
                }

                if (side!=Layout_KandinskyPortSide(a,orientation)) continue;

                if (side==PORT_NORTH || side==PORT_SOUTH)
                {
                    // Vertical ports
                    if (dx<0)
                    {
                        // By assigning negative keys, nodes in the left of v
                        // preceed the right hand nodes in the output of R
                        key = sgny*dy-TFloat(n*nodeSpacing);
                    }
                    else
                    {
                        key = TFloat(n*nodeSpacing)-sgny*dy;
                    }

                    // To prevent crossings of parallel arcs, break ties for
                    // equal keys according to the arc indices
                    if (fabs(dx)<bendSpacing/2 || side==PORT_NORTH)
                    {
                        key += 0.1*a/m;
                    }
                    else
                    {
                        key -= 0.1*a/m;
                    }
                }
                else
                {
                    // Horizontal ports
                    if (dy<0)
                    {
                        // By assigning negative keys, nodes in top of v
                        // preceed the nodes below in the output of R
                        key = sgnx*dx-TFloat(n*nodeSpacing);
                    }
                    else
                    {
                        key = TFloat(n*nodeSpacing)-sgnx*dx;
                    }

                    // To prevent crossings of parallel arcs, break ties for
                    // equal keys according to the arc indices
                    if (fabs(dy)<bendSpacing/2 || side==PORT_WEST)
                    {
                        key += 0.1*a/m;
                    }
                    else
                    {
                        key -= 0.1*a/m;
                    }
                }

                R -> Insert(a>>1,key);
            }

            nPorts[4*v+side] = R->Cardinality();
            TFloat offset = nodeSpacing/2-1*bendSpacing;
            TFloat shift = -bendSpacing*(nPorts[4*v+side]-1.0);

            while (!R->Empty())
            {
                TArc a = R->Delete();

                TNode w = EndNode(2*a);
                TNode x = port1[a];
                TNode y = bend[a];
                TNode z = align[a];

                if (StartNode(2*a)==w)
                {
                    // Delete the 2nd occurence of the loop a on R
                    R->Delete();

                    TNode u = port2[a];

                    if (side==PORT_NORTH || side==PORT_SOUTH)
                    {
                        // Vertical ports
                        X -> SetC(x,0,C(v,0)+shift);
                        X -> SetC(x,1,C(v,1)+sgny*offset);
                        X -> SetC(u,0,C(v,0)+shift+2*bendSpacing);
                        X -> SetC(u,1,C(v,1)+sgny*offset);
                    }
                    else
                    {
                        // Horizontal ports
                        X -> SetC(x,0,C(v,0)+sgnx*offset);
                        X -> SetC(x,1,C(v,1)+shift);
                        X -> SetC(u,0,C(v,0)+sgnx*offset);
                        X -> SetC(u,1,C(v,1)+shift+2*bendSpacing);
                    }

                    X -> SetC(y,0,C(u,0)+2*sgnx*bendSpacing);
                    X -> SetC(y,1,C(u,1)+2*sgny*bendSpacing);

                    u = ThreadSuccessor(x);

                    X -> SetC(u,0,C(x,0)+2*sgnx*bendSpacing);
                    X -> SetC(u,1,C(x,1)+2*sgny*bendSpacing);

                    shift += 4*bendSpacing;

                    X -> SetC(z,0,C(x,0)/2+C(y,0)/2);
                    X -> SetC(z,1,C(x,1)/2+C(y,1)/2);
                }
                else
                {
                    // Arc a is not a loop

                    if (w==v)
                    {
                        // Revert arc implicitly
                        w = StartNode(2*a);
                        x = port2[a];
                    }

                    TFloat dx = C(w,0)-C(v,0);
                    TFloat dy = C(w,1)-C(v,1);
                    TFloat thisSgnX = (dx<-0.01) ? -1 : 1;
                    TFloat thisSgnY = (dy< 0.01) ? -1 : 1;

                    if (side==PORT_NORTH || side==PORT_SOUTH)
                    {
                        // Vertical ports
                        X -> SetC(x,1,C(v,1)+sgny*offset);
                        X -> SetC(x,0,C(v,0)+shift);
                        X -> SetC(y,0,C(x,0));
                        X -> SetC(z,0,C(x,0)+thisSgnX*2*bendSpacing);

                        if (fabs(dx)<0.01)
                        {
                            X -> SetC(y,1,(C(v,1)+C(w,1))/2);
                            X -> SetC(z,1,(C(v,1)+C(w,1))/2);
                        }
                    }
                    else
                    {
                        // Horizontal ports
                        X -> SetC(x,0,C(v,0)+sgnx*offset);
                        X -> SetC(x,1,C(v,1)+shift);
                        X -> SetC(y,1,C(x,1));
                        X -> SetC(z,1,C(x,1)+thisSgnY*2*bendSpacing);

                        if (fabs(dy)<0.01)
                        {
                            X -> SetC(y,0,(C(v,0)+C(w,0))/2);
                            X -> SetC(z,0,(C(v,0)+C(w,0))/2);
                        }
                    }

                    shift += 2*bendSpacing;
                }
            }
        }
    }

    Close(H);

    delete R;

    // Align the ports of unbent edges for those end nodes with only one edge
    // routed at this side (the given node positions must be appropriate)
    for (TArc a=0;a<m;a++)
    {
        TPortSide side1 = Layout_KandinskyPortSide(2*a,orientation);
        TPortSide side2 = Layout_KandinskyPortSide(2*a+1,orientation);

        // Sort out all bent edges
        if (side1!=(side2^2)) continue;

        TNode v1 = StartNode(2*a);
        TNode v2 = EndNode(2*a);

        if (nPorts[4*v1+side1]==1)
        {
            if (side1==PORT_NORTH || side1==PORT_SOUTH)
            {
                // Vertical ports
                X -> SetC(port1[a],0,C(port2[a],0));
                X -> SetC(bend[a],0,C(port2[a],0));

                TFloat thisSgnX = (C(port2[a],0)<C(v1,0)) ? -1 : 1;
                X -> SetC(align[a],0,C(bend[a],0)+thisSgnX*2*bendSpacing);
            }
            else
            {
                // Horizontal ports
                X -> SetC(port1[a],1,C(port2[a],1));
                X -> SetC(bend[a],1,C(port2[a],1));

                TFloat thisSgnY = (C(port2[a],1)<C(v1,1)) ? -1 : 1;
                X -> SetC(align[a],1,C(bend[a],1)+thisSgnY*2*bendSpacing);
            }
        }
        else if (nPorts[4*v2+side2]==1)
        {
            if (side2==PORT_NORTH || side2==PORT_SOUTH)
            {
                // Vertical ports
                X -> SetC(port2[a],0,C(port1[a],0));
                X -> SetC(bend[a],0,C(port1[a],0));

                TFloat thisSgnX = (C(port1[a],0)<C(v1,0)) ? -1 : 1;
                X -> SetC(align[a],0,C(bend[a],0)+thisSgnX*2*bendSpacing);
            }
            else
            {
                // Horizontal ports
                X -> SetC(port2[a],1,C(port1[a],1));
                X -> SetC(bend[a],1,C(port1[a],1));

                TFloat thisSgnY = (C(port1[a],1)<C(v1,1)) ? -1 : 1;
                X -> SetC(align[a],1,C(bend[a],1)+thisSgnY*2*bendSpacing);
            }
        }
    }

    delete[] port1;
    delete[] port2;
    delete[] bend;
    delete[] align;
    delete[] nPorts;

    X -> ReleaseCoveredEdgeControlPoints(PORTS_EXPLICIT);
}


void abstractMixedGraph::Layout_StaircaseDrawing(TArc aBasis,TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (aBasis>=2*m && aBasis!=NoArc)
        NoSuchArc("Layout_StaircaseDrawing",aBasis);

    #endif

    moduleGuard M(ModStaircase,*this,"Embedding the graph nodes...");

    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);
    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
    Layout_ConvertModel(LAYOUT_KANDINSKI);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(9,1);

    #endif

    GrowExteriorFace();

    sparseGraph G(n,CT);
    G.ImportLayoutData(*this);
    sparseRepresentation* GR = static_cast<sparseRepresentation*>(G.Representation());
    GR -> SetCapacity(n,m);

    // Restrict to a simple graph. For the deleted non-loop arcs,
    // memorize the maintained parallel arcs by representant[].
    TArc* adjacent = new TArc[n];
    TArc* representant = new TArc[m];
    TArc* mapArc = new TArc[m];

    for (TNode w=0;w<n;w++) adjacent[w] = NoArc;
    for (TArc a=0;a<m;a++) representant[a] = mapArc[a] = NoArc;

    // Generate the reduced incidence structure of G and all mappings
    for (TNode u=0;u<n;u++)
    {
        TArc a = First(u);

        if (a==NoArc) continue;

        do
        {
            TNode v = EndNode(a);

            if (u<v)
            {
                if (adjacent[v]!=NoArc && StartNode(adjacent[v])==u)
                {
                    representant[a>>1] = adjacent[v] ^ (a&1);
                }
                else
                {
                    TArc aG = G.InsertArc(u,v);
                    mapArc[a>>1] = 2*aG ^ (a&1);

                    adjacent[v] = a;
                }
            }

            a = Right(a,u);
        }
        while (a!=First(u));
    }

    TArc* rightHandArc = new TArc[2*G.M()];

    // Planarize the incidence lists of G
    for (TNode u=0;u<n;u++)
    {
        TArc a = First(u);
        TArc firstMappedArc = NoArc;
        TArc prevMappedArc = NoArc;

        if (a==NoArc) continue;

        do
        {
            TArc aG = mapArc[a>>1];

            if (aG!=NoArc)
            {
                aG ^= (a&1);

                if (firstMappedArc==NoArc)
                {
                    firstMappedArc = aG;
                }
                else
                {
                    rightHandArc[prevMappedArc] = aG;
                }

                prevMappedArc = aG;
            }

            a = Right(a,u);
        }
        while (a!=First(u));

        rightHandArc[prevMappedArc] = firstMappedArc;
    }

    GR -> ReorderIncidences(rightHandArc,true);

    delete[] adjacent;
    delete[] rightHandArc;

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif


    // Connectivity augmentation
    G.PlanarConnectivityAugmentation();

    M.Trace(G,1);


    // Biconnectivity augmentation
    G.PlanarBiconnectivityAugmentation();

    M.Trace(G,1);


    // Insert chords into the existing faces
    G.Triangulation();

    M.Trace(G,1);


    // Set up the exterior arc
    if (aBasis==NoArc) aBasis = ExteriorArc();

    if (aBasis==NoArc) aBasis = First(0);

    SetExteriorArc(aBasis);


    // Map the exterior arc to G
    TArc aBasisG = NoArc;

    if (mapArc[aBasis>>1]!=NoArc)
    {
        aBasisG = mapArc[aBasis>>1] ^ (aBasis&1);
    }
    else
    {
        TArc aBasisRep = representant[aBasis>>1];
        aBasisG = mapArc[aBasisRep>>1] ^ (aBasisRep&1);
    }


    char* orientation = new char[m];
    char* orientationG = new char[G.M()];

    // Compute preliminary node coordinates and implicit arc orientations
    G.Layout_StaircaseSketch(aBasisG,spacing,orientationG);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Copy the node positions from G
    for (TNode v=0;v<n;v++)
    {
        SetC(v,0,G.C(v,0));
        SetC(v,1,G.C(v,1));
    }

    // Copy the orientation of the arcs which have been mapped to G
    for (TArc a=0;a<m;a++)
    {
        if (mapArc[a]!=NoArc)
        {
            orientation[a] = orientationG[mapArc[a]>>1] ^ (mapArc[a]&1);
        }
        else
        {
            // This is only to fix the orientation of loops which are
            // neither mapped to G nor represented
            orientation[a] = 0;
        }
    }

    delete[] orientationG;
    delete[] mapArc;

    // Determine the orientation of the parallels which have been eliminated in G
    for (TArc a=0;a<m;a++)
    {
        if (representant[a]!=NoArc)
        {
            // The arcs a and representant[a] must be routed at the same
            // node sides. Choose orientation[a] accordingly.
            orientation[a] = orientation[representant[a]>>1] ^ (representant[a]&1);
        }
    }

    delete[] representant;

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Try to merge some lines
    Layout_KandinskyCompaction(orientation,true);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Determine the effective node size and assign final node positions
    Layout_KandinskyScaleNodes(orientation);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Grow the nodes and assign ports to every arc.
    //    Place the control points based on the orientation computed before:
    //    Every arc starts with a horizontal segment
    Layout_KandinskyRouteArcs(orientation);

    delete[] orientation;

    M.Shutdown(LOG_RES, "...Planar Kandisky drawing found");
}


void abstractMixedGraph::Layout_StaircaseTriconnected(TArc aBasis,TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (aBasis>=2*m && aBasis!=NoArc) NoSuchArc("Layout_StaircaseTriconnected",aBasis);

    #endif

    moduleGuard M(ModStaircase,*this,"Embedding the graph nodes...");

    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);
    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
    Layout_ConvertModel(LAYOUT_KANDINSKI);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(10,2);

    #endif

    char* orientation = new char[m];

    // Step 1: Compute preliminary node coordinates and implicit arc orientations
    try
    {
        Layout_StaircaseSketch(aBasis,spacing,orientation);
    }
    catch (ERRejected)
    {
        delete[] orientation;

        throw ERRejected();
    }

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(6);

    #endif

    // Step 2: Try to merge some lines
    Layout_KandinskyCompaction(orientation,true);

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(1);

    #endif

    // Step 3: Determine the effective node size and assign final node positions
    Layout_KandinskyScaleNodes(orientation);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Step 4: Grow the nodes and assign ports to every arc.
    //    Place the control points based on the orientation computed before:
    //    Every arc starts with a horizontal segment
    Layout_KandinskyRouteArcs(orientation);

    delete[] orientation;

    M.Shutdown(LOG_RES, "...Planar Kandinsky drawing found");
}


void abstractMixedGraph::Layout_StaircaseSketch(TArc aBasis,TFloat spacing,char* orientation)
    throw(ERRejected)
{
    moduleGuard M(ModStaircase,*this,moduleGuard::NO_INDENT);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(m+n,m);

    #endif

    if (aBasis!=NoArc) SetExteriorArc(aBasis);

    // Data structures associated with the ordered partition.
    // Number k of components is not known a priori but bounded by n.
    TNode k = 0;

    // Leftmost / rightmost contact arc of each component
    TArc* cLeft  = new TArc[n]; // Directed towards the component
    TArc* cRight = new TArc[n]; // Directed away from the component

    // Right hand node of each graph node in its component
    TNode* vRight = new TNode[n];

    // Compute the shelling order. Exit if graph is not triconnected
    try
    {
        k = LMCOrderedPartition(cLeft,cRight,vRight);
    }
    catch (ERRejected)
    {
        delete[] cLeft;
        delete[] cRight;
        delete[] vRight;

        throw ERRejected();
    }

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(1);

    #endif


    // Step 1: Implicitly orient the arcs so that the arc routing will be planar

    LogEntry(LOG_METH,"Choose arc orientations...");

    TNode* layer = GetNodeColours();

    for (TArc a=0;a<m;a++)
    {
        TArc a0 = 2*a;
        TNode u = StartNode(a0);
        TNode v = EndNode(a0);

        if (vRight[u]==v)
        {
            // a must enter v horizontally
            orientation[a] = 1;
            continue;
        }
        else if (vRight[v]==u)
        {
            // a must enter v vertically
            orientation[a] = 0;
            continue;
        }

        if (layer[v]<layer[u])
        {
            a0 = a0^1;
            u = StartNode(a0);
            v = EndNode(a0);
        }

        if (cRight[layer[v]]==a0)
        {
            // a0 must enter v vertically
            orientation[a] = (a0^1)&1;
        }
        else if (cLeft[layer[v]]==a0)
        {
            // a0 must enter v horizontally
            orientation[a] = (a0^1)&1;
        }
        else
        {
            orientation[a] = (a0)&1;
        }
    }


    LogEntry(LOG_METH,"Node placement...");

    // The end nodes of the basis arc
    TNode v1 = EndNode(ExteriorArc());
    TNode v2 = StartNode(ExteriorArc());


    incrementalGeometry Geo(*this,n);

    // Embed the first two nodes v1,v2

    Geo.Init(v1);
    Geo.InsertColumnRightOf(v1,v2);
    Geo.InsertRowBelowOf(v1,v2);

    #if defined(_PROGRESS_)

    M.ProgressStep(2);

    #endif

    staticStack<TNode> S(n,CT);

    for (TNode l=1;l<k;++l)
    {
        // Sweep through this component and assign rows relative
        // to the nodes which have been placed in advance
        TNode vPrev = StartNode(cLeft[l]);
        TNode vNext = EndNode(cLeft[l]);
        TNode q = 0;

        while (vNext!=NoNode)
        {
            S.Insert(vNext);
            Geo.InsertRowBelowOf(vPrev,vNext);
            vPrev = vNext;
            vNext = vRight[vNext];
            q++;
        }

        // Sweep back and assign columns relative
        // to the nodes which have been placed in advance
        vPrev = EndNode(cRight[l]);

        while (!S.Empty())
        {
            vNext = S.Delete();
            Geo.InsertColumnLeftOf(vPrev,vNext);
            vPrev = vNext;
        }

        #if defined(_PROGRESS_)

        M.ProgressStep(q);

        #endif

        if (CT.traceLevel>2)
        {
            // Code to display intermediate results

            CT.SuppressLogging();

            Geo.AssignNumbers();

            sparseGraph G(*this,OPT_CLONE);
            G.Layout_ConvertModel(LAYOUT_KANDINSKI);

            for (TNode i=0;i<n;++i)
            {
                if (NodeColour(i)<=l)
                {
                    G.SetC(i,0,Geo.ColumnNumber(i));
                    G.SetC(i,1,Geo.RowNumber(i));
                }
                else
                {
                    G.SetC(i,0,0);
                    G.SetC(i,1,0);
                }
            }

            G.Layout_KandinskyScaleNodes(orientation);
            G.Layout_KandinskyRouteArcs(orientation);

            // Restrict display to the nodes which have already been embedded
            for (TNode i=0;i<n;i++)
            {
                if (NodeColour(i)>l) G.SetNodeVisibility(i,false);
            }

            CT.RestoreLogging();

            M.Trace(G,0);
        }
    }

    if (CT.traceLevel<=2) Geo.AssignNumbers();

    for (TNode i=0;i<n;i++)
    {
        SetC(i,0,Geo.ColumnNumber(i));
        SetC(i,1,Geo.RowNumber(i));
    }

    delete[] cLeft;
    delete[] cRight;
    delete[] vRight;
}


void abstractMixedGraph::Layout_KandinskyTree(TNode root,TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || !Representation())
        NoSparseRepresentation("Layout_KandinskyTree");

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_KandinskyTree","Coordinates are fixed");

    #endif

    LogEntry(LOG_METH,"Drawing guided by predecessor tree...");
    moduleGuard M(ModKandinsky,*this,moduleGuard::SHOW_TITLE);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(7*n,1);

    #endif

    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);

    TArc* pred = GetPredecessors();
    bool fixedPredecessors = false;

    if (pred)
    {
        LogEntry(LOG_METH,"Starting with existing predecessor tree...");

        root = NoNode;

        for (TNode v=0;v<n;++v)
        {
            #if defined(_PROGRESS_)

            M.ProgressStep();

            #endif

            if (pred[v]!=NoArc)
            {
                fixedPredecessors = true;
                continue;
            }

            if (root!=NoNode)
            {
                Error(ERR_REJECTED,
                    "Layout_KandinskyTree","Multiple root nodes");
            }

            root = v;
        }

        if (root==NoNode)
        {
            Error(ERR_REJECTED,"Layout_KandinskyTree","Missing root node");
        }
    }
    else
    {
        pred = InitPredecessors();

        if (root==NoNode) root = DefaultRootNode();
    }

    if (First(root)==NoArc)
    {
        Error(ERR_REJECTED,"Layout_KandinskyTree","Root node is isolated");
    }


    LogEntry(LOG_METH,"Checking for tree...");
    OpenFold();

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Expanded nodes: ");

    #endif

    TNode* ordered = new TNode[n];

    staticQueue<TNode> Q(n,CT);
    Q.Insert(root);

    TNode nVisited = 0;

    while (!(Q.Empty()))
    {
        TNode u = Q.Delete();
        ordered[nVisited] = u;
        ++nVisited;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(u));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        TArc a = First(u);

        do
        {
            // By construction, u is not an isolated node

            TNode v = EndNode(a);

            if (!fixedPredecessors && pred[v]==NoArc && v!=root) pred[v] = a;

            if (pred[v]==a) Q.Insert(v);

            a = Right(a,u);
        }
        while (a!=First(u));

        #if defined(_PROGRESS_)

        M.ProgressStep();

        #endif
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    CloseFold();

    if (nVisited<n)
    {
        M.Shutdown(LOG_RES,"...Predecessor arcs contain cycles");
        return;
    }


    long* rangeLeft = new long[n];
    long* rangeRight = new long[n];
    long* height = new long[n];
    char* orientation = new char[m];


    // Second pass: Bottom up determination of the rangeLeft[], rangeRight[] and height[] of subtrees
    LogEntry(LOG_METH,"Determine subtree heights and widths...");

    for (TNode i=n;i>0;)
    {
        --i;
        TNode u = ordered[i];
        TArc a = First(u);
        bool leaveNode = true;
        TNode fullRange = 0;

        do
        {
            TNode v = EndNode(a);

            if (pred[v]==a)
            {
                leaveNode = false;
                break;
            }

            a = Right(a,u);
        }
        while (a!=First(u));

        height[u] = rangeLeft[u] = rangeRight[u] = 0;

        #if defined(_PROGRESS_)

        M.ProgressStep();

        #endif

        if (leaveNode) continue;

        TNode v = EndNode(a);
        TArc aMedian = a;
        TArc descendants = 0;
        rangeLeft[u] = rangeLeft[v];

        do
        {
            if (height[u]<height[v]+1) height[u] = height[v]+1;

            fullRange += rangeLeft[v]+rangeRight[v]+1;
            orientation[a>>1] = a&1;

            do
            {
                a = Right(a,u);
                v = EndNode(a);
            }
            while (pred[v]!=a && a!=First(u));

            ++descendants;

            if (descendants>1 && (descendants&1))
            {
                rangeLeft[u] += rangeRight[EndNode(aMedian)]+1;

                do
                {
                    aMedian = Right(aMedian,u);
                }
                while (pred[EndNode(aMedian)]!=aMedian);

                rangeLeft[u] += rangeLeft[EndNode(aMedian)];
            }
        }
        while (a!=First(u));

        if (true)
        {
            // Align the subtree root u with its median descendant
            rangeRight[u] = fullRange-rangeLeft[u]-1;
        }
        else
        {
            // Align the subtree root u with its left-most descendant
            rangeLeft[u] = 0;
            rangeRight[u] = fullRange-1;
        }
    }


    // Final pass: Assign coordinates according to the width[] and height[]
    // computations as applied in the previous pass

    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
    Layout_ConvertModel(LAYOUT_KANDINSKI);

    LogEntry(LOG_METH,"Assigning coordinates...");

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    X -> SetC(root,0,rangeLeft[root]);
    X -> SetC(root,1,0);

    for (TNode i=0;i<n;++i)
    {
        TNode u = ordered[i];
        TArc a = First(u);
        bool leaveNode = true;
        long offset = 0;

        do
        {
            TNode v = EndNode(a);

            if (pred[v]==a)
            {
                leaveNode = false;
                break;
            }

            a = Right(a,u);
        }
        while (a!=First(u));

        #if defined(_PROGRESS_)

        M.ProgressStep();

        #endif

        if (leaveNode) continue;

        TNode v = EndNode(a);

        do
        {
            // Place v below of u and right-hand of the previously visited descendant of u
            X -> SetC(v,0,C(u,0)+offset+rangeLeft[v]-rangeLeft[u]);
            X -> SetC(v,1,C(u,1)+1);

            offset += rangeLeft[v]+rangeRight[v]+1;

            do
            {
                a = Right(a,u);
                v = EndNode(a);
            }
            while (pred[v]!=a && a!=First(u));
        }
        while (a!=First(u));
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Layout size is %ld x %ld",
            rangeLeft[root]+rangeRight[root]+1,height[root]);
        LogEntry(LOG_RES,CT.logBuffer);
    }

    delete[] rangeLeft;
    delete[] rangeRight;
    delete[] height;

    #if defined(_PROGRESS_)

    M.SetProgressNext(n);

    #endif

    // Step 2: Try to merge some lines
    Layout_KandinskyCompaction(orientation,true);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Step 3: Determine the effective node size and assign final node positions
    Layout_KandinskyScaleNodes(orientation);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Step 4: Grow the nodes and assign ports to every arc.
    //    Place the control points based on the orientation computed before:
    //    Every arc starts with a horizontal segment
    Layout_KandinskyRouteArcs(orientation);

    delete[] orientation;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, November 2006
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file layoutKandinsky.cpp
/// \brief Methods for drawing graphs in the Kandinsky layout model

#include "sparseGraph.h"
#include "staticStack.h"
#include "staticQueue.h"
#include "incrementalGeometry.h"
#include "moduleGuard.h"


void abstractMixedGraph::Layout_Kandinsky(TMethOrthogonal method,TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_Kandinsky","Coordinates are fixed");

    #endif


    LogEntry(LOG_METH,"Computing orthogonal drawing...");

    moduleGuard M(ModKandinsky,*this,moduleGuard::SHOW_TITLE);

    if (method==ORTHO_DEFAULT) method=TMethOrthogonal(CT.methOrthogonal);


    // This stores the virtual arc orientations
    char* orientation = new char[m];


    if (method==ORTHO_EXPLICIT)
    {
        // Orient the arcs so that the original edge routing of a 1-bent
        // Kandinsky drawing with node in general position is preserved

        LogEntry(LOG_METH,"Choose arc orientations...");

        for (TArc a=0;a<m;a++)
        {
            orientation[a] = 0;

            TNode x1 = PortNode(2*a);

            if (x1!=NoNode)
            {
                TNode x2 = ThreadSuccessor(x1);
                TNode x3 = NoNode;

                if (x2!=NoNode) x3 = ThreadSuccessor(x2);

                if (x3!=NoNode)
                {
                    TNode v = StartNode(2*a);
                    TFloat dx = C(v,0)-C(x2,0);
                    TFloat dy = C(v,1)-C(x2,1);

                    if (fabs(dx)<fabs(dy)) orientation[a] = 1;
                }
            }
        }
    }


    // Clean up the current drawing and set some default display parameters
    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);
    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
    Layout_ConvertModel(LAYOUT_KANDINSKI);


    LogEntry(LOG_METH,"Place nodes...");

    // Disturb the current positions to break the ties for nodes on common grid lines

    for (TArc a=0;a<m;a++)
    {
        TNode v = StartNode(2*a);
        TNode w = EndNode(2*a);
        TFloat dx = C(v,0)-C(w,0);
        TFloat dy = C(v,1)-C(w,1);
        TFloat signX = (dx>0) ? +1 : ( (dx<0) ? -1 : 0 );
        TFloat signY = (dx>0) ? +1 : ( (dy<0) ? -1 : 0 );

        SetC(w,0,C(w,0)+0.5*signX/m);
        SetC(v,0,C(v,0)-0.5*signX/m);
        SetC(w,1,C(w,1)+0.5*signY/m);
        SetC(v,1,C(v,1)-0.5*signY/m);
    }


    // Place nodes in general position based on the current order of X and Y coordinates

    goblinQueue<TNode,TFloat> *Q = NULL;

    if (nHeap!=NULL)
    {
        Q = nHeap;
        Q -> Init();
    }
    else Q = NewNodeHeap();

    for (TNode v=0;v<n;v++) Q->Insert(v,C(v,0));

    for (TNode i=0;i<n;i++)
    {
        TNode u = Q->Delete();
        SetC(u,0,i);
    }

    for (TNode v=0;v<n;v++) Q->Insert(v,C(v,1));

    for (TNode i=0;i<n;i++)
    {
        TNode u = Q->Delete();
        SetC(u,1,i);
    }

    if (nHeap==NULL) delete Q;


    if (method!=ORTHO_EXPLICIT)
    {
        // Orient the arcs so that for every node, the number of entering
        // arcs and the number of leaving arcs are (almost) equal.
        // The procedure basically computes an Eulerian cycle.

        LogEntry(LOG_METH,"Choose arc orientations...");

        for (TArc a=0;a<m;a++) orientation[a] = 2;

        THandle H = Investigate();
        investigator &I = Investigator(H);

        TNode u = 0;

        while (First(u)==NoArc) u++;

        TArc a = I.Read(u);

        for (TArc i=0;i<m;i++)
        {
            if (a&1) orientation[a>>1] = 1;
            else orientation[a>>1] = 0;

            u = EndNode(a);
            a = NoArc;

            // If the degree of u is odd, and all incident arcs are comsumed,
            // jump to another node from which the search can be continued
            while (i<m-1 && a==NoArc)
            {
                while (!I.Active(u)) u = (u+1)%n;

                a = I.Read(u);

                if (orientation[a>>1]!=2) a = NoArc;
            }

        }

        Close(H);
    }
    else
    {
        //
        Layout_KandinskyCrossingMinimization(orientation);
    }


    // Postoptimization of the node placement and arc orientations
    Layout_KandinskyCompaction(orientation,false);


    // Determine the effective node size CT.nodeSep and assign final node positions
    Layout_KandinskyScaleNodes(orientation);


    // Grow the nodes and assign ports to every arc. Place the control points based
    // on the orientation computed before: Every arc starts with a horizontal segment
    Layout_KandinskyRouteArcs(orientation);


    delete[] orientation;
}


abstractMixedGraph::TPortSide abstractMixedGraph::Layout_KandinskyPortSide(TArc a,const char* orientation)
    throw(ERRange)
{
    TNode v = StartNode(a);
    TNode w = EndNode(a);

    if (v==w)
    {
        if (a&1) return PORT_EAST;
        else return PORT_SOUTH;
    }

    TFloat dx = C(w,0)-C(v,0);
    TFloat dy = C(w,1)-C(v,1);

    if (fabs(dx)<0.5)
    {
        if (dy<0) return PORT_NORTH;
        else return PORT_SOUTH;
    }

    if (fabs(dy)<0.5)
    {
        if (dx<0) return PORT_WEST;
        else return PORT_EAST;
    }

    if ((orientation[a>>1]^(a&1))==0)
    {
        if (dx<0) return PORT_WEST;
        else return PORT_EAST;
    }
    else
    {
        if (dy<0) return PORT_NORTH;
        else return PORT_SOUTH;
    }
}


TArc abstractMixedGraph::Layout_KandinskyCrossingMinimization(char* orientation)
    throw()
{
    moduleGuard M(ModKandinsky,*this,"Reducing the number of edge crossings...");

    TArc a1 = 0;
    TArc lapsToGo = m;

    while (lapsToGo>0  && CT.SolverRunning())
    {
        TNode v1 = StartNode(2*a1);
        TNode w1 = EndNode(2*a1);
        TFloat v1x = C(v1,0);
        TFloat v1y = C(v1,1);
        TFloat w1x = C(w1,0);
        TFloat w1y = C(w1,1);
        TFloat max1x = (w1x<v1x) ? v1x : w1x;
        TFloat min1x = (w1x<v1x) ? w1x : v1x;
        TFloat max1y = (w1y<v1y) ? v1y : w1y;
        TFloat min1y = (w1y<v1y) ? w1y : v1y;
        long profit = 0;

        for (TArc a2=0;a2<m;a2++)
        {
            TNode v2 = StartNode(2*a2);
            TNode w2 = EndNode(2*a2);
            TFloat v2x = C(v2,0);
            TFloat v2y = C(v2,1);
            TFloat w2x = C(w2,0);
            TFloat w2y = C(w2,1);
            TFloat max2x = (w2x<v2x) ? v2x : w2x;
            TFloat min2x = (w2x<v2x) ? w2x : v2x;
            TFloat max2y = (w2y<v2y) ? v2y : w2y;
            TFloat min2y = (w2y<v2y) ? w2y : v2y;

            // The two possible routings of a1 and a2 form each a rectangle.
            // Inspect edge crossing properties in terms of these rectangles

            if (    min1x>=max2x || min2x>=max1x || min1y>=max2y || min2y>=max1y
                || (min1x>=min2x && max1x<=max2x && min1y>=min2y && max1y<=max2y)
                || (min1x<=min2x && max1x>=max2x && min1y<=min2y && max1y>=max2y)
                || (min1x>min2x && max1x<max2x && min1y<min2y && max1y>max2y)
                || (min1x<min2x && max1x>max2x && min1y>min2y && max1y<max2y)
               )
            {
                // Both rectangles are disjoint, nested or crossing each other.
                // That is, crossing does not depending on the a1 and a2 orientations
// cout << "(5) a1 = " << a1 << ", a2 = " << a2 << endl;
                continue;
            }

            // By now, only one of v1 and w1 can be enclosed by the a2 rectangle,
            // and only one of v2 and w2 can be enclosed by the a1 rectangle

            short sign1 = 0;
            short sign2 = 0;

            if (v1x>min2x && v1x<max2x && v1y>min2y && v1y<max2y)
            {
                // v1 is enclosed by the a2 rectangle but w1 is not
                sign1 = 1;
            }
            else if (w1x>min2x && w1x<max2x && w1y>min2y && w1y<max2y)
            {
                // w1 is enclosed by the a2 rectangle but v1 is not
                sign1 = -1;
            }

            if (v2x>min1x && v2x<max1x && v2y>min1y && v2y<max1y)
            {
                // v2 is enclosed by the a1 rectangle but w2 is not
                sign2 = 1;
            }
            else if (w2x>min1x && w2x<max1x && w2y>min1y && w2y<max1y)
            {
                // w2 is enclosed by the a1 rectangle but v2 is not
                sign2 = -1;
            }

            if (sign1!=0 && sign2==0)
            {
                // The number of edge crossings does not depend on the a1 orientation
// cout << "(4) a1 = " << a1 << ", a2 = " << a2 << endl;
                continue;
            }

            if (orientation[a1]) sign1 *= -1;
            if (orientation[a2]) sign2 *= -1;

            // By now, the sign1 and sign2 values encode the following:
            //   0: Neither the start nor the end node of an edge is enclosed by the other rectangle
            //   1: The virtual start node of an edge is enclosed by the other rectangle
            //  -1: The virtual end node of an edge is enclosed by the other rectangle

            if (sign1==0 && sign2==0)
            {
                // There is only one orientation of a1 and a2 which can produce crossings
                // (and, in that case, will cause double crossings)
                char conflictingOri1 =
                    (w1y<min2y || w1y>max2y || v1x<min2x || v1x>max2x) ?
                    orientation[a1]^1 : orientation[a1];
                char conflictingOri2 =
                    (w2y<min1y || w2y>max1y || v2x<min1x || v2x>max1x) ?
                    orientation[a2]^1 : orientation[a2];
                long nCross = (v1==v2 || v1==w2 || w1==v2 || w1==w2) ? 1 : 2;

                if (conflictingOri1 && conflictingOri2)
                {
                    profit += nCross;
// cout << "(1a) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                }
                else if (conflictingOri2)
                {
                    profit -= nCross;
// cout << "(1b) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                }
                else
                {
// cout << "(1c) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                }
            }
            else if (sign1==0 && sign2!=0)
            {
                TFloat bend1x = (orientation[a1]) ? w1x : v1x;
                TFloat bend1y = (orientation[a1]) ? v1y : w1y;
                TFloat bend2x = (orientation[a1]) ? v1x : w1x;
                TFloat bend2y = (orientation[a1]) ? w1y : v1y;

                if (bend1x>min2x && bend1x<max2x && bend1y>min2y && bend1y<max2y)
                {
                    profit -= 1;

// cout << "(2a) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                }
                else if (bend2x>min2x && bend2x<max2x && bend2y>min2y && bend2y<max2y)
                {
                    profit += 1;

// cout << "(2b) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                }
                else
                {
                    // The a2 rectangle is crossing exactly one side of the a1
                    // rectangle (twice). Determine this side
                    char min1yOri = (w1y>v1y) ? 0 : 1;
                    char max1yOri = min1yOri^1;
                    char min1xOri = (w1x<v1x) ? 0 : 1;
                    char max1xOri = min1xOri^1;

                    if (max2x>max1x)
                    {
                        profit += (max1xOri==orientation[a1]) ? +1 : -1;
// cout << "(2c) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                    }
                    else if (min2x<min1x)
                    {
                        profit += (min1xOri==orientation[a1]) ? +1 : -1;
// cout << "(2d) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                    }
                    else if (max2y>max1y)
                    {
                        profit += (max1yOri==orientation[a1]) ? +1 : -1;
// cout << "(2e) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                    }
                    else if (min2y<min1y)
                    {
                        profit += (min1yOri==orientation[a1]) ? +1 : -1;
// cout << "(2f) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
                    }
                    else
                    {

// cout << "(2g) a1 = " << a1 << ", a2 = " << a2 << endl;
                    }
                }
            }
            else
            {
                // For every orientation of a2, there is one a1 orientation
                // which results in an edge crossing
                profit -= sign1*sign2;
// cout << "(3) a1 = " << a1 << ", a2 = " << a2 << ",profit = " << profit << endl;
            }
        }

        --lapsToGo;

// cout << "profit = " << profit << endl;
        // Flipping the virtual orientation of a1 decreases the number of edge crossings
        if (profit>0)
        {
            orientation[a1] ^= 1;
            lapsToGo = m;
        }

        a1 = (a1+1)%m;
    }

    return 0;
}


bool abstractMixedGraph::Layout_KandinskyCompaction(char* orientation,bool planar)
    throw()
{
    if (int(CT.methOrthoRefine)<int(ORTHO_REFINE_SWEEP)) return false;

    moduleGuard M(ModKandinsky,*this,"Reducing sketch grid size...");

    goblinQueue<TNode,TFloat> *Q = NULL;

    if (nHeap!=NULL)
    {
        Q = nHeap;
        Q -> Init();
    }
    else Q = NewNodeHeap();

    TNode* line[2];
    line[0] = new TNode[n+1];
    line[1] = new TNode[n+1];

    unsigned short firstLine = 0;
    short lapsToGo = 2;
    bool improvedAtLeastOnce = false;
    TNode maxGridLine[2] = {0,0};

    for (TDim i=0;lapsToGo>0 && CT.SolverRunning();i^=1)
    {
        --lapsToGo;

        if (i==0) LogEntry(LOG_METH,"Sweeping horizontally for line merges...");
        else      LogEntry(LOG_METH,"Sweeping vertically for line merges...");

        OpenFold();

        TNode targetLine = 0;
        TNode sourceLine = 0;
        TNode fixedNodes = 0;
        bool improvedThisTime = false;

        while (fixedNodes<n)
        {
            #if defined(_LOGGING_)

            THandle LH = NoHandle;

            #endif

            fixedNodes = 0;

            for (TNode v=0;v<n;v++)
            {
                if (C(v,i)<sourceLine+0.5) ++fixedNodes;
               // else
                {
                    // cout << "C(" << v << ")  == " << C(v,i) << endl;
                }

                if (fabs(C(v,i)-sourceLine)<0.5)
                {
                    Q->Insert(v,C(v,i^1));

                    #if defined(_LOGGING_)

                    if (CT.logMeth>1)
                    {
                        if (LH==NoHandle)
                        {
                            LH = LogStart(LOG_METH2,"Nodes in line: ");
                        }

                        sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(v));
                        LogAppend(LH,CT.logBuffer);
                    }

                    #endif
                }
            }

            #if defined(_LOGGING_)

            if (CT.logMeth>1 && LH!=NoHandle) LogEnd(LH);

            #endif

            if (sourceLine==targetLine)
            {
                if (Q->Empty())
                {
                    ++targetLine;
                    ++sourceLine;
                    continue;
                }

                for (TNode k=0;;k++)
                {
                    if (Q->Empty())
                    {
                        line[firstLine][k] = NoNode;
                        break;
                    }

                    line[firstLine][k] = Q->Delete();
                }

                ++sourceLine;
                continue;
            }

            for (TNode k=0;k<n;k++)
            {
                if (Q->Empty())
                {
                    line[firstLine^1][k] = NoNode;
                    break;
                }

                line[firstLine^1][k] = Q->Delete();
            }

            if (line[firstLine^1][0]==NoNode)
            {
                ++sourceLine;
                continue;
            }

            unsigned short lineAhead = firstLine;

            if (C(line[lineAhead^1][0],i^1)>C(line[lineAhead][0],i^1))
            {
                lineAhead = firstLine^1;
            }

// Deactivated buggy code:
/*
            if (Layout_KandinskyRefineRouting(orientation,i,line,firstLine))
            {
                improvedThisTime = true;

                #if defined(_LOGGING_)

                LogEntry(LOG_METH2,"...Arc orientations have been changed");

                #endif
            }
*/
            if (Layout_KandinskyShiftChain(orientation,i,line,firstLine,true))
            {
                // This results in a rescan of the target line:
                sourceLine = targetLine;

                #if defined(_LOGGING_)

                LogEntry(LOG_METH2,"...Lines have been merged");

                #endif
            }
            else if (Layout_KandinskyShiftChain(orientation,i,line,firstLine,false))
            {
                // This results in a rescan of the target line:
                sourceLine = targetLine;

                improvedThisTime = true;

                #if defined(_LOGGING_)

                LogEntry(LOG_METH2,"...A chain of collinear edge segments has been shifted");

                #endif
            }
            else
            {
                if (sourceLine>targetLine+1)
                {
                    for (TNode k=0;line[firstLine^1][k]!=NoNode;k++)
                    {
                        SetC(line[firstLine^1][k],i,targetLine+1);
                    }

                    improvedThisTime = true;

                    #if defined(_LOGGING_)

                    LogEntry(LOG_METH2,"...Line has been moved");

                    #endif
                }

                // Rotate the line buffers and skip the current target line
                firstLine ^= 1;
                targetLine = sourceLine;
                ++sourceLine;
            }
        }

        if (planar && Layout_KandinskyShiftNodes(orientation))
        {
            improvedThisTime = true;

            #if defined(_LOGGING_)

            LogEntry(LOG_METH2,"...Nodes have been shifted");

            #endif
        }

        CloseFold();

        maxGridLine[i] = TNode(C(line[firstLine][0],i)+0.5);
        improvedAtLeastOnce |= improvedThisTime;

        if (improvedThisTime)
        {
            lapsToGo = 2;

            #if defined(_PROGRESS_)

            if (maxGridLine[0]*maxGridLine[1]>0)
            {
                // In the very first iteration, maxGridLine[1] is still zero
                M.SetProgressCounter(1.0 - ((maxGridLine[0]+1.0)*(maxGridLine[1]+1.0)-n) / (n*n));
            }

            #endif

            if (CT.traceLevel>2)
            {
                sparseGraph G(*this,OPT_CLONE);
                G.Layout_ConvertModel(LAYOUT_KANDINSKI);
                G.Layout_KandinskyScaleNodes(orientation);
                G.Layout_KandinskyRouteArcs(orientation);
                G.Layout_DefaultBoundingBox();
                M.Trace(G,0);
            }
        }
    }

    if (nHeap==NULL) delete Q;

    delete[] line[0];
    delete[] line[1];

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Final coarse grid size is (%lu,%lu)",
            static_cast<unsigned long>(maxGridLine[0]),
            static_cast<unsigned long>(maxGridLine[1]));
        M.Shutdown(LOG_RES,CT.logBuffer);
    }

    return improvedAtLeastOnce;
}


bool abstractMixedGraph::Layout_KandinskyShiftNodes(char* orientation)
    throw()
{
    bool improved = false;
    goblinQueue<TArc,TFloat> *R = NewArcHeap();

    for (TNode v=0;v<n;v++)
    {
        TArc a = First(v);

        if (a==NoArc) continue;

        TNode maxNode[4]   = {NoNode,NoNode,NoNode,NoNode};
        TNode maxArc[4]    = {NoArc,NoArc,NoArc,NoArc};
        TNode unbentArc[4] = {NoArc,NoArc,NoArc,NoArc};
        short sign[4]      = {-1,+1,+1,-1};
        short coord[4]     = {1,0,1,0};

        do
        {
            TNode u = EndNode(a);
            TPortSide side = Layout_KandinskyPortSide(a,orientation);

            if (fabs(C(u,0)-C(v,0))<0.5 || fabs(C(u,1)-C(v,1))<0.5)
            {
                // Unbent arcs do not prevent from shifting in a perpendicular direction.
                // Memorize such arcs which must be bent after shifting v
                unbentArc[side]  = a;
            }
            else if (   maxNode[side]==NoNode
                     || sign[side]*C(u,coord[side])<sign[side]*C(maxNode[side],coord[side])
                    )
            {
                maxArc[side]  = a;
                maxNode[side] = u;
            }

            a = Right(a,v);
        }
        while (a!=First(v));

        short freeCoord = 2;

        if (maxArc[PORT_NORTH]==NoArc && maxArc[PORT_SOUTH]==NoArc)
        {
            freeCoord = 0;
        }
        else if (maxArc[PORT_WEST]==NoArc && maxArc[PORT_EAST]==NoArc)
        {
            freeCoord = 1;
        }
        else
        {
            continue;
        }

        // Reaching this means that v can be shifted basically.Now determine
        // the new position targetPos. This means hifting v to an adjacent bend
        // node. It takes some care not to introduce node-edge crossings

        a = First(v);

        do
        {
            TNode u = EndNode(a);
            R -> Insert(a>>1,C(u,freeCoord));
            a = Right(a,v);
        }
        while (a!=First(v));

        TArc medianIndex = R->Cardinality()/2;
        TFloat minRange = InfFloat;
        TFloat maxRange = -InfFloat;
        TFloat prevPos = -InfFloat;
        TFloat medianPos = -InfFloat;
        TFloat currentPos = C(v,freeCoord);
        TFloat targetPos = currentPos;

        for (TArc i=0;!R->Empty();++i)
        {
            TArc a = R->Delete();
            TNode u = EndNode(2*a);
            if (u==v) u = StartNode(2*a);
            TFloat thisPos = C(u,freeCoord);

            if (i==0)
            {
                minRange = prevPos = maxRange = thisPos;
            }
            else if (thisPos>maxRange)
            {
                prevPos = maxRange;
                maxRange = thisPos;
            }

            if (i==medianIndex) medianPos = thisPos;
        }

        if (medianPos==maxRange || medianPos==minRange) medianPos = prevPos;

        if (currentPos>=minRange && currentPos<=maxRange)
        {
            if (medianPos>minRange && medianPos<maxRange) targetPos = medianPos;
        }
        else if (currentPos<minRange)
        {
            if (medianPos<maxRange)
            {
                targetPos = medianPos;
            }
            else if (minRange<maxRange)
            {
                targetPos = minRange;
            }
        }
        else // if (currentPos>maxRange)
        {
            if (medianPos>minRange)
            {
                targetPos = medianPos;
            }
            else if (minRange<maxRange)
            {
                targetPos = maxRange;
            }
        }

        if (currentPos!=targetPos)
        {
            SetC(v,freeCoord,targetPos);
            improved = true;

            // Assign virtual orientations to arcs which become bent by this
            // node movement. The port side must change for those arcs!
            if (unbentArc[0^freeCoord]!=NoArc)
            {
                orientation[unbentArc[0^freeCoord]>>1] = (unbentArc[0^freeCoord]&1)^freeCoord;
            }

            if (unbentArc[2^freeCoord]!=NoArc)
            {
                orientation[unbentArc[2^freeCoord]>>1] = (unbentArc[2^freeCoord]&1)^freeCoord;
            }
        }
    }

    delete R;

    return improved;
}


bool abstractMixedGraph::Layout_KandinskySeparableNodes(const char* orientation,TDim i,TNode v,TNode w)
    throw()
{
    if (v==NoNode || w==NoNode) return true;

    TArc nAdj = 0; // The number of arcs connecting v and w
    TArc vAdj = 0; // The number of arcs connecting v with other nodes
    TArc wAdj = 0; // The number of arcs connecting w with other nodes

    TNode minNeighbor = v;
    TArc a = First(v);

    do
    {
        if (a==NoArc) break;

        TNode u = EndNode(a);
        TPortSide side = Layout_KandinskyPortSide(a,orientation);

        if (   (side==PORT_NORTH && i==0)
            || (side==PORT_WEST  && i==1)
            )
        {
            if (u==w)
            {
                ++nAdj;
            }
            else
            {
                ++vAdj;
            }

            if (   C(u,i^1)<C(minNeighbor,i^1)
                || (u!=w && C(u,i^1)<C(minNeighbor,i^1)+0.5)
               )
            {
                minNeighbor = u;
            }
        }

        a = Right(a,v);
    }
    while (a!=First(v));

    TNode maxNeighbor = w;
    a = First(w);

    do
    {
        if (a==NoArc) break;

        TNode u = EndNode(a);
        TPortSide side = Layout_KandinskyPortSide(a,orientation);

        if (   (side==PORT_SOUTH && i==0)
            || (side==PORT_EAST  && i==1)
            )
        {
            if (u==v)
            {
                ++nAdj;
            }
            else
            {
                ++wAdj;
            }

            if (   C(u,i^1)>C(maxNeighbor,i^1)+0.5
                || (u!=v && C(u,i^1)>C(maxNeighbor,i^1)-0.5)
               )
            {
                maxNeighbor = u;
            }
        }

        a = Right(a,w);
    }
    while (a!=First(w));


    if (C(maxNeighbor,i^1)>C(minNeighbor,i^1)+0.5)
    {
        // Merging the lines possibly introduces edge crossing
        return false;
    }

    if (   C(maxNeighbor,i^1)>C(minNeighbor,i^1)-0.5
        && maxNeighbor!=v
        && minNeighbor!=w
        && maxNeighbor!=minNeighbor
       )
    {
        // Merging the lines possibly introduces edge/edge or even node/edge crossings
        return false;
    }

    if (nAdj>1 && vAdj>0 && wAdj>0)
    {
        // This is only to avoid complications when assigning ports
        // to the arcs connecting v and w. The case (nAdj>0 && vAdj+wAdj>0)
        // requires some care for the routing of unbent edges.
        return false;
    }

    return true;
}


bool abstractMixedGraph::Layout_KandinskyShiftChain(const char* orientation,TDim i,TNode** line,TNode firstLine,bool mergeWholeLine)
    throw()
{
    TArc* pred = GetPredecessors();

    bool wholeLineMergable = true;

    TNode indexInLine[2] = {0,0};
    unsigned short lineAhead = firstLine;

    if (C(line[lineAhead^1][0],i^1)>C(line[lineAhead][0],i^1))
    {
        lineAhead = firstLine^1;
    }

    TFloat linePos[2];
    linePos[0] = C(line[0][0],i);
    linePos[1] = C(line[1][0],i);

    // In what follows, chains of line segments in the two grid lines are
    // considered. It is checked if one of these chains can be shifted to
    // the other grid line. For the latter purpose, the index of the first
    // chain (bend) node in the line[] buffer is maintained.
    TNode firstOfChain[2] = {NoNode,NoNode};

    // This tells how the total edge length changes if the current chain
    // is shifted to the other grid line.
    int profit[2] = {0,0};

    while (line[lineAhead^1][indexInLine[lineAhead^1]]!=NoNode)
    {
        // Verify that the arcs pointing back from line[indexInLine[lineAhead]]
        // and the arcs pointing forward from line[indexInLine[lineAhead^1]]
        // do not have any orthogonal grid line in common

        TNode v = line[lineAhead][indexInLine[lineAhead]];
        TNode w = line[lineAhead^1][indexInLine[lineAhead^1]];
        TNode x = line[lineAhead^1][indexInLine[lineAhead^1]+1];
        TNode y = NoNode;
        if (indexInLine[lineAhead^1]>0) y = line[lineAhead^1][indexInLine[lineAhead^1]-1];
        TNode z = NoNode;
        if (indexInLine[lineAhead]>0) z = line[lineAhead][indexInLine[lineAhead]-1];

        // The shifting direction
        TFloat sign = (lineAhead==firstLine) ? -1 : 1;

        TArc xAdj = 0; // The number of arcs connecting w and x (including the case x==NoNode)
        TArc yAdj = 0; // The number of arcs connecting w and y (including the case y==NoNode)

        TArc a = First(w);
        int thisProfit = 0;

        do
        {
            if (a==NoArc) break;

            TNode u = EndNode(a);

            if (u==x) ++xAdj;
            if (u==y) ++yAdj;

            if (sign*(C(u,i)-C(w,i))>0.5)
            {
                thisProfit++;

                if (fabs(C(u,i)-linePos[lineAhead])<0.5)
                {
                    // Shifting w to the grid line of u saves a control point
                    ++thisProfit;
                }

                if (pred && pred[w]==(a^1))
                {
                    // Shifting towards a fixed root node might decrease the
                    // total edge length in a later step
                    ++thisProfit;
                }

                if (pred && pred[u]==a)
                {
                    // Avoid to shift parental nodes towards the descendants
                    --thisProfit;
                }
            }
            else if (sign*(C(u,i)-C(w,i))<-0.5)
            {
                --thisProfit;
            }

            a = Right(a,w);
        }
        while (a!=First(w));


        // When shifting chains of nodes rather than full grid lines,
        // keep track of the current chain and if shifting is profitable
        if (yAdj==0 && firstOfChain[lineAhead^1]==NoNode)
        {
            firstOfChain[lineAhead^1] = indexInLine[lineAhead^1];
            profit[lineAhead^1] = 0;
        }

        profit[lineAhead^1] += thisProfit;

        bool mergable = Layout_KandinskySeparableNodes(orientation,i,v,w);

        if (mergable) mergable = Layout_KandinskySeparableNodes(orientation,i,w,z);

        if (mergeWholeLine)
        {
            wholeLineMergable &= mergable;

            if (!wholeLineMergable) return false;
        }
        else if (!mergable)
        {
            // No way to shift the current chain (if there is one)
            firstOfChain[lineAhead] = NoNode;
            firstOfChain[lineAhead^1] = NoNode;
        }
        else if (xAdj==0 && firstOfChain[lineAhead^1]!=NoNode)
        {
            // A chain has been completed and shifting is possible

            if (profit[lineAhead^1]>0)
            {
                // Shift the current chain to the other grid line since this will
                // reduce the total edge length and, possibly, the number of control points
                for (TNode k=firstOfChain[lineAhead^1];k<=indexInLine[lineAhead^1];k++)
                {
                    SetC(line[lineAhead^1][k],i,linePos[lineAhead]);
                }

                return true;
            }

            // Invalidate this chain
            firstOfChain[lineAhead^1] = NoNode;
        }

        ++indexInLine[lineAhead^1];

        if (   line[lineAhead^1][indexInLine[lineAhead^1]]==NoNode
            || line[lineAhead][indexInLine[lineAhead]]!=NoNode
                && (  C(line[lineAhead^1][indexInLine[lineAhead^1]],i^1)
                        > C(line[lineAhead][indexInLine[lineAhead]],i^1) )
            )
        {
            lineAhead ^= 1;
        }
    }

    if (mergeWholeLine)
    {
        for (TNode k=0;line[firstLine^1][k]!=NoNode;k++)
        {
            SetC(line[firstLine^1][k],i,linePos[firstLine]);
        }

        return true;
    }

    return false;
}


bool abstractMixedGraph::Layout_KandinskyRefineRouting(char* orientation,TDim i,TNode** line,TNode firstLine)
    throw()
{
    bool improved = false;
    TNode indexInLine[2] = {0,0};
    unsigned short lineAhead = firstLine;

    TFloat eps = CT.epsilon;

    while (line[lineAhead][indexInLine[lineAhead]]!=NoNode)
    {
        TNode w = line[lineAhead^1][indexInLine[lineAhead^1]];
        TNode v = line[lineAhead][indexInLine[lineAhead]];
        TNode x = line[lineAhead^1][indexInLine[lineAhead^1]+1];

        if (C(w,i^1)>C(v,i^1)+eps)
        {
            lineAhead ^= 1;
            continue;
        }

        while (x!=NoNode && C(x,i^1)<C(v,i^1)+eps)
        {
            ++indexInLine[lineAhead^1];
            w = x;
            x = line[lineAhead^1][indexInLine[lineAhead^1]+1];
        }

        if (C(w,i^1)>C(v,i^1)-eps)
        {
            ++indexInLine[lineAhead^1];
            ++indexInLine[lineAhead];
            continue;
        }

        // Now, C(x,i^1) > C(v,i^1) > C(w,i^1) and the potential arc a connecting v and
        // w can be flipped without introducing or deleting node-edge crossings.

        // Unfortunately, this does not exclude edge-edge crossings of a and arcs incident
        // with x and with arcs incident with the prededecessor of v

        // Next determine this arc a and if flipping a reduces the port side imbalances

        TPortSide sideForward = (i==0) ? PORT_SOUTH : PORT_EAST;
        TPortSide sideBackward = TPortSide(sideForward^2);
        TPortSide sideToLineBack = (C(w,i)-C(v,i)>0)
            ? ( (i==0) ? PORT_EAST : PORT_SOUTH )
            : ( (i==0) ? PORT_WEST : PORT_NORTH );
        TPortSide sideToLineAhead = TPortSide(sideToLineBack^2);

        TArc attachedArcs[4] = {0,0,0,0};
        TArc foundUnbent[4] = {false,false,false,false};
        TFloat maxPos = -InfFloat;
        TFloat minPos = InfFloat;

        TArc a = First(v);
        TArc aConnect = NoArc;

        do
        {
            if (a==NoArc) break;

            TNode u = EndNode(a);

            if (u==w) aConnect = a;

            TPortSide side = Layout_KandinskyPortSide(a,orientation);

            if (side==sideBackward || side==sideToLineBack)
            {
                attachedArcs[side]++;

                if (fabs(C(u,0)-C(v,0))<eps || fabs(C(u,1)-C(v,1))<eps) foundUnbent[side] = true;

                if (   side==sideBackward
                    && C(u,i^1)>maxPos
                    && (C(u,i)-C(v,i))*(C(w,i)-C(v,i))>0
                   )
                {
                    maxPos = C(u,i^1);
                }
            }

            a = Right(a,v);
        }
        while (a!=First(v));

        if (aConnect!=NoArc)
        {
            a = First(w);

            do
            {
                TPortSide side = Layout_KandinskyPortSide(a,orientation);

                if (side==sideForward || side==sideToLineAhead)
                {
                    attachedArcs[side]++;
                    TNode u = EndNode(a);

                    if (fabs(C(u,0)-C(w,0))<0.5 || fabs(C(u,1)-C(w,1))<0.5) foundUnbent[side] = true;

                    if (   side==sideForward
                        && C(u,i^1)<minPos
                        && (C(u,i)-C(w,i))*(C(v,i)-C(w,i))>0
                       )
                    {
                        minPos = C(u,i^1);
                    }
                }

                a = Right(a,w);
            }
            while (a!=First(w));

            TPortSide currentSideV = Layout_KandinskyPortSide(aConnect,orientation);
            TPortSide currentSideW = (currentSideV==sideBackward) ? sideToLineAhead : sideForward;
            TPortSide alternativeSideV = (currentSideV==sideBackward) ? sideToLineBack : sideBackward;
            TPortSide alternativeSideW = (currentSideV==sideBackward) ? sideForward : sideToLineAhead;

            long currentMax = (attachedArcs[currentSideV]>attachedArcs[currentSideW])
                ? attachedArcs[currentSideV] : attachedArcs[currentSideW];
            long alternativeMax = (attachedArcs[alternativeSideV]>attachedArcs[alternativeSideW])
                ? attachedArcs[alternativeSideV] : attachedArcs[alternativeSideW];


            if (   (currentSideV==sideBackward   && C(w,i^1)==maxPos)
                || (currentSideV==sideToLineBack && C(v,i^1)==minPos)
                )
            {
                if (   (!foundUnbent[alternativeSideV] || attachedArcs[alternativeSideV]!=1)
                    && (!foundUnbent[alternativeSideW] || attachedArcs[alternativeSideW]!=1)
                    && currentMax>alternativeMax+1
                   )
                {
                    orientation[aConnect>>1] ^= 1;
                    improved = true;
                }
            }
        }


        ++indexInLine[lineAhead^1];
        lineAhead ^= 1;
    }

    return improved;
}


TArc abstractMixedGraph::Layout_KandinskyScaleNodes(const char* orientation)
    throw()
{
    LogEntry(LOG_METH,"Determine node size...");

    TArc nodeSize = 1;

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=0;v<n;v++)
    {
        TArc nPorts[4] = {0,0,0,0};

        while (I.Active(v))
        {
            TArc a = I.Read(v);
            TPortSide side = Layout_KandinskyPortSide(a,orientation);
            nPorts[side]++;
        }

        if (nPorts[PORT_NORTH]>nodeSize) nodeSize = nPorts[PORT_NORTH];
        if (nPorts[PORT_EAST]>nodeSize)  nodeSize = nPorts[PORT_EAST];
        if (nPorts[PORT_SOUTH]>nodeSize) nodeSize = nPorts[PORT_SOUTH];
        if (nPorts[PORT_WEST]>nodeSize)  nodeSize = nPorts[PORT_WEST];
    }

    Close(H);

    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);
    TFloat bendSpacing = 0.5*nodeSpacing/(nodeSize+1);
    SetLayoutParameter(TokLayoutBendSpacing,bendSpacing,LAYOUT_KANDINSKI);
    SetLayoutParameter(TokLayoutNodeSize,500.0*nodeSize,LAYOUT_KANDINSKI);

    for (TNode v=0;v<n;v++)
    {
        SetC(v,0,C(v,0)*nodeSpacing);
        SetC(v,1,C(v,1)*nodeSpacing);
    }

    Layout_DefaultBoundingBox();

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...At most %lu ports are attached to each side",
            static_cast<unsigned long>(nodeSize));
        LogEntry(LOG_RES,CT.logBuffer);
    }

    return nodeSize;
}


void abstractMixedGraph::Layout_KandinskyRouteArcs(const char* orientation)
    throw()
{
    LogEntry(LOG_METH,"Draw edges...");

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    // Allocate 3 control points for every arc (4 control points in the case of loops)

    TNode* port1 = new TNode[m];
    TNode* port2 = new TNode[m];
    TNode* bend = new TNode[m];
    TNode* align = new TNode[m];
    TNode* nPorts = new TNode[4*n];

    TFloat bendSpacing = 0.0;
    GetLayoutParameter(TokLayoutBendSpacing,bendSpacing);
    TFloat nodeSpacing = 0.0;
    GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);

    TNode controlPoint[4];

    for (TArc a=0;a<m;a++)
    {
        align[a] = X->ProvideArcLabelAnchor(2*a);

        TNode nControlPoints = (StartNode(2*a)==EndNode(2*a)) ? 4 : 3;
        X -> ProvideEdgeControlPoints(a,controlPoint,nControlPoints,PORTS_EXPLICIT);
        port1[a] = controlPoint[0];
        bend[a]  = controlPoint[nControlPoints-2];
        port2[a] = controlPoint[nControlPoints-1];
    }

    goblinQueue<TArc,TFloat> *R = NewArcHeap();

    THandle H = Investigate();
    investigator &I = Investigator(H);

    for (TNode v=0;v<n;v++)
    {
        for (char i=0;i<4;i++)
        {
            TPortSide side = TPortSide(i);
            TFloat sgnx = (side==PORT_EAST)  ? (+1) : ((side==PORT_WEST)  ? (-1) : 0);
            TFloat sgny = (side==PORT_SOUTH) ? (+1) : ((side==PORT_NORTH) ? (-1) : 0);

            I.Reset(v);

            while (I.Active(v))
            {
                TArc a = I.Read(v);
                TNode w = EndNode(a);
                TFloat key = 0;
                TFloat dx = C(w,0)-C(v,0);
                TFloat dy = C(w,1)-C(v,1);

                if (v==w)
                {
                    if (i==char((a>>1)%4) && a%2==0)
                    {
                        // Add loops twice, just to obtain the
                        // correct number of ports
                        R -> Insert(a>>1,TFloat(n*nodeSpacing+a));
                        R -> Insert(a>>1,TFloat(n*nodeSpacing+a));
                    }

                    continue;
                }

                if (side!=Layout_KandinskyPortSide(a,orientation)) continue;

                if (side==PORT_NORTH || side==PORT_SOUTH)
                {
                    // Vertical ports
                    if (dx<0)
                    {
                        // By assigning negative keys, nodes in the left of v
                        // preceed the right hand nodes in the output of R
                        key = sgny*dy-TFloat(n*nodeSpacing);
                    }
                    else
                    {
                        key = TFloat(n*nodeSpacing)-sgny*dy;
                    }

                    // To prevent crossings of parallel arcs, break ties for
                    // equal keys according to the arc indices
                    if (fabs(dx)<bendSpacing/2 || side==PORT_NORTH)
                    {
                        key += 0.1*a/m;
                    }
                    else
                    {
                        key -= 0.1*a/m;
                    }
                }
                else
                {
                    // Horizontal ports
                    if (dy<0)
                    {
                        // By assigning negative keys, nodes in top of v
                        // preceed the nodes below in the output of R
                        key = sgnx*dx-TFloat(n*nodeSpacing);
                    }
                    else
                    {
                        key = TFloat(n*nodeSpacing)-sgnx*dx;
                    }

                    // To prevent crossings of parallel arcs, break ties for
                    // equal keys according to the arc indices
                    if (fabs(dy)<bendSpacing/2 || side==PORT_WEST)
                    {
                        key += 0.1*a/m;
                    }
                    else
                    {
                        key -= 0.1*a/m;
                    }
                }

                R -> Insert(a>>1,key);
            }

            nPorts[4*v+side] = R->Cardinality();
            TFloat offset = nodeSpacing/2-1*bendSpacing;
            TFloat shift = -bendSpacing*(nPorts[4*v+side]-1.0);

            while (!R->Empty())
            {
                TArc a = R->Delete();

                TNode w = EndNode(2*a);
                TNode x = port1[a];
                TNode y = bend[a];
                TNode z = align[a];

                if (StartNode(2*a)==w)
                {
                    // Delete the 2nd occurence of the loop a on R
                    R->Delete();

                    TNode u = port2[a];

                    if (side==PORT_NORTH || side==PORT_SOUTH)
                    {
                        // Vertical ports
                        X -> SetC(x,0,C(v,0)+shift);
                        X -> SetC(x,1,C(v,1)+sgny*offset);
                        X -> SetC(u,0,C(v,0)+shift+2*bendSpacing);
                        X -> SetC(u,1,C(v,1)+sgny*offset);
                    }
                    else
                    {
                        // Horizontal ports
                        X -> SetC(x,0,C(v,0)+sgnx*offset);
                        X -> SetC(x,1,C(v,1)+shift);
                        X -> SetC(u,0,C(v,0)+sgnx*offset);
                        X -> SetC(u,1,C(v,1)+shift+2*bendSpacing);
                    }

                    X -> SetC(y,0,C(u,0)+2*sgnx*bendSpacing);
                    X -> SetC(y,1,C(u,1)+2*sgny*bendSpacing);

                    u = ThreadSuccessor(x);

                    X -> SetC(u,0,C(x,0)+2*sgnx*bendSpacing);
                    X -> SetC(u,1,C(x,1)+2*sgny*bendSpacing);

                    shift += 4*bendSpacing;

                    X -> SetC(z,0,C(x,0)/2+C(y,0)/2);
                    X -> SetC(z,1,C(x,1)/2+C(y,1)/2);
                }
                else
                {
                    // Arc a is not a loop

                    if (w==v)
                    {
                        // Revert arc implicitly
                        w = StartNode(2*a);
                        x = port2[a];
                    }

                    TFloat dx = C(w,0)-C(v,0);
                    TFloat dy = C(w,1)-C(v,1);
                    TFloat thisSgnX = (dx<-0.01) ? -1 : 1;
                    TFloat thisSgnY = (dy< 0.01) ? -1 : 1;

                    if (side==PORT_NORTH || side==PORT_SOUTH)
                    {
                        // Vertical ports
                        X -> SetC(x,1,C(v,1)+sgny*offset);
                        X -> SetC(x,0,C(v,0)+shift);
                        X -> SetC(y,0,C(x,0));
                        X -> SetC(z,0,C(x,0)+thisSgnX*2*bendSpacing);

                        if (fabs(dx)<0.01)
                        {
                            X -> SetC(y,1,(C(v,1)+C(w,1))/2);
                            X -> SetC(z,1,(C(v,1)+C(w,1))/2);
                        }
                    }
                    else
                    {
                        // Horizontal ports
                        X -> SetC(x,0,C(v,0)+sgnx*offset);
                        X -> SetC(x,1,C(v,1)+shift);
                        X -> SetC(y,1,C(x,1));
                        X -> SetC(z,1,C(x,1)+thisSgnY*2*bendSpacing);

                        if (fabs(dy)<0.01)
                        {
                            X -> SetC(y,0,(C(v,0)+C(w,0))/2);
                            X -> SetC(z,0,(C(v,0)+C(w,0))/2);
                        }
                    }

                    shift += 2*bendSpacing;
                }
            }
        }
    }

    Close(H);

    delete R;

    // Align the ports of unbent edges for those end nodes with only one edge
    // routed at this side (the given node positions must be appropriate)
    for (TArc a=0;a<m;a++)
    {
        TPortSide side1 = Layout_KandinskyPortSide(2*a,orientation);
        TPortSide side2 = Layout_KandinskyPortSide(2*a+1,orientation);

        // Sort out all bent edges
        if (side1!=(side2^2)) continue;

        TNode v1 = StartNode(2*a);
        TNode v2 = EndNode(2*a);

        if (nPorts[4*v1+side1]==1)
        {
            if (side1==PORT_NORTH || side1==PORT_SOUTH)
            {
                // Vertical ports
                X -> SetC(port1[a],0,C(port2[a],0));
                X -> SetC(bend[a],0,C(port2[a],0));

                TFloat thisSgnX = (C(port2[a],0)<C(v1,0)) ? -1 : 1;
                X -> SetC(align[a],0,C(bend[a],0)+thisSgnX*2*bendSpacing);
            }
            else
            {
                // Horizontal ports
                X -> SetC(port1[a],1,C(port2[a],1));
                X -> SetC(bend[a],1,C(port2[a],1));

                TFloat thisSgnY = (C(port2[a],1)<C(v1,1)) ? -1 : 1;
                X -> SetC(align[a],1,C(bend[a],1)+thisSgnY*2*bendSpacing);
            }
        }
        else if (nPorts[4*v2+side2]==1)
        {
            if (side2==PORT_NORTH || side2==PORT_SOUTH)
            {
                // Vertical ports
                X -> SetC(port2[a],0,C(port1[a],0));
                X -> SetC(bend[a],0,C(port1[a],0));

                TFloat thisSgnX = (C(port1[a],0)<C(v1,0)) ? -1 : 1;
                X -> SetC(align[a],0,C(bend[a],0)+thisSgnX*2*bendSpacing);
            }
            else
            {
                // Horizontal ports
                X -> SetC(port2[a],1,C(port1[a],1));
                X -> SetC(bend[a],1,C(port1[a],1));

                TFloat thisSgnY = (C(port1[a],1)<C(v1,1)) ? -1 : 1;
                X -> SetC(align[a],1,C(bend[a],1)+thisSgnY*2*bendSpacing);
            }
        }
    }

    delete[] port1;
    delete[] port2;
    delete[] bend;
    delete[] align;
    delete[] nPorts;

    X -> ReleaseCoveredEdgeControlPoints(PORTS_EXPLICIT);
}


void abstractMixedGraph::Layout_StaircaseDrawing(TArc aBasis,TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (aBasis>=2*m && aBasis!=NoArc)
        NoSuchArc("Layout_StaircaseDrawing",aBasis);

    #endif

    moduleGuard M(ModStaircase,*this,"Embedding the graph nodes...");

    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);
    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
    Layout_ConvertModel(LAYOUT_KANDINSKI);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(9,1);

    #endif

    GrowExteriorFace();

    sparseGraph G(n,CT);
    G.ImportLayoutData(*this);
    sparseRepresentation* GR = static_cast<sparseRepresentation*>(G.Representation());
    GR -> SetCapacity(n,m);

    // Restrict to a simple graph. For the deleted non-loop arcs,
    // memorize the maintained parallel arcs by representant[].
    TArc* adjacent = new TArc[n];
    TArc* representant = new TArc[m];
    TArc* mapArc = new TArc[m];

    for (TNode w=0;w<n;w++) adjacent[w] = NoArc;
    for (TArc a=0;a<m;a++) representant[a] = mapArc[a] = NoArc;

    // Generate the reduced incidence structure of G and all mappings
    for (TNode u=0;u<n;u++)
    {
        TArc a = First(u);

        if (a==NoArc) continue;

        do
        {
            TNode v = EndNode(a);

            if (u<v)
            {
                if (adjacent[v]!=NoArc && StartNode(adjacent[v])==u)
                {
                    representant[a>>1] = adjacent[v] ^ (a&1);
                }
                else
                {
                    TArc aG = G.InsertArc(u,v);
                    mapArc[a>>1] = 2*aG ^ (a&1);

                    adjacent[v] = a;
                }
            }

            a = Right(a,u);
        }
        while (a!=First(u));
    }

    TArc* rightHandArc = new TArc[2*G.M()];

    // Planarize the incidence lists of G
    for (TNode u=0;u<n;u++)
    {
        TArc a = First(u);
        TArc firstMappedArc = NoArc;
        TArc prevMappedArc = NoArc;

        if (a==NoArc) continue;

        do
        {
            TArc aG = mapArc[a>>1];

            if (aG!=NoArc)
            {
                aG ^= (a&1);

                if (firstMappedArc==NoArc)
                {
                    firstMappedArc = aG;
                }
                else
                {
                    rightHandArc[prevMappedArc] = aG;
                }

                prevMappedArc = aG;
            }

            a = Right(a,u);
        }
        while (a!=First(u));

        rightHandArc[prevMappedArc] = firstMappedArc;
    }

    GR -> ReorderIncidences(rightHandArc,true);

    delete[] adjacent;
    delete[] rightHandArc;

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif


    // Connectivity augmentation
    G.PlanarConnectivityAugmentation();

    M.Trace(G,1);


    // Biconnectivity augmentation
    G.PlanarBiconnectivityAugmentation();

    M.Trace(G,1);


    // Insert chords into the existing faces
    G.Triangulation();

    M.Trace(G,1);


    // Set up the exterior arc
    if (aBasis==NoArc) aBasis = ExteriorArc();

    if (aBasis==NoArc) aBasis = First(0);

    SetExteriorArc(aBasis);


    // Map the exterior arc to G
    TArc aBasisG = NoArc;

    if (mapArc[aBasis>>1]!=NoArc)
    {
        aBasisG = mapArc[aBasis>>1] ^ (aBasis&1);
    }
    else
    {
        TArc aBasisRep = representant[aBasis>>1];
        aBasisG = mapArc[aBasisRep>>1] ^ (aBasisRep&1);
    }


    char* orientation = new char[m];
    char* orientationG = new char[G.M()];

    // Compute preliminary node coordinates and implicit arc orientations
    G.Layout_StaircaseSketch(aBasisG,spacing,orientationG);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Copy the node positions from G
    for (TNode v=0;v<n;v++)
    {
        SetC(v,0,G.C(v,0));
        SetC(v,1,G.C(v,1));
    }

    // Copy the orientation of the arcs which have been mapped to G
    for (TArc a=0;a<m;a++)
    {
        if (mapArc[a]!=NoArc)
        {
            orientation[a] = orientationG[mapArc[a]>>1] ^ (mapArc[a]&1);
        }
        else
        {
            // This is only to fix the orientation of loops which are
            // neither mapped to G nor represented
            orientation[a] = 0;
        }
    }

    delete[] orientationG;
    delete[] mapArc;

    // Determine the orientation of the parallels which have been eliminated in G
    for (TArc a=0;a<m;a++)
    {
        if (representant[a]!=NoArc)
        {
            // The arcs a and representant[a] must be routed at the same
            // node sides. Choose orientation[a] accordingly.
            orientation[a] = orientation[representant[a]>>1] ^ (representant[a]&1);
        }
    }

    delete[] representant;

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Try to merge some lines
    Layout_KandinskyCompaction(orientation,true);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Determine the effective node size and assign final node positions
    Layout_KandinskyScaleNodes(orientation);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Grow the nodes and assign ports to every arc.
    //    Place the control points based on the orientation computed before:
    //    Every arc starts with a horizontal segment
    Layout_KandinskyRouteArcs(orientation);

    delete[] orientation;

    M.Shutdown(LOG_RES, "...Planar Kandisky drawing found");
}


void abstractMixedGraph::Layout_StaircaseTriconnected(TArc aBasis,TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (aBasis>=2*m && aBasis!=NoArc) NoSuchArc("Layout_StaircaseTriconnected",aBasis);

    #endif

    moduleGuard M(ModStaircase,*this,"Embedding the graph nodes...");

    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);
    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
    Layout_ConvertModel(LAYOUT_KANDINSKI);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(10,2);

    #endif

    char* orientation = new char[m];

    // Step 1: Compute preliminary node coordinates and implicit arc orientations
    try
    {
        Layout_StaircaseSketch(aBasis,spacing,orientation);
    }
    catch (ERRejected)
    {
        delete[] orientation;

        throw ERRejected();
    }

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(6);

    #endif

    // Step 2: Try to merge some lines
    Layout_KandinskyCompaction(orientation,true);

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(1);

    #endif

    // Step 3: Determine the effective node size and assign final node positions
    Layout_KandinskyScaleNodes(orientation);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Step 4: Grow the nodes and assign ports to every arc.
    //    Place the control points based on the orientation computed before:
    //    Every arc starts with a horizontal segment
    Layout_KandinskyRouteArcs(orientation);

    delete[] orientation;

    M.Shutdown(LOG_RES, "...Planar Kandinsky drawing found");
}


void abstractMixedGraph::Layout_StaircaseSketch(TArc aBasis,TFloat spacing,char* orientation)
    throw(ERRejected)
{
    moduleGuard M(ModStaircase,*this,moduleGuard::NO_INDENT);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(m+n,m);

    #endif

    if (aBasis!=NoArc) SetExteriorArc(aBasis);

    // Data structures associated with the ordered partition.
    // Number k of components is not known a priori but bounded by n.
    TNode k = 0;

    // Leftmost / rightmost contact arc of each component
    TArc* cLeft  = new TArc[n]; // Directed towards the component
    TArc* cRight = new TArc[n]; // Directed away from the component

    // Right hand node of each graph node in its component
    TNode* vRight = new TNode[n];

    // Compute the shelling order. Exit if graph is not triconnected
    try
    {
        k = LMCOrderedPartition(cLeft,cRight,vRight);
    }
    catch (ERRejected)
    {
        delete[] cLeft;
        delete[] cRight;
        delete[] vRight;

        throw ERRejected();
    }

    #if defined(_PROGRESS_)

    M.ProgressStep();
    M.SetProgressNext(1);

    #endif


    // Step 1: Implicitly orient the arcs so that the arc routing will be planar

    LogEntry(LOG_METH,"Choose arc orientations...");

    TNode* layer = GetNodeColours();

    for (TArc a=0;a<m;a++)
    {
        TArc a0 = 2*a;
        TNode u = StartNode(a0);
        TNode v = EndNode(a0);

        if (vRight[u]==v)
        {
            // a must enter v horizontally
            orientation[a] = 1;
            continue;
        }
        else if (vRight[v]==u)
        {
            // a must enter v vertically
            orientation[a] = 0;
            continue;
        }

        if (layer[v]<layer[u])
        {
            a0 = a0^1;
            u = StartNode(a0);
            v = EndNode(a0);
        }

        if (cRight[layer[v]]==a0)
        {
            // a0 must enter v vertically
            orientation[a] = (a0^1)&1;
        }
        else if (cLeft[layer[v]]==a0)
        {
            // a0 must enter v horizontally
            orientation[a] = (a0^1)&1;
        }
        else
        {
            orientation[a] = (a0)&1;
        }
    }


    LogEntry(LOG_METH,"Node placement...");

    // The end nodes of the basis arc
    TNode v1 = EndNode(ExteriorArc());
    TNode v2 = StartNode(ExteriorArc());


    incrementalGeometry Geo(*this,n);

    // Embed the first two nodes v1,v2

    Geo.Init(v1);
    Geo.InsertColumnRightOf(v1,v2);
    Geo.InsertRowBelowOf(v1,v2);

    #if defined(_PROGRESS_)

    M.ProgressStep(2);

    #endif

    staticStack<TNode> S(n,CT);

    for (TNode l=1;l<k;++l)
    {
        // Sweep through this component and assign rows relative
        // to the nodes which have been placed in advance
        TNode vPrev = StartNode(cLeft[l]);
        TNode vNext = EndNode(cLeft[l]);
        TNode q = 0;

        while (vNext!=NoNode)
        {
            S.Insert(vNext);
            Geo.InsertRowBelowOf(vPrev,vNext);
            vPrev = vNext;
            vNext = vRight[vNext];
            q++;
        }

        // Sweep back and assign columns relative
        // to the nodes which have been placed in advance
        vPrev = EndNode(cRight[l]);

        while (!S.Empty())
        {
            vNext = S.Delete();
            Geo.InsertColumnLeftOf(vPrev,vNext);
            vPrev = vNext;
        }

        #if defined(_PROGRESS_)

        M.ProgressStep(q);

        #endif

        if (CT.traceLevel>2)
        {
            // Code to display intermediate results

            CT.SuppressLogging();

            Geo.AssignNumbers();

            sparseGraph G(*this,OPT_CLONE);
            G.Layout_ConvertModel(LAYOUT_KANDINSKI);

            for (TNode i=0;i<n;++i)
            {
                if (NodeColour(i)<=l)
                {
                    G.SetC(i,0,Geo.ColumnNumber(i));
                    G.SetC(i,1,Geo.RowNumber(i));
                }
                else
                {
                    G.SetC(i,0,0);
                    G.SetC(i,1,0);
                }
            }

            G.Layout_KandinskyScaleNodes(orientation);
            G.Layout_KandinskyRouteArcs(orientation);

            // Restrict display to the nodes which have already been embedded
            for (TNode i=0;i<n;i++)
            {
                if (NodeColour(i)>l) G.SetNodeVisibility(i,false);
            }

            CT.RestoreLogging();

            M.Trace(G,0);
        }
    }

    if (CT.traceLevel<=2) Geo.AssignNumbers();

    for (TNode i=0;i<n;i++)
    {
        SetC(i,0,Geo.ColumnNumber(i));
        SetC(i,1,Geo.RowNumber(i));
    }

    delete[] cLeft;
    delete[] cRight;
    delete[] vRight;
}


void abstractMixedGraph::Layout_KandinskyTree(TNode root,TFloat spacing)
    throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (!IsSparse() || !Representation())
        NoSparseRepresentation("Layout_KandinskyTree");

    if (MetricType()!=METRIC_DISABLED && IsDense())
        Error(ERR_REJECTED,"Layout_KandinskyTree","Coordinates are fixed");

    #endif

    LogEntry(LOG_METH,"Drawing guided by predecessor tree...");
    moduleGuard M(ModKandinsky,*this,moduleGuard::SHOW_TITLE);

    #if defined(_PROGRESS_)

    M.InitProgressCounter(7*n,1);

    #endif

    SyncSpacingParameters(TokLayoutNodeSpacing,spacing);

    TArc* pred = GetPredecessors();
    bool fixedPredecessors = false;

    if (pred)
    {
        LogEntry(LOG_METH,"Starting with existing predecessor tree...");

        root = NoNode;

        for (TNode v=0;v<n;++v)
        {
            #if defined(_PROGRESS_)

            M.ProgressStep();

            #endif

            if (pred[v]!=NoArc)
            {
                fixedPredecessors = true;
                continue;
            }

            if (root!=NoNode)
            {
                Error(ERR_REJECTED,
                    "Layout_KandinskyTree","Multiple root nodes");
            }

            root = v;
        }

        if (root==NoNode)
        {
            Error(ERR_REJECTED,"Layout_KandinskyTree","Missing root node");
        }
    }
    else
    {
        pred = InitPredecessors();

        if (root==NoNode) root = DefaultRootNode();
    }

    if (First(root)==NoArc)
    {
        Error(ERR_REJECTED,"Layout_KandinskyTree","Root node is isolated");
    }


    LogEntry(LOG_METH,"Checking for tree...");
    OpenFold();

    #if defined(_LOGGING_)

    THandle LH = LogStart(LOG_METH2,"Expanded nodes: ");

    #endif

    TNode* ordered = new TNode[n];

    staticQueue<TNode> Q(n,CT);
    Q.Insert(root);

    TNode nVisited = 0;

    while (!(Q.Empty()))
    {
        TNode u = Q.Delete();
        ordered[nVisited] = u;
        ++nVisited;

        #if defined(_LOGGING_)

        if (CT.logMeth>1)
        {
            sprintf(CT.logBuffer,"%lu ",static_cast<unsigned long>(u));
            LogAppend(LH,CT.logBuffer);
        }

        #endif

        TArc a = First(u);

        do
        {
            // By construction, u is not an isolated node

            TNode v = EndNode(a);

            if (!fixedPredecessors && pred[v]==NoArc && v!=root) pred[v] = a;

            if (pred[v]==a) Q.Insert(v);

            a = Right(a,u);
        }
        while (a!=First(u));

        #if defined(_PROGRESS_)

        M.ProgressStep();

        #endif
    }

    #if defined(_LOGGING_)

    if (CT.logMeth>1) LogEnd(LH);

    #endif

    CloseFold();

    if (nVisited<n)
    {
        M.Shutdown(LOG_RES,"...Predecessor arcs contain cycles");
        return;
    }


    long* rangeLeft = new long[n];
    long* rangeRight = new long[n];
    long* height = new long[n];
    char* orientation = new char[m];


    // Second pass: Bottom up determination of the rangeLeft[], rangeRight[] and height[] of subtrees
    LogEntry(LOG_METH,"Determine subtree heights and widths...");

    for (TNode i=n;i>0;)
    {
        --i;
        TNode u = ordered[i];
        TArc a = First(u);
        bool leaveNode = true;
        TNode fullRange = 0;

        do
        {
            TNode v = EndNode(a);

            if (pred[v]==a)
            {
                leaveNode = false;
                break;
            }

            a = Right(a,u);
        }
        while (a!=First(u));

        height[u] = rangeLeft[u] = rangeRight[u] = 0;

        #if defined(_PROGRESS_)

        M.ProgressStep();

        #endif

        if (leaveNode) continue;

        TNode v = EndNode(a);
        TArc aMedian = a;
        TArc descendants = 0;
        rangeLeft[u] = rangeLeft[v];

        do
        {
            if (height[u]<height[v]+1) height[u] = height[v]+1;

            fullRange += rangeLeft[v]+rangeRight[v]+1;
            orientation[a>>1] = a&1;

            do
            {
                a = Right(a,u);
                v = EndNode(a);
            }
            while (pred[v]!=a && a!=First(u));

            ++descendants;

            if (descendants>1 && (descendants&1))
            {
                rangeLeft[u] += rangeRight[EndNode(aMedian)]+1;

                do
                {
                    aMedian = Right(aMedian,u);
                }
                while (pred[EndNode(aMedian)]!=aMedian);

                rangeLeft[u] += rangeLeft[EndNode(aMedian)];
            }
        }
        while (a!=First(u));

        if (true)
        {
            // Align the subtree root u with its median descendant
            rangeRight[u] = fullRange-rangeLeft[u]-1;
        }
        else
        {
            // Align the subtree root u with its left-most descendant
            rangeLeft[u] = 0;
            rangeRight[u] = fullRange-1;
        }
    }


    // Final pass: Assign coordinates according to the width[] and height[]
    // computations as applied in the previous pass

    Layout_ConvertModel(LAYOUT_STRAIGHT_2DIM);
    Layout_ConvertModel(LAYOUT_KANDINSKI);

    LogEntry(LOG_METH,"Assigning coordinates...");

    sparseRepresentation* X = static_cast<sparseRepresentation*>(Representation());

    X -> SetC(root,0,rangeLeft[root]);
    X -> SetC(root,1,0);

    for (TNode i=0;i<n;++i)
    {
        TNode u = ordered[i];
        TArc a = First(u);
        bool leaveNode = true;
        long offset = 0;

        do
        {
            TNode v = EndNode(a);

            if (pred[v]==a)
            {
                leaveNode = false;
                break;
            }

            a = Right(a,u);
        }
        while (a!=First(u));

        #if defined(_PROGRESS_)

        M.ProgressStep();

        #endif

        if (leaveNode) continue;

        TNode v = EndNode(a);

        do
        {
            // Place v below of u and right-hand of the previously visited descendant of u
            X -> SetC(v,0,C(u,0)+offset+rangeLeft[v]-rangeLeft[u]);
            X -> SetC(v,1,C(u,1)+1);

            offset += rangeLeft[v]+rangeRight[v]+1;

            do
            {
                a = Right(a,u);
                v = EndNode(a);
            }
            while (pred[v]!=a && a!=First(u));
        }
        while (a!=First(u));
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Layout size is %ld x %ld",
            rangeLeft[root]+rangeRight[root]+1,height[root]);
        LogEntry(LOG_RES,CT.logBuffer);
    }

    delete[] rangeLeft;
    delete[] rangeRight;
    delete[] height;

    #if defined(_PROGRESS_)

    M.SetProgressNext(n);

    #endif

    // Step 2: Try to merge some lines
    Layout_KandinskyCompaction(orientation,true);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Step 3: Determine the effective node size and assign final node positions
    Layout_KandinskyScaleNodes(orientation);

    #if defined(_PROGRESS_)

    M.ProgressStep();

    #endif

    // Step 4: Grow the nodes and assign ports to every arc.
    //    Place the control points based on the orientation computed before:
    //    Every arc starts with a horizontal segment
    Layout_KandinskyRouteArcs(orientation);

    delete[] orientation;
}
