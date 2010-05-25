
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, March 2007
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   exportToDot.h
/// \brief  #exportToDot class interface

#ifndef _EXPORT_TO_DOT_H_
#define _EXPORT_TO_DOT_H_

#include "canvasBuilder.h"


/// \addtogroup groupCanvasBuilder
/// @{

/// \brief This class implements a builder for graphviz dot files

class exportToDot : public canvasBuilder
{
private:

    ofstream       expFile;

    char*          canvasName;

public:

    exportToDot(const abstractMixedGraph &GG,const char* expFileName) throw(ERFile);
    ~exportToDot() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    void    DisplayLegenda(long xm,long ym,long radius) throw() {};

    void    WriteArc(TArc,TNode,TNode,TDashMode,int,TArrowDir,TIndex colourIndex,int depth) throw();
    void    WritePolyLine(vector<double>& cx,vector<double>& cy,TDashMode dashMode,
                int width,TArrowDir displayedArrows,TIndex colourIndex,int depth) throw() {};
    void    WriteArrow(TArc a,long xtop,long ytop,double dx,double dy) throw() {};
    void    WriteArcLabel(TArc a,long xm,long ym) throw() {};

    void    DisplayNode(TNode v) throw();

};

/// @}

#endif

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, March 2007
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   exportToDot.h
/// \brief  #exportToDot class interface

#ifndef _EXPORT_TO_DOT_H_
#define _EXPORT_TO_DOT_H_

#include "canvasBuilder.h"


/// \addtogroup groupCanvasBuilder
/// @{

/// \brief This class implements a builder for graphviz dot files

class exportToDot : public canvasBuilder
{
private:

    ofstream       expFile;

    char*          canvasName;

public:

    exportToDot(const abstractMixedGraph &GG,const char* expFileName) throw(ERFile);
    ~exportToDot() throw();

    unsigned long   Size() const throw();
    unsigned long   Allocated() const throw();

    void    DisplayLegenda(long xm,long ym,long radius) throw() {};

    void    WriteArc(TArc,TNode,TNode,TDashMode,int,TArrowDir,TIndex colourIndex,int depth) throw();
    void    WritePolyLine(vector<double>& cx,vector<double>& cy,TDashMode dashMode,
                int width,TArrowDir displayedArrows,TIndex colourIndex,int depth) throw() {};
    void    WriteArrow(TArc a,long xtop,long ytop,double dx,double dy) throw() {};
    void    WriteArcLabel(TArc a,long xm,long ym) throw() {};

    void    DisplayNode(TNode v) throw();

};

/// @}

#endif
