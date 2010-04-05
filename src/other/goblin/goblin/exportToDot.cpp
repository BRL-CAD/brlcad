
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, March 2007
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   exportToDot.cpp
/// \brief  #exportToDot class implementation

#include "exportToDot.h"


exportToDot::exportToDot(const abstractMixedGraph& _G,const char* expFileName)
    throw(ERFile) : canvasBuilder(_G), expFile(expFileName, ios::out)
{
    if (!expFile)
    {
        sprintf(CT.logBuffer,"Could not open export file %s, io_state %d",
            expFileName,expFile.rdstate());
        Error(ERR_FILE,"exportToDot",CT.logBuffer);
    }

    expFile.flags(expFile.flags() | ios::right);
    expFile.setf(ios::floatfield);
    expFile.precision(5);

    if (G.IsUndirected())
    {
        expFile << "graph G {" << endl;
    }
    else
    {
        expFile << "digraph G {" << endl;
    }
}


unsigned long exportToDot::Size() const throw()
{
    return
          sizeof(exportToDot)
        + managedObject::Allocated();
}


unsigned long exportToDot::Allocated() const throw()
{
    return 0;
}


void exportToDot::WriteArc(TArc a,TNode u,TNode v,TDashMode dashMode,int width,
    TArrowDir displayedArrows,TIndex colourIndex,int depth) throw()
{
    char rgbColour[8];

    if (DP.arcColourMode!=graphDisplayProxy::ARCS_FLOATING_COLOURS)
    {
        sprintf(rgbColour,"#%06lX",DP.RGBFixedColour(colourIndex));
    }
    else
    {
        sprintf(rgbColour,"#%06lX",DP.RGBSmoothColour(colourIndex,ZERO_COLOUR+DP.maxEdgeColour));
    }

    expFile << "  v" << u;

    if (displayedArrows==ARROW_FORWARD)
    {
        expFile << " -> ";
    }
    else
    {
        expFile << " -- ";
    }

    expFile << "v" << v << " ";

    expFile << "[color = \"" << rgbColour << "\", label = \"";
    expFile << DP.CompoundArcLabel(tmpLabelBuffer,LABEL_BUFFER_SIZE,2*a);

    expFile << "\"";

    const char* dashModes[] = {"",", style = dotted",", style = dashed",", style = bold"};
    expFile << dashModes[dashMode%4];

    expFile << "];" << endl;
}


void exportToDot::DisplayNode(TNode v) throw()
{
    expFile << "  v" << v << " [";

    char fillColour[8];
    DP.CanvasNodeColour(fillColour,v);

    expFile << "style = filled, fillcolor = \"" << fillColour << "\", label = \"";
    expFile << DP.CompoundNodeLabel(tmpLabelBuffer,LABEL_BUFFER_SIZE,v);
    expFile << "\", shape = ";

    switch (DP.NodeShapeMode(v))
    {
        case NODE_SHAPE_POINT:
        {
            expFile << "point";
            break;
        }
        case NODE_SHAPE_CIRCULAR:
        {
            expFile << "circle";
            break;
        }
        case NODE_SHAPE_BOX:
        {
            expFile << "box";
            break;
        }
        case NODE_SHAPE_BY_COLOUR:
        {
            if (G.NodeColour(v)==0)
            {
                expFile << "circle";
            }
            else
            {
                expFile << "box";
            }

            break;
        }
    }

    expFile << "];" << endl;
}


exportToDot::~exportToDot() throw()
{
    expFile << "}" << endl;
    expFile.close();
}
