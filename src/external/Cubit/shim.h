/* This file is provided as a compilation stub to ensure ongoing build testing
 * of the g-sat converter.  Various bits were copied from the LGPL CGM sources
 * at https://bitbucket.org/fathomteam/cgm and some were further simplified
 * (since this is intended only for build testing, not for running.)
 */

#include "common.h"
#include "vmath.h"
#include <vector>
#include <string>

/* Stubbing the CubitVector got a bit tricky, so just snarf in a trimmed
 * down and BRL-CAD-ified version of the original file */
#include "shim_cv.hpp"

#define CUBIT_FAILURE 0
#define CUBIT_TRUE 1
#define STRAIGHT_CURVE_TYPE 1
#define PLANE_SURFACE_TYPE 1
#define CubitString std::string

typedef int CubitStatus;

#define CAST_LIST_TO_PARENT(classOneList, classTwoList) { }


template<typename T>
struct vector_const_reference_type {
    typedef typename std::vector<T>::const_reference type;
};

template<>
struct vector_const_reference_type<bool> {
    typedef bool type;
};

template<class X> class DLIList
{
    public:
	typedef typename vector_const_reference_type<X>::type const_reference;
	void append(const_reference new_item);
	const_reference operator[](int index) const;
	void clean_out();
	int size();
	X pop();
	DLIList<X>& operator+=(const DLIList<X>& from);
	std::vector<X> v;
};

template <class X> inline void DLIList<X>::clean_out() {}
template <class X> inline void DLIList<X>::append(const_reference) {}
template <class X> inline X DLIList<X>::pop() { return NULL; }
template <class X> inline int DLIList<X>::size() { return 0; }
template <class X> inline typename DLIList<X>::const_reference DLIList<X>::operator[](int ind) const { return v[ind]; }
template <class X> inline DLIList<X>& DLIList<X>::operator+=(const DLIList<X> &) { return *this; }

class RefVertex {};
class RefFace {};
class RefEdge {};
class RefEntity {};
class Body {
    public:
	int size();
};
int Body::size() { return 0; }

class GeometryModifyTool {
    public:
	static GeometryModifyTool* instance(void *p = NULL);
	RefVertex* make_RefVertex(CubitVector const&) const;
	RefEdge* make_RefEdge(int,RefVertex *,RefVertex *) const;
	RefFace* make_RefFace(int, DLIList<RefEdge*>&) const;
	CubitStatus create_solid_bodies_from_surfs(DLIList<RefFace*> &,DLIList<Body*> &) const;
	CubitStatus regularize_body(Body *, Body *&);
	Body* brick(double , double , double);
	Body* torus(double, double);
	Body* cylinder(double, double, double, double);
	Body* sphere(double);
	CubitStatus intersect(Body*,DLIList<Body*> &,DLIList<Body*> &,int);
	CubitStatus subtract(Body*,DLIList<Body*> &,DLIList<Body*> &,bool);
	CubitStatus unite(DLIList<Body*> &,DLIList<Body*> &,bool);
};
GeometryModifyTool* GeometryModifyTool::instance(void *) { return NULL; }
RefVertex* GeometryModifyTool::make_RefVertex(const CubitVector &UNUSED(c)) const { return NULL; }
RefEdge* GeometryModifyTool::make_RefEdge(int, RefVertex *, RefVertex *) const { return NULL; }
RefFace* GeometryModifyTool::make_RefFace(int, DLIList<RefEdge*>&) const { return NULL; }
CubitStatus GeometryModifyTool::create_solid_bodies_from_surfs(DLIList<RefFace*> &, DLIList<Body*> &) const { return 0; }
CubitStatus GeometryModifyTool::regularize_body(Body *, Body *&) { return 0; }
Body* GeometryModifyTool::brick(double, double, double) { return NULL; }
Body* GeometryModifyTool::torus(double, double) { return NULL; }
Body* GeometryModifyTool::cylinder(double, double, double, double) { return NULL; }
Body* GeometryModifyTool::sphere(double) { return NULL; }
CubitStatus GeometryModifyTool::intersect(Body*, DLIList<Body*> &, DLIList<Body*> &, int) { return 0; }
CubitStatus GeometryModifyTool::subtract(Body*, DLIList<Body*> &, DLIList<Body*> &, bool) { return 0; }
CubitStatus GeometryModifyTool::unite(DLIList<Body*> &, DLIList<Body*> &, bool) { return 0; }

class GeometryQueryTool
{
    public:
	static GeometryQueryTool* instance(void *p = NULL);
	Body *get_last_body();
	CubitStatus translate(Body*,const CubitVector&);
	CubitStatus rotate(Body*, const CubitVector&, double);
	CubitStatus scale(Body*, const CubitVector&, bool);
	CubitStatus export_solid_model(DLIList<RefEntity*>&,const char*,const char*, int, const CubitString &);
	const char *get_engine_version_string();
	Body *get_body(int);
};
GeometryQueryTool* GeometryQueryTool::instance(void *) { return NULL; }
Body *GeometryQueryTool::get_last_body() { return NULL; }
CubitStatus GeometryQueryTool::translate(Body*, const CubitVector&) { return 0; }
CubitStatus GeometryQueryTool::rotate(Body*, const CubitVector&, double) { return 0; }
CubitStatus GeometryQueryTool::scale(Body*, const CubitVector&, bool) { return 0; }
CubitStatus GeometryQueryTool::export_solid_model(DLIList<RefEntity*>&, const char*, const char*, int, const CubitString &) { return 0; }
const char *GeometryQueryTool::get_engine_version_string() { return NULL; }
Body *GeometryQueryTool::get_body(int) { return NULL; }

class AcisQueryEngine
{
    public:
	static void instance();
};
void AcisQueryEngine::instance() { return; }

class AcisModifyEngine
{
    public:
	static void instance();
};
void AcisModifyEngine::instance() { return; }


class AppUtil
{
    public:
	static AppUtil* instance();
	void startup(int&, char**&);
};
AppUtil* AppUtil::instance() { return NULL; }
void AppUtil::startup(int&, char**&) { return; }

class CubitAttribManager
{
    public:
	void auto_flag(bool);

};
void CubitAttribManager::auto_flag(bool) { return; }

class CGMApp
{
    public:
	static CGMApp* instance();
	void startup(int&, char**&);
	void shutdown();
	CubitAttribManager* attrib_manager();
};
CGMApp* CGMApp::instance() { return NULL; }
void CGMApp::startup(int&, char**&) { return; }
void CGMApp::shutdown() { return; }
CubitAttribManager* CGMApp::attrib_manager() { return NULL; }

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

