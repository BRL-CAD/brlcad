
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file orthogonalGrid.cpp
/// \brief Class interface of a stripe dissection model


#include "orthogonalGrid.h"


orthogonalGrid::orthogonalGrid(abstractMixedGraph &_G) throw(ERRejected) :
    managedObject(_G.Context()), G(_G), CFG(_G.Context())
{
    GR = static_cast<sparseRepresentation*>(G.Representation());

    layoutModel = G.LayoutModel();
    G.GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);
    G.GetLayoutParameter(TokLayoutBendSpacing,bendSpacing);

    G.Layout_GetBoundingInterval(0,minX,maxX);
    G.Layout_GetBoundingInterval(1,minY,maxY);

    gridSizeX = TIndex((maxX-minX)/nodeSpacing+0.5)+1;
    gridSizeY = TIndex((maxY-minY)/nodeSpacing+0.5)+1;

    primalNodeType =
        new goblinHashTable<TIndex,int>(gridSizeX*gridSizeY,G.N(),GRID_NODE_EMPTY,CT);

    horizontalSegmentType =
        new goblinHashTable<TIndex,int>((gridSizeX-1)*gridSizeY,G.M(),GRID_SEGMENT_EMPTY,CT);
    verticalSegmentType =
        new goblinHashTable<TIndex,int>((gridSizeY-1)*gridSizeX,G.M(),GRID_SEGMENT_EMPTY,CT);
}

orthogonalGrid::~orthogonalGrid() throw()
{
    if (primalNodeType) delete primalNodeType;
    if (horizontalSegmentType) delete horizontalSegmentType;
    if (verticalSegmentType) delete verticalSegmentType;
}


TIndex orthogonalGrid::GridPos(TFloat layoutPos,TDim i) const throw(ERRange)
{
    return TIndex((layoutPos-((i==0) ? minX : minY))/nodeSpacing+0.5);
}


TIndex orthogonalGrid::GridPos(TNode p,TDim i) const throw(ERRange)
{
    return GridPos(G.C(p,i),i);
}


TIndex orthogonalGrid::GridIndex(TNode p) const throw(ERRange)
{
    return GridIndex(GridPos(p,0),GridPos(p,1));
}


TIndex orthogonalGrid::GridIndex(TIndex cx,TIndex cy) const throw(ERRange)
{
    return cx+cy*gridSizeX;
}


TIndex orthogonalGrid::HoriSegmentIndex(TIndex cx,TIndex cy) const throw(ERRange)
{
    return cx+cy*(gridSizeX-1);
}


TIndex orthogonalGrid::VertSegmentIndex(TIndex cx,TIndex cy) const throw(ERRange)
{
    return cx+cy*gridSizeX;
}


TIndex orthogonalGrid::HoriSegmentPosX(TIndex segmentIndex) const throw(ERRange)
{
    return segmentIndex%(gridSizeX-1);
}


TIndex orthogonalGrid::HoriSegmentPosY(TIndex segmentIndex) const throw(ERRange)
{
    return segmentIndex/(gridSizeX-1);
}


TIndex orthogonalGrid::VertSegmentPosX(TIndex segmentIndex) const throw(ERRange)
{
    return segmentIndex%gridSizeX;
}


TIndex orthogonalGrid::VertSegmentPosY(TIndex segmentIndex) const throw(ERRange)
{
    return segmentIndex/gridSizeX;
}


bool orthogonalGrid::PlaceEdgeInteriorGridNode(TIndex cx,TIndex cy) throw()
{
    TGridNode followUpType[] = {
        GRID_NODE_NO_BEND,  // from GRID_NODE_EMPTY
        GRID_NODE_INVALID,  // from GRID_NODE_PORT
        GRID_NODE_INVALID,  // from GRID_NODE_BEND
        GRID_NODE_INVALID,  // from GRID_NODE_NO_PORT
        GRID_NODE_CROSSING, // from GRID_NODE_NO_BEND
        GRID_NODE_INVALID,  // from GRID_NODE_CROSSIN
        GRID_NODE_INVALID   // from GRID_NODE_INVALID
    };

    TIndex thisGridNode = GridIndex(cx,cy);
    TGridNode newNodeType = followUpType[primalNodeType->Key(thisGridNode)];
    primalNodeType->ChangeKey(thisGridNode,newNodeType);

    if (newNodeType==GRID_NODE_INVALID)
    {
        sprintf(CT.logBuffer,"Drawing collision at grid node (%lu,%lu)",
            static_cast<unsigned long>(cx),static_cast<unsigned long>(cy));
        CT.Error(MSG_WARN,OH,__FUNCTION__,CT.logBuffer);
        return true;
    }

    return false;
}


bool orthogonalGrid::ExtractPrimalGrid() throw(ERRejected)
{
    bool collisionsDetected = false;

    for (TArc a=0;a<G.M();++a)
    {
        // Map all edge segments of the arc a to the grid representation
        TNode nControlPoints = GR->GetArcControlPoints(
            2*a,controlPoint,TNode(MAX_CONTROL_POINTS),
            (layoutModel==LAYOUT_ORTHO_SMALL) ? PORTS_IMPLICIT : PORTS_EXPLICIT);

        TIndex gridIndex1 = GridIndex(controlPoint[0]);

        primalNodeType->ChangeKey(gridIndex1,GRID_NODE_PORT);

        for (TNode i=1;i<nControlPoints;++i)
        {
            TIndex gridIndex2 = GridIndex(controlPoint[i]);
            primalNodeType->ChangeKey(gridIndex2,
                (i==nControlPoints-1) ? GRID_NODE_PORT : GRID_NODE_BEND);

            TIndex posX1 = GridPos(controlPoint[i-1],0);
            TIndex posX2 = GridPos(controlPoint[i],0);
            TIndex posY1 = GridPos(controlPoint[i-1],1);
            TIndex posY2 = GridPos(controlPoint[i],1);

            if (fabs(posX1-posX2)>0.5*bendSpacing && fabs(posY1-posY2)>0.5*bendSpacing)
                CT.Error(ERR_REJECTED,OH,__FUNCTION__,"Skew edge segment detected");

//            if (fabs(posX1-posX2)<0.5*bendSpacing && fabs(posY1-posY2)<0.5*bendSpacing)
//                CT.Error(ERR_REJECTED,OH,__FUNCTION__,"Null edge segment detected");

            if (posX1<posX2)
            {
                for (TIndex j=posX1+1;j<posX2;++j)
                {
                    collisionsDetected |= PlaceEdgeInteriorGridNode(j,posY1);
                    horizontalSegmentType->ChangeKey(HoriSegmentIndex(j-1,posY1),GRID_SEGMENT_EDGE);
                }

                horizontalSegmentType->ChangeKey(HoriSegmentIndex(posX2-1,posY1),GRID_SEGMENT_EDGE);
            }
            else if (posX1>posX2)
            {
                for (TIndex j=posX2+1;j<posX1;++j)
                {
                    collisionsDetected |= PlaceEdgeInteriorGridNode(j,posY1);
                    horizontalSegmentType->ChangeKey(HoriSegmentIndex(j-1,posY1),GRID_SEGMENT_EDGE);
                }

                horizontalSegmentType->ChangeKey(HoriSegmentIndex(posX1-1,posY1),GRID_SEGMENT_EDGE);
            }
            else if (posY1<posY2)
            {
                for (TIndex j=posY1+1;j<posY2;++j)
                {
                    collisionsDetected |= PlaceEdgeInteriorGridNode(posX1,j);
                    verticalSegmentType->ChangeKey(VertSegmentIndex(posX1,j-1),GRID_SEGMENT_EDGE);
                }

                verticalSegmentType->ChangeKey(VertSegmentIndex(posX1,posY2-1),GRID_SEGMENT_EDGE);
            }
            else // if (posY1>=posY2)
            {
                for (TIndex j=posY2+1;j<posY1;++j)
                {
                    collisionsDetected |= PlaceEdgeInteriorGridNode(posX1,j);
                    verticalSegmentType->ChangeKey(VertSegmentIndex(posX1,j-1),GRID_SEGMENT_EDGE);
                }

                verticalSegmentType->ChangeKey(VertSegmentIndex(posX1,posY1-1),GRID_SEGMENT_EDGE);
            }

            gridIndex1 = gridIndex2;
        }
    }

    for (TNode v=0;v<G.N();++v)
    {
        // Map the box display of the graph node v to the grid representation
        TFloat cMin[2] = {0.0,0.0};
        TFloat cMax[2] = {0.0,0.0};

        GR -> Layout_GetNodeRange(v,0,cMin[0],cMax[0]);
        GR -> Layout_GetNodeRange(v,1,cMin[1],cMax[1]);

        TIndex gridMin[2] = {GridPos(cMin[0],0),GridPos(cMin[1],1)};
        TIndex gridMax[2] = {GridPos(cMax[0],0),GridPos(cMax[1],1)};

        for (TIndex j=gridMin[0];j<gridMax[0];++j)
        {
            horizontalSegmentType ->
                ChangeKey(HoriSegmentIndex(j,gridMin[1]),GRID_SEGMENT_NODE);
            horizontalSegmentType ->
                ChangeKey(HoriSegmentIndex(j,gridMax[1]),GRID_SEGMENT_NODE);

            if (primalNodeType->Key(GridIndex(j,gridMin[1]))==GRID_NODE_EMPTY)
                primalNodeType -> ChangeKey(GridIndex(j,gridMin[1]),GRID_NODE_NO_PORT);

            if (primalNodeType->Key(GridIndex(j,gridMax[1]))==GRID_NODE_EMPTY)
                primalNodeType -> ChangeKey(GridIndex(j,gridMax[1]),GRID_NODE_NO_PORT);
        }

        if (primalNodeType->Key(GridIndex(gridMax[0],gridMin[1]))==GRID_NODE_EMPTY)
            primalNodeType -> ChangeKey(GridIndex(gridMax[0],gridMin[1]),GRID_NODE_NO_PORT);

        if (primalNodeType->Key(GridIndex(gridMax[0],gridMax[1]))==GRID_NODE_EMPTY)
            primalNodeType -> ChangeKey(GridIndex(gridMax[0],gridMax[1]),GRID_NODE_NO_PORT);



        for (TIndex j=gridMin[1];j<gridMax[1];++j)
        {
            verticalSegmentType ->
                ChangeKey(VertSegmentIndex(gridMin[0],j),GRID_SEGMENT_NODE);
            verticalSegmentType ->
                ChangeKey(VertSegmentIndex(gridMax[0],j),GRID_SEGMENT_NODE);

            if (primalNodeType->Key(GridIndex(gridMin[1],j))==GRID_NODE_EMPTY)
                primalNodeType -> ChangeKey(GridIndex(gridMin[1],j),GRID_NODE_NO_PORT);

            if (primalNodeType->Key(GridIndex(gridMax[1],j))==GRID_NODE_EMPTY)
                primalNodeType -> ChangeKey(GridIndex(gridMax[1],j),GRID_NODE_NO_PORT);
        }
    }
/*
    for (TIndex i=0;i<gridSizeY;++i)
    {
        for (TIndex j=0;j<gridSizeX;++j)
        {
            char tabType[] = {".PBNXC"};

            cout << tabType[primalNodeType->Key(GridIndex(j,i))];
        }

        cout << endl;
    }

    cout << endl;*/

    return collisionsDetected;
}


goblinHashTable<TIndex,TNode>* orthogonalGrid::ExtractNodeReference() throw()
{
    goblinHashTable<TIndex,TNode>* primalNodeReference =
        new goblinHashTable<TIndex,TNode>(gridSizeX*gridSizeY,G.M(),NoNode,CT);

    for (TArc a=0;a<G.M();++a)
    {
        TNode nControlPoints = GR->GetArcControlPoints(
            2*a,controlPoint,TNode(MAX_CONTROL_POINTS),
            (layoutModel==LAYOUT_ORTHO_SMALL) ? PORTS_IMPLICIT : PORTS_EXPLICIT);

        for (TNode i=1;i<nControlPoints;++i)
        {
            primalNodeReference -> ChangeKey(GridIndex(controlPoint[i]),controlPoint[i]);
        }
    }

    for (TNode v=0;v<G.N();++v)
    {
//        primalNodeReference -> ChangeKey(GridIndex(v),v);

        // Map the box display of the graph node v to the grid representation
        TFloat cMin[2] = {0.0,0.0};
        TFloat cMax[2] = {0.0,0.0};

        GR -> Layout_GetNodeRange(v,0,cMin[0],cMax[0]);
        GR -> Layout_GetNodeRange(v,1,cMin[1],cMax[1]);

        TIndex gridMin[2] = {GridPos(cMin[0],0),GridPos(cMin[1],1)};
        TIndex gridMax[2] = {GridPos(cMax[0],0),GridPos(cMax[1],1)};

        for (TIndex j=gridMin[0];j<=gridMax[0];++j)
        {
            primalNodeReference -> ChangeKey(GridIndex(j,gridMin[1]),v);

            if (gridMin[1]<gridMax[1]) primalNodeReference -> ChangeKey(GridIndex(j,gridMax[1]),v);
        }

        for (TIndex j=gridMin[1]+1;j<gridMax[1];++j)
        {
            primalNodeReference -> ChangeKey(GridIndex(gridMin[0],j),v);

            if (gridMin[0]<gridMax[0]) primalNodeReference -> ChangeKey(GridIndex(gridMax[0],j),v);
        }
    }
/*
    for (TIndex i=0;i<gridSizeY;++i)
    {
        for (TIndex j=0;j<gridSizeX;++j)
        {
            cout << ((primalNodeReference->Key(GridIndex(j,i))!=NoNode) ? "X" : ".");
        }

        cout << endl;
    }

    cout << endl;
*/
    return primalNodeReference;
}


goblinHashTable<TIndex,TArc>* orthogonalGrid::ExtractVerticalArcReference() throw()
{
    goblinHashTable<TIndex,TArc>* verticalArcReference =
        new goblinHashTable<TIndex,TArc>((gridSizeY-1)*gridSizeX,G.M(),NoArc,CT);

    for (TArc a=0;a<G.M();++a)
    {
        // Map all edge segments of the arc a to the grid representation
        TNode nControlPoints = GR->GetArcControlPoints(
            2*a,controlPoint,TNode(MAX_CONTROL_POINTS),
            (layoutModel==LAYOUT_ORTHO_SMALL) ? PORTS_IMPLICIT : PORTS_EXPLICIT);

        TIndex gridIndex1 = GridIndex(controlPoint[0]);

        for (TNode i=1;i<nControlPoints;++i)
        {
            TIndex gridIndex2 = GridIndex(controlPoint[i]);
            TIndex posX  = GridPos(controlPoint[i-1],0);
            TIndex posY1 = GridPos(controlPoint[i-1],1);
            TIndex posY2 = GridPos(controlPoint[i],1);

            if (posY1<posY2)
            {
                for (TIndex j=posY1+1;j<=posY2;++j)
                {
                    verticalArcReference->ChangeKey(VertSegmentIndex(posX,j-1),2*a);
                }
            }
            else if (posY1>posY2)
            {
                for (TIndex j=posY2+1;j<=posY1;++j)
                {
                    verticalArcReference->ChangeKey(VertSegmentIndex(posX,j-1),2*a+1);
                }
            }

            gridIndex1 = gridIndex2;
        }
    }

    return verticalArcReference;
}


goblinHashTable<TIndex,TArc>* orthogonalGrid::ExtractHorizontalArcReference() throw()
{
    goblinHashTable<TIndex,TArc>* horizontalArcReference =
        new goblinHashTable<TIndex,TArc>((gridSizeX-1)*gridSizeY,G.M(),NoArc,CT);

    for (TArc a=0;a<G.M();++a)
    {
        // Map all edge segments of the arc a to the grid representation
        TNode nControlPoints = GR->GetArcControlPoints(
            2*a,controlPoint,TNode(MAX_CONTROL_POINTS),
            (layoutModel==LAYOUT_ORTHO_SMALL) ? PORTS_IMPLICIT : PORTS_EXPLICIT);

        TIndex gridIndex1 = GridIndex(controlPoint[0]);

        for (TNode i=1;i<nControlPoints;++i)
        {
            TIndex gridIndex2 = GridIndex(controlPoint[i]);
            TIndex posX1 = GridPos(controlPoint[i-1],0);
            TIndex posX2 = GridPos(controlPoint[i],0);
            TIndex posY  = GridPos(controlPoint[i-1],1);

            if (posX1<posX2)
            {
                for (TIndex j=posX1+1;j<=posX2;++j)
                {
                    horizontalArcReference->ChangeKey(HoriSegmentIndex(j-1,posY),2*a);
                }
            }
            else if (posX1>posX2)
            {
                for (TIndex j=posX2+1;j<=posX1;++j)
                {
                    horizontalArcReference->ChangeKey(HoriSegmentIndex(j-1,posY),2*a+1);
                }
            }

            gridIndex1 = gridIndex2;
        }
    }

    return horizontalArcReference;
}

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file orthogonalGrid.cpp
/// \brief Class interface of a stripe dissection model


#include "orthogonalGrid.h"


orthogonalGrid::orthogonalGrid(abstractMixedGraph &_G) throw(ERRejected) :
    managedObject(_G.Context()), G(_G), CFG(_G.Context())
{
    GR = static_cast<sparseRepresentation*>(G.Representation());

    layoutModel = G.LayoutModel();
    G.GetLayoutParameter(TokLayoutNodeSpacing,nodeSpacing);
    G.GetLayoutParameter(TokLayoutBendSpacing,bendSpacing);

    G.Layout_GetBoundingInterval(0,minX,maxX);
    G.Layout_GetBoundingInterval(1,minY,maxY);

    gridSizeX = TIndex((maxX-minX)/nodeSpacing+0.5)+1;
    gridSizeY = TIndex((maxY-minY)/nodeSpacing+0.5)+1;

    primalNodeType =
        new goblinHashTable<TIndex,int>(gridSizeX*gridSizeY,G.N(),GRID_NODE_EMPTY,CT);

    horizontalSegmentType =
        new goblinHashTable<TIndex,int>((gridSizeX-1)*gridSizeY,G.M(),GRID_SEGMENT_EMPTY,CT);
    verticalSegmentType =
        new goblinHashTable<TIndex,int>((gridSizeY-1)*gridSizeX,G.M(),GRID_SEGMENT_EMPTY,CT);
}

orthogonalGrid::~orthogonalGrid() throw()
{
    if (primalNodeType) delete primalNodeType;
    if (horizontalSegmentType) delete horizontalSegmentType;
    if (verticalSegmentType) delete verticalSegmentType;
}


TIndex orthogonalGrid::GridPos(TFloat layoutPos,TDim i) const throw(ERRange)
{
    return TIndex((layoutPos-((i==0) ? minX : minY))/nodeSpacing+0.5);
}


TIndex orthogonalGrid::GridPos(TNode p,TDim i) const throw(ERRange)
{
    return GridPos(G.C(p,i),i);
}


TIndex orthogonalGrid::GridIndex(TNode p) const throw(ERRange)
{
    return GridIndex(GridPos(p,0),GridPos(p,1));
}


TIndex orthogonalGrid::GridIndex(TIndex cx,TIndex cy) const throw(ERRange)
{
    return cx+cy*gridSizeX;
}


TIndex orthogonalGrid::HoriSegmentIndex(TIndex cx,TIndex cy) const throw(ERRange)
{
    return cx+cy*(gridSizeX-1);
}


TIndex orthogonalGrid::VertSegmentIndex(TIndex cx,TIndex cy) const throw(ERRange)
{
    return cx+cy*gridSizeX;
}


TIndex orthogonalGrid::HoriSegmentPosX(TIndex segmentIndex) const throw(ERRange)
{
    return segmentIndex%(gridSizeX-1);
}


TIndex orthogonalGrid::HoriSegmentPosY(TIndex segmentIndex) const throw(ERRange)
{
    return segmentIndex/(gridSizeX-1);
}


TIndex orthogonalGrid::VertSegmentPosX(TIndex segmentIndex) const throw(ERRange)
{
    return segmentIndex%gridSizeX;
}


TIndex orthogonalGrid::VertSegmentPosY(TIndex segmentIndex) const throw(ERRange)
{
    return segmentIndex/gridSizeX;
}


bool orthogonalGrid::PlaceEdgeInteriorGridNode(TIndex cx,TIndex cy) throw()
{
    TGridNode followUpType[] = {
        GRID_NODE_NO_BEND,  // from GRID_NODE_EMPTY
        GRID_NODE_INVALID,  // from GRID_NODE_PORT
        GRID_NODE_INVALID,  // from GRID_NODE_BEND
        GRID_NODE_INVALID,  // from GRID_NODE_NO_PORT
        GRID_NODE_CROSSING, // from GRID_NODE_NO_BEND
        GRID_NODE_INVALID,  // from GRID_NODE_CROSSIN
        GRID_NODE_INVALID   // from GRID_NODE_INVALID
    };

    TIndex thisGridNode = GridIndex(cx,cy);
    TGridNode newNodeType = followUpType[primalNodeType->Key(thisGridNode)];
    primalNodeType->ChangeKey(thisGridNode,newNodeType);

    if (newNodeType==GRID_NODE_INVALID)
    {
        sprintf(CT.logBuffer,"Drawing collision at grid node (%lu,%lu)",
            static_cast<unsigned long>(cx),static_cast<unsigned long>(cy));
        CT.Error(MSG_WARN,OH,__FUNCTION__,CT.logBuffer);
        return true;
    }

    return false;
}


bool orthogonalGrid::ExtractPrimalGrid() throw(ERRejected)
{
    bool collisionsDetected = false;

    for (TArc a=0;a<G.M();++a)
    {
        // Map all edge segments of the arc a to the grid representation
        TNode nControlPoints = GR->GetArcControlPoints(
            2*a,controlPoint,TNode(MAX_CONTROL_POINTS),
            (layoutModel==LAYOUT_ORTHO_SMALL) ? PORTS_IMPLICIT : PORTS_EXPLICIT);

        TIndex gridIndex1 = GridIndex(controlPoint[0]);

        primalNodeType->ChangeKey(gridIndex1,GRID_NODE_PORT);

        for (TNode i=1;i<nControlPoints;++i)
        {
            TIndex gridIndex2 = GridIndex(controlPoint[i]);
            primalNodeType->ChangeKey(gridIndex2,
                (i==nControlPoints-1) ? GRID_NODE_PORT : GRID_NODE_BEND);

            TIndex posX1 = GridPos(controlPoint[i-1],0);
            TIndex posX2 = GridPos(controlPoint[i],0);
            TIndex posY1 = GridPos(controlPoint[i-1],1);
            TIndex posY2 = GridPos(controlPoint[i],1);

            if (fabs(posX1-posX2)>0.5*bendSpacing && fabs(posY1-posY2)>0.5*bendSpacing)
                CT.Error(ERR_REJECTED,OH,__FUNCTION__,"Skew edge segment detected");

//            if (fabs(posX1-posX2)<0.5*bendSpacing && fabs(posY1-posY2)<0.5*bendSpacing)
//                CT.Error(ERR_REJECTED,OH,__FUNCTION__,"Null edge segment detected");

            if (posX1<posX2)
            {
                for (TIndex j=posX1+1;j<posX2;++j)
                {
                    collisionsDetected |= PlaceEdgeInteriorGridNode(j,posY1);
                    horizontalSegmentType->ChangeKey(HoriSegmentIndex(j-1,posY1),GRID_SEGMENT_EDGE);
                }

                horizontalSegmentType->ChangeKey(HoriSegmentIndex(posX2-1,posY1),GRID_SEGMENT_EDGE);
            }
            else if (posX1>posX2)
            {
                for (TIndex j=posX2+1;j<posX1;++j)
                {
                    collisionsDetected |= PlaceEdgeInteriorGridNode(j,posY1);
                    horizontalSegmentType->ChangeKey(HoriSegmentIndex(j-1,posY1),GRID_SEGMENT_EDGE);
                }

                horizontalSegmentType->ChangeKey(HoriSegmentIndex(posX1-1,posY1),GRID_SEGMENT_EDGE);
            }
            else if (posY1<posY2)
            {
                for (TIndex j=posY1+1;j<posY2;++j)
                {
                    collisionsDetected |= PlaceEdgeInteriorGridNode(posX1,j);
                    verticalSegmentType->ChangeKey(VertSegmentIndex(posX1,j-1),GRID_SEGMENT_EDGE);
                }

                verticalSegmentType->ChangeKey(VertSegmentIndex(posX1,posY2-1),GRID_SEGMENT_EDGE);
            }
            else // if (posY1>=posY2)
            {
                for (TIndex j=posY2+1;j<posY1;++j)
                {
                    collisionsDetected |= PlaceEdgeInteriorGridNode(posX1,j);
                    verticalSegmentType->ChangeKey(VertSegmentIndex(posX1,j-1),GRID_SEGMENT_EDGE);
                }

                verticalSegmentType->ChangeKey(VertSegmentIndex(posX1,posY1-1),GRID_SEGMENT_EDGE);
            }

            gridIndex1 = gridIndex2;
        }
    }

    for (TNode v=0;v<G.N();++v)
    {
        // Map the box display of the graph node v to the grid representation
        TFloat cMin[2] = {0.0,0.0};
        TFloat cMax[2] = {0.0,0.0};

        GR -> Layout_GetNodeRange(v,0,cMin[0],cMax[0]);
        GR -> Layout_GetNodeRange(v,1,cMin[1],cMax[1]);

        TIndex gridMin[2] = {GridPos(cMin[0],0),GridPos(cMin[1],1)};
        TIndex gridMax[2] = {GridPos(cMax[0],0),GridPos(cMax[1],1)};

        for (TIndex j=gridMin[0];j<gridMax[0];++j)
        {
            horizontalSegmentType ->
                ChangeKey(HoriSegmentIndex(j,gridMin[1]),GRID_SEGMENT_NODE);
            horizontalSegmentType ->
                ChangeKey(HoriSegmentIndex(j,gridMax[1]),GRID_SEGMENT_NODE);

            if (primalNodeType->Key(GridIndex(j,gridMin[1]))==GRID_NODE_EMPTY)
                primalNodeType -> ChangeKey(GridIndex(j,gridMin[1]),GRID_NODE_NO_PORT);

            if (primalNodeType->Key(GridIndex(j,gridMax[1]))==GRID_NODE_EMPTY)
                primalNodeType -> ChangeKey(GridIndex(j,gridMax[1]),GRID_NODE_NO_PORT);
        }

        if (primalNodeType->Key(GridIndex(gridMax[0],gridMin[1]))==GRID_NODE_EMPTY)
            primalNodeType -> ChangeKey(GridIndex(gridMax[0],gridMin[1]),GRID_NODE_NO_PORT);

        if (primalNodeType->Key(GridIndex(gridMax[0],gridMax[1]))==GRID_NODE_EMPTY)
            primalNodeType -> ChangeKey(GridIndex(gridMax[0],gridMax[1]),GRID_NODE_NO_PORT);



        for (TIndex j=gridMin[1];j<gridMax[1];++j)
        {
            verticalSegmentType ->
                ChangeKey(VertSegmentIndex(gridMin[0],j),GRID_SEGMENT_NODE);
            verticalSegmentType ->
                ChangeKey(VertSegmentIndex(gridMax[0],j),GRID_SEGMENT_NODE);

            if (primalNodeType->Key(GridIndex(gridMin[1],j))==GRID_NODE_EMPTY)
                primalNodeType -> ChangeKey(GridIndex(gridMin[1],j),GRID_NODE_NO_PORT);

            if (primalNodeType->Key(GridIndex(gridMax[1],j))==GRID_NODE_EMPTY)
                primalNodeType -> ChangeKey(GridIndex(gridMax[1],j),GRID_NODE_NO_PORT);
        }
    }
/*
    for (TIndex i=0;i<gridSizeY;++i)
    {
        for (TIndex j=0;j<gridSizeX;++j)
        {
            char tabType[] = {".PBNXC"};

            cout << tabType[primalNodeType->Key(GridIndex(j,i))];
        }

        cout << endl;
    }

    cout << endl;*/

    return collisionsDetected;
}


goblinHashTable<TIndex,TNode>* orthogonalGrid::ExtractNodeReference() throw()
{
    goblinHashTable<TIndex,TNode>* primalNodeReference =
        new goblinHashTable<TIndex,TNode>(gridSizeX*gridSizeY,G.M(),NoNode,CT);

    for (TArc a=0;a<G.M();++a)
    {
        TNode nControlPoints = GR->GetArcControlPoints(
            2*a,controlPoint,TNode(MAX_CONTROL_POINTS),
            (layoutModel==LAYOUT_ORTHO_SMALL) ? PORTS_IMPLICIT : PORTS_EXPLICIT);

        for (TNode i=1;i<nControlPoints;++i)
        {
            primalNodeReference -> ChangeKey(GridIndex(controlPoint[i]),controlPoint[i]);
        }
    }

    for (TNode v=0;v<G.N();++v)
    {
//        primalNodeReference -> ChangeKey(GridIndex(v),v);

        // Map the box display of the graph node v to the grid representation
        TFloat cMin[2] = {0.0,0.0};
        TFloat cMax[2] = {0.0,0.0};

        GR -> Layout_GetNodeRange(v,0,cMin[0],cMax[0]);
        GR -> Layout_GetNodeRange(v,1,cMin[1],cMax[1]);

        TIndex gridMin[2] = {GridPos(cMin[0],0),GridPos(cMin[1],1)};
        TIndex gridMax[2] = {GridPos(cMax[0],0),GridPos(cMax[1],1)};

        for (TIndex j=gridMin[0];j<=gridMax[0];++j)
        {
            primalNodeReference -> ChangeKey(GridIndex(j,gridMin[1]),v);

            if (gridMin[1]<gridMax[1]) primalNodeReference -> ChangeKey(GridIndex(j,gridMax[1]),v);
        }

        for (TIndex j=gridMin[1]+1;j<gridMax[1];++j)
        {
            primalNodeReference -> ChangeKey(GridIndex(gridMin[0],j),v);

            if (gridMin[0]<gridMax[0]) primalNodeReference -> ChangeKey(GridIndex(gridMax[0],j),v);
        }
    }
/*
    for (TIndex i=0;i<gridSizeY;++i)
    {
        for (TIndex j=0;j<gridSizeX;++j)
        {
            cout << ((primalNodeReference->Key(GridIndex(j,i))!=NoNode) ? "X" : ".");
        }

        cout << endl;
    }

    cout << endl;
*/
    return primalNodeReference;
}


goblinHashTable<TIndex,TArc>* orthogonalGrid::ExtractVerticalArcReference() throw()
{
    goblinHashTable<TIndex,TArc>* verticalArcReference =
        new goblinHashTable<TIndex,TArc>((gridSizeY-1)*gridSizeX,G.M(),NoArc,CT);

    for (TArc a=0;a<G.M();++a)
    {
        // Map all edge segments of the arc a to the grid representation
        TNode nControlPoints = GR->GetArcControlPoints(
            2*a,controlPoint,TNode(MAX_CONTROL_POINTS),
            (layoutModel==LAYOUT_ORTHO_SMALL) ? PORTS_IMPLICIT : PORTS_EXPLICIT);

        TIndex gridIndex1 = GridIndex(controlPoint[0]);

        for (TNode i=1;i<nControlPoints;++i)
        {
            TIndex gridIndex2 = GridIndex(controlPoint[i]);
            TIndex posX  = GridPos(controlPoint[i-1],0);
            TIndex posY1 = GridPos(controlPoint[i-1],1);
            TIndex posY2 = GridPos(controlPoint[i],1);

            if (posY1<posY2)
            {
                for (TIndex j=posY1+1;j<=posY2;++j)
                {
                    verticalArcReference->ChangeKey(VertSegmentIndex(posX,j-1),2*a);
                }
            }
            else if (posY1>posY2)
            {
                for (TIndex j=posY2+1;j<=posY1;++j)
                {
                    verticalArcReference->ChangeKey(VertSegmentIndex(posX,j-1),2*a+1);
                }
            }

            gridIndex1 = gridIndex2;
        }
    }

    return verticalArcReference;
}


goblinHashTable<TIndex,TArc>* orthogonalGrid::ExtractHorizontalArcReference() throw()
{
    goblinHashTable<TIndex,TArc>* horizontalArcReference =
        new goblinHashTable<TIndex,TArc>((gridSizeX-1)*gridSizeY,G.M(),NoArc,CT);

    for (TArc a=0;a<G.M();++a)
    {
        // Map all edge segments of the arc a to the grid representation
        TNode nControlPoints = GR->GetArcControlPoints(
            2*a,controlPoint,TNode(MAX_CONTROL_POINTS),
            (layoutModel==LAYOUT_ORTHO_SMALL) ? PORTS_IMPLICIT : PORTS_EXPLICIT);

        TIndex gridIndex1 = GridIndex(controlPoint[0]);

        for (TNode i=1;i<nControlPoints;++i)
        {
            TIndex gridIndex2 = GridIndex(controlPoint[i]);
            TIndex posX1 = GridPos(controlPoint[i-1],0);
            TIndex posX2 = GridPos(controlPoint[i],0);
            TIndex posY  = GridPos(controlPoint[i-1],1);

            if (posX1<posX2)
            {
                for (TIndex j=posX1+1;j<=posX2;++j)
                {
                    horizontalArcReference->ChangeKey(HoriSegmentIndex(j-1,posY),2*a);
                }
            }
            else if (posX1>posX2)
            {
                for (TIndex j=posX2+1;j<=posX1;++j)
                {
                    horizontalArcReference->ChangeKey(HoriSegmentIndex(j-1,posY),2*a+1);
                }
            }

            gridIndex1 = gridIndex2;
        }
    }

    return horizontalArcReference;
}
