#include <gtest/gtest.h>
#include <Unittests/unittests_common.hh>

#include <OpenMesh/Core/Mesh/PolyConnectivity.hh>
#include <OpenMesh/Core/Utils/PropertyManager.hh>
#include <OpenMesh/Core/Utils/Predicates.hh>

#include <iostream>
#include <chrono>

namespace {

class OpenMeshSmartRanges : public OpenMeshBase {

protected:

  // This function is called before each test is run
  virtual void SetUp() {

    mesh_.clear();

    // Add some vertices
    Mesh::VertexHandle vhandle[8];
    vhandle[0] = mesh_.add_vertex(Mesh::Point(-1, -1,  1));
    vhandle[1] = mesh_.add_vertex(Mesh::Point( 1, -1,  1));
    vhandle[2] = mesh_.add_vertex(Mesh::Point( 1,  1,  1));
    vhandle[3] = mesh_.add_vertex(Mesh::Point(-1,  1,  1));
    vhandle[4] = mesh_.add_vertex(Mesh::Point(-1, -1, -1));
    vhandle[5] = mesh_.add_vertex(Mesh::Point( 1, -1, -1));
    vhandle[6] = mesh_.add_vertex(Mesh::Point( 1,  1, -1));
    vhandle[7] = mesh_.add_vertex(Mesh::Point(-1,  1, -1));

    // Add six faces to form a cube
    std::vector<Mesh::VertexHandle> face_vhandles;

    face_vhandles.clear();
    face_vhandles.push_back(vhandle[0]);
    face_vhandles.push_back(vhandle[1]);
    face_vhandles.push_back(vhandle[3]);
    mesh_.add_face(face_vhandles);

    face_vhandles.clear();
    face_vhandles.push_back(vhandle[1]);
    face_vhandles.push_back(vhandle[2]);
    face_vhandles.push_back(vhandle[3]);
    mesh_.add_face(face_vhandles);

    //=======================

    face_vhandles.clear();
    face_vhandles.push_back(vhandle[7]);
    face_vhandles.push_back(vhandle[6]);
    face_vhandles.push_back(vhandle[5]);
    mesh_.add_face(face_vhandles);

    face_vhandles.clear();
    face_vhandles.push_back(vhandle[7]);
    face_vhandles.push_back(vhandle[5]);
    face_vhandles.push_back(vhandle[4]);
    mesh_.add_face(face_vhandles);

    //=======================

    face_vhandles.clear();
    face_vhandles.push_back(vhandle[1]);
    face_vhandles.push_back(vhandle[0]);
    face_vhandles.push_back(vhandle[4]);
    mesh_.add_face(face_vhandles);

    face_vhandles.clear();
    face_vhandles.push_back(vhandle[1]);
    face_vhandles.push_back(vhandle[4]);
    face_vhandles.push_back(vhandle[5]);
    mesh_.add_face(face_vhandles);

    //=======================

    face_vhandles.clear();
    face_vhandles.push_back(vhandle[2]);
    face_vhandles.push_back(vhandle[1]);
    face_vhandles.push_back(vhandle[5]);
    mesh_.add_face(face_vhandles);

    face_vhandles.clear();
    face_vhandles.push_back(vhandle[2]);
    face_vhandles.push_back(vhandle[5]);
    face_vhandles.push_back(vhandle[6]);
    mesh_.add_face(face_vhandles);


    //=======================

    face_vhandles.clear();
    face_vhandles.push_back(vhandle[3]);
    face_vhandles.push_back(vhandle[2]);
    face_vhandles.push_back(vhandle[6]);
    mesh_.add_face(face_vhandles);

    face_vhandles.clear();
    face_vhandles.push_back(vhandle[3]);
    face_vhandles.push_back(vhandle[6]);
    face_vhandles.push_back(vhandle[7]);
    mesh_.add_face(face_vhandles);

    //=======================

    face_vhandles.clear();
    face_vhandles.push_back(vhandle[0]);
    face_vhandles.push_back(vhandle[3]);
    face_vhandles.push_back(vhandle[7]);
    mesh_.add_face(face_vhandles);

    face_vhandles.clear();
    face_vhandles.push_back(vhandle[0]);
    face_vhandles.push_back(vhandle[7]);
    face_vhandles.push_back(vhandle[4]);
    mesh_.add_face(face_vhandles);


    // Test setup:
    //
    //
    //    3 ======== 2
    //   /          /|
    //  /          / |      z
    // 0 ======== 1  |      |
    // |          |  |      |   y
    // |  7       |  6      |  /
    // |          | /       | /
    // |          |/        |/
    // 4 ======== 5         -------> x
    //

    // Check setup
    EXPECT_EQ(18u, mesh_.n_edges() )     << "Wrong number of Edges";
    EXPECT_EQ(36u, mesh_.n_halfedges() ) << "Wrong number of HalfEdges";
    EXPECT_EQ(8u, mesh_.n_vertices() )   << "Wrong number of vertices";
    EXPECT_EQ(12u, mesh_.n_faces() )     << "Wrong number of faces";
  }

  // This function is called after all tests are through
  virtual void TearDown() {

    // Do some final stuff with the member data here...

    mesh_.clear();
  }

  // Member already defined in OpenMeshBase
  //Mesh mesh_;
};

/*
 * ====================================================================
 * Define tests below
 * ====================================================================
 */


template <typename HandleT>
struct F
{
  unsigned int operator()(HandleT ) { return 1; }
};

/* Test if smart ranges work
 */
TEST_F(OpenMeshSmartRanges, Sum)
{
  auto one = [](OpenMesh::VertexHandle ) { return 1u; };
  EXPECT_EQ(mesh_.vertices().sum(one), mesh_.n_vertices());
  EXPECT_EQ(mesh_.vertices().sum(F<OpenMesh::VertexHandle>()), mesh_.n_vertices());
  EXPECT_EQ(mesh_.halfedges().sum(F<OpenMesh::HalfedgeHandle>()), mesh_.n_halfedges());
  EXPECT_EQ(mesh_.edges().sum(F<OpenMesh::EdgeHandle>()), mesh_.n_edges());
  EXPECT_EQ(mesh_.faces().sum(F<OpenMesh::FaceHandle>()), mesh_.n_faces());

  for (auto vh : mesh_.vertices())
    EXPECT_EQ(vh.vertices().sum(F<OpenMesh::VertexHandle>()), mesh_.valence(vh));
  for (auto vh : mesh_.vertices())
    EXPECT_EQ(vh.faces().sum(F<OpenMesh::FaceHandle>()), mesh_.valence(vh));
  for (auto vh : mesh_.vertices())
    EXPECT_EQ(vh.outgoing_halfedges().sum(F<OpenMesh::HalfedgeHandle>()), mesh_.valence(vh));
  for (auto vh : mesh_.vertices())
    EXPECT_EQ(vh.incoming_halfedges().sum(F<OpenMesh::HalfedgeHandle>()), mesh_.valence(vh));

  for (auto fh : mesh_.faces())
    EXPECT_EQ(fh.vertices().sum(F<OpenMesh::VertexHandle>()), mesh_.valence(fh));
  for (auto fh : mesh_.faces())
    EXPECT_EQ(fh.halfedges().sum(F<OpenMesh::HalfedgeHandle>()), mesh_.valence(fh));
  for (auto fh : mesh_.faces())
    EXPECT_EQ(fh.edges().sum(F<OpenMesh::EdgeHandle>()), mesh_.valence(fh));
  for (auto fh : mesh_.faces())
    EXPECT_EQ(fh.faces().sum(F<OpenMesh::FaceHandle>()), 3u);
}


/* Test if Property Manager can be used in smart ranges
 */
TEST_F(OpenMeshSmartRanges, PropertyManagerAsFunctor)
{
  OpenMesh::VProp<Mesh::Point> myPos(mesh_);
  for (auto vh : mesh_.vertices())
    myPos(vh) = mesh_.point(vh);

  Mesh::Point cog(0,0,0);
  for (auto vh : mesh_.vertices())
    cog += mesh_.point(vh);
  cog /= mesh_.n_vertices();

  auto cog2 = mesh_.vertices().avg(myPos);

  EXPECT_LT(norm(cog - cog2), 0.00001) << "Computed center of gravities are significantly different.";
}

/* Test to vector
 */
TEST_F(OpenMeshSmartRanges, ToVector)
{
  OpenMesh::HProp<OpenMesh::Vec2d> uvs(mesh_);

  for (auto heh : mesh_.halfedges())
    uvs(heh) = OpenMesh::Vec2d(heh.idx(), (heh.idx() * 13)%7);

  for (auto fh : mesh_.faces())
  {
    auto tri_uvs = fh.halfedges().to_vector(uvs);
    auto heh_handles = fh.halfedges().to_vector();
    for (auto heh : heh_handles)
      heh.next();
  }

  auto vertex_vec = mesh_.vertices().to_vector();
  for (auto vh : vertex_vec)
    vh.out();
}

/* Test to array
 */
TEST_F(OpenMeshSmartRanges, ToArray)
{
  OpenMesh::HProp<OpenMesh::Vec2d> uvs(mesh_);

  for (auto heh : mesh_.halfedges())
    uvs(heh) = OpenMesh::Vec2d(heh.idx(), (heh.idx() * 13)%7);

  for (auto fh : mesh_.faces())
  {
    fh.halfedges().to_array<3>(uvs);
    fh.halfedges().to_array<3>();
  }
}


/* Test bounding box
 */
TEST_F(OpenMeshSmartRanges, BoundingBox)
{
  // The custom vecs OpenMesh are tested with here do not implement a min or max function.
  // Thus we convert here.
  OpenMesh::VProp<OpenMesh::Vec3f> myPos(mesh_);
  for (auto vh : mesh_.vertices())
    for (int i = 0; i < 3; ++i)
      myPos(vh)[i] = mesh_.point(vh)[i];

  auto bb_min = mesh_.vertices().min(myPos);
  auto bb_max = mesh_.vertices().max(myPos);
  mesh_.vertices().minmax(myPos);

  EXPECT_LT(norm(bb_min - OpenMesh::Vec3f(-1,-1,-1)), 0.000001) << "Bounding box minimum seems off";
  EXPECT_LT(norm(bb_max - OpenMesh::Vec3f( 1, 1, 1)), 0.000001) << "Bounding box maximum seems off";


  auto uvs = OpenMesh::makeTemporaryProperty<OpenMesh::HalfedgeHandle, OpenMesh::Vec2d>(mesh_);
  for (auto heh : mesh_.halfedges())
    uvs(heh) = OpenMesh::Vec2d(heh.idx(), (heh.idx() * 13)%7);

  for (auto fh : mesh_.faces())
  {
    fh.halfedges().min(uvs);
    fh.halfedges().max(uvs);
  }
}


/* Test for each
 */
TEST_F(OpenMeshSmartRanges, ForEach)
{
  std::vector<int> vec;
  auto f = [&vec](OpenMesh::VertexHandle vh) { vec.push_back(vh.idx()); };

  mesh_.vertices().for_each(f);

  ASSERT_EQ(vec.size(), mesh_.n_vertices()) << "vec has wrong size";
  for (size_t i = 0; i < vec.size(); ++i)
    EXPECT_EQ(vec[i], static_cast<int>(i)) << "wrong index in vector";
}


/* Test filter
 */
TEST_F(OpenMeshSmartRanges, Filtered)
{
  using VH = OpenMesh::VertexHandle;
  using FH = OpenMesh::FaceHandle;

  auto is_even            = [](VH vh) { return vh.idx() % 2 == 0; };
  auto is_odd             = [](VH vh) { return vh.idx() % 2 == 1; };
  auto is_divisible_by_3  = [](VH vh) { return vh.idx() % 3 == 0; };
  auto to_id              = [](VH vh) { return vh.idx(); };

  auto even_vertices = mesh_.vertices().filtered(is_even).to_vector(to_id);
  EXPECT_EQ(even_vertices.size(), 4u);
  EXPECT_EQ(even_vertices[0], 0);
  EXPECT_EQ(even_vertices[1], 2);
  EXPECT_EQ(even_vertices[2], 4);
  EXPECT_EQ(even_vertices[3], 6);

  auto odd_vertices = mesh_.vertices().filtered(is_odd).to_vector(to_id);
  EXPECT_EQ(odd_vertices.size(), 4u);
  EXPECT_EQ(odd_vertices[0], 1);
  EXPECT_EQ(odd_vertices[1], 3);
  EXPECT_EQ(odd_vertices[2], 5);
  EXPECT_EQ(odd_vertices[3], 7);

  auto even_3_vertices = mesh_.vertices().filtered(is_even).filtered(is_divisible_by_3).to_vector(to_id);
  EXPECT_EQ(even_3_vertices.size(), 2u);
  EXPECT_EQ(even_3_vertices[0], 0);
  EXPECT_EQ(even_3_vertices[1], 6);

  auto odd_3_vertices = mesh_.vertices().filtered(is_odd).filtered(is_divisible_by_3).to_vector(to_id);
  EXPECT_EQ(odd_3_vertices.size(), 1u);
  EXPECT_EQ(odd_3_vertices[0], 3);


  // create a vector of vertices in the order they are visited when iterating over face vertices, but every vertex only once
  std::vector<VH> vertices;
  OpenMesh::VProp<bool> to_be_processed(true, mesh_);
  auto store_vertex = [&](VH vh) { to_be_processed(vh) = false; vertices.push_back(vh); };

  for (auto fh : mesh_.faces())
    fh.vertices().filtered(to_be_processed).for_each(store_vertex);

  EXPECT_EQ(vertices.size(), mesh_.n_vertices()) << " number of visited vertices not correct";
  EXPECT_TRUE(mesh_.vertices().all_of([&](VH vh) { return !to_be_processed(vh); })) << "did not visit all vertices";

  {
    OpenMesh::FProp<bool> to_be_visited(true, mesh_);
    size_t visited_faces_in_main_loop = 0;
    size_t visited_faces_in_sub_loop = 0;
    for (auto fh : mesh_.faces().filtered(to_be_visited))
    {
      to_be_visited(fh) = false;
      ++visited_faces_in_main_loop;
      for (auto neighbor : fh.faces().filtered(to_be_visited))
      {
        to_be_visited(neighbor) = false;
        ++visited_faces_in_sub_loop;
      }
    }

    EXPECT_LT(visited_faces_in_main_loop, mesh_.n_faces()) << "Visted more faces than expected";
    EXPECT_TRUE(mesh_.faces().all_of([&](FH fh) { return !to_be_visited(fh); })) << "did not visit all faces";
    EXPECT_EQ(visited_faces_in_main_loop + visited_faces_in_sub_loop, mesh_.n_faces()) << "Did not visited all faces exactly once";
  }

  {
    OpenMesh::FProp<bool> to_be_visited(true, mesh_);
    const auto& to_be_visited_const_ref = to_be_visited;
    size_t visited_faces_in_main_loop = 0;
    size_t visited_faces_in_sub_loop = 0;
    for (auto fh : mesh_.faces().filtered(to_be_visited_const_ref))
    {
      to_be_visited(fh) = false;
      ++visited_faces_in_main_loop;
      for (auto neighbor : fh.faces().filtered(to_be_visited_const_ref))
      {
        to_be_visited(neighbor) = false;
        ++visited_faces_in_sub_loop;
      }
    }

    EXPECT_LT(visited_faces_in_main_loop, mesh_.n_faces()) << "Visted more faces than expected";
    EXPECT_TRUE(mesh_.faces().all_of([&](FH fh) { return !to_be_visited(fh); })) << "did not visit all faces";
    EXPECT_EQ(visited_faces_in_main_loop + visited_faces_in_sub_loop, mesh_.n_faces()) << "Did not visited all faces exactly once";
  }

}

/* Test avg
 */
TEST_F(OpenMeshSmartRanges, Avg)
{

  Mesh::Point cog(0,0,0);
  for (auto vh : mesh_.vertices())
    cog += mesh_.point(vh);
  cog /= mesh_.n_vertices();

  auto points = OpenMesh::getPointsProperty(mesh_);
  auto cog2 = mesh_.vertices().avg(points);

  EXPECT_LT(norm(cog - cog2), 0.00001) << "Computed center of gravities are significantly different.";
}

/* Test weighted avg
 */
TEST_F(OpenMeshSmartRanges, WeightedAvg)
{
  Mesh::Point cog(0,0,0);
  for (auto fh : mesh_.faces())
    cog += mesh_.calc_face_centroid(fh);
  cog /= mesh_.n_faces();

  OpenMesh::FProp<float> area(mesh_);
  for (auto fh : mesh_.faces())
    area[fh] = mesh_.calc_face_area(fh);

  auto cog2 = mesh_.faces().avg([&](OpenMesh::FaceHandle fh) { return mesh_.calc_face_centroid(fh); }, area);

  EXPECT_LT(norm(cog - cog2), 0.00001) << "Computed area weighted center of gravities are significantly different.";
}


template <typename HandleT>
void test_range_predicates(Mesh& _mesh)
{
  using namespace OpenMesh::Predicates;

  auto get_random_set = [&](int n)
  {
    auto max = _mesh.n_elements<HandleT>();
    std::vector<HandleT> set;
    set.push_back(HandleT(0));
    for (int i = 0; i < n; ++i)
      set.push_back(HandleT(rand() % max));
    std::sort(set.begin(), set.end());
    set.erase(std::unique(set.begin(), set.end()), set.end());
    return set;
  };

  // Feature
  {
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_feature(false);
    auto set = get_random_set(4);
    for (auto el : set)
      _mesh.status(el).set_feature(true);

    auto set2 = _mesh.elements<HandleT>().filtered(Feature()).to_vector();

    EXPECT_EQ(set.size(), set2.size()) << "Set sizes differ";
    for (size_t i = 0; i < std::min(set.size(), set2.size()); ++i)
      EXPECT_EQ(set[i], set2[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_feature(false);
  }

  // Selected
  {
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    auto set = get_random_set(4);
    for (auto el : set)
      _mesh.status(el).set_selected(true);

    auto set2 = _mesh.elements<HandleT>().filtered(Selected()).to_vector();

    EXPECT_EQ(set.size(), set2.size()) << "Set sizes differ";
    for (size_t i = 0; i < std::min(set.size(), set2.size()); ++i)
      EXPECT_EQ(set[i], set2[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
  }

  // Tagged
  {
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_tagged(false);
    auto set = get_random_set(4);
    for (auto el : set)
      _mesh.status(el).set_tagged(true);

    auto set2 = _mesh.elements<HandleT>().filtered(Tagged()).to_vector();

    EXPECT_EQ(set.size(), set2.size()) << "Set sizes differ";
    for (size_t i = 0; i < std::min(set.size(), set2.size()); ++i)
      EXPECT_EQ(set[i], set2[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_tagged(false);
  }

  // Tagged2
  {
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_tagged2(false);
    auto set = get_random_set(4);
    for (auto el : set)
      _mesh.status(el).set_tagged2(true);

    auto set2 = _mesh.elements<HandleT>().filtered(Tagged2()).to_vector();

    EXPECT_EQ(set.size(), set2.size()) << "Set sizes differ";
    for (size_t i = 0; i < std::min(set.size(), set2.size()); ++i)
      EXPECT_EQ(set[i], set2[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_tagged2(false);
  }

  // Locked
  {
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_locked(false);
    auto set = get_random_set(4);
    for (auto el : set)
      _mesh.status(el).set_locked(true);

    auto set2 = _mesh.elements<HandleT>().filtered(Locked()).to_vector();

    EXPECT_EQ(set.size(), set2.size()) << "Set sizes differ";
    for (size_t i = 0; i < std::min(set.size(), set2.size()); ++i)
      EXPECT_EQ(set[i], set2[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_locked(false);
  }

  // Hidden
  {
    for (auto el : _mesh.all_elements<HandleT>())
      _mesh.status(el).set_hidden(false);
    auto set = get_random_set(4);
    for (auto el : set)
      _mesh.status(el).set_hidden(true);

    auto set2 = _mesh.all_elements<HandleT>().filtered(Hidden()).to_vector();

    EXPECT_EQ(set.size(), set2.size()) << "Set sizes differ";
    for (size_t i = 0; i < std::min(set.size(), set2.size()); ++i)
      EXPECT_EQ(set[i], set2[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.all_elements<HandleT>())
      _mesh.status(el).set_hidden(false);
  }

  // Deleted
  {
    for (auto el : _mesh.all_elements<HandleT>())
      _mesh.status(el).set_deleted(false);
    auto set = get_random_set(4);
    for (auto el : set)
      _mesh.status(el).set_deleted(true);

    auto set2 = _mesh.all_elements<HandleT>().filtered(Deleted()).to_vector();

    EXPECT_EQ(set.size(), set2.size()) << "Set sizes differ";
    for (size_t i = 0; i < std::min(set.size(), set2.size()); ++i)
      EXPECT_EQ(set[i], set2[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.all_elements<HandleT>())
      _mesh.status(el).set_deleted(false);
  }

  // Custom property
  {
    OpenMesh::PropertyManager<typename OpenMesh::PropHandle<HandleT>::template type<bool>> prop(false, _mesh);
    auto set = get_random_set(4);
    for (auto el : set)
      prop[el] = true;

    auto set2 = _mesh.elements<HandleT>().filtered(prop).to_vector();

    EXPECT_EQ(set.size(), set2.size()) << "Set sizes differ";
    for (size_t i = 0; i < std::min(set.size(), set2.size()); ++i)
      EXPECT_EQ(set[i], set2[i]) << "Sets differ at position " << i;
  }

  // boundary
  {
    for (auto el : _mesh.elements<HandleT>().filtered(Boundary()))
      EXPECT_TRUE(el.is_boundary());
    int n_boundary1 = 0.0;
    for (auto el : _mesh.elements<HandleT>())
      if (el.is_boundary())
        n_boundary1 += 1;
    int n_boundary2 = _mesh.elements<HandleT>().count_if(Boundary());
    EXPECT_EQ(n_boundary1, n_boundary2);
  }
}

template <typename HandleT>
void test_range_predicate_combinations(Mesh& _mesh)
{
  using namespace OpenMesh::Predicates;

  auto n_elements = _mesh.n_elements<HandleT>();
  auto get_random_set = [&](int n)
  {
    std::vector<HandleT> set;
    for (int i = 0; i < n; ++i)
      set.push_back(HandleT(rand() % n_elements));
    std::sort(set.begin(), set.end());
    set.erase(std::unique(set.begin(), set.end()), set.end());
    return set;
  };

  // negation
  {
    auto set = get_random_set(4);
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : set)
      _mesh.status(el).set_selected(true);

    auto true_set  = _mesh.elements<HandleT>().filtered(Selected()).to_vector();
    auto false_set = _mesh.elements<HandleT>().filtered(!Selected()).to_vector();

    std::vector<HandleT> intersection;
    std::set_intersection(true_set.begin(), true_set.end(), false_set.begin(), false_set.end(), std::back_inserter(intersection));

    EXPECT_TRUE(intersection.empty());
    EXPECT_EQ(true_set.size() + false_set.size(), n_elements);

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
  }

  // conjunction
  {
    auto set1 = get_random_set(4);
    auto set2 = get_random_set(4);
    // make sure there is some overlap
    {
      auto set3 = get_random_set(3);
      set1.insert(set1.end(), set3.begin(), set3.end());
      set2.insert(set2.end(), set3.begin(), set3.end());
      std::sort(set1.begin(), set1.end());
      std::sort(set2.begin(), set2.end());
      set1.erase(std::unique(set1.begin(), set1.end()), set1.end());
      set2.erase(std::unique(set2.begin(), set2.end()), set2.end());
    }

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_tagged(false);
    for (auto el : set1)
      _mesh.status(el).set_selected(true);
    for (auto el : set2)
      _mesh.status(el).set_tagged(true);

    auto set  = _mesh.elements<HandleT>().filtered(Selected() && Tagged()).to_vector();

    std::vector<HandleT> intersection;
    std::set_intersection(set1.begin(), set1.end(), set2.begin(), set2.end(), std::back_inserter(intersection));

    EXPECT_EQ(intersection.size(), set.size());
    for (size_t i = 0; i < std::min(intersection.size(), set.size()); ++i)
      EXPECT_EQ(intersection[i], set[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_tagged(false);
  }

  // Disjunction
  {
    auto set1 = get_random_set(4);
    auto set2 = get_random_set(4);
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_tagged(false);
    for (auto el : set1)
      _mesh.status(el).set_selected(true);
    for (auto el : set2)
      _mesh.status(el).set_tagged(true);

    auto set  = _mesh.elements<HandleT>().filtered(Selected() || Tagged()).to_vector();

    std::vector<HandleT> union_set;
    std::set_union(set1.begin(), set1.end(), set2.begin(), set2.end(), std::back_inserter(union_set));

    EXPECT_EQ(union_set.size(), set.size());
    for (size_t i = 0; i < std::min(union_set.size(), set.size()); ++i)
      EXPECT_EQ(union_set[i], set[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_tagged(false);
  }

}

template <typename HandleT>
bool test_func(HandleT _h)
{
  return _h.idx() % 3 == 0;
}

template <typename HandleT>
void test_make_predicate(Mesh& _mesh)
{
  using namespace OpenMesh::Predicates;

  auto n_elements = _mesh.n_elements<HandleT>();
  auto get_random_set = [&](int n)
  {
    std::vector<HandleT> set;
    for (int i = 0; i < n; ++i)
      set.push_back(HandleT(rand() % n_elements));
    std::sort(set.begin(), set.end());
    set.erase(std::unique(set.begin(), set.end()), set.end());
    return set;
  };

  // custom property
  {
    OpenMesh::PropertyManager<typename OpenMesh::PropHandle<HandleT>::template type<bool>> prop(false, _mesh);
    auto set1 = get_random_set(4);
    auto set2 = get_random_set(4);
    for (auto el : set1)
      prop[el] = true;
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : set2)
      _mesh.status(el).set_selected(true);

    auto set  = _mesh.elements<HandleT>().filtered(Selected() || make_predicate(prop)).to_vector();

    std::vector<HandleT> union_set;
    std::set_union(set1.begin(), set1.end(), set2.begin(), set2.end(), std::back_inserter(union_set));

    EXPECT_EQ(union_set.size(), set.size());
    for (size_t i = 0; i < std::min(union_set.size(), set.size()); ++i)
      EXPECT_EQ(union_set[i], set[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_tagged(false);
  }

  // lambda
  {
    OpenMesh::PropertyManager<typename OpenMesh::PropHandle<HandleT>::template type<bool>> prop(false, _mesh);
    auto set1 = get_random_set(4);
    auto set2 = get_random_set(4);
    for (auto el : set1)
      prop[el] = true;
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : set2)
      _mesh.status(el).set_selected(true);

    auto test = [&](HandleT h) { return prop(h); };

    auto set  = _mesh.elements<HandleT>().filtered(Selected() || make_predicate(test)).to_vector();

    std::vector<HandleT> union_set;
    std::set_union(set1.begin(), set1.end(), set2.begin(), set2.end(), std::back_inserter(union_set));

    EXPECT_EQ(union_set.size(), set.size());
    for (size_t i = 0; i < std::min(union_set.size(), set.size()); ++i)
      EXPECT_EQ(union_set[i], set[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_tagged(false);
  }

  // r value lambda
  {
    OpenMesh::PropertyManager<typename OpenMesh::PropHandle<HandleT>::template type<bool>> prop(false, _mesh);
    auto set1 = get_random_set(4);
    auto set2 = get_random_set(4);
    for (auto el : set1)
      prop[el] = true;
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : set2)
      _mesh.status(el).set_selected(true);

    auto set  = _mesh.elements<HandleT>().filtered(Selected() || make_predicate([&](HandleT h) { return prop(h); })).to_vector();

    std::vector<HandleT> union_set;
    std::set_union(set1.begin(), set1.end(), set2.begin(), set2.end(), std::back_inserter(union_set));

    EXPECT_EQ(union_set.size(), set.size());
    for (size_t i = 0; i < std::min(union_set.size(), set.size()); ++i)
      EXPECT_EQ(union_set[i], set[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_tagged(false);
  }

  // function pointer
  {
    auto set1 = _mesh.elements<HandleT>().filtered([&](const HandleT& h) { return test_func(h); }).to_vector();
    auto set2 = get_random_set(4);
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : set2)
      _mesh.status(el).set_selected(true);

    auto set  = _mesh.elements<HandleT>().filtered(Selected() || make_predicate(test_func<HandleT>)).to_vector();

    std::vector<HandleT> union_set;
    std::set_union(set1.begin(), set1.end(), set2.begin(), set2.end(), std::back_inserter(union_set));

    EXPECT_EQ(union_set.size(), set.size());
    for (size_t i = 0; i < std::min(union_set.size(), set.size()); ++i)
      EXPECT_EQ(union_set[i], set[i]) << "Sets differ at position " << i;

    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_selected(false);
    for (auto el : _mesh.elements<HandleT>())
      _mesh.status(el).set_tagged(false);
  }
}

TEST_F(OpenMeshSmartRanges, Predicate)
{
  using namespace OpenMesh;

  mesh_.request_vertex_status();
  mesh_.request_halfedge_status();
  mesh_.request_edge_status();
  mesh_.request_face_status();

  mesh_.delete_face(FaceHandle(0)); // ensure there is a boundary
  mesh_.garbage_collection();

  test_range_predicates<VertexHandle>(mesh_);
  test_range_predicates<HalfedgeHandle>(mesh_);
  test_range_predicates<EdgeHandle>(mesh_);
  test_range_predicates<FaceHandle>(mesh_);

  test_range_predicate_combinations<VertexHandle>(mesh_);
  test_range_predicate_combinations<HalfedgeHandle>(mesh_);
  test_range_predicate_combinations<EdgeHandle>(mesh_);
  test_range_predicate_combinations<FaceHandle>(mesh_);

  test_make_predicate<VertexHandle>(mesh_);
  test_make_predicate<HalfedgeHandle>(mesh_);
  test_make_predicate<EdgeHandle>(mesh_);
  test_make_predicate<FaceHandle>(mesh_);
}

struct MemberFunctionWrapperTestStruct
{
  MemberFunctionWrapperTestStruct(int _i)
    :
      i_(_i)
  {
  }

  int get_i(const OpenMesh::SmartEdgeHandle& /*_eh*/) const
  {
    return i_;
  }

  bool id_divisible_by_2(const OpenMesh::SmartEdgeHandle& _eh) const
  {
    return _eh.idx() % 2 == 0;
  }

  int valence_times_i(const OpenMesh::SmartVertexHandle& vh)
  {
   return vh.edges().sum(OM_MFW(get_i));
  }

  int i_;
};

TEST_F(OpenMeshSmartRanges, MemberFunctionFunctor)
{
  using namespace OpenMesh::Predicates;

  EXPECT_TRUE(mesh_.n_vertices() > 0) << "Mesh has no vertices";
  EXPECT_TRUE(mesh_.n_edges() > 0) << "Mesh has no edges";

  int factor = 3;
  MemberFunctionWrapperTestStruct test_object(factor);

  // Test using a MemberFunctionWrapper as Functorstatic_cast<int>(
  EXPECT_EQ(static_cast<int>(mesh_.n_edges() / 2), mesh_.edges().count_if(make_member_function_wrapper(test_object, &MemberFunctionWrapperTestStruct::id_divisible_by_2)));


  // Test using a MemberFunctionWrapper as Functor that is created using the convenience macro from inside the struct
  for (auto vh : mesh_.vertices())
    EXPECT_EQ(test_object.valence_times_i(vh), static_cast<int>(vh.valence()) * factor);

  factor = 4;
  test_object.i_ = factor;
  for (auto vh : mesh_.vertices())
  {
    EXPECT_EQ(test_object.valence_times_i(vh), static_cast<int>(vh.valence() * factor));
  }



}

}
