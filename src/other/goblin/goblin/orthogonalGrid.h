
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file orthogonalGrid.h
/// \brief Class interface of a stripe dissection model


#ifndef _ORTHOGONAL_GRID_H_
#define _ORTHOGONAL_GRID_H_

#include "hashTable.h"
#include "dynamicQueue.h"
#include "sparseDigraph.h"


/// \addtogroup groupOrthogonalDrawing
/// @{

/// \brief  A class for analysis and post-optimization of orthogonal grid representations
///
/// This class keeps tools for analysis and post-optimization of 2D orthogonal grid
/// representations.

class orthogonalGrid : protected managedObject
{
friend class stripeDissectionModel;
friend class movingLineModel;

private:

    abstractMixedGraph &G;    ///< The addressed graph object
    goblinController &CFG;    ///< The context of the addressed object
    sparseRepresentation* GR; ///< A pointer to the representational object of G

    TLayoutModel layoutModel; ///< The current layout model, copied from the layout pool data of G

    enum {MAX_CONTROL_POINTS = 10};
    TNode controlPoint[MAX_CONTROL_POINTS]; ///< A buffer store the control points of an edge

    TFloat nodeSpacing; ///< The minimum distance between graph nodes, copied from the layout pool data of G
    TFloat bendSpacing; ///< The minimum distance between bend points, copied from the layout pool data of G

    TFloat minX; ///< The left-most position of the bounding box of G
    TFloat maxX; ///< The right-most position of the bounding box of G
    TFloat minY; ///< The top-line position of the bounding box of G
    TFloat maxY; ///< The bottom-line position of the bounding box of G

    TIndex gridSizeX; ///< The number of bend point grid columns in the bounding box, including the borders
    TIndex gridSizeY; ///< The number of bend point grid rows in the bounding box, including the borders


    enum TGridNode
    {
        GRID_NODE_EMPTY    = 0, ///< The grid node has not been assigned to a graph node or edge (yet)
        GRID_NODE_PORT     = 1, ///< The grid node is a node port
        GRID_NODE_BEND     = 2, ///< The grid node is the common end point of two edge segments
        GRID_NODE_NO_PORT  = 3, ///< The grid node is on the graph node exterior, but not a port
        GRID_NODE_NO_BEND  = 4, ///< The grid node is in the interior of an edge segment
        GRID_NODE_CROSSING = 5, ///< The grid node is in the interior of two edge segments
        GRID_NODE_INVALID  = 6  ///< At this grid node, 
    };

    enum TGridSegment
    {
        GRID_SEGMENT_EMPTY = 0, ///< The grid segment has not been assigned to a graph node or edge (yet)
        GRID_SEGMENT_EDGE  = 1, ///< The grid segment is in the interior of an edge segment
        GRID_SEGMENT_NODE  = 2  ///< The grid segment connects two (potential) node ports
    };

    /// A hash table, assigning layout point types to the grid nodes
    goblinHashTable<TIndex,int>* primalNodeType;

    /// A hash table, assigning segment types to the horizontal grid segments
    goblinHashTable<TIndex,int>* horizontalSegmentType;

    /// A hash table, assigning segment types to the vertical grid segments
    goblinHashTable<TIndex,int>* verticalSegmentType;

public:

    orthogonalGrid(abstractMixedGraph &_G) throw(ERRejected);
    ~orthogonalGrid() throw();

    unsigned long   Allocated() const throw() {return 0;};

    unsigned long   Size() const throw() {return 0;};

    /// \brief  Return the grid row or column index for a given layout position
    /// \param layoutPos  The layout position to be mapped
    /// \param i          A coordinate index 0 or 1
    TIndex  GridPos(TFloat layoutPos,TDim i) const throw(ERRange);

    /// \brief  Return the grid row or column index of the given control point
    /// \param p  A control point index
    /// \param i  A coordinate index 0 or 1
    TIndex  GridPos(TNode p,TDim i) const throw(ERRange);

    /// \brief  Return the grid hash table index of the given control point
    /// \param p  A control point index
    TIndex  GridIndex(TNode p) const throw(ERRange);

    /// \brief  Return the grid hash table index of the given grid coordinates
    /// \param cx  A grid column index
    /// \param cy  A grid row index
    TIndex  GridIndex(TIndex cx,TIndex cy) const throw(ERRange);

    /// \brief  Return the segment hash table index of a given horizontal segment
    /// \param cx  A grid column index, of the left-hand segment end point
    /// \param cy  A grid row index
    ///
    /// This determines the index of the segment connecting (cx,cy)
    /// and (cx+1,cy) in the hash table horizontalSegmentType.
    TIndex  HoriSegmentIndex(TIndex cx,TIndex cy) const throw(ERRange);

    /// \brief  Return the segment hash table index of a given vertical segment
    /// \param cx  A grid column index
    /// \param cy  A grid row index, of the upper segment end point
    ///
    /// This determines the index of the segment connecting (cx,cy)
    /// and (cx,cy+1) in the hash table verticalSegmentType.
    TIndex  VertSegmentIndex(TIndex cx,TIndex cy) const throw(ERRange);


    /// \brief  Return the grid column index of a given horizontal segment
    /// \param segmentIndex  A horizontal grid segment hash table index
    /// \return              The grid column index cx of both segment end points
    TIndex  HoriSegmentPosX(TIndex segmentIndex) const throw(ERRange);

    /// \brief  Return the grid row index of a given horizontal segment
    /// \param segmentIndex  A horizontal grid segment hash table index
    /// \return              The grid column index of the left-hand segment end point
    TIndex  HoriSegmentPosY(TIndex segmentIndex) const throw(ERRange);

    /// \brief  Return the grid column index of a given vertical segment
    /// \param segmentIndex  A vertical grid segment hash table index
    /// \return              The grid column index of the upper segment end point
    TIndex  VertSegmentPosX(TIndex segmentIndex) const throw(ERRange);

    /// \brief  Return the grid row index of a given vertical segment
    /// \param segmentIndex  A vertical grid segment hash table index
    /// \return              The grid row index of both segment end points
    TIndex  VertSegmentPosY(TIndex segmentIndex) const throw(ERRange);


    /// \brief  Setup the grid hash table and both segment hash tables
    /// \retval true  If collisions have been detected
    ///
    /// This allows for edge-edge crossings, as long as both edges meet at
    /// edge segment interior grid nodes or both edges meet at port nodes.
    /// Up to the port nodes and edge-edge crossings, a grid node may be
    /// occupied by at most one one graph edge or graph node. Or grid nodes
    /// are marked GRID_NODE_INVALID, and true is returned.
    bool  ExtractPrimalGrid() throw(ERRejected);

    /// \brief  Assign an edge interior node type in the grid hash table
    /// \retval true  If a collision has been detected
    ///
    /// If the preceding grid node type is GRID_NODE_EMPTY, it is updated to
    /// GRID_NODE_NO_BEND. This also allows for edge-edge crossings, by updating
    /// a preceding grid node type GRID_NODE_NO_BEND to GRID_NODE_CROSSING.
    bool  PlaceEdgeInteriorGridNode(TIndex cx,TIndex cy) throw();

    /// \brief   Construct a mapping from the grid node indices to the original layout point indices
    /// \return  A pointer to the constructed hash table
    goblinHashTable<TIndex,TNode>* ExtractNodeReference() throw();

    /// \brief   Construct a mapping from the vertical grid edge indices to the original arc indices
    /// \return  A pointer to the constructed hash table
    goblinHashTable<TIndex,TArc>* ExtractVerticalArcReference() throw();

    /// \brief   Construct a mapping from the horizontal grid edge indices to the original arc indices
    /// \return  A pointer to the constructed hash table
    goblinHashTable<TIndex,TArc>* ExtractHorizontalArcReference() throw();

};

/// @}

#endif


//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, July 2009
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file orthogonalGrid.h
/// \brief Class interface of a stripe dissection model


#ifndef _ORTHOGONAL_GRID_H_
#define _ORTHOGONAL_GRID_H_

#include "hashTable.h"
#include "dynamicQueue.h"
#include "sparseDigraph.h"


/// \addtogroup groupOrthogonalDrawing
/// @{

/// \brief  A class for analysis and post-optimization of orthogonal grid representations
///
/// This class keeps tools for analysis and post-optimization of 2D orthogonal grid
/// representations.

class orthogonalGrid : protected managedObject
{
friend class stripeDissectionModel;
friend class movingLineModel;

private:

    abstractMixedGraph &G;    ///< The addressed graph object
    goblinController &CFG;    ///< The context of the addressed object
    sparseRepresentation* GR; ///< A pointer to the representational object of G

    TLayoutModel layoutModel; ///< The current layout model, copied from the layout pool data of G

    enum {MAX_CONTROL_POINTS = 10};
    TNode controlPoint[MAX_CONTROL_POINTS]; ///< A buffer store the control points of an edge

    TFloat nodeSpacing; ///< The minimum distance between graph nodes, copied from the layout pool data of G
    TFloat bendSpacing; ///< The minimum distance between bend points, copied from the layout pool data of G

    TFloat minX; ///< The left-most position of the bounding box of G
    TFloat maxX; ///< The right-most position of the bounding box of G
    TFloat minY; ///< The top-line position of the bounding box of G
    TFloat maxY; ///< The bottom-line position of the bounding box of G

    TIndex gridSizeX; ///< The number of bend point grid columns in the bounding box, including the borders
    TIndex gridSizeY; ///< The number of bend point grid rows in the bounding box, including the borders


    enum TGridNode
    {
        GRID_NODE_EMPTY    = 0, ///< The grid node has not been assigned to a graph node or edge (yet)
        GRID_NODE_PORT     = 1, ///< The grid node is a node port
        GRID_NODE_BEND     = 2, ///< The grid node is the common end point of two edge segments
        GRID_NODE_NO_PORT  = 3, ///< The grid node is on the graph node exterior, but not a port
        GRID_NODE_NO_BEND  = 4, ///< The grid node is in the interior of an edge segment
        GRID_NODE_CROSSING = 5, ///< The grid node is in the interior of two edge segments
        GRID_NODE_INVALID  = 6  ///< At this grid node, 
    };

    enum TGridSegment
    {
        GRID_SEGMENT_EMPTY = 0, ///< The grid segment has not been assigned to a graph node or edge (yet)
        GRID_SEGMENT_EDGE  = 1, ///< The grid segment is in the interior of an edge segment
        GRID_SEGMENT_NODE  = 2  ///< The grid segment connects two (potential) node ports
    };

    /// A hash table, assigning layout point types to the grid nodes
    goblinHashTable<TIndex,int>* primalNodeType;

    /// A hash table, assigning segment types to the horizontal grid segments
    goblinHashTable<TIndex,int>* horizontalSegmentType;

    /// A hash table, assigning segment types to the vertical grid segments
    goblinHashTable<TIndex,int>* verticalSegmentType;

public:

    orthogonalGrid(abstractMixedGraph &_G) throw(ERRejected);
    ~orthogonalGrid() throw();

    unsigned long   Allocated() const throw() {return 0;};

    unsigned long   Size() const throw() {return 0;};

    /// \brief  Return the grid row or column index for a given layout position
    /// \param layoutPos  The layout position to be mapped
    /// \param i          A coordinate index 0 or 1
    TIndex  GridPos(TFloat layoutPos,TDim i) const throw(ERRange);

    /// \brief  Return the grid row or column index of the given control point
    /// \param p  A control point index
    /// \param i  A coordinate index 0 or 1
    TIndex  GridPos(TNode p,TDim i) const throw(ERRange);

    /// \brief  Return the grid hash table index of the given control point
    /// \param p  A control point index
    TIndex  GridIndex(TNode p) const throw(ERRange);

    /// \brief  Return the grid hash table index of the given grid coordinates
    /// \param cx  A grid column index
    /// \param cy  A grid row index
    TIndex  GridIndex(TIndex cx,TIndex cy) const throw(ERRange);

    /// \brief  Return the segment hash table index of a given horizontal segment
    /// \param cx  A grid column index, of the left-hand segment end point
    /// \param cy  A grid row index
    ///
    /// This determines the index of the segment connecting (cx,cy)
    /// and (cx+1,cy) in the hash table horizontalSegmentType.
    TIndex  HoriSegmentIndex(TIndex cx,TIndex cy) const throw(ERRange);

    /// \brief  Return the segment hash table index of a given vertical segment
    /// \param cx  A grid column index
    /// \param cy  A grid row index, of the upper segment end point
    ///
    /// This determines the index of the segment connecting (cx,cy)
    /// and (cx,cy+1) in the hash table verticalSegmentType.
    TIndex  VertSegmentIndex(TIndex cx,TIndex cy) const throw(ERRange);


    /// \brief  Return the grid column index of a given horizontal segment
    /// \param segmentIndex  A horizontal grid segment hash table index
    /// \return              The grid column index cx of both segment end points
    TIndex  HoriSegmentPosX(TIndex segmentIndex) const throw(ERRange);

    /// \brief  Return the grid row index of a given horizontal segment
    /// \param segmentIndex  A horizontal grid segment hash table index
    /// \return              The grid column index of the left-hand segment end point
    TIndex  HoriSegmentPosY(TIndex segmentIndex) const throw(ERRange);

    /// \brief  Return the grid column index of a given vertical segment
    /// \param segmentIndex  A vertical grid segment hash table index
    /// \return              The grid column index of the upper segment end point
    TIndex  VertSegmentPosX(TIndex segmentIndex) const throw(ERRange);

    /// \brief  Return the grid row index of a given vertical segment
    /// \param segmentIndex  A vertical grid segment hash table index
    /// \return              The grid row index of both segment end points
    TIndex  VertSegmentPosY(TIndex segmentIndex) const throw(ERRange);


    /// \brief  Setup the grid hash table and both segment hash tables
    /// \retval true  If collisions have been detected
    ///
    /// This allows for edge-edge crossings, as long as both edges meet at
    /// edge segment interior grid nodes or both edges meet at port nodes.
    /// Up to the port nodes and edge-edge crossings, a grid node may be
    /// occupied by at most one one graph edge or graph node. Or grid nodes
    /// are marked GRID_NODE_INVALID, and true is returned.
    bool  ExtractPrimalGrid() throw(ERRejected);

    /// \brief  Assign an edge interior node type in the grid hash table
    /// \retval true  If a collision has been detected
    ///
    /// If the preceding grid node type is GRID_NODE_EMPTY, it is updated to
    /// GRID_NODE_NO_BEND. This also allows for edge-edge crossings, by updating
    /// a preceding grid node type GRID_NODE_NO_BEND to GRID_NODE_CROSSING.
    bool  PlaceEdgeInteriorGridNode(TIndex cx,TIndex cy) throw();

    /// \brief   Construct a mapping from the grid node indices to the original layout point indices
    /// \return  A pointer to the constructed hash table
    goblinHashTable<TIndex,TNode>* ExtractNodeReference() throw();

    /// \brief   Construct a mapping from the vertical grid edge indices to the original arc indices
    /// \return  A pointer to the constructed hash table
    goblinHashTable<TIndex,TArc>* ExtractVerticalArcReference() throw();

    /// \brief   Construct a mapping from the horizontal grid edge indices to the original arc indices
    /// \return  A pointer to the constructed hash table
    goblinHashTable<TIndex,TArc>* ExtractHorizontalArcReference() throw();

};

/// @}

#endif

