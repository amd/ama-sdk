# Copyright (C) 2024, Xilinx Inc - All rights reserved

# Licensed under the Apache License, Version 2.0 (the "License"). You may
# not use this file except in compliance with the License. A copy of the
# License is located at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.

cmake_minimum_required(VERSION 3.16)

include (CMakeDependentOption)
set(GRPC_APPS_PROJ "simple_grpc_ma35_apps")
set(GRPC_APPS_VER  "0.0.0")
set(GRPC_APPS_PROJ_FULL_NAME "${GRPC_APPS_PROJ}_${GRPC_APPS_VER}")

project(${GRPC_APPS_PROJ_FULL_NAME})

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -DDSA64 ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_STANDARD 17)

add_subdirectory (tunnel)
add_subdirectory (bindings)



