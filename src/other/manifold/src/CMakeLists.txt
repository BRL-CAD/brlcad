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
	QuickHull.cpp
	clipper.engine.cpp
	clipper.offset.cpp
	clipper.rectclip.cpp
	)

set(MANIFOLD_SRCS
	${THIRD_PARTY_SRCS}
	boolean3.cpp
	boolean_result.cpp
	collider.cpp
	constructors.cpp
	cross_section.cpp
	csg_tree.cpp
	edge_op.cpp
	face_op.cpp
	impl.cpp
	manifold.cpp
	polygon.cpp
	properties.cpp
	sdf.cpp
	smoothing.cpp
	sort.cpp
	)
if (ASSETIMPORT_LIBRARIES)
	set(MANIFOLD_SRCS ${MANIFOLD_SRCS} meshIO.cpp)
endif (ASSETIMPORT_LIBRARIES)

add_library(manifold SHARED ${MANIFOLD_SRCS})
target_include_directories(manifold PUBLIC
  	${PROJECT_SOURCE_DIR}/include
  	${PROJECT_SOURCE_DIR}/include/manifold
  	${PROJECT_SOURCE_DIR}/glm
  	${PROJECT_SOURCE_DIR}/thrust
	${PROJECT_SOURCE_DIR}/thrust/dependencies/libcudacxx/include
	${PROJECT_SOURCE_DIR}/thrust/dependencies/cub
	)
if (ASSETIMPORT_LIBRARIES)
	target_link_libraries(manifold ${ASSETIMPORT_LIBRARIES})
endif (ASSETIMPORT_LIBRARIES)
target_compile_options(manifold PRIVATE ${MANIFOLD_FLAGS} -DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_CPP)
target_compile_features(manifold PUBLIC cxx_std_17)
set_property(TARGET manifold APPEND PROPERTY COMPILE_DEFINITIONS "MANIFOLD_DLL_EXPORTS")

install(
  TARGETS manifold
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR}
  )

