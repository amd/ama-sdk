# Copyright 2024, Advanced Micro Device, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License"); 
# you may not use this file except in compliance with the License. 
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software 
# distributed under the License is distributed on an "AS IS" BASIS, 
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
# See the License for the specific language governing permissions and 
# limitations under the License.
#

project(xmaif C CXX)

include(../common.cmake)
# Proto file
get_filename_component(xmaif_proto "../protos/xmaif.proto" ABSOLUTE)
get_filename_component(xmaif_proto_path "${xmaif_proto}" PATH)

# Generated sources
set(xmaif_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/xmaif.pb.cc")
set(xmaif_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/xmaif.pb.h")
set(xmaif_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/xmaif.grpc.pb.cc")
set(xmaif_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/xmaif.grpc.pb.h")
add_custom_command(
      OUTPUT "${xmaif_proto_srcs}" "${xmaif_proto_hdrs}" "${xmaif_grpc_srcs}" "${xmaif_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${xmaif_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${xmaif_proto}"
      DEPENDS "${xmaif_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# xmaif_grpc_proto
add_library(xmaif_grpc_proto
  ${xmaif_grpc_srcs}
  ${xmaif_grpc_hdrs}
  ${xmaif_proto_srcs}
  ${xmaif_proto_hdrs})
target_link_libraries(xmaif_grpc_proto
  absl::check
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# Building encoder xma app
file(GLOB SERVER_SRC "./*.cpp")
message("Source files: ${SERVER_SRC}")

add_executable(${PROJECT_NAME} ${SERVER_SRC})

target_link_libraries(${PROJECT_NAME}
    xmaif_grpc_proto
    absl::check
    absl::flags
    absl::flags_parse
    absl::log
    ${_REFLECTION}
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    "-L../${XMAIF_LIB_DIR}" "${XMAIF_LIB}"
    "-L/opt/amd/ama/ma35/lib/" "xma"
    "-L/opt/amd/ama/ma35/lib/" "xrm"
    "-L/opt/amd/ama/ma35/lib/" "xrm_interface"
    "-Wl,-rpath ${CMAKE_CURRENT_BINARY_DIR}/../${XMAIF_LIB_DIR} -Wl,-rpath /opt/amd/ama/ma35/lib"
    )

target_include_directories(${PROJECT_NAME}
    PUBLIC "${INC_DIR}/xma/"
    PUBLIC "${INC_DIR}/xrm/" 
    PUBLIC "${INC_DIR}/xrm_interface/"
    PUBLIC "${XMAIF_INC_DIR}"
    )

# Compile and link options for xma apps
set_property(TARGET ${PROJECT_NAME} 
    APPEND PROPERTY COMPILE_OPTIONS "-fmax-errors=1")

set_property(TARGET ${PROJECT_NAME} 
    APPEND PROPERTY LINK_LIBRARIES -lm)


