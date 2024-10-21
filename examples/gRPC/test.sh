#!/bin/bash

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

IF_IP="10.0.0.1"
YuvFile=$1
EncFile=$2


killall xmaif
pip install grpcio; pip install grpcio-tools pydevd
(cd ./tunnel && ./tap.sh ${IF_IP})
build/bindings/cpp_server/xmaif ${IF_IP} &
sleep 5
(killall -9 decoder_xmaif_client.py; cd bindings/python_client && time ./decoder_xmaif_client.py ${IF_IP} ${EncFile} ${YuvFile%/*}/GRPC_${YuvFile##*/})
(killall -9 encoder_xmaif_client.py; cd bindings/python_client && time ./encoder_xmaif_client.py ${IF_IP} ${YuvFile} ${EncFile%/*}/GRPC_${EncFile##*/})
killall xmaif
