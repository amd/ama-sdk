#!/bin/bash
set -x
# Copyright 2026, Advanced Micro Device, Inc.
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

IF_IP="127.0.0.1"

GRPC_TAG="v1.78.0"

DecFile="<Input Video File Path (.h264)>"
EncFile="Output Labelled File Path (.h264)"

export HF_HOME=<Path to Model Cache Directory>
export HF_TOKEN=<Hugging Face Token>

killall xmaif

../../gRPC/build/bindings/cpp_server/xmaif ${IF_IP} &
sleep 5
source /opt/venv_gRPC_${GRPC_TAG}/bin/activate
cd python

killall -9 ml_xmaif_client.py; time /opt/venv_gRPC_${GRPC_TAG}/bin/python3 -Xfrozen_modules=off ./ml_xmaif_client.py ${IF_IP} ${DecFile} ${EncFile}
deactivate

