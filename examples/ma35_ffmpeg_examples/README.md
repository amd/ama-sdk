# AMD AMA FFmpeg Hardware Acceleration Examples

This repository contains reference examples demonstrating how to use AMD AMA hardware acceleration with FFmpeg for video processing tasks.

## Overview

These examples showcase different aspects of AMD AMA hardware acceleration:

1. **ama\_hwdownload\_ama** - Hardware to software frame transfer with optional scaling
2. **ama\_hwframe\_transfer\_data** - Direct hwdownload_ama filter usage with linear format output
3. **ama\_long\_term** - Long-term looped playback with hardware scaling

## Prerequisites

- AMD AMA hardware and SDK installed
- A test H.264 video file

## Building the Examples

### Using System FFmpeg (Default)
```bash
make clean
make all
```

## Get help
make help
```
```


## Example Programs

### 1. ama_hwdownload_ama

Demonstrates hardware-accelerated decoding with frame transfer to software memory, optionally with scaling.

**Features:**

- Hardware H.264 decoding using h264_ama
- Frame transfer from VPE (hardware) format to software (YUV420P/NV12)
- Optional hardware scaling before download
- Configurable output format

**Usage:**
```bash
./ama_hwdownload_ama [options] input_file

Options:
  -d <level>  Set debug level (quiet, panic, fatal, error, warning, info, verbose, debug, trace)
  -o <format> Output pixel format (yuv420p, nv12) [default: nv12]
  -s <size>   Scale output size WxH (e.g., 1280x720)
  -D <device> Device name [default: /dev/ama_transcoder0]
  -h          Show help message

Examples:
  # Basic usage
  ./ama_hwdownload_ama input.mp4
  
  # Scale to 720p and transfer to software
  ./ama_hwdownload_ama -s 1280x720 input.mp4
  
  # Output as NV12 format with debug logging
  ./ama_hwdownload_ama -d debug -o nv12 input.mp4
```

### 2. ama_hwframe_transfer_data

Demonstrates using the hwdownload_ama filter to transfer decoded frames from hardware to software memory.

**Features:**

- Hardware H.264 decoding with linear format output (avoids tiled format issues)
- Direct hwdownload_ama filter usage
- Proper hardware frames context management

**Usage:**
```bash
./ama_hwframe_transfer_data [options] input_file

Options:
  -d <level>  Set debug level (quiet, panic, fatal, error, warning, info, verbose, debug, trace)
  -o <format> Output pixel format (nv12) [default: nv12]
  -D <device> Device name [default: /dev/ama_transcoder0]
  -h          Show help message

Examples:
  # Basic usage
  ./ama_hwframe_transfer_data input.mp4
  
  # Output as NV12 with info logging
  ./ama_hwframe_transfer_data -d info -o nv12 input.mp4
```

### 3. ama_long_term

Demonstrates continuous looped video playback with hardware scaling, suitable for long-term testing and stress testing.

**Features:**

- Hardware H.264 decoding
- Hardware video scaling using scaler_ama filter
- Automatic looping with timestamp adjustment
- Configurable loop count (including infinite loops)
- Graceful shutdown on Ctrl+C

**Usage:**
```bash
./ama_long_term [options] input_file

Options:
  -d <level>  Set debug level (quiet, panic, fatal, error, warning, info, verbose, debug, trace)
  -s <size>   Scale output size WxH (e.g., 1280x720) [default: 1280x720]
  -l <count>  Number of loops (0 = infinite) [default: 0]
  -D <device> Device name [default: /dev/ama_transcoder0]
  -h          Show help message

Examples:
  # Infinite loop with scaling to 1080p
  ./ama_long_term -s 1920x1080 input.mp4
  
  # Play 5 loops with debug output
  ./ama_long_term -d debug -l 5 input.mp4
  
  # Scale to 4K with info logging
  ./ama_long_term -d info -s 3840x2160 input.mp4
```

Press Ctrl+C to stop infinite loop playback.

## Key Concepts

### Hardware Device Context
All examples initialize the AMD AMA hardware device using:
```c
av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_AMA, device, opts, 0);
```

### VPE Pixel Format
The examples use `AV_PIX_FMT_VPE` (value 228) which is the AMD hardware pixel format.

### Linear vs Tiled Format
The h264_ama decoder can output in either tiled or linear format. To avoid issues with hwdownload_ama, specify linear format:
```c
av_dict_set(&opts, "out_fmt", "nv12", 0);
```

### Hardware Frames Context
Proper management of hardware frames context is critical for filter graph operations:
```c
par->hw_frames_ctx = av_buffer_ref(dec_ctx->hw_frames_ctx);
av_buffersrc_parameters_set(buffersrc_ctx, par);
```

### VPI Frame Management
VPE frames require special handling for unreferencing:
```c
if (frame->format == AV_PIX_FMT_VPE) {
    vpi_frame_unref((VpiFrm *)frame->data[0]);
}
av_frame_unref(frame);
```

## Filter Graphs

### Hardware Download
```
VPE frames → hwdownload_ama → Software frames (NV12/YUV420P)
```

### Hardware Scaling + Download
```
VPE frames → scaler_ama → hwdownload_ama → Software frames
```

### Hardware to Hardware Scaling
```
VPE frames → scaler_ama → VPE frames (scaled)
```

## Troubleshooting

### Common Issues

1. **"h264_ama decoder not found"**
   - Ensure FFmpeg is built with AMA support
   - Check that AMA drivers are installed

2. **"Failed to create AMA device"**
   - Verify device exists: `ls -la /dev/ama_transcoder*`
   - Check user permissions for device access
   - Ensure AMA drivers are loaded: `lsmod | grep ama`

3. **"tile and/or compressed format do not support download"**
   - Add `av_dict_set(&opts, "out_fmt", "nv12", 0)` to decoder options
   - This forces linear format output from the decoder

4. **Build errors**
   - Run `make show-config` to verify paths
   - Ensure FFmpeg and VPI headers are accessible
   - Check pkg-config files exist in the specified paths

### Debug Tips

- Use `-d debug` or `-d trace` for detailed logging
- Check `dmesg` for kernel driver messages
- Monitor GPU usage with AMD tools
- Use `ffmpeg -hwaccels` to verify AMA support

## Performance Considerations

- Hardware decoding is most efficient for high-resolution content
- Keeping frames in hardware memory (VPE format) avoids transfer overhead
- Use hardware scaling (scaler_ama) when possible
- Batch processing reduces context switching overhead

## Additional Resources

- [AMD AMA SDK Documentation](https://amd.github.io/ama-sdk/latest/index.html)
- [FFmpeg Hardware Acceleration](https://trac.ffmpeg.org/wiki/HWAccelIntro)
- [FFmpeg Filtering Guide](https://ffmpeg.org/ffmpeg-filters.html)

## License

These examples are provided as reference implementations for educational purposes.
