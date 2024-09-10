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

set(XMA_APPS_VER  "0.0.0")
set(XMAIF_LIB_DIR "../../../c_api/SimpleXMA/build/lib/")
set(XMAIF_LIB "xma-lib-simple_${XMA_APPS_VER}")
set(XMAIF_INC_DIR "../../../c_api/SimpleXMA/lib/")

set(LIB_DIR "/opt/amd/ama/ma35/lib")
set(INC_DIR "/opt/amd/ama/ma35/include")

list( APPEND CMAKE_INSTALL_RPATH "../${CMAKE_CURRENT_BINARY_DIR}/${XMAIF_LIB_DIR}")

add_subdirectory (cpp_server)
add_subdirectory (python_client)




