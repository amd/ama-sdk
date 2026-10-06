#!/bin/bash
set -x -e
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

SDK_VER="1.5.0"
SDK_ZIP="https://www.amd.com/content/dam/amd/en/documents/products/adaptive-socs-and-fpgas/guest-resources/2026/sdk/ma35d_ama_sdk_1.5.0.zip"

GRPC_TAG="v1.78.0"
XMAIF_PY_PATH="../../gRPC/bindings/python_client"
PIP_PACKAGES="grpcio==${GRPC_TAG} grpcio-tools==${GRPC_TAG} pydevd"
GRPC_FILES="xmaif_pb2.py xmaif_pb2_grpc.py setup.py"
TAG="rocm-ama-sdk-grpc"

cp ${XMAIF_PY_PATH}/xmaif_pb2_grpc.py ${XMAIF_PY_PATH}/xmaif_pb2.py .
zip gRPC_files.zip ${GRPC_FILES}

USRID=$(id $USER -u)
GRPID=$(id $USER -g)

DOCKER_BUILDKIT=1 docker build \
    --build-arg SDK_VER=$SDK_VER --build-arg USER=$USER --build-arg UID=$USRID --build-arg GID=$GRPID \
    --build-arg GRPC_TAG=$GRPC_TAG --build-arg SDK_ZIP=$SDK_ZIP --build-arg PIP_PACKAGES="$PIP_PACKAGES" --build-arg GRPC_FILES=gRPC_files.zip \
    --tag=${TAG}\
    --rm \
    .

docker system prune --force

