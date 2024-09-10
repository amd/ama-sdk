# gRPC Example
This directory and its sub-folders contain a sample of gRPC binding, a docker build container, and a tap interface utility. The main goal in proto definitions, stub implementation, etc., has been to provide a simple to understand flow. As such, typical error checking, argument compatibility, etc., have been ignored in favor of clarity and simplicity.

# Content

1. Root folder: Contains a simple CMake file and the build script `ama-sdk/examples/gRPC/build.sh`.
1. __bindings__ folder: Contains protobuf, service definition, stub implmantation, and sample Python clients, for AVC decode and encode operations.
1. __docker__ folder: Contains build environment for gRPC.
1. __tunnel__ folder: Contains a simple tap/tunnel application.


Note that this gRPC application assumes `c_api/SimplaXMA` library, `xma-lib-simple_x.x.x`, is installed, with the relative path `../../../c_api/SimpleXMA/build/lib/`. (Update `XMAIF_LIB_DIR` in `ama-sdk/examples/gRPC/bindings/CMakeLists.txt` to change this default.)

# Build Steps

1. Begin by creating the docker develpment container, by excuting `ama-sdk/examples/gRPC/docker/build.sh` script. Modify both this script and its respective `dockerfile` as needed.
1. Proceed by submitting `ama-sdk/examples/gRPC/build.sh` script to a running container, e.g., by using `docker exec`

# Test Run

1. Map or move `build/bindings/cpp_server/xmaif ` binary to the test environment.
1. Execute `ama-sdk/examples/gRPC/test.sh` to run the test script.