
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, May 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   exportToTk.h
/// \brief  #exportToTk class interface

#ifndef _EXPORT_TO_TK_H_
#define _EXPORT_TO_TK_H_

#include "canvasBuilder.h"


/// \addtogroup groupCanvasBuilder
/// @{

/// \brief This class implements a builder for Tk canvas files of graph objects

class exportToTk : public canvasBuilder
{
private:

    ofstream       expFile;

    char*          canvasName;

public:

    exportToTk(const abstractMixedGraph &GG,const char* expFileName) throw(ERFile);
    ~exportToTk() throw();

    /// \brief  Types which are interpreted by the Tcl user interface to generate canvas objects from the table in file
    enum TObjectTypeID {
        ID_GRAPH_NODE  = 0,  ///< What follows is the configuration set of a oval object associated with a graph node
        ID_GRAPH_EDGE  = 1,  ///< What follows is the configuration set of a line object associated with a graph edge
        ID_NODE_LABEL  = 2,  ///< What follows is the configuration set of a text object associated with a graph node
        ID_EDGE_LABEL  = 3,  ///< What follows is the configuration set of a text object associated with a graph edge
        ID_UPPER_LEFT  = 4,  ///< What follows is the configuration set of the upper left corner of the bounding box
        ID_LOWER_RIGHT = 5,  ///< What follows is the configuration set of the bottom right corner of the bounding box
        ID_BEND_NODE   = 6,  ///< What follows is the configuration set of a rectangle object associated with a layout bend point
        ID_ALIGN_NODE  = 7,  ///< What follows is the configuration set of a rectangle object associated with an arc label anchor
        ID_BACKGROUND  = 8,  ///< What follows is the configuration set of the background image
        ID_PRED_ARC    = 9,  ///<
        ID_ARROW       = 10, ///< What follows is the configuration set of a polygone object associated with a graph arc
        ID_GRID_LINE   = 11  ///< What follows is the configuration set of a grid line
    };

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    void    DisplayLegenda(long xm,long ym,long radius) throw();
    void    WriteNodeLegenda(long x,long y,char* nodeLabel) throw();

    void    WritePolyLine(vector<double>& cx,vector<double>& cy,
                TDashMode dashMode,int width,TArrowDir displayedArrows,
                TIndex colourIndex,int depth) throw();
    void    WriteArrow(TArc a,long xtop,long ytop,double dx,double dy) throw();
    void    WriteArcLabel(TArc a,long xm,long ym) throw();

    void    DisplayNode(TNode v) throw();
    void    WriteSmallNode(TNode v,long x,long y) throw();
    void    WriteCircularNode(TNode v,long x,long y,char* fillColour) throw();
    void    WriteRectangularNode(TNode v,long x,long y,char* fillColour) throw();
    void    WriteRegularNode(TNode v,long x,long y,char* fillColour) throw();
    void    WriteNodeLabel(TNode v,long x,long y) throw();

    void    DisplayArtificialNode(TNode v) throw();

};

/// @}

#endif
