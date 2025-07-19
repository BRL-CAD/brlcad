// Example: Ball Pivoting Surface Reconstruction on Stanford Bunny Point Cloud
// Usage: ./bpivot bunny.ply out.obj
//
// Dependencies:
//   - nanoflann
//   - brlcad vmath.h
//   - robust_orient3d.hpp (must support vect_t/std::array<fastf_t,3>)
//   - BallPivot.hpp (see previous response)
//   - TinyPLY (header-only, https://github.com/ddiakopoulos/tinyply)
//
// Compile example (assuming all headers in include/):
//   g++ -std=c++17 -Iinclude -o bpivot bpivot.cpp

#include <iostream>
#include <fstream>
#include <vector>
#include <array>
extern "C" {
#include "vmath.h"
}
#include "BallPivot.hpp"

// --- Minimal TinyPLY loader (header-only) ---
#include "tinyply.h"
using namespace tinyply;

// --- Minimal OBJ Writer for vmath/array ---
void write_obj(const std::string &filename,
               const std::vector<std::array<fastf_t, 3>> &vertices,
               const std::vector<std::array<int, 3>> &triangles)
{
    std::ofstream ofs(filename);
    if (!ofs) throw std::runtime_error("Failed to open OBJ for writing");
    for (const auto &v : vertices)
        ofs << "v " << v[0] << " " << v[1] << " " << v[2] << "\n";
    for (const auto &t : triangles)
        ofs << "f " << (t[0] + 1) << " " << (t[1] + 1) << " " << (t[2] + 1) << "\n";
    ofs.close();
}

// --- Load Stanford Bunny PLY (vertices, normals if present) ---
void load_ply(const std::string &filename,
              std::vector<std::array<fastf_t, 3>> &points,
              std::vector<std::array<fastf_t, 3>> &normals)
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
    for (size_t i = 0; i < nverts; ++i) {
        points[i][0] = static_cast<fastf_t>(pv[3*i]);
        points[i][1] = static_cast<fastf_t>(pv[3*i+1]);
        points[i][2] = static_cast<fastf_t>(pv[3*i+2]);
    }

    // Read normals (if present), otherwise estimate dummy normals
    normals.resize(nverts);
    if (norms && norms->count == nverts)
    {
        const float *pn = reinterpret_cast<const float *>(norms->buffer.get());
        for (size_t i = 0; i < nverts; ++i) {
            normals[i][0] = static_cast<fastf_t>(pn[3*i]);
            normals[i][1] = static_cast<fastf_t>(pn[3*i+1]);
            normals[i][2] = static_cast<fastf_t>(pn[3*i+2]);
        }
    }
    else
    {
        // Dummy normals (all up)
        std::cerr << "Warning: PLY file contains no normals. Using dummy normals.\n";
        for (size_t i = 0; i < nverts; ++i) {
            normals[i][0] = 0; normals[i][1] = 1; normals[i][2] = 0;
        }
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

    std::vector<std::array<fastf_t, 3>> points, normals;
    std::cout << "Loading PLY: " << in_ply << "\n";
    load_ply(in_ply, points, normals);

    std::cout << "Estimating average spacing...\n";
    double avg_spacing = ball_pivoting::BallPivoting::estimate_average_spacing(points);
    double radius = avg_spacing * 3.1; // tweak as needed
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
