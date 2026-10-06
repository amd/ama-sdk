# ML Integration Example
This directory and its sub-folders contain an example of object detection workflow with a ROCm compatible GPU and MA35D, where the former is responsible for object detection, using hustvl/yolos-tiny model, and the latter is used for decoding and encoding the input and output videos. This sample program assumes:

1. The accompanying c_api and gRPC directories have been built properly.
1. The host has proper drivers and libraries installed as per https://rocm.docs.amd.com/en/docs-7.14.0
1. OpenCL is installed:
    1. `sudo amdgpu-install --usecase=opencl --no-dkms`
    1. `echo "/opt/rocm-7.2.0/lib/libamdocl64.so" | sudo tee /etc/OpenCL/vendors/amdocl64.icd`
 
## Data flow

    Input H264 NAL Video -> XMA Decoder -> GPU Object Detection -> XMA Encoder -> Output H264 NAL Video

# Content

1. __docker__ folder: Contains build environment for the demo.
1. __python__ folder: Contains the demo program `ml_xmaif_client.py` and the test script `test.sh`.

# Docker Build and Run Steps

1. Begin by creating the docker runtime container, by executing the `ama-sdk/examples/ML/docker/build.sh` script. Modify both this script and its respective `dockerfile` as needed.
1. Run the generated container with the `ama-sdk/examples/ML/docker/run.sh` script. Modify this script as needed.

# Test Run

1. Execute `ama-sdk/examples/ML/python/test.sh` to run the test script. Modify this script as needed.

With an R9700 GPU, the following result is obtained, on a 600-frame 1080p input video:

    ...
    --- Timing (active work excludes queue-wait; wall-clock = pipeline throughput) ---
    Stage       Frames  Total (s)  Avg (ms/frame)
    ----------------------------------------------
    decode         600      5.870            9.78
    ml             600     15.356           25.59
    encode         600      3.993            6.66
    ----------------------------------------------
    sum                                     42.03  (sequential cost/frame)
    pipeline       600     15.628           26.05  (wall-clock/frame)
    ----------------------------------------------
