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

project(Python-Client)

set ( PROTO_PY ${CMAKE_CURRENT_LIST_DIR}/xmaif_pb2.py ${CMAKE_CURRENT_LIST_DIR}/xmaif_pb2_grpc.py )
message("Source files: ${PROTO_PY}   ")
# compile dpe.proto
add_custom_command (
    OUTPUT ${PROTO_PY}
    COMMAND python3 -m grpc_tools.protoc -I${CMAKE_CURRENT_LIST_DIR}/../protos --pyi_out=${CMAKE_CURRENT_LIST_DIR} --grpc_python_out=${CMAKE_CURRENT_LIST_DIR} --python_out=${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../protos/xmaif.proto
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../protos/xmaif.proto"
    VERBATIM
)

add_library(proto_py INTERFACE ${PROTO_PY})

