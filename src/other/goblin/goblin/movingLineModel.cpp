
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file movingLineModel.cpp
/// \brief Implementation of a moving line model


#include "movingLineModel.h"


movingLineModel::movingLineModel(abstractMixedGraph& _G,TDim _movingDirection) throw() :
    managedObject(_G.Context()), sparseDiGraph(1), G(_G), GM(_G),
    movingDirection((_movingDirection==0) ? MOVE_X_PLUS : MOVE_Y_PLUS),
    blockAssignment(GM.gridSizeX*GM.gridSizeY,G.N(),BLOCK_UNASSIGNED,CT)
{
    GM.ExtractPrimalGrid();

    sprintf(CT.logBuffer,"Generating dual %s-moving line model...",
        (movingDirection==MOVE_X_PLUS) ? "X" : "Y");
    LogEntry(LOG_MEM,CT.logBuffer);


    // Assign the problem dimnsions, and generate the graph nodes:
    //
    // The nodes of G represent the unit grid cells of the original graph, and
    // so is a subgraph of the planar dual of the original grid.
    //
    // The arc set for G is restricted such that every arc connects neighboring
    // grid cells (and hence can be associated with a primal grid line segment),
    // and such that for a directed cycle C in G, it is always feasible to move the
    // grid nodes left-hand of C one grid line in the specified movingDirection.
    // This is guaranteed only by local inspection of the two end nodes of each
    // interior grid line segment.

    TNode nCap = (GM.gridSizeX-1)*(GM.gridSizeY-1);
    TNode mCap = 2*( (GM.gridSizeX-1)*(GM.gridSizeY-2) + (GM.gridSizeX-2)*(GM.gridSizeY-1));

    X.SetCapacity(nCap,mCap,nCap+2);

    for (TNode v=0;v<nCap-1;++v) InsertNode();


    // Inherit the bounding box from G
    for (TDim i=0;i<G.Dim();++i)
    {
        TFloat cMin = 0.0;
        TFloat cMax = 0.0;

        G.Layout_GetBoundingInterval(i,cMin,cMax);
        X.Layout_SetBoundingInterval(i,cMin,cMax);
    }


    for (TNode i=0;i<GM.gridSizeX-1;++i)
    {
        for (TNode j=0;j<GM.gridSizeY-1;++j)
        {
            // The dual node index of the (i,j)-th square:
            TNode v = i+j*(GM.gridSizeX-1);

            SetC(v,0,GM.minX+GM.nodeSpacing*(i+0.5));
            SetC(v,1,GM.minY+GM.nodeSpacing*(j+0.5));

            TFloat minReverseLength = 0;

            TIndex pXMinusYMinus = GM.GridIndex(i  ,j  );
            TIndex pXMinusYPlus  = GM.GridIndex(i  ,j+1);
            TIndex pXPlusYMinus  = GM.GridIndex(i+1,j  );
            TIndex pXPlusYPlus   = GM.GridIndex(i+1,j+1);

            if (   GM.primalNodeType->Key(pXMinusYPlus) ==GM.GRID_NODE_EMPTY
                && GM.primalNodeType->Key(pXPlusYPlus)  ==GM.GRID_NODE_EMPTY
                && GM.primalNodeType->Key(pXMinusYMinus)==GM.GRID_NODE_EMPTY
                && GM.primalNodeType->Key(pXPlusYMinus) ==GM.GRID_NODE_EMPTY
               )
            {
                // Reduce the problem size: If this dual node v does not touch
                // any region's boundary, it can be bypassed by the moving line C
                continue;
            }


            // The virtual cost of bend points
            const TFloat cBend    = 3.0;

            // The virtual cost of a unit length edge segment
            const TFloat cSegment = 1.0;

            // Consider the two grid line segments in X+ and Y+ direction of v

            if (movingDirection==MOVE_X_PLUS)
            {
                if (j<GM.gridSizeY-2)
                {
                    TIndex segYPlus = GM.HoriSegmentIndex(i,j+1);

                    // Consider the direction Y- of contracting the line segment, and the
                    // primal edge segment in Y+ direction of v, a horizontal line segment.

                    if (GM.horizontalSegmentType->Key(segYPlus)==GM.GRID_SEGMENT_EMPTY)
                    {
                        if (   GM.primalNodeType->Key(pXMinusYPlus)==GM.GRID_NODE_EMPTY
                            || GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_EMPTY
                           )
                        {
                            if (i>0) InsertArc(v+(GM.gridSizeX-1),v,1,0);
                        }
                    }
                    else // if (GM.horizontalSegmentType->Key(segYPlus)!=GM.GRID_SEGMENT_EMPTY)
                    {
                        if (   GM.primalNodeType->Key(pXMinusYPlus)==GM.GRID_NODE_NO_BEND
                            || GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_NO_BEND
                           )
                        {
                            InsertArc(v+(GM.gridSizeX-1),v,1,-cSegment);
                            minReverseLength = cSegment;
                        }
                        else if (   GM.primalNodeType->Key(pXMinusYPlus)==GM.GRID_NODE_BEND
                                 && GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_BEND
                           )
                        {
                            InsertArc(v+(GM.gridSizeX-1),v,1,-cSegment-2*cBend);
                            minReverseLength = cSegment+2*cBend;
                        }
                        else if (   GM.primalNodeType->Key(pXMinusYPlus)==GM.GRID_NODE_BEND
                                 || GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_BEND
                           )
                        {
                            // One segYPlus end node is a graph node, and the other is a bend.
                            // Contracting segYPlus is feasible only if a diagonal pair of
                            // vertical segments is missing

                            TIndex segXMinus      = GM.VertSegmentIndex(i,j);
                            TIndex segXPlus       = GM.VertSegmentIndex(i+1,j);
                            TIndex segYPlusXMinus = GM.VertSegmentIndex(i,j+1);
                            TIndex segYPlusXPlus  = GM.VertSegmentIndex(i+1,j+1);

                            if (   (   GM.verticalSegmentType->Key(segXMinus)==GM.GRID_SEGMENT_EMPTY
                                    || GM.verticalSegmentType->Key(segXPlus)==GM.GRID_SEGMENT_EMPTY
                                   )
                                && (   GM.verticalSegmentType->Key(segYPlusXMinus)==GM.GRID_SEGMENT_EMPTY
                                    || GM.verticalSegmentType->Key(segYPlusXPlus)==GM.GRID_SEGMENT_EMPTY
                                   )
                               )
                            {
                                InsertArc(v+(GM.gridSizeX-1),v,1,-cSegment-cBend);
                                minReverseLength = cSegment+cBend;
                            }
                        }
                    }

                    // The trivial direction Y+ of tearing apart the two segment end nodes
                    if (i<GM.gridSizeX-2)
                    {
                        if (   GM.horizontalSegmentType->Key(segYPlus)!=GM.GRID_SEGMENT_EMPTY
                            && minReverseLength<cSegment
                           )
                        {
                            minReverseLength = cSegment;
                        }

                        // If both (v+(gridSizeX-1),v) and (v,v+(gridSizeX-1)) are added,
                        // these two arcs may not form a negative length cycle
                        InsertArc(v,v+(GM.gridSizeX-1),1,minReverseLength);
                    }
                }

                if (i<GM.gridSizeX-2)
                {
                    // Consider the directions of shifting the two segment end nodes
                    TIndex segXPlus = GM.VertSegmentIndex(i+1,j);

                    if (GM.verticalSegmentType->Key(segXPlus)==GM.GRID_SEGMENT_EMPTY)
                    {
                        InsertArc(v,v+1,1,0);
                        InsertArc(v+1,v,1,0);
                    }
                }
            }
            else if (movingDirection==MOVE_Y_PLUS)
            {
                if (i<GM.gridSizeX-2)
                {
                    TIndex segXPlus = GM.VertSegmentIndex(i+1,j);

                    // Consider the direction X+ of contracting the line segment, and the
                    // primal edge segment in X+ direction of v, a vertical line segment.

                    if (GM.verticalSegmentType->Key(segXPlus)==GM.GRID_SEGMENT_EMPTY)
                    {
                        if (   GM.primalNodeType->Key(pXPlusYMinus)==GM.GRID_NODE_EMPTY
                            || GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_EMPTY
                           )
                        {
                            if (j>0) InsertArc(v,v+1,1,0);
                        }
                    }
                    else // if (GM.verticalSegmentType->Key(segXPlus)!=GM.GRID_SEGMENT_EMPTY)
                    {
                        if (   GM.primalNodeType->Key(pXPlusYMinus)==GM.GRID_NODE_NO_BEND
                            || GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_NO_BEND
                           )
                        {
                            InsertArc(v,v+1,1,-cSegment);
                            minReverseLength = cSegment;
                        }
                        else if (   GM.primalNodeType->Key(pXPlusYMinus)==GM.GRID_NODE_BEND
                                 && GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_BEND
                           )
                        {
                            InsertArc(v,v+1,1,-cSegment-2*cBend);
                            minReverseLength = cSegment+2*cBend;
                        }
                        else if (   GM.primalNodeType->Key(pXPlusYMinus)==GM.GRID_NODE_BEND
                                 || GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_BEND
                           )
                        {
                            // One segXPlus end node is a graph node, and the other is a bend.
                            // Contracting segXPlus is feasible only if a diagonal pair of
                            // horizontal segments is missing

                            TIndex segYMinus      = GM.HoriSegmentIndex(i,j);
                            TIndex segYPlus       = GM.HoriSegmentIndex(i,j+1);
                            TIndex segXPlusYMinus = GM.HoriSegmentIndex(i+1,j);
                            TIndex segXPlusYPlus  = GM.HoriSegmentIndex(i+1,j+1);

                            if (   (   GM.horizontalSegmentType->Key(segYMinus)==GM.GRID_SEGMENT_EMPTY
                                    || GM.horizontalSegmentType->Key(segYPlus)==GM.GRID_SEGMENT_EMPTY
                                   )
                                && (   GM.horizontalSegmentType->Key(segXPlusYMinus)==GM.GRID_SEGMENT_EMPTY
                                    || GM.horizontalSegmentType->Key(segXPlusYPlus)==GM.GRID_SEGMENT_EMPTY
                                   )
                               )
                            {
                                InsertArc(v,v+1,1,-cSegment-cBend);
                                minReverseLength = cSegment+cBend;
                            }
                        }
                    }

                    // The trivial direction X- of tearing apart the two segment end nodes
                    if (j<GM.gridSizeY-2)
                    {
                        if (   GM.verticalSegmentType->Key(segXPlus)!=GM.GRID_SEGMENT_EMPTY
                            && minReverseLength<cSegment
                           )
                        {
                            minReverseLength = cSegment;
                        }

                        // If both (v+1,v) and (v,v+1) are added, these two arcs may not form
                        // a negative length cycle
                        InsertArc(v+1,v,1,minReverseLength);
                    }
                }

                if (j<GM.gridSizeY-2)
                {
                    // Consider the directions of shifting the two segment end nodes
                    TIndex segYPlus = GM.HoriSegmentIndex(i,j+1);

                    if (GM.horizontalSegmentType->Key(segYPlus)==GM.GRID_SEGMENT_EMPTY)
                    {
                        InsertArc(v,v+(GM.gridSizeX-1),1,0);
                        InsertArc(v+(GM.gridSizeX-1),v,1,0);
                    }
                }
            }
        }
    }
}


bool movingLineModel::ExtractMovingBlock() throw()
{
    // Due to the limited performance of this method, do not apply to large instances
    if (m>5000) return false;

    TNode r = NegativeCycle(SPX_ORIGINAL,nonBlockingArcs(*this));

    if (r==NoNode) return false;

    sprintf(CT.logBuffer,"Extracting moving %s-block...",(movingDirection==MOVE_X_PLUS) ? "X" : "Y");
    LogEntry(LOG_METH,CT.logBuffer);

    TArc* pred = GetPredecessors();

    // First pass: Determine the primal grid nodes incident with the found
    // negative cycle, and whether the left-hand or right-hand side is interior
    TIndex minPosLeftHand  = NoIndex;
    TIndex minPosRightHand = NoIndex;

    dynamicQueue<TIndex> Q(GM.gridSizeX*GM.gridSizeY,CT);

    TNode v = r;

    do
    {
        TArc a = pred[v];
        TNode u = StartNode(a);

        // Determine the primal grid line segment cut by the arc a,
        // respectively the end nodes pLeft and pRight - where left
        // and right are relative to the cycle direction u->v.
        TIndex primalXPosLeft  = NoIndex;
        TIndex primalXPosRight = NoIndex;
        TIndex primalYPosRight = NoIndex;
        TIndex primalYPosLeft  = NoIndex;

        if (u==v+1)
        {
            // a points to X- and left-hand side of cycle is Y+
            primalXPosLeft  = primalXPosRight = u%(GM.gridSizeX-1);
            primalYPosRight = u/(GM.gridSizeX-1);
            primalYPosLeft  = primalYPosRight+1;

            // Keep track of the minimum Y-position of right-hand assigned grid nodes
            if (minPosRightHand>primalYPosRight) minPosRightHand = primalYPosRight;
        }
        else if (v==u+1)
        {
            // a points to X+ and left-hand side of cycle is Y-
            primalXPosLeft  = primalXPosRight = v%(GM.gridSizeX-1);
            primalYPosLeft  = v/(GM.gridSizeX-1);
            primalYPosRight = primalYPosLeft+1;

            // Keep track of the minimum Y-position of left-hand assigned grid nodes
            if (minPosLeftHand>primalYPosLeft) minPosLeftHand  = primalYPosLeft;
        }
        else if (u==v+(GM.gridSizeX-1))
        {
            // a points to Y- and left-hand side of cycle is X-
            primalYPosLeft  = primalYPosRight = u/(GM.gridSizeX-1);
            primalXPosLeft  = u%(GM.gridSizeX-1);
            primalXPosRight = primalXPosLeft+1;
        }
        else // if (v==u+(GM.gridSizeX-1))
        {
            // a points to Y+ and left-hand side of cycle is X+
            primalYPosLeft  = primalYPosRight = v/(GM.gridSizeX-1);
            primalXPosRight = v%(GM.gridSizeX-1);
            primalXPosLeft  = primalXPosRight+1;
        }

        TNode pLeft  = GM.GridIndex(primalXPosLeft,primalYPosLeft);
        TNode pRight = GM.GridIndex(primalXPosRight,primalYPosRight);

        blockAssignment.ChangeKey(pLeft ,BLOCK_LEFT);
        blockAssignment.ChangeKey(pRight,BLOCK_RIGHT);

        Q.Insert(pLeft);
        Q.Insert(pRight);

        v = u;
    }
    while (v!=r);


    TBlockAssignment interiorBlock =
        (minPosLeftHand>minPosRightHand) ? BLOCK_LEFT : BLOCK_RIGHT;

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Moving %s-hand side",(minPosLeftHand>minPosRightHand) ? "left" : "right");
        LogEntry(LOG_RES,CT.logBuffer);
    }

    if (interiorBlock==BLOCK_RIGHT && movingDirection==MOVE_X_PLUS) movingDirection = MOVE_X_MINUS;
    if (interiorBlock==BLOCK_RIGHT && movingDirection==MOVE_Y_PLUS) movingDirection = MOVE_Y_MINUS;


    // Second pass: Perform a search on the primal grid, and assign the entire
    // interior block

    TNode movedGridNodes = 0;

    while (!Q.Empty())
    {
        TIndex p = Q.Delete();

        // Initially, Q also contains exterior nodes
        if (static_cast<TBlockAssignment>(blockAssignment.Key(p))!=interiorBlock) continue;

        ++movedGridNodes;

        for (char i=0;i<4;++i)
        {
            TIndex q = NoIndex;

            if (i==0) q = p-1;
            if (i==1) q = p+1;
            if (i==2) q = p-GM.gridSizeX;
            if (i==3) q = p+GM.gridSizeX;

            if (static_cast<TBlockAssignment>(blockAssignment.Key(q))==BLOCK_UNASSIGNED)
            {
                Q.Insert(q);
                blockAssignment.ChangeKey(q,interiorBlock);
            }
        }
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Moved grid nodes: %lu",movedGridNodes);
        LogEntry(LOG_RES,CT.logBuffer);
    }

    return true;
}


void movingLineModel::PerformBlockMove() throw()
{
    sparseRepresentation* GR = static_cast<sparseRepresentation*>(G.Representation());

    TBlockAssignment interiorBlock =
        (movingDirection==MOVE_X_PLUS || movingDirection==MOVE_Y_PLUS) ? BLOCK_LEFT : BLOCK_RIGHT;

    TFloat xShift = (movingDirection==MOVE_X_PLUS) ? +GM.nodeSpacing :
                    (movingDirection==MOVE_X_MINUS ? -GM.nodeSpacing : 0);
    TFloat yShift = (movingDirection==MOVE_Y_PLUS) ? +GM.nodeSpacing :
                    (movingDirection==MOVE_Y_MINUS ? -GM.nodeSpacing : 0);

    TNode movedBendNodes = 0;

    for (TArc a=0;a<G.M();++a)
    {
        // Do not shift the graph nodes in the first step, to prevent
        // from moving them several times. This explains PORTS_EXPLICIT
        TNode nControlPoints = GR->GetArcControlPoints(
            2*a,GM.controlPoint,TNode(GM.MAX_CONTROL_POINTS),PORTS_EXPLICIT);

        for (TNode i=0;i<nControlPoints;++i)
        {
            TNode  p = GM.controlPoint[i];
            TIndex q = GM.GridIndex(p);

            if (blockAssignment.Key(q)==interiorBlock)
            {
                // Move this bend node as required
                G.SetC(p,0,G.C(p,0)+xShift);
                G.SetC(p,1,G.C(p,1)+yShift);
                ++movedBendNodes;
            }
        }
    }

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...Moved bend nodes: %lu",movedBendNodes);
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    TNode movedGraphNodes = 0;

    for (TNode v=0;v<G.N();++v)
    {
        TIndex p = GM.GridIndex(v);

        if (blockAssignment.Key(p)==interiorBlock)
        {
            // Move this graph node as required
            G.SetC(v,0,G.C(v,0)+xShift);
            G.SetC(v,1,G.C(v,1)+yShift);
            ++movedGraphNodes;
        }
    }

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...Moved graph nodes: %lu",movedGraphNodes);
        LogEntry(LOG_RES2,CT.logBuffer);
    }
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file movingLineModel.cpp
/// \brief Implementation of a moving line model


#include "movingLineModel.h"


movingLineModel::movingLineModel(abstractMixedGraph& _G,TDim _movingDirection) throw() :
    managedObject(_G.Context()), sparseDiGraph(1), G(_G), GM(_G),
    movingDirection((_movingDirection==0) ? MOVE_X_PLUS : MOVE_Y_PLUS),
    blockAssignment(GM.gridSizeX*GM.gridSizeY,G.N(),BLOCK_UNASSIGNED,CT)
{
    GM.ExtractPrimalGrid();

    sprintf(CT.logBuffer,"Generating dual %s-moving line model...",
        (movingDirection==MOVE_X_PLUS) ? "X" : "Y");
    LogEntry(LOG_MEM,CT.logBuffer);


    // Assign the problem dimnsions, and generate the graph nodes:
    //
    // The nodes of G represent the unit grid cells of the original graph, and
    // so is a subgraph of the planar dual of the original grid.
    //
    // The arc set for G is restricted such that every arc connects neighboring
    // grid cells (and hence can be associated with a primal grid line segment),
    // and such that for a directed cycle C in G, it is always feasible to move the
    // grid nodes left-hand of C one grid line in the specified movingDirection.
    // This is guaranteed only by local inspection of the two end nodes of each
    // interior grid line segment.

    TNode nCap = (GM.gridSizeX-1)*(GM.gridSizeY-1);
    TNode mCap = 2*( (GM.gridSizeX-1)*(GM.gridSizeY-2) + (GM.gridSizeX-2)*(GM.gridSizeY-1));

    X.SetCapacity(nCap,mCap,nCap+2);

    for (TNode v=0;v<nCap-1;++v) InsertNode();


    // Inherit the bounding box from G
    for (TDim i=0;i<G.Dim();++i)
    {
        TFloat cMin = 0.0;
        TFloat cMax = 0.0;

        G.Layout_GetBoundingInterval(i,cMin,cMax);
        X.Layout_SetBoundingInterval(i,cMin,cMax);
    }


    for (TNode i=0;i<GM.gridSizeX-1;++i)
    {
        for (TNode j=0;j<GM.gridSizeY-1;++j)
        {
            // The dual node index of the (i,j)-th square:
            TNode v = i+j*(GM.gridSizeX-1);

            SetC(v,0,GM.minX+GM.nodeSpacing*(i+0.5));
            SetC(v,1,GM.minY+GM.nodeSpacing*(j+0.5));

            TFloat minReverseLength = 0;

            TIndex pXMinusYMinus = GM.GridIndex(i  ,j  );
            TIndex pXMinusYPlus  = GM.GridIndex(i  ,j+1);
            TIndex pXPlusYMinus  = GM.GridIndex(i+1,j  );
            TIndex pXPlusYPlus   = GM.GridIndex(i+1,j+1);

            if (   GM.primalNodeType->Key(pXMinusYPlus) ==GM.GRID_NODE_EMPTY
                && GM.primalNodeType->Key(pXPlusYPlus)  ==GM.GRID_NODE_EMPTY
                && GM.primalNodeType->Key(pXMinusYMinus)==GM.GRID_NODE_EMPTY
                && GM.primalNodeType->Key(pXPlusYMinus) ==GM.GRID_NODE_EMPTY
               )
            {
                // Reduce the problem size: If this dual node v does not touch
                // any region's boundary, it can be bypassed by the moving line C
                continue;
            }


            // The virtual cost of bend points
            const TFloat cBend    = 3.0;

            // The virtual cost of a unit length edge segment
            const TFloat cSegment = 1.0;

            // Consider the two grid line segments in X+ and Y+ direction of v

            if (movingDirection==MOVE_X_PLUS)
            {
                if (j<GM.gridSizeY-2)
                {
                    TIndex segYPlus = GM.HoriSegmentIndex(i,j+1);

                    // Consider the direction Y- of contracting the line segment, and the
                    // primal edge segment in Y+ direction of v, a horizontal line segment.

                    if (GM.horizontalSegmentType->Key(segYPlus)==GM.GRID_SEGMENT_EMPTY)
                    {
                        if (   GM.primalNodeType->Key(pXMinusYPlus)==GM.GRID_NODE_EMPTY
                            || GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_EMPTY
                           )
                        {
                            if (i>0) InsertArc(v+(GM.gridSizeX-1),v,1,0);
                        }
                    }
                    else // if (GM.horizontalSegmentType->Key(segYPlus)!=GM.GRID_SEGMENT_EMPTY)
                    {
                        if (   GM.primalNodeType->Key(pXMinusYPlus)==GM.GRID_NODE_NO_BEND
                            || GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_NO_BEND
                           )
                        {
                            InsertArc(v+(GM.gridSizeX-1),v,1,-cSegment);
                            minReverseLength = cSegment;
                        }
                        else if (   GM.primalNodeType->Key(pXMinusYPlus)==GM.GRID_NODE_BEND
                                 && GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_BEND
                           )
                        {
                            InsertArc(v+(GM.gridSizeX-1),v,1,-cSegment-2*cBend);
                            minReverseLength = cSegment+2*cBend;
                        }
                        else if (   GM.primalNodeType->Key(pXMinusYPlus)==GM.GRID_NODE_BEND
                                 || GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_BEND
                           )
                        {
                            // One segYPlus end node is a graph node, and the other is a bend.
                            // Contracting segYPlus is feasible only if a diagonal pair of
                            // vertical segments is missing

                            TIndex segXMinus      = GM.VertSegmentIndex(i,j);
                            TIndex segXPlus       = GM.VertSegmentIndex(i+1,j);
                            TIndex segYPlusXMinus = GM.VertSegmentIndex(i,j+1);
                            TIndex segYPlusXPlus  = GM.VertSegmentIndex(i+1,j+1);

                            if (   (   GM.verticalSegmentType->Key(segXMinus)==GM.GRID_SEGMENT_EMPTY
                                    || GM.verticalSegmentType->Key(segXPlus)==GM.GRID_SEGMENT_EMPTY
                                   )
                                && (   GM.verticalSegmentType->Key(segYPlusXMinus)==GM.GRID_SEGMENT_EMPTY
                                    || GM.verticalSegmentType->Key(segYPlusXPlus)==GM.GRID_SEGMENT_EMPTY
                                   )
                               )
                            {
                                InsertArc(v+(GM.gridSizeX-1),v,1,-cSegment-cBend);
                                minReverseLength = cSegment+cBend;
                            }
                        }
                    }

                    // The trivial direction Y+ of tearing apart the two segment end nodes
                    if (i<GM.gridSizeX-2)
                    {
                        if (   GM.horizontalSegmentType->Key(segYPlus)!=GM.GRID_SEGMENT_EMPTY
                            && minReverseLength<cSegment
                           )
                        {
                            minReverseLength = cSegment;
                        }

                        // If both (v+(gridSizeX-1),v) and (v,v+(gridSizeX-1)) are added,
                        // these two arcs may not form a negative length cycle
                        InsertArc(v,v+(GM.gridSizeX-1),1,minReverseLength);
                    }
                }

                if (i<GM.gridSizeX-2)
                {
                    // Consider the directions of shifting the two segment end nodes
                    TIndex segXPlus = GM.VertSegmentIndex(i+1,j);

                    if (GM.verticalSegmentType->Key(segXPlus)==GM.GRID_SEGMENT_EMPTY)
                    {
                        InsertArc(v,v+1,1,0);
                        InsertArc(v+1,v,1,0);
                    }
                }
            }
            else if (movingDirection==MOVE_Y_PLUS)
            {
                if (i<GM.gridSizeX-2)
                {
                    TIndex segXPlus = GM.VertSegmentIndex(i+1,j);

                    // Consider the direction X+ of contracting the line segment, and the
                    // primal edge segment in X+ direction of v, a vertical line segment.

                    if (GM.verticalSegmentType->Key(segXPlus)==GM.GRID_SEGMENT_EMPTY)
                    {
                        if (   GM.primalNodeType->Key(pXPlusYMinus)==GM.GRID_NODE_EMPTY
                            || GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_EMPTY
                           )
                        {
                            if (j>0) InsertArc(v,v+1,1,0);
                        }
                    }
                    else // if (GM.verticalSegmentType->Key(segXPlus)!=GM.GRID_SEGMENT_EMPTY)
                    {
                        if (   GM.primalNodeType->Key(pXPlusYMinus)==GM.GRID_NODE_NO_BEND
                            || GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_NO_BEND
                           )
                        {
                            InsertArc(v,v+1,1,-cSegment);
                            minReverseLength = cSegment;
                        }
                        else if (   GM.primalNodeType->Key(pXPlusYMinus)==GM.GRID_NODE_BEND
                                 && GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_BEND
                           )
                        {
                            InsertArc(v,v+1,1,-cSegment-2*cBend);
                            minReverseLength = cSegment+2*cBend;
                        }
                        else if (   GM.primalNodeType->Key(pXPlusYMinus)==GM.GRID_NODE_BEND
                                 || GM.primalNodeType->Key(pXPlusYPlus)==GM.GRID_NODE_BEND
                           )
                        {
                            // One segXPlus end node is a graph node, and the other is a bend.
                            // Contracting segXPlus is feasible only if a diagonal pair of
                            // horizontal segments is missing

                            TIndex segYMinus      = GM.HoriSegmentIndex(i,j);
                            TIndex segYPlus       = GM.HoriSegmentIndex(i,j+1);
                            TIndex segXPlusYMinus = GM.HoriSegmentIndex(i+1,j);
                            TIndex segXPlusYPlus  = GM.HoriSegmentIndex(i+1,j+1);

                            if (   (   GM.horizontalSegmentType->Key(segYMinus)==GM.GRID_SEGMENT_EMPTY
                                    || GM.horizontalSegmentType->Key(segYPlus)==GM.GRID_SEGMENT_EMPTY
                                   )
                                && (   GM.horizontalSegmentType->Key(segXPlusYMinus)==GM.GRID_SEGMENT_EMPTY
                                    || GM.horizontalSegmentType->Key(segXPlusYPlus)==GM.GRID_SEGMENT_EMPTY
                                   )
                               )
                            {
                                InsertArc(v,v+1,1,-cSegment-cBend);
                                minReverseLength = cSegment+cBend;
                            }
                        }
                    }

                    // The trivial direction X- of tearing apart the two segment end nodes
                    if (j<GM.gridSizeY-2)
                    {
                        if (   GM.verticalSegmentType->Key(segXPlus)!=GM.GRID_SEGMENT_EMPTY
                            && minReverseLength<cSegment
                           )
                        {
                            minReverseLength = cSegment;
                        }

                        // If both (v+1,v) and (v,v+1) are added, these two arcs may not form
                        // a negative length cycle
                        InsertArc(v+1,v,1,minReverseLength);
                    }
                }

                if (j<GM.gridSizeY-2)
                {
                    // Consider the directions of shifting the two segment end nodes
                    TIndex segYPlus = GM.HoriSegmentIndex(i,j+1);

                    if (GM.horizontalSegmentType->Key(segYPlus)==GM.GRID_SEGMENT_EMPTY)
                    {
                        InsertArc(v,v+(GM.gridSizeX-1),1,0);
                        InsertArc(v+(GM.gridSizeX-1),v,1,0);
                    }
                }
            }
        }
    }
}


bool movingLineModel::ExtractMovingBlock() throw()
{
    // Due to the limited performance of this method, do not apply to large instances
    if (m>5000) return false;

    TNode r = NegativeCycle(SPX_ORIGINAL,nonBlockingArcs(*this));

    if (r==NoNode) return false;

    sprintf(CT.logBuffer,"Extracting moving %s-block...",(movingDirection==MOVE_X_PLUS) ? "X" : "Y");
    LogEntry(LOG_METH,CT.logBuffer);

    TArc* pred = GetPredecessors();

    // First pass: Determine the primal grid nodes incident with the found
    // negative cycle, and whether the left-hand or right-hand side is interior
    TIndex minPosLeftHand  = NoIndex;
    TIndex minPosRightHand = NoIndex;

    dynamicQueue<TIndex> Q(GM.gridSizeX*GM.gridSizeY,CT);

    TNode v = r;

    do
    {
        TArc a = pred[v];
        TNode u = StartNode(a);

        // Determine the primal grid line segment cut by the arc a,
        // respectively the end nodes pLeft and pRight - where left
        // and right are relative to the cycle direction u->v.
        TIndex primalXPosLeft  = NoIndex;
        TIndex primalXPosRight = NoIndex;
        TIndex primalYPosRight = NoIndex;
        TIndex primalYPosLeft  = NoIndex;

        if (u==v+1)
        {
            // a points to X- and left-hand side of cycle is Y+
            primalXPosLeft  = primalXPosRight = u%(GM.gridSizeX-1);
            primalYPosRight = u/(GM.gridSizeX-1);
            primalYPosLeft  = primalYPosRight+1;

            // Keep track of the minimum Y-position of right-hand assigned grid nodes
            if (minPosRightHand>primalYPosRight) minPosRightHand = primalYPosRight;
        }
        else if (v==u+1)
        {
            // a points to X+ and left-hand side of cycle is Y-
            primalXPosLeft  = primalXPosRight = v%(GM.gridSizeX-1);
            primalYPosLeft  = v/(GM.gridSizeX-1);
            primalYPosRight = primalYPosLeft+1;

            // Keep track of the minimum Y-position of left-hand assigned grid nodes
            if (minPosLeftHand>primalYPosLeft) minPosLeftHand  = primalYPosLeft;
        }
        else if (u==v+(GM.gridSizeX-1))
        {
            // a points to Y- and left-hand side of cycle is X-
            primalYPosLeft  = primalYPosRight = u/(GM.gridSizeX-1);
            primalXPosLeft  = u%(GM.gridSizeX-1);
            primalXPosRight = primalXPosLeft+1;
        }
        else // if (v==u+(GM.gridSizeX-1))
        {
            // a points to Y+ and left-hand side of cycle is X+
            primalYPosLeft  = primalYPosRight = v/(GM.gridSizeX-1);
            primalXPosRight = v%(GM.gridSizeX-1);
            primalXPosLeft  = primalXPosRight+1;
        }

        TNode pLeft  = GM.GridIndex(primalXPosLeft,primalYPosLeft);
        TNode pRight = GM.GridIndex(primalXPosRight,primalYPosRight);

        blockAssignment.ChangeKey(pLeft ,BLOCK_LEFT);
        blockAssignment.ChangeKey(pRight,BLOCK_RIGHT);

        Q.Insert(pLeft);
        Q.Insert(pRight);

        v = u;
    }
    while (v!=r);


    TBlockAssignment interiorBlock =
        (minPosLeftHand>minPosRightHand) ? BLOCK_LEFT : BLOCK_RIGHT;

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Moving %s-hand side",(minPosLeftHand>minPosRightHand) ? "left" : "right");
        LogEntry(LOG_RES,CT.logBuffer);
    }

    if (interiorBlock==BLOCK_RIGHT && movingDirection==MOVE_X_PLUS) movingDirection = MOVE_X_MINUS;
    if (interiorBlock==BLOCK_RIGHT && movingDirection==MOVE_Y_PLUS) movingDirection = MOVE_Y_MINUS;


    // Second pass: Perform a search on the primal grid, and assign the entire
    // interior block

    TNode movedGridNodes = 0;

    while (!Q.Empty())
    {
        TIndex p = Q.Delete();

        // Initially, Q also contains exterior nodes
        if (static_cast<TBlockAssignment>(blockAssignment.Key(p))!=interiorBlock) continue;

        ++movedGridNodes;

        for (char i=0;i<4;++i)
        {
            TIndex q = NoIndex;

            if (i==0) q = p-1;
            if (i==1) q = p+1;
            if (i==2) q = p-GM.gridSizeX;
            if (i==3) q = p+GM.gridSizeX;

            if (static_cast<TBlockAssignment>(blockAssignment.Key(q))==BLOCK_UNASSIGNED)
            {
                Q.Insert(q);
                blockAssignment.ChangeKey(q,interiorBlock);
            }
        }
    }

    if (CT.logRes)
    {
        sprintf(CT.logBuffer,"...Moved grid nodes: %lu",movedGridNodes);
        LogEntry(LOG_RES,CT.logBuffer);
    }

    return true;
}


void movingLineModel::PerformBlockMove() throw()
{
    sparseRepresentation* GR = static_cast<sparseRepresentation*>(G.Representation());

    TBlockAssignment interiorBlock =
        (movingDirection==MOVE_X_PLUS || movingDirection==MOVE_Y_PLUS) ? BLOCK_LEFT : BLOCK_RIGHT;

    TFloat xShift = (movingDirection==MOVE_X_PLUS) ? +GM.nodeSpacing :
                    (movingDirection==MOVE_X_MINUS ? -GM.nodeSpacing : 0);
    TFloat yShift = (movingDirection==MOVE_Y_PLUS) ? +GM.nodeSpacing :
                    (movingDirection==MOVE_Y_MINUS ? -GM.nodeSpacing : 0);

    TNode movedBendNodes = 0;

    for (TArc a=0;a<G.M();++a)
    {
        // Do not shift the graph nodes in the first step, to prevent
        // from moving them several times. This explains PORTS_EXPLICIT
        TNode nControlPoints = GR->GetArcControlPoints(
            2*a,GM.controlPoint,TNode(GM.MAX_CONTROL_POINTS),PORTS_EXPLICIT);

        for (TNode i=0;i<nControlPoints;++i)
        {
            TNode  p = GM.controlPoint[i];
            TIndex q = GM.GridIndex(p);

            if (blockAssignment.Key(q)==interiorBlock)
            {
                // Move this bend node as required
                G.SetC(p,0,G.C(p,0)+xShift);
                G.SetC(p,1,G.C(p,1)+yShift);
                ++movedBendNodes;
            }
        }
    }

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...Moved bend nodes: %lu",movedBendNodes);
        LogEntry(LOG_RES2,CT.logBuffer);
    }

    TNode movedGraphNodes = 0;

    for (TNode v=0;v<G.N();++v)
    {
        TIndex p = GM.GridIndex(v);

        if (blockAssignment.Key(p)==interiorBlock)
        {
            // Move this graph node as required
            G.SetC(v,0,G.C(v,0)+xShift);
            G.SetC(v,1,G.C(v,1)+yShift);
            ++movedGraphNodes;
        }
    }

    if (CT.logRes>1)
    {
        sprintf(CT.logBuffer,"...Moved graph nodes: %lu",movedGraphNodes);
        LogEntry(LOG_RES2,CT.logBuffer);
    }
}
