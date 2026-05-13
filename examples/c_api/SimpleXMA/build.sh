#/bin/bash

# Copyright (C) 2023, Xilinx Inc - All rights reserved
# Xilinx Emcoder
#
# Licensed under the Apache License, Version 2.0 (the "License"). You may
# not use this file except in compliance with the License. A copy of the
# License is located at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.
#



mkdir -p build
pushd build
make clean
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS_DEBUG="-O0 -g" -DCMAKE_INSTALL_PREFIX=./build ..
make VERBOSE=1 -j
