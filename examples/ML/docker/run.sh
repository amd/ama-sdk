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
NAME="rocm-ama-sdk-grpc"
Storage="/storage_chassis"

docker ps -a | awk -v NAME=${NAME} '$0~NAME{system("docker stop "NAME" && docker wait "$1"; docker rm "$1)}'

docker run -u $(id -u):$(id -g) --net=host  \
 --privileged \
 -e MIOPEN_FIND_MODE=1 \
 -e MIOPEN_USER_DB_PATH=/tmp/.miopen \
 -e MIOPEN_SYSTEM_DB_PATH=/tmp/.miopen \
 -e LD_LIBRARY_PATH=/opt/rocm-7.2.0/lib:${LD_LIBRARY_PATH} \
 --ulimit core=-1 \
 --device=/dev/kfd  --device=/dev/dri \
 -e GIDLIST=$(getent group video | cut -d: -f3),$(getent group render | cut -d: -f4) \
 --cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined \
 --ipc=host --shm-size 16G \
 --group-add $(getent group video | cut -d: -f3) \
 --group-add $(getent group render | cut -d: -f3) \
 --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
 -it --name ${NAME}  --detach --rm \
 -v /etc/passwd:/etc/passwd -v /etc/group:/etc/group -v /etc/shadow:/etc/shadow -v /etc/gshadow:/etc/gshadow -v /dev/fuse:/dev/fuse  \
 -v ${Storage}:/storage \
 -v ${HOME}:${HOME}  -w ${HOME}  --entrypoint bash \
 -v /opt/rocm-7.2.0:/opt/rocm-7.2.0:ro \
 -v /etc/OpenCL:/etc/OpenCL:ro \
 ${NAME}
