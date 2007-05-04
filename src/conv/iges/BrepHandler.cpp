#include "n_iges.hpp"

#include <iostream>
using namespace std;

namespace brlcad {

  BrepHandler::BrepHandler() {}

  BrepHandler::~BrepHandler() {}

  void
  BrepHandler::extract(IGES* iges, const DirectoryEntry* de) {
    _iges = iges;
    extractBrep(de);
  }

  void 
  BrepHandler::extractBrep(const DirectoryEntry* de) {
    ParameterData params;
    _iges->getParameter(de->paramData(), params);
    
    Pointer shell = params.getPointer(1);
    extractShell(_iges->getDirectoryEntry(shell), false, params.getLogical(2));
    int numVoids = params.getInteger(3);
    
    if (numVoids <= 0) return;    
    
    int index = 4;
    for (int i = 0; i < numVoids; i++) {
      shell = params.getPointer(index);      
      extractShell(_iges->getDirectoryEntry(shell),
		   true,
		   params.getLogical(index+1));
      index += 2;
    }
  }
  
  void
  BrepHandler::extractShell(const DirectoryEntry* de, bool isVoid, bool orientWithFace) {
    ParameterData params;
    _iges->getParameter(de->paramData(), params);
    
    handleShell(isVoid, orientWithFace);

    int numFaces = params.getInteger(1);
    for (int i = 1; i <= numFaces; i++) {
      Pointer facePtr = params.getPointer(i*2);
      bool orientWithSurface = params.getLogical(i*2+1);
      DirectoryEntry* faceDE = _iges->getDirectoryEntry(facePtr);
      extractFace(faceDE, orientWithSurface);
    }
  }

  void 
  BrepHandler::extractFace(const DirectoryEntry* de, bool orientWithSurface) {
    ParameterData params;
    _iges->getParameter(de->paramData(), params);
        
    Pointer surfaceDE = params.getPointer(1);
    int surf = extractSurface(_iges->getDirectoryEntry(surfaceDE));

    int face = handleFace(orientWithSurface, surf);

    int numLoops = params.getInteger(2);
    bool isOuter = params.getLogical(3);    
    for (int i = 4; (i-4) < numLoops; i++) {
      Pointer loopDE = params.getPointer(i);
      extractLoop(_iges->getDirectoryEntry(loopDE), isOuter, face);
      isOuter = false;
    }
  }

  int
  BrepHandler::extractSurface(const DirectoryEntry* de) {
    ParameterData params;
    _iges->getParameter(de->paramData(), params);
    return handleSurface((IGESEntity)(int)de->type(), params);
  }

  class EdgeList {
  public:
    
  };

  DirectoryEntry* 
  BrepHandler::getEdge(Pointer& edgeList, int index) {    
    DirectoryEntry* edgeListDE = _iges->getDirectoryEntry(edgeList);
    
  }

  void 
  BrepHandler::extractLoop(const DirectoryEntry* de, bool isOuter, int face) {
    ParameterData params;
    _iges->getParameter(de->paramData(), params);

    int loop = handleLoop(isOuter, face);

    int numberOfEdges = params.getInteger(1);

    int i = 2; // extract the edge uses!
    for (int _i = 0; _i < numberOfEdges; _i++) {
      bool isVertex = (1 == 
params.getInteger(i)) ? true : false;
      Pointer edgePtr = params.getPointer(i+1);
      int index = params.getPointer(i+2);
      bool orientWithCurve = params.getLogical(i+3);
      int numCurves = params.getLogical(i+4);
      int j = i+5;
      list<int> curve_list;
      for (int _j = 0; _j < numCurves; _j++) {	
	bool isIso = params.getLogical(j);
	Pointer curvePtr = params.getPointer(j+1);
	curve_list.push_back(extractCurve(_iges->getDirectoryEntry(curvePtr), isIso));
	j += 2;
      }
      
      // need to get the edge list, and extract the edge info
      DirectoryEntry* edgeDE = getEdge(edgePtr, index);
      //extractEdge(, isVertex, 


      i = j;      
    }
  }

  void 
  BrepHandler::extractEdge(const DirectoryEntry* de) {
    
  }
  
  void 
  BrepHandler::extractVertex(const DirectoryEntry* de) {

  }

  int 
  BrepHandler::extractCurve(const DirectoryEntry* de, bool isISO) {

  }

}
