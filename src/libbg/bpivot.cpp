// Example: Ball Pivoting Surface Reconstruction on PLY Point Cloud
// Usage: ./bpivot bunny.ply out.obj
//
// Dependencies:
//   - Eigen
//   - nanoflann
//   - libigl-predicates
//   - ball_pivoting.hpp (from previous response)
//   - TinyPLY (header-only, https://github.com/ddiakopoulos/tinyply)
//   - Simple OBJ writer (provided inline)
//
// Compile example (assuming all headers in include/):
//   g++ -std=c++17 -Iinclude -o example_bunny_ball_pivoting example_bunny_ball_pivoting.cpp

#include <iostream>
#include <fstream>
#include <vector>
#include <Eigen/Dense>
#include "ball_pivoting.hpp"

// --- Minimal TinyPLY loader ---
#define TINYPLY_IMPLEMENTATION
#include "tinyply.h"
using namespace tinyply;

// --- Minimal OBJ Writer ---
void write_obj(const std::string &filename,
               const std::vector<Eigen::Vector3d> &vertices,
               const std::vector<Eigen::Vector3i> &triangles)
{
    std::ofstream ofs(filename);
    if (!ofs) throw std::runtime_error("Failed to open OBJ for writing");
    for (const auto &v : vertices)
        ofs << "v " << v.x() << " " << v.y() << " " << v.z() << "\n";
    for (const auto &t : triangles)
        ofs << "f " << (t[0] + 1) << " " << (t[1] + 1) << " " << (t[2] + 1) << "\n";
    ofs.close();
}

// --- Load Stanford Bunny PLY (vertices, normals if present) ---
void load_ply(const std::string &filename,
              std::vector<Eigen::Vector3d> &points,
              std::vector<Eigen::Vector3d> &normals)
{
    std::ifstream ss(filename, std::ios::binary);
    if (!ss) throw std::runtime_error("Cannot open PLY file");

    PlyFile file;
    file.parse_header(ss);

    std::shared_ptr<PlyData> verts, norms;
    verts = file.request_properties_from_element("vertex", { "x", "y", "z" });
    try { norms = file.request_properties_from_element("vertex", { "nx", "ny", "nz" }); }
    catch (...) { norms = nullptr; }
    file.read(ss);

    // Read points
    const size_t nverts = verts->count;
    points.resize(nverts);
    const float *pv = reinterpret_cast<const float *>(verts->buffer.get());
    for (size_t i = 0; i < nverts; ++i)
        points[i] = Eigen::Vector3d(pv[3*i], pv[3*i+1], pv[3*i+2]);

    // Read normals (if present), otherwise estimate dummy normals
    normals.resize(nverts);
    if (norms && norms->count == nverts)
    {
        const float *pn = reinterpret_cast<const float *>(norms->buffer.get());
        for (size_t i = 0; i < nverts; ++i)
            normals[i] = Eigen::Vector3d(pn[3*i], pn[3*i+1], pn[3*i+2]);
    }
    else
    {
        // Dummy normals (all up)
        std::cerr << "Warning: PLY file contains no normals. Using dummy normals.\n";
        for (size_t i = 0; i < nverts; ++i)
            normals[i] = Eigen::Vector3d(0, 1, 0);
    }
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " bunny.ply out.obj\n";
        return 1;
    }
    std::string in_ply = argv[1], out_obj = argv[2];

    std::vector<Eigen::Vector3d> points, normals;
    std::cout << "Loading PLY: " << in_ply << "\n";
    load_ply(in_ply, points, normals);

    std::cout << "Estimating average spacing...\n";
    double avg_spacing = ball_pivoting::BallPivoting::estimate_average_spacing(points);
    double radius = avg_spacing * 2.0; // tweak as needed
    std::cout << "Average spacing: " << avg_spacing << ", using radius: " << radius << "\n";

    std::cout << "Running ball pivoting...\n";
    ball_pivoting::BallPivoting bp(points, normals);
    // Optional: Enable debugging
    // ball_pivoting::BallPivoting::set_debug(true);

    auto mesh = bp.Run({radius});

    if(mesh.triangles.empty())
    {
        std::cerr << "[BallPivoting] No triangles generated!\n";
        std::cerr << ball_pivoting::BallPivoting::get_debug_log();
        return 2;
    }

    std::cout << "Writing OBJ: " << out_obj << "\n";
    write_obj(out_obj, mesh.vertices, mesh.triangles);
    std::cout << "Done. Mesh has " << mesh.triangles.size() << " triangles.\n";
    return 0;
}
