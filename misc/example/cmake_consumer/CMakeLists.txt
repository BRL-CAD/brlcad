# Minimal example of consuming BRL-CAD via find_package() Config mode.
#
# Usage (against an installed BRL-CAD):
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/brlcad/rel-7.43.0
#   cmake --build build
#
# Usage (against an uninstalled build tree):
#
#   cmake -S . -B build \
#     -DBRLCAD_DIR=/path/to/brlcad_build \
#     -DCMAKE_PREFIX_PATH=/path/to/brlcad_build
#   cmake --build build
#
cmake_minimum_required(VERSION 3.22)
project(brlcad_consumer_example C)

# --- Locate BRL-CAD -------------------------------------------------------
#
# find_package locates BRLCADConfig.cmake from the install (or build)
# tree.  The COMPONENTS list causes CMake to fail immediately if any
# requested component is missing, giving a clear diagnostic.
#
find_package(BRLCAD CONFIG REQUIRED COMPONENTS bu rt wdb)

# --- Simple tool that calls a BRL-CAD API ---------------------------------
add_executable(brlcad_hello main.c)

# Link against the namespaced imported targets.  All include paths,
# compile definitions, and transitive dependencies are handled
# automatically via INTERFACE_* properties on the imported targets.
target_link_libraries(brlcad_hello
  PRIVATE
    BRLCAD::bu
    BRLCAD::rt
    BRLCAD::wdb
)

# (Optional) also demonstrate the aggregate convenience target:
#   target_link_libraries(brlcad_hello PRIVATE BRLCAD::brlcad)
