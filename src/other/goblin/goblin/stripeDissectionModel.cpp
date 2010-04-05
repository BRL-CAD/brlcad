
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file stripeDissectionModel.cpp
/// \brief Implementation of a stripe dissection model


#include "stripeDissectionModel.h"


stripeDissectionModel::stripeDissectionModel(
    abstractMixedGraph& _G,TDim _movingDirection,bool preserveShape) throw() :
    managedObject(_G.Context()), sparseDiGraph(1), G(_G), GM(_G), movingDirection(_movingDirection)
{
    GM.ExtractPrimalGrid();

    const TDim MOVE_X = 0;

    sprintf(CT.logBuffer,"Generating dual %s-flow model...",(movingDirection==MOVE_X) ? "X" : "Y");
    LogEntry(LOG_MEM,CT.logBuffer);

    TNode* stripeIndex[2];
    TIndex* separator[2];

    TIndex gridSize = (GM.gridSizeX>GM.gridSizeY) ? GM.gridSizeX : GM.gridSizeY;

    // Prevent from frequent memory reallocations. If the 
    bool largeSize = (GM.gridSizeX*GM.gridSizeY>=CT.MaxNode());
    if (!largeSize) X.SetCapacity(GM.gridSizeX*GM.gridSizeY,GM.gridSizeX*GM.gridSizeY);

    for (TDim i=0;i<G.Dim();++i)
    {
        TFloat cMin = 0.0;
        TFloat cMax = 0.0;

        G.Layout_GetBoundingInterval(i,cMin,cMax);
        GM.GR -> Layout_SetBoundingInterval(i,cMin,cMax);
    }

    for (unsigned i=0;i<2;++i)
    {
        stripeIndex[i] = new TNode[unsigned(gridSize)];
        separator[i]   = new TIndex[unsigned(gridSize)];
    }

    unsigned prevBuffer = 0;
    unsigned nextBuffer = 1;

    TIndex dim1 = (movingDirection==MOVE_X) ? GM.gridSizeY : GM.gridSizeX;
    TIndex dim2 = (movingDirection==MOVE_X) ? GM.gridSizeX : GM.gridSizeY;
    TDim coord1 = (movingDirection==MOVE_X) ? 1            : 0;
    TDim coord2 = (movingDirection==MOVE_X) ? 0            : 1;
    TFloat min1 = (movingDirection==MOVE_X) ? GM.minY      : GM.minX;
    TFloat min2 = (movingDirection==MOVE_X) ? GM.minX      : GM.minY;
    TFloat max2 = (movingDirection==MOVE_X) ? GM.maxX      : GM.maxY;

    goblinHashTable<TIndex,int>* moveDirSegmentType =
        (movingDirection==MOVE_X) ? GM.horizontalSegmentType : GM.verticalSegmentType;
    goblinHashTable<TIndex,int>* orthoSegmentType =
        (movingDirection==MOVE_X) ? GM.verticalSegmentType : GM.horizontalSegmentType;

    // The first line is covered by a single stripe
    stripeIndex[prevBuffer][0] = 0;
    separator[prevBuffer][0] = NoIndex;
    SetC(0,coord1,min1+GM.nodeSpacing*0.5);
    SetC(0,coord2,(min2+max2)/2.0);

    // Iterate on the rows [columns]
    for (TIndex sweep1=1;sweep1<dim1-1;++sweep1)
    {

        if (largeSize) X.SetCapacity(n+dim2,m+dim2);

        // Generate the left-most [upper-most] stripe
        stripeIndex[nextBuffer][0] = InsertNode();
// cout << "LINE " << sweep1 << ":\n  " << stripeIndex[nextBuffer][0];

        TNode numStripesInLine = 1;
        TIndex stripeStartPos = 0;

        // Keep track of the adjacent stripes in the previous line
        TNode indexInPrevLine = 0;

        // Iterate on the columns [rows]
        for (TIndex sweep2=1;sweep2<dim2;++sweep2)
        {
            TIndex thisSegment = (movingDirection==MOVE_X)
                ? GM.VertSegmentIndex(sweep2,sweep1)
                : GM.HoriSegmentIndex(sweep1,sweep2);
            TIndex thisSegType = orthoSegmentType->Key(thisSegment);

            if (thisSegType==GM.GRID_SEGMENT_EMPTY && sweep2<dim2-1)
            {
                // No vertical [horizontal] edge segment was found at this grid line
                continue;
            }


            separator[nextBuffer][numStripesInLine-1] = thisSegment;
            SetC(stripeIndex[nextBuffer][numStripesInLine-1],
                coord1,min1+GM.nodeSpacing*(sweep1+0.5));
            SetC(stripeIndex[nextBuffer][numStripesInLine-1],
                coord2,min2+GM.nodeSpacing*(stripeStartPos+sweep2)/2.0);

            if (sweep2<dim2-1)
            {
                // So, an edge segment is found, separating two stripes in the current line

                stripeIndex[nextBuffer][numStripesInLine] = InsertNode();
// cout << " [" << sweep2 << "] " << stripeIndex[nextBuffer][numStripesInLine];
            }

            // Complete the inspection of the stripe scanned before thisSegment.
            // Thas is, add arcs from all adjacent stripes on the previous line
            TIndex minPosOverlap = stripeStartPos;

            do
            {
                // Determine the absolute position of separatorInPrevLine
                TIndex separatorInPrevLine = separator[prevBuffer][indexInPrevLine];
                TIndex stripeEndPosInPrevLine = dim2; // -1 ???

                if (separatorInPrevLine!=NoIndex)
                {
                    stripeEndPosInPrevLine = (movingDirection==MOVE_X)
                        ? GM.VertSegmentPosX(separatorInPrevLine)
                        : GM.HoriSegmentPosY(separatorInPrevLine);
                }

                TIndex maxPosOverlap = (stripeEndPosInPrevLine<sweep2) ? stripeEndPosInPrevLine : sweep2;

                // Determine the minimum length of the common line between the two stripes,
                // Both line end nodes can either be graph or bend nodes. In the line interior,
                // only graph nodes of degree less than 3 can occur, as incident edges run
                // parallel with the stripes.
                //
                // The exterior bend nodes can be colocated with adjacent graph nodes, but
                // only if this does introduge edge-edge collisions. U
                TIndex startGridNode = (movingDirection==MOVE_X)
                    ? GM.GridIndex(minPosOverlap,sweep1)
                    : GM.GridIndex(sweep1,minPosOverlap);
                TIndex endGridNode = (movingDirection==MOVE_X)
                    ? GM.GridIndex(maxPosOverlap,sweep1)
                    : GM.GridIndex(sweep1,maxPosOverlap);

                TIndex nInteriorNodes = 0;

                for (TIndex sweep3=minPosOverlap+1;sweep3<maxPosOverlap;++sweep3)
                {
                    TIndex thisGridNode = (movingDirection==MOVE_X)
                        ? GM.GridIndex(sweep3,sweep1)
                        : GM.GridIndex(sweep1,sweep3);

                    if (   GM.primalNodeType->Key(thisGridNode)==GM.GRID_NODE_PORT
                        || GM.primalNodeType->Key(thisGridNode)==GM.GRID_NODE_CROSSING
                       )
                    {
                        ++nInteriorNodes;
                    }
                }

                TIndex minSpanOverlap = 1+nInteriorNodes;

                if (   GM.primalNodeType->Key(startGridNode)==GM.GRID_NODE_BEND
                    || GM.primalNodeType->Key(endGridNode)  ==GM.GRID_NODE_BEND )
                {
                    TIndex startSegment = (movingDirection==MOVE_X)
                        ? GM.HoriSegmentIndex(minPosOverlap,sweep1)
                        : GM.VertSegmentIndex(sweep1,minPosOverlap);
                    TIndex endSegment = (movingDirection==MOVE_X)
                        ? GM.HoriSegmentIndex(maxPosOverlap,sweep1)
                        : GM.VertSegmentIndex(sweep1,maxPosOverlap);

                    // Also consider the orthogonal grid line segments
                    TIndex startSegmentPlus = (movingDirection==MOVE_X)
                        ? GM.VertSegmentIndex(minPosOverlap,sweep1)
                        : GM.HoriSegmentIndex(sweep1,minPosOverlap);
                    TIndex startSegmentMinus = (movingDirection==MOVE_X)
                        ? GM.VertSegmentIndex(minPosOverlap,sweep1-1)
                        : GM.HoriSegmentIndex(sweep1-1,minPosOverlap);
                    TIndex endSegmentPlus = (movingDirection==MOVE_X)
                        ? GM.VertSegmentIndex(maxPosOverlap,sweep1)
                        : GM.HoriSegmentIndex(sweep1,maxPosOverlap);
                    TIndex endSegmentMinus = (movingDirection==MOVE_X)
                        ? GM.VertSegmentIndex(maxPosOverlap,sweep1-1)
                        : GM.HoriSegmentIndex(sweep1-1,maxPosOverlap);

                    if (nInteriorNodes==0)
                    {
                        if (   moveDirSegmentType->Key(startSegment)!=GM.GRID_SEGMENT_EMPTY
                            && (   orthoSegmentType->Key(startSegmentPlus)==GM.GRID_SEGMENT_EMPTY
                                || orthoSegmentType->Key(endSegmentPlus)  ==GM.GRID_SEGMENT_EMPTY )
                            && (   orthoSegmentType->Key(startSegmentMinus)==GM.GRID_SEGMENT_EMPTY
                                || orthoSegmentType->Key(endSegmentMinus)  ==GM.GRID_SEGMENT_EMPTY )
                        )
                        {
                            minSpanOverlap = 0;
                        }
                    }
                    else if (   nInteriorNodes==1
                             && moveDirSegmentType->Key(startSegment)!=GM.GRID_SEGMENT_EMPTY
                             && moveDirSegmentType->Key(endSegment)  !=GM.GRID_SEGMENT_EMPTY
                            )
                    {
                        if (   GM.primalNodeType->Key(startGridNode)==GM.GRID_NODE_BEND
                            && GM.primalNodeType->Key(endGridNode)  ==GM.GRID_NODE_BEND
                            && (   orthoSegmentType->Key(startSegmentPlus)==GM.GRID_SEGMENT_EMPTY
                                || orthoSegmentType->Key(endSegmentPlus)  ==GM.GRID_SEGMENT_EMPTY )
                            && (   orthoSegmentType->Key(startSegmentMinus)==GM.GRID_SEGMENT_EMPTY
                                || orthoSegmentType->Key(endSegmentMinus)  ==GM.GRID_SEGMENT_EMPTY )
                           )
                        {
                            minSpanOverlap = 0;
                        }
                        else
                        {
                            minSpanOverlap = 1;
                        }
                    }
                    else
                    {
                        if (   GM.primalNodeType->Key(startGridNode)==GM.GRID_NODE_BEND
                            && orthoSegmentType->Key(startSegmentPlus)==GM.GRID_SEGMENT_EMPTY )
                        {
                            --minSpanOverlap;
                        }

                        if (   GM.primalNodeType->Key(endGridNode)==GM.GRID_NODE_BEND
                            && orthoSegmentType->Key(endSegmentPlus)==GM.GRID_SEGMENT_EMPTY )
                        {
                            --minSpanOverlap;
                        }
                    }
                }

                if (preserveShape && minSpanOverlap==0) minSpanOverlap = 1;

                TArc a = InsertArc(
                    stripeIndex[prevBuffer][indexInPrevLine],
                    stripeIndex[nextBuffer][numStripesInLine-1],
                    gridSize,1,minSpanOverlap);

                SetSub(2*a,maxPosOverlap-minPosOverlap);
// cout << " (" << stripeIndex[prevBuffer][indexInPrevLine] << " : " << maxPosOverlap-minPosOverlap << ")";
                if (stripeEndPosInPrevLine<=sweep2)
                {
                    ++indexInPrevLine;
                }

                minPosOverlap = maxPosOverlap;
            }
            while (minPosOverlap<sweep2);

            ++numStripesInLine;
            stripeStartPos = sweep2;

        }
// cout << " NUM = " << numStripesInLine << endl;


        // Swap the line buffers
        prevBuffer = nextBuffer;
        nextBuffer = 1-nextBuffer;
    }

    for (unsigned i=0;i<2;++i)
    {
        delete[] stripeIndex[i];
        delete[] separator[i];
    }

    if (CT.traceLevel==2) Display();
}


void stripeDissectionModel::UpdateDrawing() throw()
{
    goblinHashTable<TIndex,TNode>* primalNodeReference    = GM.ExtractNodeReference();
    goblinHashTable<TIndex,TArc>*  verticalArcReference   = GM.ExtractVerticalArcReference();
    goblinHashTable<TIndex,TArc>*  horizontalArcReference = GM.ExtractHorizontalArcReference();

    const TDim MOVE_X = 0;

    TIndex dim1 = (movingDirection==MOVE_X) ? GM.gridSizeY : GM.gridSizeX;
    TIndex dim2 = (movingDirection==MOVE_X) ? GM.gridSizeX : GM.gridSizeY;
    TDim coord2 = (movingDirection==MOVE_X) ? 0            : 1;
    TFloat min2 = (movingDirection==MOVE_X) ? GM.minX      : GM.minY;

    // The first line is covered by a single stripe
    TNode stripeIdx = 0;

    // Iterate on the rows [columns]
    for (TIndex sweep1=1;sweep1<dim1-1;++sweep1)
    {
        // Start from the left-most [upper-most] stripe
        TFloat newStripeStartPos = 0.0;

        // Keep track of where to place low degree graph nodes, which are
        // not at a stripe start or end position in the original drawing
        TFloat nextInternalNodePos = 0.0;

        // For the update of high-degree node representations:
        TNode unclearedNode = NoNode;
        bool minRangeFound = false;
        TFloat minRangePos = 0.0;
        TFloat maxRangePos = 0.0;

        // Iterate on the columns [rows]
        for (TIndex sweep2=1;sweep2<dim2-1;++sweep2)
        {
            TIndex thisGridNode = (movingDirection==MOVE_X)
                ? GM.GridIndex(sweep2,sweep1)
                : GM.GridIndex(sweep1,sweep2);
            TIndex thisNodeType = GM.primalNodeType->Key(thisGridNode);
            TIndex thisSegment = (movingDirection==MOVE_X)
                ? GM.VertSegmentIndex(sweep2,sweep1)
                : GM.HoriSegmentIndex(sweep1,sweep2);
            TIndex thisSegType = (movingDirection==MOVE_X)
                ? GM.verticalSegmentType->Key(thisSegment)
                : GM.horizontalSegmentType->Key(thisSegment);
            TIndex prevSegment = (movingDirection==MOVE_X)
                ? GM.VertSegmentIndex(sweep2,sweep1-1)
                : GM.HoriSegmentIndex(sweep1-1,sweep2);
            TIndex prevSegType = (movingDirection==MOVE_X)
                ? GM.verticalSegmentType->Key(prevSegment)
                : GM.horizontalSegmentType->Key(prevSegment);
            TIndex orthoSegment = (movingDirection==MOVE_X)
                ? GM.HoriSegmentIndex(sweep2,sweep1)
                : GM.VertSegmentIndex(sweep1,sweep2);
// cout << "orthoSegment : " << sweep1 << " " << sweep2 << " " << orthoSegment << endl;
            TIndex orthoSegType = (movingDirection==MOVE_X)
                ? GM.horizontalSegmentType->Key(orthoSegment)
                : GM.verticalSegmentType->Key(orthoSegment);

            if (   unclearedNode==NoNode
                && (   thisNodeType==GM.GRID_NODE_PORT
                    || thisNodeType==GM.GRID_NODE_NO_PORT )
                && thisSegType!=GM.GRID_SEGMENT_NODE
               )
            {
                // So this is a vertex of a graph node box representation
                unclearedNode = primalNodeReference->Key(thisGridNode);
                minRangeFound = false;
            }

            if (unclearedNode!=NoNode)
            {
                if (thisSegType!=GM.GRID_SEGMENT_EMPTY || prevSegType!=GM.GRID_SEGMENT_EMPTY)
                {
                    // This line must be covered by the optimized box representation

                    if (!minRangeFound)
                    {
                        minRangeFound = true;
                        minRangePos = newStripeStartPos;
                    }

                    maxRangePos = newStripeStartPos;
                }

                if (orthoSegType!=GM.GRID_SEGMENT_NODE)
                {
                    // Place this uncleared node
// cout << "unclearedNode = "  << unclearedNode << " vs " << NoNode << endl;
// cout << "minRangePos = " << minRangePos << ", maxRangePos = " << maxRangePos << endl;
                    GM.GR->Layout_SetNodeRange(unclearedNode,dim2,
                        min2+GM.nodeSpacing*minRangePos,
                        min2+GM.nodeSpacing*maxRangePos);
// cout << "<<<\n";
                    unclearedNode = NoNode;
                }
            }

            // In some cases, 
            bool testForCollision = false;

            if (thisSegType==GM.GRID_SEGMENT_EMPTY)
            {
                // No vertical [horizontal] edge segment was found at this grid line...

                if (prevSegType!=GM.GRID_SEGMENT_EMPTY)
                {
                    // Reaching this implies either thisNodeType==GM.GRID_NODE_PORT or
                    // thisNodeType==GM.GRID_NODE_BEND. As thisSegment separates two
                    // stripes in the previous row [column], the represented graph node
                    // or bend has been already placed during the inspection of the
                    // previous row [column].
                    nextInternalNodePos =
                        (G.C(primalNodeReference->Key(thisGridNode),coord2)-min2)/GM.nodeSpacing;
                    testForCollision = true;
                }
                else if (thisNodeType==GM.GRID_NODE_PORT)
                {
                    // This is a stripe internal graph node. Place it
                    G.SetC(primalNodeReference->Key(thisGridNode),coord2,
                        min2+GM.nodeSpacing*nextInternalNodePos);
// cout << "D(" << (primalNodeReference->Key(thisGridNode)) << "," << int(coord2) << ") = "
//    << G.C(primalNodeReference->Key(thisGridNode),coord2) << endl;
                    nextInternalNodePos += 1.0;
                }
                else if (thisNodeType==GM.GRID_NODE_CROSSING)
                {
                    // This is a stripe internal crossing point
                    nextInternalNodePos += 1.0;
                }
            }
            else
            {
                // So, an edge segment is found, separating two stripes in the current line
                ++stripeIdx;
                newStripeStartPos += DegIn(stripeIdx);
//     cout << DegIn(stripeIdx) << " - ";

                TArc thisSegReference = (movingDirection==MOVE_X)
                    ? verticalArcReference->Key(thisSegment)
                    : horizontalArcReference->Key(thisSegment);
// cout << "thisSegReference = " << thisSegReference << endl;

                // Place the end nodes of thisSegment
                for (TIndex sweep3=sweep1;sweep3<=sweep1+1;++sweep3)
                {
                    TIndex thisGridNode = (movingDirection==MOVE_X)
                        ? GM.GridIndex(sweep2,sweep3)
                        : GM.GridIndex(sweep3,sweep2);
                    TIndex thisNodeType = GM.primalNodeType->Key(thisGridNode);

                    if (thisNodeType==GM.GRID_NODE_EMPTY) continue;
                    if (thisNodeType==GM.GRID_NODE_NO_PORT) continue;
                    if (thisNodeType==GM.GRID_NODE_NO_BEND) continue;
                    if (thisNodeType==GM.GRID_NODE_CROSSING) continue;

                    G.SetC(primalNodeReference->Key(thisGridNode),coord2,
                        min2+GM.nodeSpacing*newStripeStartPos);
// cout << "E(" << (primalNodeReference->Key(thisGridNode)) << "," << int(coord2) << ") = "
//    << G.C(primalNodeReference->Key(thisGridNode),coord2) << endl;
// cout << "thisNodeType = " << thisNodeType << endl;
                    if (   thisNodeType==GM.GRID_NODE_PORT
                        && GM.layoutModel!=LAYOUT_ORTHO_SMALL
                        && thisSegReference!=NoArc
                       )
                    {
                        // Place this explicit port node

                        if (sweep3==sweep1)
                        {
                            // Arc runs in positive coord1 direction
                            G.SetC(G.PortNode(thisSegReference),coord2,min2+GM.nodeSpacing*newStripeStartPos);
// cout << "C(" << G.PortNode(thisSegReference) << "," << int(coord2) << ") = " << G.C(G.PortNode(thisSegReference),coord2) << endl;
                        }
                        else
                        {
                            // Arc runs in negative coord1 direction
                            G.SetC(G.PortNode(thisSegReference^1),coord2,min2+GM.nodeSpacing*newStripeStartPos);
// cout << "C(" << G.PortNode(thisSegReference^1) << "," << int(coord2) << ") = " << G.C(G.PortNode(thisSegReference^1),coord2) << endl;
                        }
                    }
                }

                nextInternalNodePos = newStripeStartPos;
                testForCollision = true;
            }


            if (   orthoSegType==GM.GRID_SEGMENT_EDGE
                && GM.layoutModel!=LAYOUT_ORTHO_SMALL
                )
            {
                TArc orthoSegReference = (movingDirection==MOVE_X)
                    ? horizontalArcReference->Key(orthoSegment)
                    : verticalArcReference->Key(orthoSegment);
// cout << "orthoSegReference = " << orthoSegReference << endl;

                // Place the end nodes of thisSegment
                for (TIndex sweep3=sweep2;sweep3<=sweep2+1 && orthoSegReference!=NoArc;++sweep3)
                {
                    TIndex thisGridNode = (movingDirection==MOVE_X)
                        ? GM.GridIndex(sweep3,sweep1)
                        : GM.GridIndex(sweep1,sweep3);
                    TIndex thisNodeType = GM.primalNodeType->Key(thisGridNode);

                    if (thisNodeType!=GM.GRID_NODE_PORT) continue;

                        // Place this explicit port node

                    if (sweep3==sweep2)
                    {
                        // Arc runs in positive coord2 direction
                        G.SetC(G.PortNode(orthoSegReference),coord2,min2+GM.nodeSpacing*newStripeStartPos);
// cout << "C(" << G.PortNode(orthoSegReference) << "," << int(coord2) << ") = " << G.C(G.PortNode(orthoSegReference),coord2) << endl;
                    }
                    else
                    {
                        // Arc runs in negative coord2 direction
                        G.SetC(G.PortNode(orthoSegReference^1),coord2,
                            min2+GM.nodeSpacing*(newStripeStartPos+DegIn(stripeIdx+1)));
// cout << "C(" << G.PortNode(orthoSegReference^1) << "," << int(coord2) << ") = " << G.C(G.PortNode(orthoSegReference^1),coord2) << endl;
                    }
                }
            }

            if (testForCollision)
            {
                // If the next graph node in this line is internal, it can be
                // colocated with thisGridNode if and only if thisGridNode is
                // a bend node of an arc emanating from this internal node.

                TIndex testSegment = (movingDirection==MOVE_X)
                    ? GM.HoriSegmentIndex(sweep2,sweep1)
                    : GM.VertSegmentIndex(sweep1,sweep2);
                TIndex testSegType = (movingDirection==MOVE_X)
                    ? GM.horizontalSegmentType->Key(testSegment)
                    : GM.verticalSegmentType->Key(testSegment);

                if (   thisNodeType==GM.GRID_NODE_PORT
                    || thisNodeType==GM.GRID_NODE_CROSSING
                    || testSegType==GM.GRID_SEGMENT_EMPTY
                   )
                {
                    // Cannot place the next internal node at thisGridNode
                    nextInternalNodePos += 1.0;
                }
            }
        }

        ++stripeIdx;
    }

    delete primalNodeReference;
}

