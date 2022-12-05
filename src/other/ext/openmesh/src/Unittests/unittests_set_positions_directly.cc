#include <gtest/gtest.h>
#include <Unittests/unittests_common.hh>
#include <iostream>

namespace {
    
class OpenMeshDirectSettingProperties : public OpenMeshBase {

    protected:

        // This function is called before each test is run
        virtual void SetUp() {
            
            // Do some initial stuff with the member data here...
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

/* Adds two triangles to a tri mesh
 */
TEST_F(OpenMeshDirectSettingProperties, SetVertexPositionsDirectly) {

  mesh_.clear();

  // Add some vertices
  Mesh::VertexHandle vhandle[4];

  vhandle[0] = mesh_.add_vertex(Mesh::Point(0, 0, 0));
  vhandle[1] = mesh_.add_vertex(Mesh::Point(0, 1, 0));
  vhandle[2] = mesh_.add_vertex(Mesh::Point(1, 1, 0));
  vhandle[3] = mesh_.add_vertex(Mesh::Point(1, 0, 0));


  auto pos_pro = mesh_.points_property_handle();

  auto point_vector = mesh_.property(pos_pro).data_vector();

  int vertex_count = 0;
  for( auto p : point_vector) {
//      std::cerr << p[0] << " " << p[1] <<  " " << p[2] << std::endl;
      ++vertex_count;
  }

  EXPECT_EQ(4u, mesh_.n_vertices() ) << "Wrong number of vertices";

  EXPECT_EQ(4, vertex_count) << "Wrong number of vertices when counting direct point property vector";


}

}
