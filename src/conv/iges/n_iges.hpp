#ifndef N_IGES_H
#define N_IGES_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <list>

#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "db.h"

#define DEBUG 1

#if DEBUG
#define debug(l) cout << l << endl;
#else
#define debug(l)
#endif

using namespace std;

namespace brlcad {

  template<class T>
  class IgesDataType {
  public:    
    IgesDataType(T val) {
      _val = val;
    }
    IgesDataType() {}
    IgesDataType(const IgesDataType<T>& i) {
      _val = i._val;
    }     
    
    bool operator==(const IgesDataType<T>& i) {
      return _val == i._val;
    }
    bool operator==(T v) {
      return _val == v;
    }

    IgesDataType<T>& operator=(const IgesDataType<T>& p) {
      _val = p._val;
      return *this;
    }
    IgesDataType<T>& operator=(T v) {
      _val = v;
      return *this;
    }

    bool operator<(const IgesDataType<T>& i) { 
      return  _val < i._val;
    }
    bool operator<(T v) {
      return _val < v;
    }

    T operator()() const { return _val; }
    operator T() const { return _val; }        

    virtual FILE* read(FILE* in) = 0;
    virtual string write(FILE* out) = 0;
  protected:
    T _val;    
  };

  
  class Integer : public IgesDataType<long> {
  public:
    Integer();
    Integer(long val);
    Integer(const string& field);
    Integer(const Integer& intg);
    FILE* read(FILE* in);
    string write(FILE* out);
  };
  
  class Pointer : public IgesDataType<int> {
  public:
    Pointer();
    Pointer(long val);
    Pointer(const string& field);
    Pointer(const Pointer& ptr);
    FILE* read(FILE* in);
    string write(FILE* out);
  };
  
  class Real : public IgesDataType<double> {
  public:
    Real();
    Real(double v);
    Real(const string& field);
    Real(const Real& real);
    FILE* read(FILE* in);
    string write(FILE* out);
  };
  
  class String : public IgesDataType<string> {
  public:
    String();
    String(const string& field);
    String(const String& str);
    String(const char* str);
    FILE* read(FILE* in);
    string write(FILE* out);
  };
  
  class Logical : public IgesDataType<bool> {
  public:
    Logical();
    Logical(const string& field);
    Logical(bool b);
    Logical(const Logical& log);
    FILE* read(FILE* in);
    string write(FILE* out);
  };

  class GlobalSection {
  public:
    GlobalSection(const string& in);
    GlobalSection();

    void read(const string& in);

    string _params[26];

    string paramDelim() { return String(_params[0]); }
    string recordDelim() { return String(_params[1]); }
    string productIDSending() { return String(_params[2]); }
    string filename() { return String(_params[3]); }
    string systemID() { return String(_params[4]); }
    string preprocessorVersion() { return String(_params[5]); }
    int    numIntegerBits()  { return Integer(_params[6]); }
    int    maxSingleExponent() { return Integer(_params[7]); }
    int    numSingleSignificant() { return Integer(_params[8]); }
    int    maxDoubleExponent() { return Integer(_params[9]); }
    int    numDoubleSignificant() { return Integer(_params[10]); }
    string productIDReceiving() { return String(_params[11]); }
    double modelSpaceScale() { return Real(_params[12]); }
    int    unitsFlag() { return Integer(_params[13]); }
    string unitsName() { return String(_params[14]); }
    int    maxLineWeightGradations() { return Integer(_params[15]); }
    double maxLineWeightUnits() { return Real(_params[16]); }
    string fileGenerationDateTime() { return String(_params[17]); }
    double minUserResolution() { return Real(_params[18]); }
    double approxMaxCoordinateValue() { return Real(_params[19]); }
    string authorName() { return String(_params[20]); }
    string authorOrganization() { return String(_params[21]); }
    int    specFlag() { return Integer(_params[22]); }
    int    draftFlag() { return Integer(_params[23]); }
    string createdModifiedDateTime() { return String(_params[24]); }
    string appProtocol() { return String(_params[25]); }

    string toString() {
      ostringstream ss;
      ss << paramDelim() << endl;
      ss << recordDelim() << endl;
      ss << productIDSending() << endl;
      ss << filename() << endl;
      ss << systemID() << endl;
      ss << preprocessorVersion() <<  endl;
      ss <<  numIntegerBits()  << endl;
      ss <<  maxSingleExponent() << endl; 
      ss <<  numSingleSignificant() << endl;
      ss <<  maxDoubleExponent() << endl;
      ss <<  numDoubleSignificant() << endl;
      ss << productIDReceiving() << endl;
      ss << modelSpaceScale() << endl;
      ss <<  unitsFlag() << endl;
      ss << unitsName() << endl;
      ss <<  maxLineWeightGradations() << endl;
      ss << maxLineWeightUnits() << endl;
      ss << fileGenerationDateTime() << endl;
      ss << minUserResolution() << endl;
      ss << approxMaxCoordinateValue() << endl;
      ss << authorName() << endl;
      ss << authorOrganization() << endl;
      ss <<  specFlag() << endl;
      ss <<  draftFlag() << endl;
      ss << createdModifiedDateTime() << endl;
      ss << appProtocol() << endl;
      return ss.str();
    }
  };
  
  template<class A, class B>
  class Dual {
  public:    
    static const int LEFT = 0;
    static const int RIGHT = 1;

    Dual() : _which(LEFT) {}
    Dual(A a) : _which(LEFT), _left(a) {}
    Dual(B b) : _which(RIGHT), _right(b) {}
    Dual(const Dual& dual) {
      _which = dual._which;
      _left = dual._left;
      _right = dual._right;
    }

    bool isLeft() { return _which == LEFT; }
    bool isRight() { return _which == RIGHT; }
    A left() { return _left; }
    B right() { return _right; }

  private:
    int _which;
    A _left;
    B _right;
  };

  typedef enum {
    VISIBLE, BLANKED
  } Visibility;

  typedef enum {
    INDEPENDENT, PHYS_DEPENDENT, LOGIC_DEPENDENT, BOTH_DEPENDENT
  } Subordinate;

  typedef enum {
    GEOMETRY, ANNOTATION, DEFINITION, OTHER, LOGICAL_POSITIONAL, TWODPARAMETRIC, CONSTRUCTION_GEOMETRY
  } EntityUse;

  typedef enum {
    GLOBAL_TOP_DOWN, GLOBAL_DEFER, USE_HIERARCHY_PROP
  } Hierarchy;

  typedef Dual<Integer,Pointer> DualIP;

  class DirectoryEntry {
  public:
    
    DirectoryEntry(const string& in);

    Integer type() const { return _type; }
    Pointer paramData() const { return _paramData; }
    Pointer structure() const { return _structure; }
    Dual<Integer,Pointer> lineFontPattern() const { return _lineFontPattern; }
    Dual<Integer,Pointer> level() const { return _level; }
    Pointer view() const { return _view; }
    Pointer xform() const { return _xform; }
    Pointer label() const { return _label; }
    Visibility visible() const { return _visible; }
    Subordinate subordinate() const { return _subordinate; }
    EntityUse use() const { return _use; }
    Hierarchy hierarchy() const { return _hierarchy; }
    Integer lineWeight() const { return _lineWeight; }
    Dual<Integer,Pointer> color() const { return _color; }
    Integer parameterLineCount() const { return _parameterLineCount; }
    Integer formId() const { return _formId; }
    String entityLabel() const { return _entityLabel; }
    Integer entitySubscript() const { return _entitySubscript; }    
    
    string toString();

  private:
    static const int FIELD_WIDTH = 8;

    string _rawEntry;
    string _field(int index); // 1-based index entry (match to spec)
    void _parseStatus(const string& status);

    Integer _type;
    Pointer _paramData; // pointer to parameter data
    Pointer _structure; // negated pointer to definition entity or 0
    Dual<Integer,Pointer> _lineFontPattern; // integer if defines a known lineFontPattern, pointer to pattern definition DE otherwise
    Dual<Integer,Pointer> _level; // the level, or pointer to level definition DE
    Pointer _view; 
    Pointer _xform; // or 0 (identity)
    Pointer _label; // or 0
    Visibility _visible; // visible or blanked
    Subordinate _subordinate;
    EntityUse _use;
    Hierarchy _hierarchy;
    Integer _lineWeight;
    Dual<Integer,Pointer> _color; // integer if numbered, otherwise pointer to color definition DE
    Integer _parameterLineCount;
    Integer _formId;
    String _entityLabel;
    Integer _entitySubscript;
  };


  class ParameterData {
  public:
    ParameterData();
    
    Pointer getPointer(int index);
    Integer getInteger(int index);
    Logical getLogical(int index);
    String  getString(int index);
    Real    getReal(int index);

    void addParam(const string& param);
    void    clear() { params.clear(); }

  private:
    vector<string> params;
  };

  const int NRECS = 20;
  const int NCHAR = 256;

  class RecordException {
  public:
    RecordException(const string& msg) : message(msg) {}

    string message;
  };

  class Record {
  public:
    Record(FILE* in);
    Record(FILE* in, int paramStart, int record);

    bool valid() { return _valid; }

    bool isStart() { return _type == 'S'; }
    bool isGlobal() { return _type == 'G'; }
    bool isDirectory() { return _type == 'D'; }
    bool isParameter() { return _type == 'P'; }
    bool isTerminal() { return _type == 'T'; }
    // reset the stream to the start of this record
    void reset() { fseek(_fp, _start, SEEK_SET); }
    long where() { return ftell(_fp); }
  
    GlobalSection* createGlobalSection();
    void createDirectory(vector<DirectoryEntry*>& dir);
    string getParameterData();
    
  private:    
    FILE* _fp;
    bool _valid; // is this record valid?
    string _line; // the current read line (temporary)
    char _type; // the record type
    long _start; // the starting position of this record
    
    ostringstream _card;

    GlobalSection* _gs;
    vector<DirectoryEntry*>* _dir;
    list<string> _dirEntries;

    static int _reclen;
    void calcRecsize(FILE* in);
    void _read();
    bool _readLine();   
    void _undoRead(int numLines = 1);
    void _readStart();
    void _readGlobal();
    void _readDirectory();
    void _readParameter(int id);
    
    string _field(int index);
  };


  class IGESException {
  public:
    string message;
    IGESException(const string& msg) : message(msg) {}
  };

  typedef list<DirectoryEntry*> DEList;

  typedef enum {
    Null                   		= 0,    
    CircularArc            		= 100,
    CompositeCurve         		= 102,
    ConicArc               		= 104,
    CopiousData            		= 106,
    Plane                  		= 108,
    Line  				= 110,
    ParametricSplineCurve 		= 112,
    ParametricSplineSurface  		= 114,
    Point  				= 116,
    RuledSurface  			= 118,
    SurfaceOfRevolution  		= 120,
    TabulatedCylinder  			= 122,
    TransformationMatrix  		= 124,
    Flash 				= 125,
    RationalBSplineCurve  		= 126,
    RationalBSplineSurface  		= 128,
    OffsetCurve  			= 130,
    OffsetSurface  			= 140,
    Boundary 				= 141,
    CurveOnAParametricSurface  		= 142,
    BoundedSurface  			= 143,
    TrimmedParametricSurface  		= 144,
    PlaneSurface  			= 190,
    RightCircularCylindricalSurface  	= 192,
    RightCircularConicalSurface  	= 194,
    SphericalSurface  			= 196,
    ToroidalSurface  			= 198,
    Block  				= 150,
    RightAngularWedge  			= 152,
    RightCircularCylinder  		= 154,
    RightCircularConeFrustum  		= 156,
    Sphere 				= 158,
    Torus 				= 160,
    SolidOfRevolution  			= 162,
    SolidOfLinearExtrusion  		= 164,
    Ellipsoid  				= 168,
    ManifoldSolidBRepObject     	= 186,
    Vertex				= 502,
    Edge				= 504,
    Loop				= 508,
    Face				= 510,
    Shell				= 514,
    AngularDimension  			= 202,
    CurveDimension  			= 204,
    DiameterDimension  		        = 206,
    FlagNote  				= 208,
    GeneralLabel  			= 210,
    GeneralNote  			= 212,
    NewGeneralNote  			= 213,
    LeaderArrow  			= 214,
    LinearDimension  			= 216,
    OrdinateDimension  		        = 218,
    PointDimension  			= 220,
    RadiusDimension  			= 222,
    GeneralSymbol  			= 228,
    SectionedArea  			= 230,
    ConnectPoint  			= 132,
    Node  				= 134,
    FiniteElement  			= 136,
    NodalDisplacementAndRotation  	= 138,
    NodalResults  			= 146,
    ElementResults  			= 148,
    AssociativityDefinition  		= 302,
    LineFontDefinition  		= 304,
    MACRODefinition  			= 306,
    SubfigureDefinition  		= 308,
    TextFontDefinition  		= 310,
    TextDisplayTemplate  		= 312,
    ColorDefinition  			= 314,
    UnitsData  				= 316,
    NetworkSubfigureDefinition  	= 320,
    AttributeTableDefinition  		= 322,
    AssociativityInstance  		= 402,
    Drawing  				= 404,
    Property  				= 406,
    SingularSubfigureInstance  		= 408,
    View  				= 410,
    RectangularArraySubfigureInstance  	= 412,
    CircularArraySubfigureInstance  	= 414,
    ExternalReference  			= 416,
    NodalLoadConstraint  		= 418,
    NetworkSubfigureInstance  		= 420,
    AttributeTableInstance  		= 422,
  } IGESEntity;

  typedef vector<DirectoryEntry*> DEVector;
  class IGES; // forward declaration

  //--------------------------------------------------------------------------------
  // Extractors
  class Extractor {
  public:
    virtual ~Extractor();
    virtual void extract(IGES* iges, const DirectoryEntry* de) = 0;
    Extractor* cascadeDelete(Extractor* handler);

  private:
    list<Extractor*> handlers;
  };

  class EdgeUse;
  class PSpaceCurve;
  class BrepHandler : public Extractor {
  public:    
    BrepHandler();
    virtual ~BrepHandler();

    void extract(IGES* iges, const DirectoryEntry* de);

    // subclass responsibility
    virtual void handleShell(bool isVoid, bool orient) = 0;
    virtual int handleFace(bool orient, int surfIndex) = 0;
    virtual int handleLoop(bool isOuter, int faceIndex) = 0;
    virtual int handleEdge(int edgeIndex) = 0;
    virtual int handleVertex(int pointIndex) = 0;
    virtual int handlePoint(double x, double y, double z) = 0; // return index

    // surface handlers (refactor to SurfaceHandler class?)
    virtual int handleParametricSplineSurface() = 0;
    virtual int handleRuledSurface() = 0;
    virtual int handleSurfaceOfRevolution() = 0;
    virtual int handleTabulatedCylinder() = 0;
    virtual int handleRationalBSplineSurface() = 0;
    virtual int handleOffsetSurface() = 0;
    virtual int handlePlaneSurface() = 0;
    virtual int handleRightCircularCylindricalSurface() = 0;
    virtual int handleRightCircularConicalSurface() = 0;
    virtual int handleSphericalSurface() = 0;
    virtual int handleToroidalSurface() = 0;    

    // curve handlers (refactor to CurveHandler class?)
    virtual int handleCircularArc() = 0;
    virtual int handleCompositeCurve() = 0;
    virtual int handleConicArc() = 0;
    virtual int handle2DPath() = 0;
    virtual int handle3DPath() = 0;
    virtual int handleSimpleClosedPlanarCurve() = 0;
    virtual int handleLine() = 0;
    virtual int handleParametricSplineCurve() = 0;
    virtual int handleRationalBSplineCurve() = 0;
    virtual int handleOffsetCurve() = 0;    

  protected:
    IGES* _iges;
    
    DirectoryEntry* getEdge(Pointer& edgeList, int index);
    virtual void extractBrep(const DirectoryEntry* de);
    virtual void extractShell(const DirectoryEntry* de, bool isVoid, bool orientWithFace);
    virtual void extractFace(const DirectoryEntry* de, bool orientWithSurface);
    virtual int  extractSurface(const DirectoryEntry* de);
    virtual void extractLoop(const DirectoryEntry* de, bool isOuter, int face);
    virtual void extractEdge(const DirectoryEntry* de);
    virtual void extractVertex(const DirectoryEntry* de);
    virtual int  extractCurve(const DirectoryEntry* de, bool isIso);

    friend class EdgeUse;
    friend class PSpaceCurve;

  private:
    int shellIndex;
    int faceIndex;
    int surfaceIndex;
    int curveIndex;
    int edgeIndex;
  };


  //--------------------------------------------------------------------------------
  // IGES 
  class IGES {
  public:
    
    // create a new IGES object, in preparation to write
    IGES();
    // create an IGES containing the entities in the given g file
    IGES(const struct db_i* dbip);
    // load an IGES file with the given filename
    IGES(const string& filename);
    
    ~IGES();    

    //--------------------------------------------------------------------------------
    // info
    const GlobalSection& global() const;
    string getTypeName(IGESEntity id) const;    
    
    //--------------------------------------------------------------------------------
    // read
    bool readBreps(Extractor* handler);
    void find(IGESEntity id, DEList& outList);
    void getParameter(const Pointer& ptr, ParameterData& outParam);
    DirectoryEntry* getDirectoryEntry(const Pointer& ptr);
    
  protected:
    void readStart(FILE* in);
    void readGlobal(FILE* in);
    void readDirectory(FILE* in);
    void locateParameters(FILE* in);
    
  private:
    FILE* _file;
    long paramSectionStart;
    GlobalSection* _global;
    vector<DirectoryEntry*> _dir;
  };


  //--------------------------------------------------------------------------------
  // utility  
  string format_arg_list(const char *fmt, va_list args);
  string format(const char *fmt, ...);
}


#endif
