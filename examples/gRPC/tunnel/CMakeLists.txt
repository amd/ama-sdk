# Copyright (C) 2023, Xilinx Inc - All rights reserved

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
set(TUN_PROJ "tap-tunnel")
set(TUN_VER  "0.0.0")
set(TUN_PROJ_FULL_NAME "${TUN_PROJ}_${TUN_VER}")

project(${TUN_PROJ_FULL_NAME})

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -DDSA64 ${CMAKE_C_FLAGS}")

# Building encoder xma app
file(GLOB SRC "./*.c" "./*.cpp")
message("Source files: ${SRC}")

add_executable(${PROJECT_NAME} ${SRC})

# Compile and link options for xma apps
set_property(TARGET ${PROJECT_NAME} 
    APPEND PROPERTY COMPILE_OPTIONS "-fmax-errors=1")

set_property(TARGET ${PROJECT_NAME} 
    APPEND PROPERTY LINK_LIBRARIES -lm)


