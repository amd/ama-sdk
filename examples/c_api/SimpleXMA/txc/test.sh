# !/bin/bash

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

ENCODED_FILE=$1
ENCODE_TYPE=$2
H264_PREFIX=$3
IN_WIDTH=$4
IN_HEIGHT=$5


echo "Usage $0 ENCODED_FILE  ENCODE_TYPE [1:AVC|2:HEVC] H264_PREFIX IN_WIDTH IN_HEIGHT"

./build/txc/xma-transcoder-simple_0.0.0 $ENCODED_FILE  $ENCODE_TYPE $H264_PREFIX $IN_WIDTH $IN_HEIGHT
