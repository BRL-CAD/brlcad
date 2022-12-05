#include <gtest/gtest.h>
#include <Unittests/unittests_common.hh>

#include <iostream>

namespace {

class OpenMeshCentroids : public OpenMeshBase {
  
protected:
  
  // This function is called before each test is run
  virtual void SetUp() {
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

};


/*
 * ====================================================================
 * Define tests below
 * ====================================================================
 */

/*
 * Calculate centroid on a single triangle
 */
TEST_F(OpenMeshCentroids, FaceCentroidTriMesh)
{
  auto centroid = mesh_.calc_centroid(OpenMesh::FaceHandle(0));

  EXPECT_EQ( centroid[0], 1.0f );
  EXPECT_EQ( centroid[1], Mesh::Scalar(2)/Mesh::Scalar(3));
  EXPECT_EQ( centroid[2], 0.0f );
}


/*
 * Calculate centroid on a single edge
 */
TEST_F(OpenMeshCentroids, EdgeCentroidTriMesh)
{
  auto centroid = mesh_.calc_centroid(OpenMesh::EdgeHandle(2));

  EXPECT_EQ( centroid[0], 1.0f );
  EXPECT_EQ( centroid[1], 1.0f );
  EXPECT_EQ( centroid[2], 0.0f );
}

/*
 * Calculate centroid on a single halfedge
 */
TEST_F(OpenMeshCentroids, HalfedgeCentroidTriMesh)
{
  auto centroid = mesh_.calc_centroid(OpenMesh::HalfedgeHandle(5));

  EXPECT_EQ( centroid[0], 1.0f );
  EXPECT_EQ( centroid[1], 1.0f );
  EXPECT_EQ( centroid[2], 0.0f );
}

/*
 * Calculate centroid on a single vertex
 */
TEST_F(OpenMeshCentroids, VertexCentroidTriMesh)
{
  auto centroid = mesh_.calc_centroid(OpenMesh::VertexHandle(0));

  EXPECT_EQ( centroid[0], 0.0f );
  EXPECT_EQ( centroid[1], 1.0f );
  EXPECT_EQ( centroid[2], 0.0f );
}


/*
 * Calculate centroid of whole mesh
 */
TEST_F(OpenMeshCentroids, MeshCentroidTriMesh)
{
  auto centroid = mesh_.calc_centroid(OpenMesh::MeshHandle());

  EXPECT_EQ( centroid[0], 1.0f );
  EXPECT_EQ( centroid[1], 0.0f );
  EXPECT_EQ( centroid[2], 0.0f );
}







}
