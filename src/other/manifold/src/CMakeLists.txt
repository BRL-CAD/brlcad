# Copyright 2022 The Manifold Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(THIRD_PARTY_SRCS
  third_party/quickhull/QuickHull.cpp
  third_party/clipper2/src/clipper.engine.cpp
  third_party/clipper2/src/clipper.offset.cpp
  third_party/clipper2/src/clipper.rectclip.cpp
  )

set(MANIFOLD_SRCS
  ${THIRD_PARTY_SRCS}
  collider/src/collider.cpp
  cross_section/src/cross_section.cpp
  manifold/src/boolean3.cpp
  manifold/src/boolean_result.cpp
  manifold/src/constructors.cpp
  manifold/src/csg_tree.cpp
  manifold/src/edge_op.cpp
  manifold/src/face_op.cpp
  manifold/src/impl.cpp
  manifold/src/manifold.cpp
  manifold/src/properties.cpp
  manifold/src/smoothing.cpp
  manifold/src/sort.cpp
  polygon/src/polygon.cpp
  sdf/src/sdf.cpp
  )

if (ASSETIMPORT_LIBRARY)
  set(MANIFOLD_SRCS
    ${MANIFOLD_SRCS}
    ../meshIO/src/meshIO.cpp
    )
endif (ASSETIMPORT_LIBRARY)

add_library(manifold SHARED ${MANIFOLD_SRCS})
target_include_directories(manifold PUBLIC
  ${PROJECT_SOURCE_DIR}/include/manifold
  ${PROJECT_SOURCE_DIR}/meshIO/include/manifold
  ${PROJECT_SOURCE_DIR}/src/collider/include
  ${PROJECT_SOURCE_DIR}/src/manifold/include
  ${PROJECT_SOURCE_DIR}/src/cross_section/include
  ${PROJECT_SOURCE_DIR}/src/polygon/include
  ${PROJECT_SOURCE_DIR}/src/sdf/include
  ${PROJECT_SOURCE_DIR}/src/utilities/include
  ${PROJECT_SOURCE_DIR}/src/third_party/clipper2/include
  ${PROJECT_SOURCE_DIR}/src/third_party/quickhull
  ${PROJECT_SOURCE_DIR}/src/third_party/glm
  ${PROJECT_SOURCE_DIR}/src/third_party/thrust
  ${PROJECT_SOURCE_DIR}/src/third_party/thrust/dependencies/libcudacxx/include
  ${PROJECT_SOURCE_DIR}/src/third_party/thrust/dependencies/cub
  )
target_compile_options(manifold PRIVATE ${MANIFOLD_FLAGS} -DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_CPP)
target_compile_features(manifold PUBLIC cxx_std_17)

install(
  TARGETS manifold
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR}
  )


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

