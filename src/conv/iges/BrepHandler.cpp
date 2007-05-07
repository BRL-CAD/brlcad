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
    debug("########################### E X T R A C T   B R E P");
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
    debug("########################### E X T R A C T   S H E L L");
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
    debug("########################### E X T R A C T   F A C E"); 
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
    debug("########################## E X T R A C T   S U R F A C E");
    ParameterData params;
    _iges->getParameter(de->paramData(), params);
    return handleSurface((IGESEntity)(int)de->type(), params);
  }

  class PSpaceCurve {
  public:
    PSpaceCurve(IGES* _iges, BrepHandler* _brep, Logical& iso, Pointer& c) 
    {
      DirectoryEntry* curveDE = _iges->getDirectoryEntry(c);
      ParameterData param;
      _iges->getParameter(curveDE->paramData(), param);
    }
    PSpaceCurve(const PSpaceCurve& ps) 
      : isIso(ps.isIso), curveIndex(ps.curveIndex) {}

    Logical isIso;
    int curveIndex;
  };
      

  class EdgeUse {
  public:
    EdgeUse(IGES* _iges, BrepHandler* _brep, Pointer& edgeList, int index)
    { 
      debug("########################## E X T R A C T   E D G E  U S E");
      DirectoryEntry* edgeListDE = _iges->getDirectoryEntry(edgeList);
      ParameterData params;
      _iges->getParameter(edgeListDE->paramData(), params);
      int paramIndex = (index-1)*5 + 1;
      Pointer msCurvePtr = params.getPointer(paramIndex);
      initVertexList = params.getPointer(paramIndex+1);
      initVertexIndex = params.getInteger(paramIndex+2);
      termVertexList = params.getPointer(paramIndex+3);
      termVertexIndex = params.getInteger(paramIndex+4);
      
      // extract the model space curves - is this even valid for MSBO?
      mCurveIndex = _brep->extractCurve(_iges->getDirectoryEntry(msCurvePtr), false);
    }
    EdgeUse(const EdgeUse& eu) 
      : mCurveIndex(eu.mCurveIndex),
	pCurveIndices(eu.pCurveIndices),
	initVertexList(eu.initVertexList), 
	initVertexIndex(eu.initVertexIndex), 
	termVertexList(eu.termVertexList), 
	termVertexIndex(eu.termVertexIndex) {}
    
    int mCurveIndex; // model space curve???
    list<PSpaceCurve> pCurveIndices; // parameter space curves
    Pointer initVertexList;
    Integer initVertexIndex;
    Pointer termVertexList;
    Integer termVertexIndex;    
  };

  void 
  BrepHandler::extractLoop(const DirectoryEntry* de, bool isOuter, int face) {
    debug("########################## E X T R A C T   L O O P");
    ParameterData params;
    _iges->getParameter(de->paramData(), params);

    int loop = handleLoop(isOuter, face);

    int numberOfEdges = params.getInteger(1);

    int i = 2; // extract the edge uses!
    for (int _i = 0; _i < numberOfEdges; _i++) {
      bool isVertex = (1 == params.getInteger(i)) ? true : false;
      Pointer edgePtr = params.getPointer(i+1);
      int index = params.getInteger(i+2);
      // need to get the edge list, and extract the edge info
      EdgeUse eu(_iges, this, edgePtr, index);
      bool orientWithCurve = params.getLogical(i+3);
      int numCurves = params.getInteger(i+4);
      debug("Num param-space curves in " << string((isOuter)?"outer":"inner") << " loop: " << numCurves);
      int j = i+5;
      for (int _j = 0; _j < numCurves; _j++) {	
	Logical iso = params.getLogical(j);
	Pointer ptr = params.getPointer(j+1);
	eu.pCurveIndices.push_back(PSpaceCurve(_iges,
					       this, 
					       iso,
					       ptr));
	j += 2;
      }
      
      

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
