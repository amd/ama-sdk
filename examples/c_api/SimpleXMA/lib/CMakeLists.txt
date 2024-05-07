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
set(XMA_LIB_PROJ "xma-lib-simple")
set(XMA_LIB_VER  "0.0.0")

set(XMA_LIB_PROJ_FULL_NAME "${XMA_LIB_PROJ}_${XMA_LIB_VER}")
set(CMAKE_CXX_STANDARD 14)
project(${XMA_LIB_PROJ_FULL_NAME})


set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -DDSA64 ${CMAKE_C_FLAGS}")

# Building encoder xma app
file(GLOB TXC_SRC "./*.c")
message("Source files: ${TXC_SRC}")

add_library(${PROJECT_NAME} SHARED ${TXC_SRC})

target_link_libraries (${PROJECT_NAME}
        PUBLIC "${LIB_DIR}/libxma.so"
        PUBLIC "${LIB_DIR}/libxrm_interface.so")

target_include_directories (${PROJECT_NAME}
        PUBLIC "${INC_DIR}/xma/"
        PUBLIC "${INC_DIR}/xrm/"
        PUBLIC "${INC_DIR}/xrm_interface/")

# Compile and link options for xma apps
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${XMA_LIB_VER})
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER ./amalib.h)

set_property(TARGET ${PROJECT_NAME}
    APPEND PROPERTY COMPILE_OPTIONS "-fmax-errors=1")

set_property(TARGET ${PROJECT_NAME}
    APPEND PROPERTY LINK_LIBRARIES -lm)
