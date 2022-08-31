/*
 * Poly2Tri Copyright (c) 2009-2010, Poly2Tri Contributors
 * http://code.google.com/p/poly2tri/
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of Poly2Tri nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "sweep_context.h"
#include <algorithm>
#include <stdexcept>
#include "advancing_front.h"

namespace p2t {

    bool SweepContext::lessthanpoint::operator() (const Point* a, const Point* b) const
    {
	if (a->y < b->y) {
	    return true;
	} else if (a->y == b->y) {
	    return (a->x < b->x);
	}
	return false;
    }

    SweepContext::SweepContext()
    {
	basin = Basin();
	edge_event = EdgeEvent();
    }

    SweepContext::SweepContext(std::vector<Point*> &polyline)
    {
	std::vector<Point*> polylineunique;

	basin = Basin();
	edge_event = EdgeEvent();

	for(unsigned int i = 0; i < polyline.size(); i++) {
	    Point *p = AddPointCheckForDuplicates(polyline[i]);
	    polylineunique.push_back(p);
	}
	InitEdges(polylineunique);

    }

    void SweepContext::AddOuterLoop(std::vector<Point*> &polyline)
    {
	std::vector<Point*> polylineunique;
	for(unsigned int i = 0; i < polyline.size(); i++) {
	    Point *p = AddPointCheckForDuplicates(polyline[i]);
	    polylineunique.push_back(p);
	}
	InitEdges(polylineunique);

    }

    void SweepContext::AddHole(std::vector<Point*> &polyline)
    {
	std::vector<Point*> polylineunique;
	for(unsigned int i = 0; i < polyline.size(); i++) {
	    Point *p = AddPointCheckForDuplicates(polyline[i]);
	    polylineunique.push_back(p);
	}
	InitEdges(polylineunique);
    }

    void SweepContext::AddPoint(Point* point) {
	points_.push_back(point);
    }

    Point* SweepContext::AddPointCheckForDuplicates(Point* point) {
	std::pair<std::set<Point*,lessthanpoint>::iterator,bool> result;

	result = points_set_.insert(point);
	if (result.second) {
	    points_.push_back(point);
	    return point;
	}
	Point* found_point = *points_set_.find(point);
	delete point;
	return found_point;
    }

    std::vector<Triangle*>& SweepContext::GetTriangles()
    {
	return triangles_;
    }

    std::list<Triangle*>& SweepContext::GetMap()
    {
	return map_;
    }

    void SweepContext::InitTriangulation()
    {
	double xmax(points_[0]->x), xmin(points_[0]->x);
	double ymax(points_[0]->y), ymin(points_[0]->y);

	// Calculate bounds.
	for (unsigned int i = 0; i < points_.size(); i++) {
	    Point& p = *points_[i];
	    if (p.x > xmax)
		xmax = p.x;
	    if (p.x < xmin)
		xmin = p.x;
	    if (p.y > ymax)
		ymax = p.y;
	    if (p.y < ymin)
		ymin = p.y;
	}

	double dx = kAlpha * (xmax - xmin);
	double dy = kAlpha * (ymax - ymin);
	head_ = new Point(xmax + dx, ymin - dy);
	tail_ = new Point(xmin - dx, ymin - dy);

	// Sort points along y-axis
	std::sort(points_.begin(), points_.end(), cmp);

    }

    void SweepContext::InitEdges(std::vector<Point*> &polyline)
    {
	size_t num_points = polyline.size();
	for (size_t i = 0; i < num_points; i++) {
	    size_t j = i < num_points - 1 ? i + 1 : 0;
	    if (!(EQ((*polyline[i]).x, (*polyline[j]).x) && EQ((*polyline[i]).y, (*polyline[j]).y)))
		edge_list.push_back(new Edge(*polyline[i], *polyline[j]));
	}
    }

    Point* SweepContext::GetPoint(size_t index)
    {
	return points_[index];
    }

    std::vector<Point*>& SweepContext::GetPoints()
    {
	return points_;
    }

    void SweepContext::AddToMap(Triangle* triangle)
    {
	map_.push_back(triangle);
    }

    Node& SweepContext::LocateNode(Point& point)
    {
	// TODO implement search tree
	return *front_->LocateNode(point.x);
    }

    void SweepContext::CreateAdvancingFront(std::vector<Node*> &nodes)
    {

	// Initial triangle
	Triangle* triangle = new Triangle(*points_[0], *tail_, *head_);

	map_.push_back(triangle);

	af_head_ = new Node(*triangle->GetPoint(1), *triangle);
	af_middle_ = new Node(*triangle->GetPoint(0), *triangle);
	af_tail_ = new Node(*triangle->GetPoint(2), *triangle);
	front_ = new AdvancingFront(*af_head_, *af_tail_);

	// TODO: More intuitive if head is middles next and not previous?
	//       so swap head and tail
	af_head_->next = af_middle_;
	af_middle_->next = af_tail_;
	af_middle_->prev = af_head_;
	af_tail_->prev = af_middle_;
    }

    void SweepContext::RemoveNode(Node* node)
    {
	delete node;
    }

    void SweepContext::MapTriangleToNodes(Triangle& t)
    {
	for (int i = 0; i < 3; i++) {
	    if (!t.GetNeighbor(i)) {
		Node* n = front_->LocatePoint(t.PointCW(*t.GetPoint(i)));
		if (n)
		    n->triangle = &t;
	    }
	}
    }

    void SweepContext::RemoveFromMap(Triangle* triangle)
    {
	map_.remove(triangle);
    }

    void SweepContext::MeshClean(Triangle& triangle)
    {
	if (&triangle == NULL)
	    throw std::runtime_error("MeshClean - NULL triangl(?)");

	meshclean_call_cnt++;
	if (meshclean_call_cnt > 20000)
	   throw std::runtime_error("SweepContext MeshClean call stack too deep");

	if (!triangle.IsInterior()) {
	    triangle.IsInterior(true);
	    triangles_.push_back(&triangle);
	    for (int i = 0; i < 3; i++) {
		if (!triangle.constrained_edge[i])
		    MeshClean(*triangle.GetNeighbor(i));
	    }
	}

	meshclean_call_cnt--;
    }

    SweepContext::~SweepContext()
    {

	// Clean up memory

	delete head_;
	delete tail_;
	delete front_;
	delete af_head_;
	delete af_middle_;
	delete af_tail_;

	typedef std::list<Triangle*> type_list;

	for(type_list::iterator iter = map_.begin(); iter != map_.end(); ++iter) {
	    Triangle* ptr = *iter;
	    delete ptr;
	}

	for(unsigned int i = 0; i < edge_list.size(); i++) {
	    delete edge_list[i];
	}
	points_.clear();
	points_set_.clear();
    }

}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

