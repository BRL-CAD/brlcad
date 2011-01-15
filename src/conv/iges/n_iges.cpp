/*                      N _ I G E S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/* interface header */
#include "n_iges.hpp"

/* implementation system headers */
#include <list>
#include <assert.h>


namespace brlcad {

////////////////////////////////////////////////////////////////////////////////
Integer::Integer() {
    _val = 0;
}
Integer::Integer(long val) {
    _val = val;
}
Integer::Integer(const string& field) {
    if (field.length() == 0) _val = 0;
    else {
	int first = field.find_first_not_of(" \t\n\r");
	_val = strtol(field.substr(first, field.length()-first).c_str(), NULL, 0);
    }
}
Integer::Integer(const Integer& intg) {
    _val = intg._val;
}
FILE*
Integer::read(FILE* in) {
    return in;
}
string
Integer::write(FILE* out) {
    return "";
}


////////////////////////////////////////////////////////////////////////////////
Pointer::Pointer() {
    _val = 0;
}
Pointer::Pointer(long val) {
    _val = labs(val);
}
Pointer::Pointer(const string& field) {
    _val = 0;
    int first = field.find_first_not_of(" \t\n\r");
    if (first >= 0) {
	_val = strtol(field.substr(first, field.length()-first).c_str(), NULL, 0);
	if (_val < 0) _val = labs(_val);
    }
}
Pointer::Pointer(const Pointer& ptr) {
    _val = ptr._val;
}
FILE*
Pointer::read(FILE* in) {
    return in;

}
string
Pointer::write(FILE* out) {
    return "";
}


////////////////////////////////////////////////////////////////////////////////
Real::Real() { _val = 0.0; }
Real::Real(double v) { _val = v; }
Real::Real(const string& field) {
    if (field.length() == 0) _val = 0.0;
    else {
	string copy = field;
	int first = field.find_first_not_of(" \t\n\r");
	int exp = field.find_first_of("DF");
	if (exp != string::npos) {
	    copy.replace(exp, 1, "e");
	    debug("Real(" << copy << ")");
	}
	_val = strtod(copy.substr(first, field.length()-first).c_str(), NULL);
    }
    debug("Real(" << _val << ")");
}
Real::Real(const Real& r) {
    _val = r._val;
}


FILE*
Real::read(FILE* in) {
    return in;

}
string
Real::write(FILE* out) {
    return "";
}


////////////////////////////////////////////////////////////////////////////////
String::String() {
    _val = "";
}
String::String(const string& field) {
    if (field.length() == 0) {
	_val = "";
    } else {
	int first = field.find_first_not_of(" \t\n\r0");
	int index = field.find_first_of("H", first);
	if (index != string::npos) {
	    int size = atoi(field.substr(first, index).c_str());
	    _val = field.substr(index+1, size);
	} else {
	    _val = field.substr(first, field.length()-first);
	}
    }
}
String::String(const String& str) {
    _val = str._val;
}
String::String(const char* str) {
    _val = str;
}


FILE*
String::read(FILE* in) {
    return in;

}
string
String::write(FILE* out) {
    return "";
}


////////////////////////////////////////////////////////////////////////////////
Logical::Logical() {
    _val = false;
}
Logical::Logical(const string& field) {
    int first = field.find_first_not_of(" \t\n\r");
    _val = strtol(field.substr(first, field.length()-first).c_str(), NULL, 0) != 0;
}
Logical::Logical(bool val) {
    _val = val;
}
Logical::Logical(const Logical& p) {
    _val = p._val;
}


FILE*
Logical::read(FILE* in) {
    return in;

}
string
Logical::write(FILE* out) {
    return "";
}


//--------------------------------------------------------------------------------
// Record
int Record::_reclen = -1;

void
Record::calcRecsize(FILE* in)
{
    int i, j, k=(-1), recl=0, length[NRECS], ch;

    for (j=0 ; j<NRECS ; j++) {
	i = 1;
	while ((ch = fgetc(in)) != '\n' && i < NCHAR && ch != EOF)
	    i++;
	if (i == NCHAR) {
	    recl = 80;
	    break;
	} else if (ch == EOF) {
	    k = j - 1;
	    break;
	} else
	    length[j] = i; /* record this record length */
    }
    if (k == (-1))	/* We didn't encounter an early EOF */
	k = NRECS;

    if (fseek(in, 0, SEEK_SET)) {
	/* rewind file */
	perror("Recsize");
	bu_exit(-1, "Cannot rewind file\n");
    }

    if (recl == 0) {
	/* then LF's were found */
	recl = length[1];	/* don't use length[0] */

	/* check for consistent record lengths */
	for (j=2 ; j<k ; j++) {
	    if (recl != length[j])
		throw new RecordException("no consistent record lengths");
	}
    }
    Record::_reclen = recl;
}


Record::Record(FILE* in) : _fp(in) {
    if (_reclen < 0) calcRecsize(in); // FIXME: ...
    _start = ftell(in);
    _read();
}


Record::Record(FILE* in, int paramStart, int record) : _fp(in) {
    if (_reclen < 0) calcRecsize(in);
    _start = ftell(in);
    int pos = (record-1)*_reclen;
    fseek(_fp, paramStart + pos, SEEK_SET);
    _read();
}


string
Record::_field(int index) {
    return _line.substr(index*8, 8);
}


bool
Record::_readLine() {
    string buf;
    buf.reserve(_reclen+1);
    for (int i = 0; i < _reclen; i++) buf[i] = fgetc(_fp);
    buf[_reclen] = 0;
    //char* str = fgets(buf, _reclen, _fp);
    _line = buf.c_str();
    _type = _line[72];
    return true;
}


void
Record::_undoRead(int numLines) {
    fseek(_fp, -(numLines * _reclen), SEEK_CUR);
}


void
Record::_read() {
    _readLine();
    switch (_type) {
	case 'S': _readStart(); break;
	case 'G': _readGlobal(); break;
	case 'D': _readDirectory(); break;
	case 'P': _readParameter(Integer(_field(8))); break;
	case 'T': break;
	default: _valid=false;
    }
}


void
Record::_readStart() {
    // handle what was already read
    _card << _line.substr(0, 72);
    while (_readLine() && isStart()) {
	_card << _line.substr(0, 72);
    }
    // push back the last line
    _undoRead(); _type = 'S';
    // output the start value
    printf("%s", _card.str().c_str());
    _valid = true;
}


void
Record::_readGlobal() {
    _card << _line.substr(0, 72);
    while (_readLine() && isGlobal()) {
	_card << _line.substr(0, 72);
    }
    _undoRead(); _type = 'G';
    printf("Read global section\n");
    _valid = true;
}


void
Record::_readDirectory() {
    // each DE is 2 lines - we've read one line
    _card << _line.substr(0, 72);
    if (_readLine() && isDirectory())
	_card << _line.substr(0, 72);
    else {
	_valid = false;
	_undoRead();
	return;
    }
    _dirEntries.push_back(_card.str());

    int i = 1;
    _card.seekp(0, ios::beg);
    while (_readLine() && isDirectory()) {
	_card << _line.substr(0, 72);
	if (i % 2 == 0) {
	    _dirEntries.push_back(_card.str());
	    _card.seekp(0, ios::beg);
	}
	i++;
    }
    _undoRead(); _type = 'D'; // should have read a parameter line!
    _valid = true;
#if DEBUG
    for (list<string>::iterator i = _dirEntries.begin(); i != _dirEntries.end(); i++) {
	cout << "dir: " << *i << endl;
    }
#endif
}


void
Record::_readParameter(int id) {
    _card << _line.substr(0, 64);
    while (_readLine() && isParameter() && (id == Integer(_field(8)))) {
	_card << _line.substr(0, 64);
    }
    _undoRead(); _type = 'P';
}


GlobalSection*
Record::createGlobalSection() {
    return new GlobalSection(_card.str());
}


void
Record::createDirectory(vector<DirectoryEntry*>& dir) {
    for (list<string>::iterator i = _dirEntries.begin(); i != _dirEntries.end(); i++) {
	dir.push_back(new DirectoryEntry(*i));
    }
}


string
Record::getParameterData() {
    return _card.str();
}


//--------------------------------------------------------------------------------
// DirectoryEntry definition
DirectoryEntry::DirectoryEntry(const string& in) : _rawEntry(in) {
    _type = Integer(_field(1));
    _paramData = Pointer(_field(2));
    _structure = Pointer(_field(3));
    long lfp   = Integer(_field(4));
    _lineFontPattern = (lfp >= 0) ? DualIP(Integer(lfp)) : DualIP(Pointer(lfp));
    long lvl   = Integer(_field(5));
    _level     = (lvl >= 0) ? DualIP(Integer(lvl)) : DualIP(Pointer(lvl));
    _view      = Pointer(_field(6));
    _xform     = Pointer(_field(7));
    _label     = Pointer(_field(8));
    _parseStatus(_field(9));
    _lineWeight = Integer(_field(12));
    long color = Integer(_field(13));
    _color     = (color > 0) ? DualIP(Integer(color)) : DualIP(Pointer(color));
    _parameterLineCount = Integer(_field(14));
    _formId    = Integer(_field(15));
    string el  = _field(18);
    el.replace(0, el.find_first_not_of(" "), "");
    _entityLabel = el;
    _entitySubscript = Integer(_field(19));
}


void
DirectoryEntry::_parseStatus(const string& status) {
    int stat = atoi(status.substr(0, 2).c_str());
    _visible = (Visibility)stat;
    stat = atoi(status.substr(2, 2).c_str());
    _subordinate = (Subordinate)stat;
    stat = atoi(status.substr(4, 2).c_str());
    _use = (EntityUse)stat;
    stat = atoi(status.substr(6, 2).c_str());
    _hierarchy = (Hierarchy)stat;
}


string
DirectoryEntry::_field(int index) {
    int xindex = (index > 10) ? index - 1 : index;
    return _rawEntry.substr((xindex-1) * FIELD_WIDTH, FIELD_WIDTH);
}


string
DirectoryEntry::toString() {
    ostringstream ss;
    ss << "Type: " << _type << ", " << "Param: " << _paramData;
    return ss.str();
}


//--------------------------------------------------------------------------------
// GlobalSection definition

// hmm - boost would be nice...
void split(list<string>& els, const string& str, const string& delim, int init = 0) {
    int start = init;
    int end = str.find_first_of(delim, start);
    while (end != string::npos) {
	if (end != string::npos) {
	    if ((end - start) != 0) {
		string s = str.substr(start, end-start);
		els.push_back(s);
	    } else
		els.push_back("");
	}
	start = end+1;
	end = str.find_first_of(delim, start);
    }
}


GlobalSection::GlobalSection() {
    // TODO
}


GlobalSection::GlobalSection(const string& in) {
    read(in);
}


void
GlobalSection::read(const string& in) {
    int index = 0;
    list<string> fields;

    cout << in << endl;

    // param delimiter
    if (in[0] == ', ' && in[1] == ', ') {
	_params[0] = ", ";
	_params[1] = ";";
	index = 2;
    } else if (in[0] == ', ') {
	_params[0] = ", ";
	_params[1] = in.substr(1, 3);
	index = 4;
    } else {
	_params[0] = in.substr(0, 3);
	if (in[4] == ', ') {
	    _params[1] = ';';
	    index = 5;
	} else {
	    index = in.find_first_of(paramDelim(), 4);
	    _params[1] = in.substr(4, index);
	    index += 4;
	}
    }
    split(fields, in, paramDelim() + recordDelim(), index);

    cout << "paramDelim:  " << paramDelim() << endl;
    cout << "recordDelim: " << recordDelim() << endl;

    index = 2;
    for (list<string>::iterator iter = fields.begin(); iter != fields.end(); iter++) {
	_params[index] = *iter;
	index++;
    }

    cout << toString();
}


//--------------------------------------------------------------------------------
// ParameterData definition
ParameterData::ParameterData() {}

Pointer
ParameterData::getPointer(int index) const {
    assert(index >= 0);
    return Pointer(params[index]);
}


Integer
ParameterData::getInteger(int index) const {
    assert(index >= 0);
    return Integer(params[index]);
}


Logical
ParameterData::getLogical(int index) const {
    assert(index >= 0);
    return Logical(params[index]);
}


String
ParameterData::getString(int index) const {
    assert(index >= 0);
    return String(params[index]);
}


Real
ParameterData::getReal(int index) const {
    assert(index >= 0);
    return Real(params[index]);
}


void
ParameterData::addParam(const string& param) {
    assert(param.size() > 0);
    params.push_back(param);
}


//--------------------------------------------------------------------------------
// IGES definition
IGES::IGES() {

}


IGES::IGES(const struct db_i* dbip) {

}


IGES::IGES(const string& filename) {
    FILE* in = fopen(filename.c_str(), "r");

    readStart(in);
    readGlobal(in);
    readDirectory(in);
    locateParameters(in);

    _file = in;
}


IGES::~IGES() {
    delete _global;
    for (vector<DirectoryEntry*>::iterator i = _dir.begin(); i != _dir.end(); i++) {
	delete (*i);
    }
}


const GlobalSection&
IGES::global() const {
    return *_global;
}


string
IGES::getTypeName(IGESEntity ide) const {
    switch (ide) {
	case Null: return "Null";
	case CircularArc: return "CircularArc";
	case CompositeCurve: return "CompositeCurve";
	case ConicArc: return "ConicArc";
	case CopiousData: return "CopiousData";
	case Plane: return "Plane";
	case Line: return "Line";
	case ParametricSplineCurve: return "ParametricSplineCurve";
	case ParametricSplineSurface: return "ParametricSplineSurface";
	case Point: return "Point";
	case RuledSurface: return "RuledSurface";
	case SurfaceOfRevolution: return "SurfaceOfRevolution";
	case TabulatedCylinder: return "TabulatedCylinder";
	case TransformationMatrix: return "TransformationMatrix";
	case Flash: return "Flash";
	case RationalBSplineCurve: return "RationalBSplineCurve";
	case RationalBSplineSurface: return "RationalBSplineSurface";
	case OffsetCurve: return "OffsetCurve";
	case OffsetSurface: return "OffsetSurface";
	case Boundary: return "Boundary";
	case CurveOnAParametricSurface: return "CurveOnAParametricSurface";
	case BoundedSurface: return "BoundedSurface";
	case TrimmedParametricSurface: return "TrimmedParametricSurface";
	case PlaneSurface: return "PlaneSurface";
	case RightCircularCylindricalSurface: return "RightCircularCylindricalSurface";
	case RightCircularConicalSurface: return "RightCircularConicalSurface";
	case SphericalSurface: return "SphericalSurface";
	case ToroidalSurface: return "ToroidalSurface";
	case Block: return "Block";
	case RightAngularWedge: return "RightAngularWedge";
	case RightCircularCylinder: return "RightCircularCylinder";
	case RightCircularConeFrustum: return "RightCircularConeFrustum";
	case Sphere: return "Sphere";
	case Torus: return "Torus";
	case SolidOfRevolution: return "SolidOfRevolution";
	case SolidOfLinearExtrusion: return "SolidOfLinearExtrusion";
	case Ellipsoid: return "Ellipsoid";
	case ManifoldSolidBRepObject: return "ManifoldSolidBRepObject";
	case Vertex: return "Vertex";
	case Edge: return "Edge";
	case Loop: return "Loop";
	case Face: return "Face";
	case Shell: return "Shell";
	case AngularDimension: return "AngularDimension";
	case CurveDimension: return "CurveDimension";
	case DiameterDimension: return "DiameterDimension";
	case FlagNote: return "FlagNote";
	case GeneralLabel: return "GeneralLabel";
	case GeneralNote: return "GeneralNote";
	case NewGeneralNote: return "NewGeneralNote";
	case LeaderArrow: return "LeaderArrow";
	case LinearDimension: return "LinearDimension";
	case OrdinateDimension: return "OrdinateDimension";
	case PointDimension: return "PointDimension";
	case RadiusDimension: return "RadiusDimension";
	case GeneralSymbol: return "GeneralSymbol";
	case SectionedArea: return "SectionedArea";
	case ConnectPoint: return "Connect";
	case Node: return "Node";
	case FiniteElement: return "Finite";
	case NodalDisplacementAndRotation: return "NodalDisplacementAndRotation";
	case NodalResults: return "NodalResults";
	case ElementResults: return "ElementResults";
	case AssociativityDefinition: return "AssociativityDefinition";
	case LineFontDefinition: return "LineFontDefinition";
	case MACRODefinition: return "MACRODefinition";
	case SubfigureDefinition: return "SubfigureDefinition";
	case TextFontDefinition: return "TextFontDefinition";
	case TextDisplayTemplate: return "TextDisplayTemplate";
	case ColorDefinition: return "ColorDefinition";
	case UnitsData: return "UnitsData";
	case NetworkSubfigureDefinition: return "NetworkSubfigureDefinition";
	case AttributeTableDefinition: return "AttributeTableDefinition";
	case AssociativityInstance: return "AssociativityInstance";
	case Drawing: return "Drawing";
	case Property: return "Property";
	case SingularSubfigureInstance: return "SingularSubfigureInstance";
	case View: return "View";
	case RectangularArraySubfigureInstance: return "RectangularArraySubfigureInstance";
	case CircularArraySubfigureInstance: return "CircularArraySubfigureInstance";
	case ExternalReference: return "ExternalReference";
	case NodalLoadConstraint: return "NodalLoad";
	case NetworkSubfigureInstance: return "NetworkSubfigureInstance";
	case AttributeTableInstance: return "AttributeTableInstance";
    }
    return "Unknown";
}


void
IGES::readBreps(Extractor* handler)
{
    DEList breps;
    // FIXME: right now this only reads one kind of object at a time.
    find(ManifoldSolidBRepObject, breps);
    //	find(RationalBSplineSurface, breps);
    debug("Found " << breps.size() << " breps!");

    for (DEList::iterator i = breps.begin(); i != breps.end(); i++) {
	handler->extract(this, *i); // should be a BrepHandler subclass, but the code doesn't enforce it
    }
}


void
IGES::find(IGESEntity id, DEList& outList) {
    debug("ID: " << id);
    for (DEVector::iterator i = _dir.begin(); i != _dir.end(); i++) {
	debug((*i)->toString());
	if (((long)id) == ((long)(*i)->type())) outList.push_back(*i);
    }
}


void
IGES::readStart(FILE* in) {
    Record start(in);
    if (!start.isStart()) {
	bu_exit(-1, "Start section not found\n");
    }
}


void
IGES::readGlobal(FILE* in) {
    Record global(in);
    _global = global.createGlobalSection();
}


void
IGES::readDirectory(FILE* in) {
    Record dir(in);
    dir.createDirectory(_dir);
}


void
IGES::locateParameters(FILE* in) {
    Record test(in);
    if (test.isParameter()) {
	test.reset();
	paramSectionStart = test.where();
	debug("Param section starts: " << paramSectionStart);
    } else {
	bu_exit(-1, "Param section not found! Aborting\n");
    }
}


void
IGES::getParameter(const Pointer& ptr, ParameterData& outParam) {
    // hmm
    Record r(_file, paramSectionStart, ptr());
    string pd = r.getParameterData();
    list<string> l;
    split(l, pd, _global->paramDelim() + _global->recordDelim());
    outParam.clear();
    for (list<string>::iterator i = l.begin(); i != l.end(); i++) {
	outParam.addParam(*i);
    }
}


DirectoryEntry*
IGES::getDirectoryEntry(const Pointer& ptr) {
    return _dir[(ptr-1)/2];
}


void
IGES::getTransformation(const Pointer& ptr, mat_t out_xform)
{
    if (ptr() == 0) return;
    //MAT_IDN(xform);
    DirectoryEntry* xform_de = getDirectoryEntry(ptr);
    ParameterData params;
    getParameter(xform_de->paramData(), params);

    mat_t xform;
    for (int i = 0; i < 12; i++) {
	xform[i] = params.getReal(i+1);
    }
    xform[12] = xform[13] = xform[14] = 0;
    xform[15] = 1.0;

    mat_t parent_xform;
    if (xform_de->xform()() != 0) {
	MAT_IDN(parent_xform);
	getTransformation(xform_de->xform(), parent_xform);
	bn_mat_mul(out_xform, parent_xform, xform);
    } else {
	MAT_COPY(out_xform, xform);
    }
}


//--------------------------------------------------------------------------------
// Extractor
Extractor::~Extractor() {
    for (list<Extractor*>::iterator i = handlers.begin(); i != handlers.end(); i++) {
	delete (*i);
    }
}


Extractor*
Extractor::cascadeDelete(Extractor* handler) {
    handlers.push_back(handler);
    return handler;
}
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
