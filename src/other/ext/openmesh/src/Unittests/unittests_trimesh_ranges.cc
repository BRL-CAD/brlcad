#include <gtest/gtest.h>
#include <Unittests/unittests_common.hh>

#include <OpenMesh/Core/Utils/Predicates.hh>

#include <iostream>

namespace {

class OpenMeshTrimeshRange : public OpenMeshBase {

protected:

  // This function is called before each test is run
  virtual void SetUp()
  {
    mesh_.clear();

    // Add some vertices
    Mesh::VertexHandle vhandle[5];

    vhandle[0] = mesh_.add_vertex(Mesh::Point(0, 1, 0));
    vhandle[1] = mesh_.add_vertex(Mesh::Point(1, 0, 0));
    vhandle[2] = mesh_.add_vertex(Mesh::Point(2, 1, 0));
    vhandle[3] = mesh_.add_vertex(Mesh::Point(0,-1, 0));
    vhandle[4] = mesh_.add_vertex(Mesh::Point(2,-1, 0));

    // Add two faces
    std::vector<Mesh::VertexHandle> face_vhandles;

    face_vhandles.push_back(vhandle[0]);
    face_vhandles.push_back(vhandle[1]);
    face_vhandles.push_back(vhandle[2]);
    mesh_.add_face(face_vhandles);

    face_vhandles.clear();

    face_vhandles.push_back(vhandle[1]);
    face_vhandles.push_back(vhandle[3]);
    face_vhandles.push_back(vhandle[4]);
    mesh_.add_face(face_vhandles);

    face_vhandles.clear();

    face_vhandles.push_back(vhandle[0]);
    face_vhandles.push_back(vhandle[3]);
    face_vhandles.push_back(vhandle[1]);
    mesh_.add_face(face_vhandles);

    face_vhandles.clear();

    face_vhandles.push_back(vhandle[2]);
    face_vhandles.push_back(vhandle[1]);
    face_vhandles.push_back(vhandle[4]);
    mesh_.add_face(face_vhandles);

    /* Test setup:
        0 ==== 2
        |\  0 /|
        | \  / |
        |2  1 3|
        | /  \ |
        |/  1 \|
        3 ==== 4 */
  }

  // This function is called after all tests are through
  virtual void TearDown() {

    // Do some final stuff with the member data here...
  }


  // Member already defined in OpenMeshBase
  //Mesh mesh_;
};

/*
 * ====================================================================
 * Define tests below
 * ====================================================================
 */


template <typename RangeT1, typename RangeT2>
void compare_ranges(RangeT1&& range1, RangeT2&& range2, int offset, bool reverse)
{
  auto vec1 = range1.to_vector();
  auto vec2 = range2.to_vector();

  ASSERT_EQ(vec1.size(), vec2.size()) << "Ranges have different number of elements";
  ASSERT_GT(vec1.size(), 0u) << "Ranges are empty";

  size_t n = vec1.size();

  std::cout << "Vec1 elements: ";
  for (auto el : vec1)
    std::cout << el.idx() << " ";
  std::cout << std::endl;

  std::cout << "Vec2 elements: ";
  for (auto el : vec2)
    std::cout << el.idx() << " ";
  std::cout << std::endl;

  for (size_t i = 0; i < n; ++i)
  {
    size_t id1 = (i+offset)%n;
    size_t id2 = reverse ? (n-i-1)%n : i;
    EXPECT_EQ(vec1[id1].idx(), vec2[id2].idx()) << "Elements are not the same with offset = " << offset << " and reverse = " << std::to_string(reverse);
  }
}


TEST_F(OpenMeshTrimeshRange, VertexVertexRangeMeshVsSmartHandle)
{
  for (auto vh : mesh_.vertices())
  {
    compare_ranges(mesh_.vv_range(vh)    , vh.vertices()    , 0, false);
    compare_ranges(mesh_.vv_cw_range(vh) , vh.vertices_cw() , 0, false);
    compare_ranges(mesh_.vv_ccw_range(vh), vh.vertices_ccw(), 0, false);
  }
}

TEST_F(OpenMeshTrimeshRange, VertexVertexRangeCWVsCCW)
{
  for (auto vh : mesh_.vertices())
    compare_ranges(vh.vertices_cw(), vh.vertices_ccw(), 0, true);
}


TEST_F(OpenMeshTrimeshRange, VertexEdgeRangeMeshVsSmartHandle)
{
  for (auto vh : mesh_.vertices())
  {
    compare_ranges(mesh_.ve_range(vh)    , vh.edges()    , 0, false);
    compare_ranges(mesh_.ve_cw_range(vh) , vh.edges_cw() , 0, false);
    compare_ranges(mesh_.ve_ccw_range(vh), vh.edges_ccw(), 0, false);
  }
}

TEST_F(OpenMeshTrimeshRange, VertexEdgeRangeCWVsCCW)
{
  for (auto vh : mesh_.vertices())
    compare_ranges(vh.edges_cw(), vh.edges_ccw(), 0, true);
}

TEST_F(OpenMeshTrimeshRange, VertexFaceRangeMeshVsSmartHandle)
{
  for (auto vh : mesh_.vertices())
  {
    compare_ranges(mesh_.vf_range(vh)    , vh.faces()    , 0, false);
    compare_ranges(mesh_.vf_cw_range(vh) , vh.faces_cw() , 0, false);
    compare_ranges(mesh_.vf_ccw_range(vh), vh.faces_ccw(), 0, false);
  }
}

TEST_F(OpenMeshTrimeshRange, VertexFaceRangeCWVsCCW)
{
  for (auto vh : mesh_.vertices())
    compare_ranges(vh.faces_cw(), vh.faces_ccw(), 0, true);
}

TEST_F(OpenMeshTrimeshRange, VertexIncomingHalfedgesRangeMeshVsSmartHandle)
{
  for (auto vh : mesh_.vertices())
  {
    compare_ranges(mesh_.vih_range(vh)    , vh.incoming_halfedges()    , 0, false);
    compare_ranges(mesh_.vih_cw_range(vh) , vh.incoming_halfedges_cw() , 0, false);
    compare_ranges(mesh_.vih_ccw_range(vh), vh.incoming_halfedges_ccw(), 0, false);
  }
}

TEST_F(OpenMeshTrimeshRange, VertexIncomingHalfedgesRangeCWVsCCW)
{
  for (auto vh : mesh_.vertices())
    compare_ranges(vh.incoming_halfedges_cw(), vh.incoming_halfedges_ccw(), 0, true);
}

TEST_F(OpenMeshTrimeshRange, VertexIncomingHalfedgesRangeCustomStart)
{
  const int offset = 2;
  for (auto vh : mesh_.vertices())
  {
    // mesh vs smart handle access
    auto it_cw  = mesh_.vih_begin(vh);
    auto it_ccw = mesh_.vih_ccwbegin(vh);
    for (int i = 0; i < offset; ++i)
    {
      ++it_cw;
      ++it_ccw;
    }
    OpenMesh::HalfedgeHandle start_heh_cw  = *it_cw;
    OpenMesh::HalfedgeHandle start_heh_ccw = *it_ccw;
    compare_ranges(mesh_.vih_range(vh)        , mesh_.vih_range(start_heh_cw)           , offset, false);
    compare_ranges(mesh_.vih_cw_range(vh)     , mesh_.vih_cw_range(start_heh_cw)        , offset, false);
    compare_ranges(mesh_.vih_ccw_range(vh)    , mesh_.vih_ccw_range(start_heh_ccw)      , offset, false);
    compare_ranges(vh.incoming_halfedges()    , vh.incoming_halfedges(start_heh_cw)     , offset, false);
    compare_ranges(vh.incoming_halfedges_cw() , vh.incoming_halfedges_cw(start_heh_cw)  , offset, false);
    compare_ranges(vh.incoming_halfedges_ccw(), vh.incoming_halfedges_ccw(start_heh_ccw), offset, false);

    // check that ranges with custom start actually start at that start
    auto hes = vh.incoming_halfedges();
    for (auto heh : hes)
    {
      EXPECT_EQ(heh, *vh.incoming_halfedges    (heh).begin()) << "Range does not start at desired start";
      EXPECT_EQ(heh, *vh.incoming_halfedges_cw (heh).begin()) << "Range does not start at desired start";
      EXPECT_EQ(heh, *vh.incoming_halfedges_ccw(heh).begin()) << "Range does not start at desired start";
    }
  }
}


TEST_F(OpenMeshTrimeshRange, VertexIncomingHalfedgesRangeBoundaryConsistency)
{
  // for boundary vertices cw iterations should start at boundary halfedge
  for (auto vh : mesh_.vertices().filtered(OpenMesh::Predicates::Boundary()))
  {
    EXPECT_TRUE(mesh_.vih_begin(vh)->opp().is_boundary());
    EXPECT_TRUE(mesh_.vih_range(vh).begin()->opp().is_boundary());

    EXPECT_TRUE(mesh_.vih_cwbegin(vh)->opp().is_boundary());
    EXPECT_TRUE(mesh_.vih_cw_range(vh).begin()->opp().is_boundary());

    EXPECT_TRUE(mesh_.vih_ccwbegin(vh)->is_boundary());
    EXPECT_TRUE(mesh_.vih_ccw_range(vh).begin()->is_boundary());
  }
}

TEST_F(OpenMeshTrimeshRange, VertexOutgoingHalfedgesRangeMeshVsSmartHandle)
{
  for (auto vh : mesh_.vertices())
  {
    compare_ranges(mesh_.voh_range(vh)    , vh.outgoing_halfedges()    , 0, false);
    compare_ranges(mesh_.voh_cw_range(vh) , vh.outgoing_halfedges_cw() , 0, false);
    compare_ranges(mesh_.voh_ccw_range(vh), vh.outgoing_halfedges_ccw(), 0, false);
  }
}

TEST_F(OpenMeshTrimeshRange, VertexOutgoingHalfedgesRangeCWVsCCW)
{
  for (auto vh : mesh_.vertices())
    compare_ranges(vh.outgoing_halfedges_cw(), vh.outgoing_halfedges_ccw(), 0, true);
}

TEST_F(OpenMeshTrimeshRange, VertexOutgoingHalfedgesRangeCustomStart)
{
  const int offset = 2;
  for (auto vh : mesh_.vertices())
  {
    // mesh vs smart handle access
    auto it_cw  = mesh_.voh_begin(vh);
    auto it_ccw = mesh_.voh_ccwbegin(vh);
    for (int i = 0; i < offset; ++i)
    {
      ++it_cw;
      ++it_ccw;
    }
    OpenMesh::HalfedgeHandle start_heh_cw  = *it_cw;
    OpenMesh::HalfedgeHandle start_heh_ccw = *it_ccw;
    compare_ranges(mesh_.voh_range(vh)        , mesh_.voh_range(start_heh_cw)           , offset, false);
    compare_ranges(mesh_.voh_cw_range(vh)     , mesh_.voh_cw_range(start_heh_cw)        , offset, false);
    compare_ranges(mesh_.voh_ccw_range(vh)    , mesh_.voh_ccw_range(start_heh_ccw)      , offset, false);
    compare_ranges(vh.outgoing_halfedges()    , vh.outgoing_halfedges(start_heh_cw)     , offset, false);
    compare_ranges(vh.outgoing_halfedges_cw() , vh.outgoing_halfedges_cw(start_heh_cw)  , offset, false);
    compare_ranges(vh.outgoing_halfedges_ccw(), vh.outgoing_halfedges_ccw(start_heh_ccw), offset, false);


    // check that ranges with custom start actually start at that start
    auto hes = vh.outgoing_halfedges();
    for (auto heh : hes)
    {
      EXPECT_EQ(heh, *vh.outgoing_halfedges    (heh).begin()) << "Range does not start at desired start";
      EXPECT_EQ(heh, *vh.outgoing_halfedges_cw (heh).begin()) << "Range does not start at desired start";
      EXPECT_EQ(heh, *vh.outgoing_halfedges_ccw(heh).begin()) << "Range does not start at desired start";
    }
  }
}


TEST_F(OpenMeshTrimeshRange, VertexOutgoingHalfedgesRangeBoundaryConsistency)
{
  // for boundary vertices cw iterations should start at boundary halfedge
  for (auto vh : mesh_.vertices().filtered(OpenMesh::Predicates::Boundary()))
  {
    EXPECT_TRUE(mesh_.voh_begin(vh)->is_boundary());
    EXPECT_TRUE(mesh_.voh_range(vh).begin()->is_boundary());

    EXPECT_TRUE(mesh_.voh_cwbegin(vh)->is_boundary());
    EXPECT_TRUE(mesh_.voh_cw_range(vh).begin()->is_boundary());

    EXPECT_TRUE(mesh_.voh_ccwbegin(vh)->opp().is_boundary());
    EXPECT_TRUE(mesh_.voh_ccw_range(vh).begin()->opp().is_boundary());
  }
}


TEST_F(OpenMeshTrimeshRange, HalfedgeLoopRangeMeshVsSmartHandle)
{
  for (auto heh : mesh_.halfedges())
  {
    compare_ranges(mesh_.hl_range(heh)    , heh.loop()    , 0, false);
    compare_ranges(mesh_.hl_cw_range(heh) , heh.loop_cw() , 0, false);
    compare_ranges(mesh_.hl_ccw_range(heh), heh.loop_ccw(), 0, false);
  }
}

TEST_F(OpenMeshTrimeshRange, HalfedgeLoopRangeCWVsCCW)
{
  // Halfedge Loops are initialized from a halfedge and always start on that halfedge
  // Thus, cw and ccw ranges start on the same element. We account for that by
  // setting the offset to 1 for the comparison check.
  // TODO: do we want to change that behavior?
  for (auto heh : mesh_.halfedges())
    compare_ranges(heh.loop_cw(), heh.loop_ccw(), 1, true);
}


TEST_F(OpenMeshTrimeshRange, FaceVertexRangeMeshVsSmartHandle)
{
  for (auto fh : mesh_.faces())
  {
    compare_ranges(mesh_.fv_range(fh)    , fh.vertices()    , 0, false);
    compare_ranges(mesh_.fv_cw_range(fh) , fh.vertices_cw() , 0, false);
    compare_ranges(mesh_.fv_ccw_range(fh), fh.vertices_ccw(), 0, false);
  }
}

TEST_F(OpenMeshTrimeshRange, FaceVertexRangeCWVsCCW)
{
  for (auto fh : mesh_.faces())
    compare_ranges(fh.vertices_cw(), fh.vertices_ccw(), 0, true);
}

TEST_F(OpenMeshTrimeshRange, FaceEdgeRangeMeshVsSmartHandle)
{
  for (auto fh : mesh_.faces())
  {
    compare_ranges(mesh_.fe_range(fh)    , fh.edges()    , 0, false);
    compare_ranges(mesh_.fe_cw_range(fh) , fh.edges_cw() , 0, false);
    compare_ranges(mesh_.fe_ccw_range(fh), fh.edges_ccw(), 0, false);
  }
}

TEST_F(OpenMeshTrimeshRange, FaceEdgeRangeCWVsCCW)
{
  for (auto fh : mesh_.faces())
    compare_ranges(fh.edges_cw(), fh.edges_ccw(), 0, true);
}

TEST_F(OpenMeshTrimeshRange, FaceHalfedgeRangeMeshVsSmartHandle)
{
  for (auto fh : mesh_.faces())
  {
    compare_ranges(mesh_.fh_range(fh)    , fh.halfedges()    , 0, false);
    compare_ranges(mesh_.fh_cw_range(fh) , fh.halfedges_cw() , 0, false);
    compare_ranges(mesh_.fh_ccw_range(fh), fh.halfedges_ccw(), 0, false);
  }
}

TEST_F(OpenMeshTrimeshRange, FaceHalfedgeRangeCWVsCCW)
{
  for (auto fh : mesh_.faces())
    compare_ranges(fh.halfedges_cw(), fh.halfedges_ccw(), 0, true);
}

TEST_F(OpenMeshTrimeshRange, FaceFaceRangeMeshVsSmartHandle)
{
  for (auto fh : mesh_.faces())
  {
    compare_ranges(mesh_.ff_range(fh)    , fh.faces()    , 0, false);
    compare_ranges(mesh_.ff_cw_range(fh) , fh.faces_cw() , 0, false);
    compare_ranges(mesh_.ff_ccw_range(fh), fh.faces_ccw(), 0, false);
  }
}

TEST_F(OpenMeshTrimeshRange, FaceFaceRangeCWVsCCW)
{
  for (auto fh : mesh_.faces())
    compare_ranges(fh.faces_cw(), fh.faces_ccw(), 0, true);
}


}
