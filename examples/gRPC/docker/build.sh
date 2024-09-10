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

CODE_PATH=$1
MODE=$2

NAME="ama_sdk_grpc"
VER=1.0

GrpcTag="v1.62.1"
SdkVer="1.1.2"
SdkVer="1.2.0"

TAG="$NAME:$VER"
Uid=$(id $USER -u)
Gid=$(id $USER -g)


if [ "$MODE" == "b" ]; then
    DOCKER_BUILDKIT=1 docker build \
        --build-arg USER=$USER --build-arg UID=$Uid --build-arg GID=$Gid --build-arg GRPC_TAG=$GrpcTag --build-arg SDK_VER=$SdkVer \
        --tag=${TAG}\
        --rm \
        .
fi
docker system prune --force
docker ps -a | awk -v NAME=${NAME} '$0~NAME{system("docker stop "NAME" && docker wait "$1"; docker rm "$1)}'
docker run -dit --name ${NAME} \
  --volume ${CODE_PATH}:${CODE_PATH} \
  --user $Uid:$Gid ${TAG}


