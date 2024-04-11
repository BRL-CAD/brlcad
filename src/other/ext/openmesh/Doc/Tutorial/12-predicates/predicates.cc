
#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/DefaultTriMesh.hh>
#include <OpenMesh/Core/Utils/PropertyManager.hh>
#include <OpenMesh/Core/Utils/Predicates.hh>

#include <iostream>
#include <vector>

using MyMesh = OpenMesh::TriMesh;

bool is_divisible_by_3(OpenMesh::FaceHandle vh) { return vh.idx() % 3 == 0; }

int main(int argc, char** argv)
{
  using namespace OpenMesh::Predicates; // for easier access to predicates

  // Read command line options
  MyMesh mesh;
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " infile" << std::endl;
    return 1;
  }
  const std::string infile = argv[1];
  
  // Read mesh file
  if (!OpenMesh::IO::read_mesh(mesh, infile)) {
    std::cerr << "Error: Cannot read mesh from " << infile << std::endl;
    return 1;
  }

  // Count boundary vertices
  std::cout << "Mesh contains " << mesh.vertices().count_if(Boundary()) << " boundary vertices";

  // Selected inner vertices
  std::cout << "These are the selected inner vertices: " << std::endl;
  for (auto vh : mesh.vertices().filtered(!Boundary() && Selected()))
    std::cout << vh.idx() << ", ";
  std::cout << std::endl;

  // Faces whose id is divisible by 3
  auto vec = mesh.faces().filtered(is_divisible_by_3).to_vector();
  std::cout << "There are " << vec.size() << " faces whose id is divisible by 3" << std::endl;

  // Faces which are tagged or whose id is not divisible by 3
  auto vec2 = mesh.faces().filtered(Tagged() || !make_predicate(is_divisible_by_3)).to_vector();
  std::cout << "There are " << vec2.size() << " faces which are tagged or whose id is not divisible by 3" << std::endl;

  // Edges that are longer than 10 or shorter than 2
  OpenMesh::EProp<bool> longer_than_10(mesh);
  for (auto eh : mesh.edges())
    longer_than_10[eh] = mesh.calc_edge_length(eh) > 10;
  std::cout << "There are " <<
    mesh.edges().count_if(make_predicate(longer_than_10) || make_predicate([&](OpenMesh::EdgeHandle eh) { return mesh.calc_edge_length(eh) < 2; })) <<
    " edges which are shorter than 2 or longer than 10" << std::endl;

}

