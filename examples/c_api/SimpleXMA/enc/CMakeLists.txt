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
set(XMA_APPS_PROJ "xma-encoder-simple")
set(XMA_APPS_VER  "0.0.0")
set(LIB_DIR "/opt/amd/ama/ma35/lib")
set(INC_DIR "/opt/amd/ama/ma35/include")
set(XMA_APPS_PROJ_FULL_NAME "${XMA_APPS_PROJ}_${XMA_APPS_VER}")

project(${XMA_APPS_PROJ_FULL_NAME})

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -DDSA64 ${CMAKE_C_FLAGS}")

# Building encoder xma app
file(GLOB TXC_SRC "./*.c" "./*.cpp" "../lib/*.c")
message("Source files: ${TXC_SRC}")

add_executable(${PROJECT_NAME} ${TXC_SRC})

target_link_libraries (${PROJECT_NAME}
        PUBLIC "${LIB_DIR}/libxma.so"
        PUBLIC "${LIB_DIR}/libxrm_interface.so")

target_include_directories (${PROJECT_NAME} 
        PUBLIC "${INC_DIR}/xma/"
        PUBLIC "${INC_DIR}/xrm/" 
        PUBLIC "${INC_DIR}/xrm_interface/")

target_include_directories (${PROJECT_NAME} 
        PRIVATE "../lib")
# Compile and link options for xma apps
set_property(TARGET ${PROJECT_NAME} 
    APPEND PROPERTY COMPILE_OPTIONS "-fmax-errors=1")

set_property(TARGET ${PROJECT_NAME} 
    APPEND PROPERTY LINK_LIBRARIES -lm)


