
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, October 2005
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file incrementalGeometry.h
/// \brief #incrementalGeometry class interface

#ifndef _INCREMENTAL_GEOMETRY_H_
#define _INCREMENTAL_GEOMETRY_H_

#include "abstractMixedGraph.h"


/// \brief A class for efficient computation of node coordinates relative to other node
///
/// Nodes are added sequentially by specifying the position to other nodes
/// which have been added in advance. When all nodes have been added,
/// absolute coordinates are determined

class incrementalGeometry : public managedObject
{
private:

    abstractMixedGraph& G;

    TArc            m;
    TArc            l;

    TArc*           rowID;
    TArc*           colID;

    TArc            maxRowNumber;
    TArc            maxColumnNumber;

    TArc*           prev;
    TArc*           next;

    TArc*           idToNumber;

    TArc            freeID;
    TArc            leftMost;
    TArc            rightMost;
    TArc            topLine;
    TArc            bottomLine;
    bool            numbersValid;

public:

    incrementalGeometry(abstractMixedGraph&,TArc) throw();
    ~incrementalGeometry() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    char*           Display() const throw();

    void            Init(TArc) throw(ERRange);

    void            InsertColumnLeftOf(TArc,TArc) throw(ERRange,ERRejected);
    void            InsertColumnRightOf(TArc,TArc) throw(ERRange,ERRejected);
    void            InsertRowAtopOf(TArc,TArc) throw(ERRange,ERRejected);
    void            InsertRowBelowOf(TArc,TArc) throw(ERRange,ERRejected);

    void            ShareRowWith(TArc,TArc) throw(ERRange,ERRejected);
    void            ShareColumnWith(TArc,TArc) throw(ERRange,ERRejected);

    void            AssignNumbers() throw(ERRejected);
    TArc            RowNumber(TArc) throw(ERRange,ERRejected);
    TArc            ColumnNumber(TArc) throw(ERRange,ERRejected);

    TArc            MaxRowNumber() throw(ERRejected);
    TArc            MaxColumnNumber() throw(ERRejected);

};


#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, October 2005
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file incrementalGeometry.h
/// \brief #incrementalGeometry class interface

#ifndef _INCREMENTAL_GEOMETRY_H_
#define _INCREMENTAL_GEOMETRY_H_

#include "abstractMixedGraph.h"


/// \brief A class for efficient computation of node coordinates relative to other node
///
/// Nodes are added sequentially by specifying the position to other nodes
/// which have been added in advance. When all nodes have been added,
/// absolute coordinates are determined

class incrementalGeometry : public managedObject
{
private:

    abstractMixedGraph& G;

    TArc            m;
    TArc            l;

    TArc*           rowID;
    TArc*           colID;

    TArc            maxRowNumber;
    TArc            maxColumnNumber;

    TArc*           prev;
    TArc*           next;

    TArc*           idToNumber;

    TArc            freeID;
    TArc            leftMost;
    TArc            rightMost;
    TArc            topLine;
    TArc            bottomLine;
    bool            numbersValid;

public:

    incrementalGeometry(abstractMixedGraph&,TArc) throw();
    ~incrementalGeometry() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();
    char*           Display() const throw();

    void            Init(TArc) throw(ERRange);

    void            InsertColumnLeftOf(TArc,TArc) throw(ERRange,ERRejected);
    void            InsertColumnRightOf(TArc,TArc) throw(ERRange,ERRejected);
    void            InsertRowAtopOf(TArc,TArc) throw(ERRange,ERRejected);
    void            InsertRowBelowOf(TArc,TArc) throw(ERRange,ERRejected);

    void            ShareRowWith(TArc,TArc) throw(ERRange,ERRejected);
    void            ShareColumnWith(TArc,TArc) throw(ERRange,ERRejected);

    void            AssignNumbers() throw(ERRejected);
    TArc            RowNumber(TArc) throw(ERRange,ERRejected);
    TArc            ColumnNumber(TArc) throw(ERRange,ERRejected);

    TArc            MaxRowNumber() throw(ERRejected);
    TArc            MaxColumnNumber() throw(ERRejected);

};


#endif
