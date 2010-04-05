
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, August 2000
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file canvasBuilder.h
/// \brief #canvasBuilder class interface

#ifndef _CANVAS_BUILDER_H_
#define _CANVAS_BUILDER_H_

#include "graphDisplayProxy.h"


/// \addtogroup groupCanvasBuilder
/// @{

/// \brief Builder class interface for displaying a canvas or writing canvas files
///
/// This class partially implements the drawing of graph objects in an unspecified
/// file format of graphical tool kit. The class interface also declares the
/// methods which must be implemented by concrete canvas buiders.

class canvasBuilder : protected managedObject
{
protected:

    const abstractMixedGraph &G; ///< The graph object to be displayed
    goblinController CFG;        ///< The context of the displayed object
    graphDisplayProxy DP;        ///< 

    enum {LABEL_BUFFER_SIZE = 256};
    char tmpLabelBuffer[LABEL_BUFFER_SIZE];

    double minX;
    double maxX;
    double minY;
    double maxY;

public:

    canvasBuilder(const abstractMixedGraph &_G,double _pixelWidth=1.0,double _pixelHeight=1.0)
        throw(ERRejected);
    virtual ~canvasBuilder() throw();

    unsigned long   Allocated() const throw();

    void            ComputeBoundingBox() throw();

    int             RoundToInt(double val) const throw()
                        {return ((val<0) ? int(val-0.5) : int(val+0.5));};
    long            RoundToLong(double val) const throw()
                        {return ((val<0) ? long(val-0.5) : long(val+0.5));};

protected:

    /// \brief Applied port mode, depending on the layout model of this graph object
    TPortMode portMode;

public:

    goblinController &Configuration() throw() {return CFG;};

    virtual void    DisplayGraph() throw(ERRejected);

    enum TArcShapeMode
    {
        ARC_SHAPE_POLYGONES = 0,
        ARC_SHAPE_SMOOTH    = 1
    };

    enum TArrowPosMode
    {
        ARROWS_AT_ENDS  = 0,
        ARROWS_CENTERED = 1
    };

    enum TGridDisplayMode
    {
        GRID_DISPLAY_OFF = 0,
        GRID_DISPLAY_ALL = 1
    };

    /// Font types as expected in the *.fig file format
    enum TXFigFontType
    {
        FONT_TIMES_ROMAN = 0,
        FONT_TIMES_ITALIC = 1,
        FONT_TIMES_BOLD = 2,
        FONT_TIMES_BOLD_ITALIC = 3,
        FONT_AVANTGARDE_BOOK = 4,
        FONT_AVANTGARDE_BOOK_OBLIQUE = 5,
        FONT_AVANTGARDE_DEMI = 6,
        FONT_AVANTGARDE_DEMI_OBLIQUE = 7,
        FONT_BOOKMAN_LIGHT = 8,
        FONT_BOOKMAN_LIGHT_ITALIC = 9,
        FONT_BOOKMAN_DEMI = 10,
        FONT_BOOKMAN_DEMI_ITALIC = 11,
        FONT_COURIER = 12,
        FONT_COURIER_OBLIQUE = 13,
        FONT_COURIER_BOLD = 14,
        FONT_COURIER_BOLD_OBLIQUE = 15,
        FONT_HELVETICA = 16,
        FONT_HELVETICA_OBLIQUE = 17,
        FONT_HELVETICA_BOLD = 18,
        FONT_HELVETICA_BOLD_OBLIQUE = 19,
        FONT_HELVETICA_NARROW = 20,
        FONT_HELVETICA_NARROW_OBLIQUE = 21,
        FONT_HELVETICA_NARROW_BOLD = 22,
        FONT_HELVETICA_NARROW_BOLD_OBLIQUE = 23,
        FONT_NEW_CENTURY_SCHOOLBOOK_ROMAN = 24,
        FONT_NEW_CENTURY_SCHOOLBOOK_ITALIC = 25,
        FONT_NEW_CENTURY_SCHOOLBOOK_BOLD = 26,
        FONT_NEW_CENTURY_SCHOOLBOOK_BOLD_ITALIC = 27,
        FONT_PALATINO_ROMAN = 28,
        FONT_PALATINO_ITALIC = 29,
        FONT_PALATINO_BOLD = 30,
        FONT_PALATINO_BOLD_ITALIC = 31,
        FONT_SYMBOL = 32,
        FONT_ZAPF_CHANCERY_MEDIUM_ITALIC = 33,
        FONT_ZAPF_DINGBATS = 34
    };


    /// \brief  Extract the layout parameters from the attribute pool
    void ExtractParameters() throw();

protected:

    /// \brief Applied arc shape mode
    TArcShapeMode arcShapeMode;

    /// \brief Applied arrow position mode
    TArrowPosMode arrowPosMode;

    /// \brief Arc postscript font type as required by xfig / fig2dev
    TXFigFontType arcFontType;

    /// \brief Node postscript font type as required by xfig / fig2dev
    TXFigFontType nodeFontType;

    /// \brief Applied grid display mode
    TGridDisplayMode gridDisplayMode;

    /// \brief Spacing between adjacent graph nodes
    double nodeSpacing;

public:

    void        DisplayArc(TArc) throw();
    void        DisplayArrow(TArc,TNode,TNode) throw();


    // *************************************************************** //
    //                Prototypes                                       //
    // *************************************************************** //

    virtual void  WriteArc(TArc,TNode,TNode,TDashMode,int,TArrowDir,TIndex colourIndex,int) throw();

    void  WriteArcSegment(vector<double>& cx,vector<double>& cy,TDashMode dashMode,
            int width,TArrowDir displayedArrows,TIndex colourIndex,int depth) throw();

    virtual void  WritePolyLine(vector<double>& cx,vector<double>& cy,TDashMode dashMode,
                    int width,TArrowDir displayedArrows,TIndex colourIndex,int depth) throw() = 0;

    void  WriteStraightLine(double cx1,double cy1,double cx2,double cy2,TDashMode dashMode,
            int width,TArrowDir displayedArrows,TIndex colourIndex,int depth) throw();

    virtual void    WriteArrow(TArc a,long xtop,long ytop,double dx,double dy) throw() = 0;
    virtual void    WriteArcLabel(TArc a,long xm,long ym) throw() = 0;
    virtual void    DisplayNode(TNode) throw() = 0;
    virtual void    DisplayLegenda(long xm,long ym,long radius) throw() = 0;

    void  DisplayPageLayout() throw();

};


extern const char* unixFontType[];


/// @}

#endif
