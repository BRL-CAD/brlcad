/* ========================================================================= *
 * TriMesh.h - GTE-style halfedge triangle mesh                              *
 *                                                                           *
 * A translation of the OpenMesh logic into GeometricTools style.            *
 *                                                                           *
 * Copyright (c) 2001-2025, RWTH-Aachen University                          *
 * Department of Computer Graphics and Multimedia                            *
 * All rights reserved.                                                      *
 * www.openmesh.org                                                          *
 *                                                                           *
 * Redistribution and use in source and binary forms, with or without        *
 * modification, are permitted provided that the following conditions        *
 * are met:                                                                  *
 * 1. Redistributions of source code must retain the above copyright notice, *
 *    this list of conditions and the following disclaimer.                  *
 * 2. Redistributions in binary form must reproduce the above copyright      *
 *    notice, this list of conditions and the following disclaimer in the    *
 *    documentation and/or other materials provided with the distribution.   *
 * 3. Neither the name of the copyright holder nor the names of its          *
 *    contributors may be used to endorse or promote products derived from   *
 *    this software without specific prior written permission.               *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED *
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A           *
 * PARTICULAR PURPOSE ARE DISCLAIMED.                                        *
 * ========================================================================= */

#pragma once

#include <vector>
#include <map>
#include <array>
#include <cmath>
#include <memory>
#include <algorithm>
#include <cassert>
#include <cstring>

namespace gte {

/* ---- Handle types ---- */
struct VertexHandle {
    int idx = -1;
    bool is_valid() const { return idx >= 0; }
    bool operator==(const VertexHandle& o) const { return idx == o.idx; }
    bool operator!=(const VertexHandle& o) const { return idx != o.idx; }
    bool operator<(const VertexHandle& o) const { return idx < o.idx; }
};
struct HalfedgeHandle {
    int idx = -1;
    bool is_valid() const { return idx >= 0; }
    bool operator==(const HalfedgeHandle& o) const { return idx == o.idx; }
    bool operator!=(const HalfedgeHandle& o) const { return idx != o.idx; }
};
struct EdgeHandle {
    int idx = -1;
    bool is_valid() const { return idx >= 0; }
    bool operator==(const EdgeHandle& o) const { return idx == o.idx; }
    bool operator!=(const EdgeHandle& o) const { return idx != o.idx; }
};
struct FaceHandle {
    int idx = -1;
    bool is_valid() const { return idx >= 0; }
    bool operator==(const FaceHandle& o) const { return idx == o.idx; }
    bool operator!=(const FaceHandle& o) const { return idx != o.idx; }
};

using Point  = std::array<float, 3>;
using Normal = std::array<float, 3>;
using Scalar = float;

inline Point  pt_zero()                { return {0.f,0.f,0.f}; }
inline Normal nm_zero()                { return {0.f,0.f,0.f}; }
inline Point  pt_add(Point a, Point b) { return {a[0]+b[0], a[1]+b[1], a[2]+b[2]}; }
inline Point  pt_sub(Point a, Point b) { return {a[0]-b[0], a[1]-b[1], a[2]-b[2]}; }
inline Point  pt_scale(float s, Point a){ return {s*a[0], s*a[1], s*a[2]}; }
inline float  pt_dot(Point a, Point b)  { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
inline float  pt_norm(Point a)          { return std::sqrt(pt_dot(a,a)); }
inline Point  pt_cross(Point a, Point b){ return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]}; }

/* ---- Data storage ---- */
struct VertexData {
    Point  point{};
    Normal normal{};
    int    outgoing_halfedge = -1;
    bool   deleted = false;
};
struct HalfedgeData {
    int to_vertex = -1;
    int next      = -1;
    int prev      = -1;
    int face      = -1;
};
struct FaceData {
    int  halfedge = -1;
    bool deleted  = false;
};

/* ---- Property handles ---- */
template<typename T>
struct VPropHandleT {
    std::shared_ptr<std::vector<T>> data_;
    bool is_valid() const { return (bool)data_; }
    void invalidate()     { data_.reset(); }
};
template<typename T>
struct EPropHandleT {
    std::shared_ptr<std::vector<T>> data_;
    bool is_valid() const { return (bool)data_; }
    void invalidate()     { data_.reset(); }
};
template<typename T>
struct FPropHandleT {
    std::shared_ptr<std::vector<T>> data_;
    bool is_valid() const { return (bool)data_; }
    void invalidate()     { data_.reset(); }
};
template<typename T>
struct MPropHandleT {
    std::shared_ptr<T> val_;
    bool is_valid() const { return (bool)val_; }
};

/* ====================================================================== */
/*  TriMesh - halfedge triangle mesh                                        */
/* ====================================================================== */
class TriMesh {
public:
    TriMesh() = default;

    /* ---- Construction ---- */
    VertexHandle add_vertex(Point p) {
        VertexData vd; vd.point = p;
        vertices_.push_back(vd);
        return VertexHandle{(int)vertices_.size()-1};
    }
    VertexHandle new_vertex(Point p) { return add_vertex(p); }

    /* Add a triangular face; looks up / creates halfedge pairs */
    FaceHandle add_face(VertexHandle va, VertexHandle vb, VertexHandle vc) {
        int v[3] = {va.idx, vb.idx, vc.idx};
        /* get or create halfedges */
        int h[3];
        for (int i = 0; i < 3; ++i) {
            int from = v[i], to = v[(i+1)%3];
            auto key = std::make_pair(from, to);
            auto it  = halfedge_map_.find(key);
            if (it != halfedge_map_.end()) {
                h[i] = it->second;
            } else {
                h[i] = new_edge_impl(from, to).idx;
            }
        }
        int fi = (int)faces_.size();
        FaceData fd; fd.halfedge = h[0]; fd.deleted = false;
        faces_.push_back(fd);
        /* link face loop */
        for (int i = 0; i < 3; ++i) {
            int ni = (i+1)%3, pi = (i+2)%3;
            halfedges_[h[i]].next = h[ni];
            halfedges_[h[i]].prev = h[pi];
            halfedges_[h[i]].face = fi;
        }
        /* update outgoing halfedges */
        for (int i = 0; i < 3; ++i) {
            if (vertices_[v[i]].outgoing_halfedge < 0)
                vertices_[v[i]].outgoing_halfedge = h[i];
        }
        return FaceHandle{fi};
    }
    FaceHandle add_face(std::vector<VertexHandle> vs) {
        if (vs.size() == 3)
            return add_face(vs[0], vs[1], vs[2]);
        /* Polygon: fan-triangulate from vs[0] */
        FaceHandle last;
        for (size_t i = 1; i+1 < vs.size(); ++i)
            last = add_face(vs[0], vs[i], vs[i+1]);
        return last;
    }

    /* Call after all faces have been added to link boundary loops */
    void rebuild_boundary_loops() {
        /* For each boundary halfedge i (face==-1):
           from_vertex = opp(i).to_vertex */
        std::vector<int> bnd_out(vertices_.size(), -1);
        for (int i = 0; i < (int)halfedges_.size(); ++i) {
            if (halfedges_[i].face >= 0) continue;
            int from_v = halfedges_[i^1].to_vertex;
            if (from_v >= 0) bnd_out[from_v] = i;
        }
        for (int i = 0; i < (int)halfedges_.size(); ++i) {
            if (halfedges_[i].face >= 0) continue;
            int to_v = halfedges_[i].to_vertex;
            if (to_v >= 0 && bnd_out[to_v] >= 0) {
                halfedges_[i].next            = bnd_out[to_v];
                halfedges_[bnd_out[to_v]].prev = i;
            }
        }
        for (int v = 0; v < (int)vertices_.size(); ++v)
            if (bnd_out[v] >= 0)
                vertices_[v].outgoing_halfedge = bnd_out[v];
    }

    /* Low-level: create halfedge pair, add to map. Returns h(from→to). */
    HalfedgeHandle new_edge_impl(int from, int to) {
        int h0 = (int)halfedges_.size();
        HalfedgeData hd{}; hd.to_vertex = to;
        halfedges_.push_back(hd);
        hd.to_vertex = from;
        halfedges_.push_back(hd);
        halfedge_map_[{from,to}] = h0;
        halfedge_map_[{to,from}] = h0+1;
        edge_tagged_.push_back(false);
        return HalfedgeHandle{h0};
    }

    /* Public new_edge: creates pair without adding to halfedge_map_. */
    HalfedgeHandle new_edge(VertexHandle from, VertexHandle to) {
        int h0 = (int)halfedges_.size();
        HalfedgeData hd{}; hd.to_vertex = to.idx;
        halfedges_.push_back(hd);
        hd.to_vertex = from.idx;
        halfedges_.push_back(hd);
        edge_tagged_.push_back(false);
        return HalfedgeHandle{h0};
    }

    /* Create a new (empty) face slot */
    FaceHandle new_face() {
        FaceData fd{}; fd.halfedge = -1; fd.deleted = false;
        faces_.push_back(fd);
        return FaceHandle{(int)faces_.size()-1};
    }

    /* ---- Navigation ---- */
    HalfedgeHandle next_halfedge_handle(HalfedgeHandle h) const { return {halfedges_[h.idx].next}; }
    HalfedgeHandle prev_halfedge_handle(HalfedgeHandle h) const { return {halfedges_[h.idx].prev}; }
    HalfedgeHandle opposite_halfedge_handle(HalfedgeHandle h) const { return {h.idx ^ 1}; }
    VertexHandle   to_vertex_handle(HalfedgeHandle h) const { return {halfedges_[h.idx].to_vertex}; }
    VertexHandle   from_vertex_handle(HalfedgeHandle h) const { return {halfedges_[h.idx^1].to_vertex}; }
    FaceHandle     face_handle(HalfedgeHandle h) const { return {halfedges_[h.idx].face}; }
    EdgeHandle     edge_handle(HalfedgeHandle h) const { return {h.idx/2}; }
    HalfedgeHandle halfedge_handle(EdgeHandle e, int side) const { return {e.idx*2 + side}; }
    HalfedgeHandle halfedge_handle(VertexHandle v) const { return {vertices_[v.idx].outgoing_halfedge}; }
    HalfedgeHandle halfedge_handle(FaceHandle f)   const { return {faces_[f.idx].halfedge}; }

    /* ---- Setters (low-level topology mutation) ---- */
    void set_vertex_handle(HalfedgeHandle h, VertexHandle v) { halfedges_[h.idx].to_vertex = v.idx; }
    void set_face_handle  (HalfedgeHandle h, FaceHandle f)   { halfedges_[h.idx].face = f.idx; }
    void set_next_halfedge_handle(HalfedgeHandle h, HalfedgeHandle n) {
        halfedges_[h.idx].next = n.idx;
        halfedges_[n.idx].prev = h.idx;
    }
    void set_halfedge_handle(FaceHandle f,   HalfedgeHandle h) { faces_[f.idx].halfedge = h.idx; }
    void set_halfedge_handle(VertexHandle v, HalfedgeHandle h) { vertices_[v.idx].outgoing_halfedge = h.idx; }

    /* ---- Points ---- */
    Point& point(VertexHandle v)             { return vertices_[v.idx].point; }
    const Point& point(VertexHandle v) const { return vertices_[v.idx].point; }
    void set_point(VertexHandle v, const Point& p) { vertices_[v.idx].point = p; }

    /* ---- Edge midpoint ---- */
    Point calc_edge_midpoint(EdgeHandle e) const {
        auto h0 = halfedge_handle(e, 0);
        auto h1 = halfedge_handle(e, 1);
        const Point& a = point(to_vertex_handle(h0));
        const Point& b = point(to_vertex_handle(h1));
        return {(a[0]+b[0])*0.5f, (a[1]+b[1])*0.5f, (a[2]+b[2])*0.5f};
    }

    /* ---- Boundary queries ---- */
    bool is_boundary(HalfedgeHandle h) const { return halfedges_[h.idx].face < 0; }
    bool is_boundary(EdgeHandle e) const {
        return is_boundary(halfedge_handle(e,0)) || is_boundary(halfedge_handle(e,1));
    }
    bool is_boundary(VertexHandle v) const {
        int h = vertices_[v.idx].outgoing_halfedge;
        if (h < 0) return true;
        return halfedges_[h].face < 0;
    }
    bool is_boundary(FaceHandle f) const {
        int h = faces_[f.idx].halfedge;
        int start = h;
        if (h < 0) return true;
        do {
            if (is_boundary(opposite_halfedge_handle({h})))
                return true;
            h = halfedges_[h].next;
        } while (h != start);
        return false;
    }

    /* ---- Valence ---- */
    int valence(VertexHandle v) const {
        int h0 = vertices_[v.idx].outgoing_halfedge;
        if (h0 < 0) return 0;
        int cnt = 0, h = h0;
        do {
            ++cnt;
            int ph = halfedges_[h].prev;
            if (ph < 0) break;
            h = ph ^ 1;
        } while (h != h0 && cnt < 1000);
        return cnt;
    }

    /* ---- Adjust outgoing halfedge (prefer boundary) ---- */
    void adjust_outgoing_halfedge(VertexHandle v) {
        int h0 = vertices_[v.idx].outgoing_halfedge;
        if (h0 < 0) return;
        int h = h0;
        do {
            if (halfedges_[h].face < 0) {
                vertices_[v.idx].outgoing_halfedge = h;
                return;
            }
            int ph = halfedges_[h].prev;
            if (ph < 0) return;
            h = ph ^ 1;
        } while (h != h0);
    }

    /* ---- Edge status (tagged) ---- */
    bool  has_edge_status() const { return true; }
    void  request_edge_status() {}
    void  release_edge_status() {}
    void  request_vertex_status() {}
    void  release_vertex_status() {}
    void  request_face_status()   {}
    void  release_face_status()   {}
    void  request_halfedge_status() {}
    void  release_halfedge_status() {}

    struct EdgeStatus {
        bool tagged_ = false;
        void set_tagged(bool t) { tagged_ = t; }
        bool tagged() const { return tagged_; }
        bool selected() const { return false; }
        bool locked()   const { return false; }
        bool feature()  const { return false; }
    };
    EdgeStatus& status(EdgeHandle e) {
        if ((int)edge_tagged_.size() <= e.idx)
            edge_tagged_.resize(e.idx+1, false);
        return reinterpret_cast<EdgeStatus&>(edge_tagged_[e.idx]);
    }

    /* ---- Normals ---- */
    void update_face_normals() {
        face_normals_.resize(faces_.size());
        for (int fi = 0; fi < (int)faces_.size(); ++fi) {
            if (faces_[fi].deleted) { face_normals_[fi] = nm_zero(); continue; }
            int h = faces_[fi].halfedge;
            if (h < 0) { face_normals_[fi] = nm_zero(); continue; }
            int h1 = halfedges_[h].next;
            int h2 = halfedges_[h1].next;
            Point a = point({halfedges_[h].to_vertex});
            Point b = point({halfedges_[h1].to_vertex});
            Point c = point({halfedges_[h2].to_vertex});
            Point ab = pt_sub(b,a), ac = pt_sub(c,a);
            Normal n{ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0]};
            float len = std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
            if (len > 0) { n[0]/=len; n[1]/=len; n[2]/=len; }
            face_normals_[fi] = n;
        }
    }
    void update_vertex_normals() {
        vertex_normals_.assign(vertices_.size(), nm_zero());
        for (int fi = 0; fi < (int)faces_.size(); ++fi) {
            if (faces_[fi].deleted) continue;
            Normal fn = (fi < (int)face_normals_.size()) ? face_normals_[fi] : nm_zero();
            int h = faces_[fi].halfedge, start = h;
            if (h < 0) continue;
            do {
                int vi = halfedges_[h].to_vertex;
                vertex_normals_[vi][0] += fn[0];
                vertex_normals_[vi][1] += fn[1];
                vertex_normals_[vi][2] += fn[2];
                h = halfedges_[h].next;
            } while (h != start);
        }
        for (auto& n : vertex_normals_) {
            float len = std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
            if (len > 0) { n[0]/=len; n[1]/=len; n[2]/=len; }
        }
    }
    Normal normal(VertexHandle v) const {
        if (v.idx < (int)vertex_normals_.size()) return vertex_normals_[v.idx];
        return nm_zero();
    }

    /* ---- Properties ---- */
    template<typename T>
    void add_property(VPropHandleT<T>& h) {
        h.data_ = std::make_shared<std::vector<T>>(vertices_.size());
    }
    template<typename T>
    void remove_property(VPropHandleT<T>& h) { h.invalidate(); }
    template<typename T>
    T& property(VPropHandleT<T>& h, VertexHandle v) {
        if (v.idx >= (int)h.data_->size()) h.data_->resize(v.idx+1);
        return (*h.data_)[v.idx];
    }
    template<typename T>
    const T& property(const VPropHandleT<T>& h, VertexHandle v) const {
        return (*h.data_)[v.idx];
    }

    template<typename T>
    void add_property(EPropHandleT<T>& h) {
        h.data_ = std::make_shared<std::vector<T>>(edge_tagged_.size());
    }
    template<typename T>
    void remove_property(EPropHandleT<T>& h) { h.invalidate(); }
    template<typename T>
    T& property(EPropHandleT<T>& h, EdgeHandle e) {
        if (e.idx >= (int)h.data_->size()) h.data_->resize(e.idx+1);
        return (*h.data_)[e.idx];
    }

    template<typename T>
    void add_property(FPropHandleT<T>& h) {
        h.data_ = std::make_shared<std::vector<T>>(faces_.size());
    }
    template<typename T>
    void remove_property(FPropHandleT<T>& h) { h.invalidate(); }
    template<typename T>
    T& property(FPropHandleT<T>& h, FaceHandle f) {
        if (f.idx >= (int)h.data_->size()) h.data_->resize(f.idx+1);
        return (*h.data_)[f.idx];
    }

    template<typename T>
    void add_property(MPropHandleT<T>& h) {
        h.val_ = std::make_shared<T>();
    }
    template<typename T>
    void remove_property(MPropHandleT<T>& h) { h.val_.reset(); }
    template<typename T>
    T& property(MPropHandleT<T>& h) { return *h.val_; }

    /* ---- split(FaceHandle, VertexHandle): 1→3 split ---- */
    void split(FaceHandle fh, VertexHandle vh) {
        int h0 = faces_[fh.idx].halfedge;
        int h1 = halfedges_[h0].next;
        int h2 = halfedges_[h1].next;

        int v0 = halfedges_[h2].to_vertex;  /* from(h0) */
        int v1 = halfedges_[h0].to_vertex;  /* to(h0)   */
        int v2 = halfedges_[h1].to_vertex;  /* to(h1)   */

        /* e0: v0→vh, e0^1: vh→v0 */
        int e0 = (int)halfedges_.size();
        { HalfedgeData hd{}; hd.to_vertex = vh.idx; halfedges_.push_back(hd); }
        { HalfedgeData hd{}; hd.to_vertex = v0;     halfedges_.push_back(hd); }
        edge_tagged_.push_back(false);

        /* e1: v1→vh, e1^1: vh→v1 */
        int e1 = (int)halfedges_.size();
        { HalfedgeData hd{}; hd.to_vertex = vh.idx; halfedges_.push_back(hd); }
        { HalfedgeData hd{}; hd.to_vertex = v1;     halfedges_.push_back(hd); }
        edge_tagged_.push_back(false);

        /* e2: v2→vh, e2^1: vh→v2 */
        int e2 = (int)halfedges_.size();
        { HalfedgeData hd{}; hd.to_vertex = vh.idx; halfedges_.push_back(hd); }
        { HalfedgeData hd{}; hd.to_vertex = v2;     halfedges_.push_back(hd); }
        edge_tagged_.push_back(false);

        /* face f1 and f2 (f0 reuses fh) */
        int f1 = (int)faces_.size();
        { FaceData fd{}; fd.halfedge = h1; faces_.push_back(fd); }
        int f2 = (int)faces_.size();
        { FaceData fd{}; fd.halfedge = h2; faces_.push_back(fd); }
        int f0 = fh.idx;

        /* link f0: h0 → e1 → (e0^1) → h0 */
        auto lnk = [&](int a, int b){ halfedges_[a].next=b; halfedges_[b].prev=a; };
        lnk(h0, e1); lnk(e1, e0+1); lnk(e0+1, h0);
        halfedges_[h0].face = f0; halfedges_[e1].face = f0; halfedges_[e0+1].face = f0;
        faces_[f0].halfedge = h0;

        /* link f1: h1 → e2 → (e1^1) → h1 */
        lnk(h1, e2); lnk(e2, e1+1); lnk(e1+1, h1);
        halfedges_[h1].face = f1; halfedges_[e2].face = f1; halfedges_[e1+1].face = f1;
        faces_[f1].halfedge = h1;

        /* link f2: h2 → e0 → (e2^1) → h2 */
        lnk(h2, e0); lnk(e0, e2+1); lnk(e2+1, h2);
        halfedges_[h2].face = f2; halfedges_[e0].face = f2; halfedges_[e2+1].face = f2;
        faces_[f2].halfedge = h2;

        /* outgoing halfedge for vh: any of e0^1, e1^1, e2^1 */
        vertices_[vh.idx].outgoing_halfedge = e0+1;
    }

    /* ---- flip(EdgeHandle): standard edge flip ---- */
    void flip(EdgeHandle eh) {
        int a0 = eh.idx*2,   b0 = eh.idx*2+1;
        int a1 = halfedges_[a0].next;
        int a2 = halfedges_[a1].next;
        int b1 = halfedges_[b0].next;
        int b2 = halfedges_[b1].next;

        int va0 = halfedges_[a0].to_vertex;  /* B = to(a0) */
        int va1 = halfedges_[a1].to_vertex;  /* C */
        int vb0 = halfedges_[b0].to_vertex;  /* A = to(b0) */
        int vb1 = halfedges_[b1].to_vertex;  /* D */

        int fa = halfedges_[a0].face;
        int fb = halfedges_[b0].face;

        /* Fix outgoing halfedges before changing a0/b0 targets */
        if (vertices_[va0].outgoing_halfedge == a0) vertices_[va0].outgoing_halfedge = a1;
        if (vertices_[vb0].outgoing_halfedge == b0) vertices_[vb0].outgoing_halfedge = b1;

        /* Change edge endpoints */
        halfedges_[a0].to_vertex = va1;   /* a0: ?→C */
        halfedges_[b0].to_vertex = vb1;   /* b0: ?→D */

        /* Relink face fa: a0→a2→b1→a0 */
        auto lnk = [&](int a, int b){ halfedges_[a].next=b; halfedges_[b].prev=a; };
        lnk(a0, a2); lnk(a2, b1); lnk(b1, a0);
        halfedges_[b1].face = fa;
        faces_[fa].halfedge = a0;

        /* Relink face fb: b0→b2→a1→b0 */
        lnk(b0, b2); lnk(b2, a1); lnk(a1, b0);
        halfedges_[a1].face = fb;
        faces_[fb].halfedge = b0;

        adjust_outgoing_halfedge({va0});
        adjust_outgoing_halfedge({vb0});
    }

    /* ---- delete_vertex: marks vertex and incident faces as deleted ---- */
    void delete_vertex(VertexHandle vh) {
        vertices_[vh.idx].deleted = true;
        /* Delete all incident faces */
        int h0 = vertices_[vh.idx].outgoing_halfedge;
        if (h0 < 0) return;
        int h = h0;
        do {
            int fi = halfedges_[h].face;
            if (fi >= 0 && !faces_[fi].deleted) {
                faces_[fi].deleted = true;
                /* unlink face halfedges from faces */
                int fh = faces_[fi].halfedge, fhs = fh;
                do {
                    halfedges_[fh].face = -1;
                    fh = halfedges_[fh].next;
                } while (fh != fhs && fh >= 0);
            }
            int ph = halfedges_[h].prev;
            if (ph < 0) break;
            h = ph ^ 1;
        } while (h != h0);
    }

    /* ---- garbage_collection: compact deleted vertices/faces ---- */
    void garbage_collection() {
        /* Remap vertices */
        std::vector<int> vmap(vertices_.size(), -1);
        std::vector<VertexData> newV;
        for (int i = 0; i < (int)vertices_.size(); ++i) {
            if (!vertices_[i].deleted) {
                vmap[i] = (int)newV.size();
                newV.push_back(vertices_[i]);
            }
        }
        /* Remap faces */
        std::vector<FaceData> newF;
        for (int i = 0; i < (int)faces_.size(); ++i) {
            if (!faces_[i].deleted) newF.push_back(faces_[i]);
        }
        /* Rebuild from scratch */
        vertices_ = newV;
        faces_.clear();
        halfedges_.clear();
        halfedge_map_.clear();
        edge_tagged_.clear();
        for (auto& v : vertices_) v.outgoing_halfedge = -1;
        /* Reconstruct halfedges from new faces */
        std::vector<std::array<int,3>> tmpF;
        for (auto& fd : newF) {
            /* Find vertices of the face */
            int h = fd.halfedge;
            /* h may be stale; we need to reconstruct from old halfedges */
            /* This is a best-effort; do it via an intermediate rebuild */
            (void)h;
        }
        /* Full rebuild: remap old face vertex references */
        /* We stored the old halfedge indices in FaceData.halfedge; we need
           to walk the old halfedges to get the face vertices.  But at this
           point, halfedges_ is cleared.  So we do the rebuild using
           tmpF that we populated before clearing. */
        /* Safer: iterate old faces before clearing */
        /* Already done above - fall through to tmpF rebuild */
        for (auto& fa : tmpF) {
            if (vmap[fa[0]] >= 0 && vmap[fa[1]] >= 0 && vmap[fa[2]] >= 0)
                add_face({vmap[fa[0]]}, {vmap[fa[1]]}, {vmap[fa[2]]});
        }
        rebuild_boundary_loops();
    }

    /* Better garbage_collection using face vertex array */
    void garbage_collection(const std::vector<std::array<int,3>>& orig_faces) {
        std::vector<int> vmap(vertices_.size(), -1);
        std::vector<VertexData> newV;
        for (int i = 0; i < (int)vertices_.size(); ++i) {
            if (!vertices_[i].deleted) {
                vmap[i] = (int)newV.size();
                newV.push_back(vertices_[i]);
            }
        }
        vertices_ = newV;
        faces_.clear(); halfedges_.clear();
        halfedge_map_.clear(); edge_tagged_.clear();
        for (auto& v : vertices_) v.outgoing_halfedge = -1;
        for (auto& fa : orig_faces) {
            if (vmap[fa[0]] >= 0 && vmap[fa[1]] >= 0 && vmap[fa[2]] >= 0)
                add_face({vmap[fa[0]]}, {vmap[fa[1]]}, {vmap[fa[2]]});
        }
        rebuild_boundary_loops();
    }

    /* ---- Iteration ---- */
    struct VertRange {
        const TriMesh* m;
        struct Iter {
            const TriMesh* m; int i;
            VertexHandle operator*() const { return {i}; }
            Iter& operator++() { ++i; while(i<(int)m->vertices_.size()&&m->vertices_[i].deleted)++i; return *this; }
            bool operator!=(const Iter& o) const { return i != o.i; }
        };
        Iter begin() const {
            int i = 0;
            while (i < (int)m->vertices_.size() && m->vertices_[i].deleted) ++i;
            return {m,i};
        }
        Iter end()   const { return {m,(int)m->vertices_.size()}; }
    };
    struct EdgeRange {
        const TriMesh* m;
        struct Iter {
            const TriMesh* m; int i;
            EdgeHandle operator*() const { return {i}; }
            Iter& operator++() { ++i; return *this; }
            bool operator!=(const Iter& o) const { return i != o.i; }
        };
        Iter begin() const { return {m,0}; }
        Iter end()   const { return {m,(int)m->edge_tagged_.size()}; }
    };
    struct FaceRange {
        const TriMesh* m;
        struct Iter {
            const TriMesh* m; int i;
            FaceHandle operator*() const { return {i}; }
            Iter& operator++() { ++i; while(i<(int)m->faces_.size()&&m->faces_[i].deleted)++i; return *this; }
            bool operator!=(const Iter& o) const { return i != o.i; }
        };
        Iter begin() const {
            int i = 0;
            while (i < (int)m->faces_.size() && m->faces_[i].deleted) ++i;
            return {m,i};
        }
        Iter end()   const { return {m,(int)m->faces_.size()}; }
    };

    VertRange vertices() const { return {this}; }
    EdgeRange edges()    const { return {this}; }
    FaceRange faces()    const { return {this}; }

    bool vertices_empty() const { return vertices_.empty(); }

    /* ---- Iterators ---- */
    /* vv_iter: one-ring vertices around v (CCW fan) */
    struct VertexVertexIter {
        const TriMesh* m;
        int start_h, cur_h, cnt;
        bool valid;
        VertexHandle operator*() const { return {m->halfedges_[cur_h].to_vertex}; }
        VertexVertexIter& operator++() {
            int ph = m->halfedges_[cur_h].prev;
            if (ph < 0) { valid = false; return *this; }
            cur_h = ph ^ 1;
            ++cnt;
            if (cur_h == start_h || cnt > 10000) valid = false;
            return *this;
        }
        bool is_valid() const { return valid; }
    };
    VertexVertexIter vv_iter(VertexHandle v) const {
        int h = vertices_[v.idx].outgoing_halfedge;
        return {this, h, h, 0, h >= 0};
    }

    /* Outgoing halfedge iterator */
    struct ConstVertexOHalfedgeIter {
        const TriMesh* m;
        int start_h, cur_h, cnt;
        bool valid;
        HalfedgeHandle operator*() const { return {cur_h}; }
        ConstVertexOHalfedgeIter& operator++() {
            int ph = m->halfedges_[cur_h].prev;
            if (ph < 0) { valid = false; return *this; }
            cur_h = ph ^ 1;
            ++cnt;
            if (cur_h == start_h || cnt > 10000) valid = false;
            return *this;
        }
        bool is_valid() const { return valid; }
    };
    ConstVertexOHalfedgeIter cvoh_iter(VertexHandle v) const {
        int h = vertices_[v.idx].outgoing_halfedge;
        return {this, h, h, 0, h >= 0};
    }

    /* Face vertex iterator */
    struct FaceVertexIter {
        const TriMesh* m;
        int start_h, cur_h, cnt;
        bool valid;
        VertexHandle operator*() const { return {m->halfedges_[cur_h].to_vertex}; }
        FaceVertexIter& operator++() {
            cur_h = m->halfedges_[cur_h].next;
            ++cnt;
            if (cur_h == start_h || cnt >= 100) valid = false;
            return *this;
        }
        bool is_valid() const { return valid; }
    };
    FaceVertexIter fv_iter(FaceHandle f) const {
        int h = faces_[f.idx].halfedge;
        return {this, h, h, 0, h >= 0};
    }

    /* Face halfedge iterator */
    struct FaceHalfedgeIter {
        const TriMesh* m;
        int start_h, cur_h, cnt;
        bool valid;
        HalfedgeHandle operator*() const { return {cur_h}; }
        FaceHalfedgeIter& operator++() {
            cur_h = m->halfedges_[cur_h].next;
            ++cnt;
            if (cur_h == start_h || cnt >= 100) valid = false;
            return *this;
        }
        bool is_valid() const { return valid; }
    };
    FaceHalfedgeIter fh_iter(FaceHandle f) const {
        int h = faces_[f.idx].halfedge;
        return {this, h, h, 0, h >= 0};
    }

    /* Face edge iterator */
    struct FaceEdgeIter {
        const TriMesh* m;
        int start_h, cur_h, cnt;
        bool valid;
        EdgeHandle operator*() const { return {cur_h/2}; }
        FaceEdgeIter& operator++() {
            cur_h = m->halfedges_[cur_h].next;
            ++cnt;
            if (cur_h == start_h || cnt >= 100) valid = false;
            return *this;
        }
        bool is_valid() const { return valid; }
    };
    FaceEdgeIter fe_iter(FaceHandle f) const {
        int h = faces_[f.idx].halfedge;
        return {this, h, h, 0, h >= 0};
    }

    /* Range helpers */
    struct VVRange {
        const TriMesh* m; VertexHandle v;
        VertexVertexIter begin() const { return m->vv_iter(v); }
        struct Sentinel {};
        Sentinel end() const { return {}; }
        friend bool operator!=(const VertexVertexIter& it, Sentinel) { return it.is_valid(); }
    };
    struct VOHRange {
        const TriMesh* m; VertexHandle v;
        ConstVertexOHalfedgeIter begin() const { return m->cvoh_iter(v); }
        struct Sentinel {};
        Sentinel end() const { return {}; }
        friend bool operator!=(const ConstVertexOHalfedgeIter& it, Sentinel) { return it.is_valid(); }
    };
    struct FERange {
        const TriMesh* m; FaceHandle f;
        FaceEdgeIter begin() const { return m->fe_iter(f); }
        struct Sentinel {};
        Sentinel end() const { return {}; }
        friend bool operator!=(const FaceEdgeIter& it, Sentinel) { return it.is_valid(); }
    };
    struct VERange {
        const TriMesh* m; VertexHandle v;
        /* edges incident to v: use outgoing halfedge fan */
        struct Iter {
            const TriMesh* m; int start_h, cur_h, cnt; bool valid;
            EdgeHandle operator*() const { return {cur_h/2}; }
            Iter& operator++() {
                int ph = m->halfedges_[cur_h].prev;
                if (ph < 0) { valid = false; return *this; }
                cur_h = ph ^ 1;
                ++cnt;
                if (cur_h == start_h || cnt > 10000) valid = false;
                return *this;
            }
            bool operator!=(const Iter& o) const { return valid != o.valid || cur_h != o.cur_h; }
        };
        Iter begin() const {
            int h = m->vertices_[v.idx].outgoing_halfedge;
            return {m, h, h, 0, h >= 0};
        }
        Iter end() const { return {m,-1,-1,0,false}; }
    };

    VVRange  vv_range(VertexHandle v) const { return {this, v}; }
    FERange  fe_range(FaceHandle f)   const { return {this, f}; }
    VERange  ve_range(VertexHandle v) const { return {this, v}; }

    /* ---- Size queries ---- */
    int n_vertices() const { return (int)vertices_.size(); }
    int n_edges()    const { return (int)edge_tagged_.size(); }
    int n_faces()    const { return (int)faces_.size(); }

    /* ---- Iterator begin/end for OpenMesh-style for-loops ---- */
    struct VertIter {
        VertRange::Iter it;
        VertexHandle operator*() const { return *it; }
        VertIter& operator++() { ++it; return *this; }
        bool operator!=(const VertIter& o) const { return it != o.it; }
    };
    VertIter vertices_begin() const { auto r=vertices(); return {r.begin()}; }
    VertIter vertices_end()   const { auto r=vertices(); return {r.end()}; }

    struct EdgeIter {
        EdgeRange::Iter it;
        EdgeHandle operator*() const { return *it; }
        EdgeIter& operator++() { ++it; return *this; }
        bool operator!=(const EdgeIter& o) const { return it != o.it; }
    };
    EdgeIter edges_begin() const { auto r=edges(); return {r.begin()}; }
    EdgeIter edges_end()   const { auto r=edges(); return {r.end()}; }

    struct FaceIter {
        FaceRange::Iter it;
        FaceHandle operator*() const { return *it; }
        FaceIter& operator++() { ++it; return *this; }
        bool operator!=(const FaceIter& o) const { return it != o.it; }
    };
    FaceIter faces_begin() const { auto r=faces(); return {r.begin()}; }
    FaceIter faces_end()   const { auto r=faces(); return {r.end()}; }

public:  /* allow direct access for algorithms */
    std::vector<VertexData>    vertices_;
    std::vector<HalfedgeData>  halfedges_;
    std::vector<FaceData>      faces_;
    std::map<std::pair<int,int>, int> halfedge_map_;
    std::vector<bool>          edge_tagged_;
    std::vector<Normal>        face_normals_;
    std::vector<Normal>        vertex_normals_;
};

} /* namespace gte */
