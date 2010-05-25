
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   exportToXFig.h
/// \brief  #exportToXFig class interface

#ifndef _EXPORT_TO_XFIG_H_
#define _EXPORT_TO_XFIG_H_

#include "canvasBuilder.h"


/// \addtogroup groupCanvasBuilder
/// @{

/// \brief This class implements a builder for xfig canvas files of graph objects

class exportToXFig : public canvasBuilder
{
private:

    ofstream    expFile;

public:

    exportToXFig(const abstractMixedGraph &GG,const char* expFileName) throw(ERFile);
    ~exportToXFig() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    void    DisplayLegenda(long xm,long ym,long radius) throw();
    void    WriteNodeLegenda(int x,int y,char* nodeLabel) throw();

    void    WritePolyLine(vector<double>& cx,vector<double>& cy,
                TDashMode dashMode,int width,TArrowDir displayedArrows,
                TIndex colourIndex,int depth) throw();
    void    WriteArrow(TArc a,long xtop,long ytop,double dx,double dy) throw();
    void    WriteArcLabel(TArc a,long xm,long ym) throw();

    void    DisplayNode(TNode v) throw();
    void    WriteSmallNode(TNode v,int x,int y,int outlineColour,int fillColour) throw();
    void    WriteCircularNode(TNode v,int x,int y,int outlineColour,int fillColour) throw();
    void    WriteRectangularNode(TNode v,int x,int y,int outlineColour,int fillColour) throw();
    void    WriteRegularNode(TNode v,int x,int y,int outlineColour,int fillColour) throw();
    void    WriteNodeLabel(TNode v,int x,int y) throw();

};

/// @}

#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   exportToXFig.h
/// \brief  #exportToXFig class interface

#ifndef _EXPORT_TO_XFIG_H_
#define _EXPORT_TO_XFIG_H_

#include "canvasBuilder.h"


/// \addtogroup groupCanvasBuilder
/// @{

/// \brief This class implements a builder for xfig canvas files of graph objects

class exportToXFig : public canvasBuilder
{
private:

    ofstream    expFile;

public:

    exportToXFig(const abstractMixedGraph &GG,const char* expFileName) throw(ERFile);
    ~exportToXFig() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    void    DisplayLegenda(long xm,long ym,long radius) throw();
    void    WriteNodeLegenda(int x,int y,char* nodeLabel) throw();

    void    WritePolyLine(vector<double>& cx,vector<double>& cy,
                TDashMode dashMode,int width,TArrowDir displayedArrows,
                TIndex colourIndex,int depth) throw();
    void    WriteArrow(TArc a,long xtop,long ytop,double dx,double dy) throw();
    void    WriteArcLabel(TArc a,long xm,long ym) throw();

    void    DisplayNode(TNode v) throw();
    void    WriteSmallNode(TNode v,int x,int y,int outlineColour,int fillColour) throw();
    void    WriteCircularNode(TNode v,int x,int y,int outlineColour,int fillColour) throw();
    void    WriteRectangularNode(TNode v,int x,int y,int outlineColour,int fillColour) throw();
    void    WriteRegularNode(TNode v,int x,int y,int outlineColour,int fillColour) throw();
    void    WriteNodeLabel(TNode v,int x,int y) throw();

};

/// @}

#endif
