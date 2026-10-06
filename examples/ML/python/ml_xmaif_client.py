#!/usr/bin/python3 -Xfrozen_modules=off

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

"""Decode a NAL file, run object-detection and draw bounding boxes, then re-encode.

Three-stage pipeline (each stage runs in its own thread):
  Stage 1 – Decode:  read NAL → extract VCL → HW decode → HW download → YUV queue
  Stage 2 – ML:      YUV queue → RGB → object-detect → draw boxes → YUV → encode queue
  Stage 3 – Encode:  encode queue → HW upload → HW encode → write NAL
"""

from __future__ import print_function

import logging
import os
import sys
import multiprocessing
import queue
import threading
import time
import warnings

warnings.filterwarnings("ignore", message=".*pipelines sequentially.*")

# Must be set before torch is imported
_compile_cache = os.path.join(os.path.dirname(os.path.abspath(__file__)), "torch_compile")
os.makedirs(_compile_cache, exist_ok=True)
os.environ.setdefault("TORCHINDUCTOR_CACHE_DIR", _compile_cache)
os.environ.setdefault("MIOPEN_USER_DB_PATH", "/tmp/.miopen")
os.environ.setdefault("TRANSFORMERS_NO_ADVISORY_WARNINGS", "1")
multiprocessing.set_start_method('fork')

import torch
import grpc
import xmaif_pb2
import xmaif_pb2_grpc
from xmaif_pb2 import (NoArgs, DevId,
                        DecArg, DownloadArg, VclOut, VclIn, DownloadIn,
                        EncArg, EncIn, UploadArg, UploadIn)
from PIL import Image, ImageDraw, ImageFont
from transformers import pipeline

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
READ_CHUNK = 1 << 20

WIDTH  = 1920
HEIGHT = 1080
FPS_NUM = 24
FPS_DEN = 1

XMA_H264_ENCODER_TYPE = 1
XMA_ENC_PRESET_MEDIUM = 2

devId = DevId(**{'device_id': 0})

decArg = DecArg(**{'ptr_dev_conf': 0, 'width': WIDTH, 'height': HEIGHT,
                   'fps_num': FPS_NUM, 'fps_den': FPS_DEN,
                   'hwdecoder_type': 1, 'decode_id': 0})
decArg.dec_xma_param_conf.add(name='low_latency',      value=0)
decArg.dec_xma_param_conf.add(name='latency_logging',  value=0)
decArg.dec_xma_param_conf.add(name='out_fmt',          value=5)

dwnArg = DownloadArg(**{'ptr_dev_conf': 0, 'width': WIDTH, 'height': HEIGHT,
                        'fps_num': FPS_NUM, 'fps_den': FPS_DEN, 'bits_per_pixel': 8})

encArg = EncArg(**{'ptr_dev_conf': 0, 'enc_output_cnt': 1, 'bits_per_pixel': {8},
                   'enc_dev': {0}, 'enc_slice': {-1}, 'enc_xav1': {0},
                   'enc_ull': {0}, 'enc_codec': {XMA_H264_ENCODER_TYPE},
                   'enc_rate': {int(5E6)}, 'enc_preset': {XMA_ENC_PRESET_MEDIUM}})
encArg.enc_xrm_conf.add(width=WIDTH, height=HEIGHT, fps_num=FPS_NUM, fps_den=FPS_DEN,
                         is_la_enabled=1, enc_cores=1, preset='medium')
encArg.enc_xma_param_conf.add(name='bf',          value=3)
encArg.enc_xma_param_conf.add(name='tune_metrics', value=4)
encArg.enc_xma_param_conf.add(name='forced_idr',   value=1)

uplArg = UploadArg(**{'ptr_dev_conf': 0, 'width': WIDTH, 'height': HEIGHT,
                      'fps_num': FPS_NUM, 'fps_den': FPS_DEN, 'bits_per_pixel': 8})

noArg = NoArgs()

# ---------------------------------------------------------------------------
# ML configuration
# ---------------------------------------------------------------------------
ML_TASK            = "object-detection"
ML_MODEL           = "hustvl/yolos-tiny"
ML_THRESHOLD       = 0.8
ML_BATCH_SIZE      = 8
OPT_COMPILE        = False
OPT_INFERENCE_MODE = True
# When True: run detection on the Y (luma) plane only, replicated to RGB.
# Skips full YUV↔RGB conversion; chroma planes are passed through unchanged.
ONLY_Y             = False
# When True: skip all ML processing and pass frames directly to the encoder. Good for timing analysis.
BYPASS_ML          = False

# Pipeline queue depths: how many frames may be buffered between stages.
# Larger values absorb burst latency at the cost of memory; 4–8 is typical.
PIPELINE_QUEUE_DEPTH = 16

#Check if OpenCV is installed and if OpenCL is available for UMat (GPU) acceleration. (Used for YUV <-> RGB conversion)
try:
    import cv2
    cv2.ocl.setUseOpenCL(True)
    _CV2_UMAT = cv2.ocl.haveOpenCL() and cv2.ocl.useOpenCL()
    if _CV2_UMAT:
        _cv2_device = cv2.ocl.Device.getDefault()
        print(f"OpenCV UMat (GPU): enabled — {_cv2_device.name()} "
              f"({'GPU' if _cv2_device.type() == cv2.ocl.Device_TYPE_GPU else 'other'})")
    else:
        print(f"OpenCV UMat (GPU): disabled "
              f"(haveOpenCL={cv2.ocl.haveOpenCL()}, useOpenCL={cv2.ocl.useOpenCL()})")
except ImportError:
    cv2 = None
    _CV2_UMAT = False
    print("OpenCV UMat (GPU): disabled (cv2 not installed)")

# ---------------------------------------------------------------------------
# Decoder helpers
# ---------------------------------------------------------------------------
def _dec_write_yuv(buf, lineSize, dwnOut, width, height):
    """Write one decoded YUV420 frame into buf (bytearray), plane by plane."""
    dst = 0
    w = width
    p0 = dwnOut.ptr_out_frame_host_p0
    for h in range(height):
        buf[dst:dst + w] = p0[h * lineSize.p0: h * lineSize.p0 + w]
        dst += w

    w = width // 2
    p1 = dwnOut.ptr_out_frame_host_p1
    for h in range(height // 2):
        buf[dst:dst + w] = p1[h * lineSize.p1: h * lineSize.p1 + w]
        dst += w

    p2 = dwnOut.ptr_out_frame_host_p2
    for h in range(height // 2):
        buf[dst:dst + w] = p2[h * lineSize.p2: h * lineSize.p2 + w]
        dst += w

    return dst  # bytes written

# ---------------------------------------------------------------------------
# Encoder helpers
# ---------------------------------------------------------------------------
def _frameGeometry(lineSize, height):
    chroma_h = (height + 1) // 2
    size0 = lineSize.p0 * height
    size1 = lineSize.p1 * chroma_h
    size2 = lineSize.p2 * chroma_h
    return chroma_h, (size0, size1, size2), (0, size0, size0 + size1)

def _enc_load_yuv(frame_buf, offs, lineSize, width, height, chroma_h, src):
    """Copy one YUV420 frame from src (bytes/bytearray) into frame_buf."""
    src_off = 0
    w = width
    dst = offs[0]
    for _ in range(height):
        row = src[src_off: src_off + w]
        if len(row) < w:
            return 0
        frame_buf[dst: dst + w] = row
        src_off += w
        dst += lineSize.p0

    w = width // 2
    dst = offs[1]
    for _ in range(height // 2):
        row = src[src_off: src_off + w]
        if len(row) < w:
            return 0
        frame_buf[dst: dst + w] = row
        src_off += w
        dst += lineSize.p1

    dst = offs[2]
    for _ in range(height // 2):
        row = src[src_off: src_off + w]
        if len(row) < w:
            return 0
        frame_buf[dst: dst + w] = row
        src_off += w
        dst += lineSize.p2

    return src_off

# ---------------------------------------------------------------------------
# ML / annotation helpers
# ---------------------------------------------------------------------------
def _build_detector():
    print(f"Loading ML model: {ML_MODEL}")
    det = pipeline(
        task=ML_TASK,
        model=ML_MODEL,
        dtype=torch.float16,
        use_fast=True,
        batch_size=ML_BATCH_SIZE,
        device=0 if torch.cuda.is_available() else -1,
    )
    if OPT_COMPILE:
        det.model = torch.compile(det.model, backend="inductor", mode="reduce-overhead")
    return det

def _annotate_rgb(img_rgb, prediction_scene, font):
    """Draw bounding boxes on a PIL RGB image in-place and return it."""
    draw = ImageDraw.Draw(img_rgb)
    for pred in prediction_scene:
        box   = pred["box"]
        label = pred["label"]
        score = pred["score"]
        xmin, ymin, xmax, ymax = box["xmin"], box["ymin"], box["xmax"], box["ymax"]
        draw.rectangle((xmin, ymin, xmax, ymax), outline="red", width=3)
        draw.text((xmin, ymin - 25), f"{label}: {round(score, 2)}", fill="red", font=font)
    return img_rgb

def _yuv420_to_rgb(yuv_bytes, width, height):
    """Convert packed YUV420p bytes to a PIL RGB image.
    Uses OpenCV UMat (GPU) when available, falls back to numpy on CPU."""
    import numpy as np
    yuv_np = np.frombuffer(yuv_bytes, dtype=np.uint8).reshape(height * 3 // 2, width)
    if _CV2_UMAT:
        rgb = cv2.cvtColor(cv2.UMat(yuv_np), cv2.COLOR_YUV2RGB_I420)
        return Image.fromarray(cv2.UMat.get(rgb))
    # CPU fallback
    y_size  = width * height
    uv_size = (width // 2) * (height // 2)
    yuv     = yuv_np.ravel()
    Y = yuv[:y_size].reshape(height, width).astype(np.float32)
    U = yuv[y_size: y_size + uv_size].reshape(height // 2, width // 2).astype(np.float32) - 128
    V = yuv[y_size + uv_size:].reshape(height // 2, width // 2).astype(np.float32) - 128
    U_up = np.repeat(np.repeat(U, 2, axis=0), 2, axis=1)
    V_up = np.repeat(np.repeat(V, 2, axis=0), 2, axis=1)
    R = np.clip(Y + 1.402 * V_up,                          0, 255).astype(np.uint8)
    G = np.clip(Y - 0.344136 * U_up - 0.714136 * V_up,    0, 255).astype(np.uint8)
    B = np.clip(Y + 1.772 * U_up,                          0, 255).astype(np.uint8)
    return Image.fromarray(np.stack([R, G, B], axis=2), 'RGB')

def _rgb_to_yuv420(img_rgb, width, height):
    """Convert a PIL RGB image to packed YUV420p bytes.
    Uses OpenCV UMat (GPU) when available, falls back to numpy on CPU."""
    import numpy as np
    if _CV2_UMAT:
        rgb_np = np.array(img_rgb)
        yuv = cv2.cvtColor(cv2.UMat(rgb_np), cv2.COLOR_RGB2YUV_I420)
        return cv2.UMat.get(yuv).tobytes()
    # CPU fallback
    rgb = np.array(img_rgb).astype(np.float32)
    R, G, B = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
    Y =  0.299 * R + 0.587 * G + 0.114 * B
    U = -0.168736 * R - 0.331264 * G + 0.5 * B + 128
    V =  0.5 * R - 0.418688 * G - 0.081312 * B + 128
    Y = np.clip(Y, 0, 255).astype(np.uint8)
    U = np.clip(U, 0, 255).astype(np.uint8)
    V = np.clip(V, 0, 255).astype(np.uint8)
    return Y.tobytes() + U[0::2, 0::2].tobytes() + V[0::2, 0::2].tobytes()

def _y_to_pil(yuv_bytes, width, height):
    """Extract the Y plane and return it as a 3-channel PIL image (Y replicated to R=G=B)."""
    import numpy as np
    Y = np.frombuffer(yuv_bytes[:width * height], dtype=np.uint8).reshape(height, width)
    rgb = np.stack([Y, Y, Y], axis=2)
    return Image.fromarray(rgb, 'RGB')

def _annotate_y_to_yuv420(img_annotated, yuv_bytes, width, height):
    """Write annotated luma back into YUV420 bytes, keeping original chroma planes."""
    import numpy as np
    y_size  = width * height
    Y_new   = np.array(img_annotated)[:, :, 0]          # R=G=B so any channel works
    return Y_new.tobytes() + yuv_bytes[y_size:]          # original U and V unchanged

# ---------------------------------------------------------------------------
# Main pipeline  (three overlapped stages, each in its own thread)
# ---------------------------------------------------------------------------
def run(IF_IP, InFile, OutFile):
    noArgs = xmaif_pb2.NoArgs()

    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf", 15)
    except IOError:
        font = ImageFont.load_default()

    detector = None if BYPASS_ML else _build_detector()

    # Unbounded queues avoid deadlock: the ML stage collects ML_BATCH_SIZE frames
    # then emits ML_BATCH_SIZE frames, so bounded queues on either side can cause
    # a circular stall (decoder blocked on put while ML is blocked on put to enc).
    # PIPELINE_QUEUE_DEPTH is kept as a soft-flow reference but not enforced here.
    dec_to_ml  = queue.Queue()  # raw YUV bytes
    ml_to_enc  = queue.Queue()  # annotated YUV bytes
    errors     = queue.Queue()  # stage exceptions

    # Single semaphore covering the full frame lifetime: acquired before
    # stubDec.Proc and released after stubEnc.Proc completes.  This bounds
    # the total number of frames the server holds across its decode, download,
    # upload, and encode pipelines simultaneously.
    _pipeline_permits = threading.Semaphore(PIPELINE_QUEUE_DEPTH)

    # Per-stage timing: each thread writes its results here before exiting.
    # Keys: 'decode', 'ml', 'encode' → {'frames': int, 'total_s': float}
    stats = {}
    stats_lock = threading.Lock()

    with grpc.insecure_channel(f'{IF_IP}:50051') as devServer, \
         grpc.insecure_channel(f'{IF_IP}:50052') as decServer, \
         grpc.insecure_channel(f'{IF_IP}:50053') as dwnServer, \
         grpc.insecure_channel(f'{IF_IP}:50054') as uplServer, \
         grpc.insecure_channel(f'{IF_IP}:50055') as encServer:

        stubDev = xmaif_pb2_grpc.DeviceStub(devServer)
        stubDec = xmaif_pb2_grpc.DecoderStub(decServer)
        stubDwn = xmaif_pb2_grpc.DownloadStub(dwnServer)
        stubUpl = xmaif_pb2_grpc.UploadStub(uplServer)
        stubEnc = xmaif_pb2_grpc.EncoderStub(encServer)

        # --- init all hardware blocks ---
        devRsp = stubDev.Init(devId)

        decArg.ptr_dev_conf = devRsp.ptr_dev_conf
        stubDec.Init(decArg)

        dwnArg.ptr_dev_conf = devRsp.ptr_dev_conf
        dwnLineSize = stubDwn.Init(dwnArg)

        uplArg.ptr_dev_conf = devRsp.ptr_dev_conf
        uplLineSize = stubUpl.Init(uplArg)

        encArg.ptr_dev_conf = devRsp.ptr_dev_conf
        stubEnc.Init(encArg)

        chroma_h, sizes, offs = _frameGeometry(uplLineSize, uplArg.height)
        yuv_plain_size = WIDTH * HEIGHT + 2 * (WIDTH // 2) * (HEIGHT // 2)

        inFile  = os.open(InFile,  os.O_RDONLY)
        outFile = os.open(OutFile, os.O_RDWR | os.O_CREAT)

        # ------------------------------------------------------------------ #
        # Stage 1 – Decode thread                                             #
        # Reads the NAL bitstream, extracts VCL units, sends them to the HW   #
        # decoder, downloads each decoded frame from the device, and pushes   #
        # the raw YUV420 bytes into dec_to_ml.                                #
        # ------------------------------------------------------------------ #
        def decode_thread():
            try:
                yuv_plain = bytearray(yuv_plain_size)
                xma_data_buffer = bytearray(READ_CHUNK)
                dataproc = 0
                vcl_end  = READ_CHUNK
                vclIn    = VclIn()
                dwnIn    = DownloadIn()
                dec_frames = 0
                dec_total_s = 0.0

                def _download_and_push(dwnOut):
                    nonlocal dec_frames, dec_total_s
                    t0 = time.perf_counter()
                    _dec_write_yuv(yuv_plain, dwnLineSize, dwnOut, WIDTH, HEIGHT)
                    dec_total_s += time.perf_counter() - t0
                    dec_frames += 1
                    dec_to_ml.put(bytes(yuv_plain))

                # --- main decode loop ---
                while True:
                    t0 = time.perf_counter()
                    tail  = READ_CHUNK - vcl_end
                    xma_data_buffer[0:tail] = xma_data_buffer[vcl_end: vcl_end + tail]
                    chunk = os.read(inFile, vcl_end)
                    xma_data_buffer[tail: tail + len(chunk)] = chunk
                    if len(chunk) < vcl_end:
                        xma_data_buffer[tail + len(chunk): READ_CHUNK] = bytes(vcl_end - len(chunk))
                    dec_total_s += time.perf_counter() - t0
                    dataread = os.lseek(inFile, 0, os.SEEK_CUR)
                    if dataproc >= dataread:
                        break

                    t0 = time.perf_counter()
                    vclIn.xma_data_buffer, vclIn.dataread = bytes(xma_data_buffer), dataread
                    vclOut = stubDec.ExtractVCL(vclIn)
                    vcl_end, dataproc = vclOut.vcl_end, vclOut.dataproc
                    dec_total_s += time.perf_counter() - t0
                    if vclOut.cont:
                        continue

                    # Acquire before Proc: permit held until encode completes,
                    # bounding total frames in the server across all stages.
                    _pipeline_permits.acquire()
                    vclOut.xma_data_buffer = vclIn.xma_data_buffer
                    vclOut.flush = False
                    t0 = time.perf_counter()
                    decOut = stubDec.Proc(vclOut)
                    dec_total_s += time.perf_counter() - t0
                    if decOut.cont:
                        _pipeline_permits.release()
                        continue

                    dwnIn = decOut
                    t0 = time.perf_counter()
                    dwnOut = stubDwn.Proc(dwnIn)
                    dec_total_s += time.perf_counter() - t0
                    if dwnOut.cont:
                        _pipeline_permits.release()
                        continue

                    _download_and_push(dwnOut)

                # --- flush decoder ---
                print("Flushing decoder")
                while True:
                    _pipeline_permits.acquire()
                    vclOut.flush = True
                    t0 = time.perf_counter()
                    decOut = stubDec.Proc(vclOut)
                    dec_total_s += time.perf_counter() - t0
                    if decOut.flush:
                        _pipeline_permits.release()
                        break
                    dwnIn = decOut
                    t0 = time.perf_counter()
                    dwnOut = stubDwn.Proc(dwnIn)
                    dec_total_s += time.perf_counter() - t0
                    if dwnOut.cont:
                        _pipeline_permits.release()
                        continue
                    _download_and_push(dwnOut)

                # --- flush downloader ---
                print("Flushing downloader")
                while True:
                    dwnIn.flush = True
                    t0 = time.perf_counter()
                    dwnOut = stubDwn.Proc(dwnIn)
                    dec_total_s += time.perf_counter() - t0
                    if dwnOut.flush:
                        break
                    _download_and_push(dwnOut)

                with stats_lock:
                    stats['decode'] = {'frames': dec_frames, 'total_s': dec_total_s}

            except Exception as e:
                errors.put(('decode', e))
            finally:
                dec_to_ml.put(None)  # signal end-of-stream to ML stage

        # ------------------------------------------------------------------ #
        # Stage 2 – ML thread                                                 #
        # Pulls raw YUV frames, converts to RGB, runs the object detector,    #
        # draws bounding boxes, converts back to YUV, and pushes into         #
        # ml_to_enc.                                                           #
        # ------------------------------------------------------------------ #
        def ml_thread():
            try:
                ml_frames = 0
                ml_total_s = 0.0
                eos = False

                while not eos:
                    # Drain up to ML_BATCH_SIZE frames; stop early on sentinel.
                    batch_yuv = []
                    while len(batch_yuv) < ML_BATCH_SIZE:
                        yuv = dec_to_ml.get()
                        if yuv is None:
                            eos = True
                            break
                        batch_yuv.append(yuv)

                    if not batch_yuv:
                        break

                    t0 = time.perf_counter()

                    if BYPASS_ML:
                        for yuv in batch_yuv:
                            ml_to_enc.put(yuv)
                    else:
                        if ONLY_Y:
                            batch_pil = [_y_to_pil(y, WIDTH, HEIGHT) for y in batch_yuv]
                        else:
                            batch_pil = [_yuv420_to_rgb(y, WIDTH, HEIGHT) for y in batch_yuv]

                        if OPT_INFERENCE_MODE:
                            with torch.inference_mode():
                                batch_predictions = detector(batch_pil, threshold=ML_THRESHOLD)
                        else:
                            batch_predictions = detector(batch_pil, threshold=ML_THRESHOLD)

                        for img_pil, preds, yuv in zip(batch_pil, batch_predictions, batch_yuv):
                            img_annotated = _annotate_rgb(img_pil, preds, font)
                            if ONLY_Y:
                                ml_to_enc.put(_annotate_y_to_yuv420(img_annotated, yuv, WIDTH, HEIGHT))
                            else:
                                ml_to_enc.put(_rgb_to_yuv420(img_annotated, WIDTH, HEIGHT))

                    ml_total_s += time.perf_counter() - t0
                    ml_frames += len(batch_yuv)

                with stats_lock:
                    stats['ml'] = {'frames': ml_frames, 'total_s': ml_total_s}

            except Exception as e:
                errors.put(('ml', e))
            finally:
                ml_to_enc.put(None)  # signal end-of-stream to encode stage

        # ------------------------------------------------------------------ #
        # Stage 3 – Encode thread                                             #
        # Pulls annotated YUV frames, uploads each to the device, encodes     #
        # via the HW encoder, and writes the output NAL stream.               #
        # ------------------------------------------------------------------ #
        def encode_thread():
            try:
                enc_frame_buf = bytearray(sizes[0] + sizes[1] + sizes[2])
                _uplIn  = UploadIn(**{'flush': False})
                _encIn  = EncIn(**{'flush': False, 'ptr_out_frames': {0}})

                enc_frames = 0
                enc_total_s = 0.0

                def _enc_write_nal(outFile, encOut):
                    for xma_data_buffer in encOut.xma_data_buffers:
                        os.write(outFile, xma_data_buffer)
                    os.fsync(outFile)

                while True:
                    yuv = ml_to_enc.get()
                    if yuv is None:
                        break

                    t0 = time.perf_counter()
                    n = _enc_load_yuv(enc_frame_buf, offs, uplLineSize,
                                      WIDTH, HEIGHT, chroma_h, yuv)
                    if n == 0:
                        _pipeline_permits.release()
                        continue

                    _uplIn.flush = False
                    _uplIn.ptr_out_frame_host_p0 = bytes(enc_frame_buf)
                    uplOut = stubUpl.Proc(_uplIn)
                    if uplOut.cont:
                        _pipeline_permits.release()
                        continue

                    _encIn.flush = False
                    _encIn.ptr_out_frames[:] = [uplOut.ptr_out_frame]
                    encOut = stubEnc.Proc(_encIn)
                    enc_total_s += time.perf_counter() - t0
                    if encOut.cont:
                        _pipeline_permits.release()
                        continue
                    if encOut.xma_data_buffers:
                        _enc_write_nal(outFile, encOut)

                    enc_frames += 1
                    _pipeline_permits.release()  # frame fully processed end-to-end

                # --- flush uploader ---
                # Clear the frame pointer from the last real frame before flushing.
                _encIn.flush = False
                _encIn.ptr_out_frames[:] = [0]
                print("Flushing uploader")
                while True:
                    t0 = time.perf_counter()
                    _uplIn.flush = True
                    uplOut = stubUpl.Proc(_uplIn)
                    enc_total_s += time.perf_counter() - t0
                    if uplOut.flush:
                        break
                    t0 = time.perf_counter()
                    _encIn.ptr_out_frames[:] = [uplOut.ptr_out_frame]
                    encOut = stubEnc.Proc(_encIn)
                    enc_total_s += time.perf_counter() - t0
                    if encOut.cont:
                        continue
                    if encOut.xma_data_buffers:
                        _enc_write_nal(outFile, encOut)

                # --- flush encoder ---
                # Clear the frame pointer so the encoder sees no stale input.
                _encIn.ptr_out_frames[:] = [0]
                print("Flushing encoder")
                while True:
                    t0 = time.perf_counter()
                    _encIn.flush = True
                    encOut = stubEnc.Proc(_encIn)
                    enc_total_s += time.perf_counter() - t0
                    if encOut.flush:
                        break
                    if encOut.xma_data_buffers:
                        _enc_write_nal(outFile, encOut)

                with stats_lock:
                    stats['encode'] = {'frames': enc_frames, 'total_s': enc_total_s}

            except Exception as e:
                errors.put(('encode', e))

        # --- launch all three stages and wait ---
        # Wall-clock starts here: all HW init is done, only pipeline work remains.
        t_dec = threading.Thread(target=decode_thread, name='decode', daemon=True)
        t_ml  = threading.Thread(target=ml_thread,     name='ml',     daemon=True)
        t_enc = threading.Thread(target=encode_thread,  name='encode', daemon=True)

        wall_start = time.perf_counter()
        t_dec.start()
        t_ml.start()
        t_enc.start()

        t_dec.join()
        t_ml.join()
        t_enc.join()
        wall_total = time.perf_counter() - wall_start

        if not errors.empty():
            stage, exc = errors.get()
            raise RuntimeError(f"Pipeline failed in '{stage}' stage") from exc

        # Per-stage active work (queue-wait excluded) + pipeline wall-clock totals.
        print("\n--- Timing (active work excludes queue-wait; wall-clock = pipeline throughput) ---")
        print(f"{'Stage':<10} {'Frames':>7} {'Total (s)':>10} {'Avg (ms/frame)':>15}")
        print("-" * 46)
        stage_avg_sum = 0.0
        ref_frames = 0
        for stage in ('decode', 'ml', 'encode'):
            s = stats.get(stage, {})
            frames  = s.get('frames', 0)
            total_s = s.get('total_s', 0.0)
            avg_ms  = (total_s / frames * 1000) if frames else 0.0
            stage_avg_sum += avg_ms
            if frames:
                ref_frames = frames
            print(f"{stage:<10} {frames:>7} {total_s:>10.3f} {avg_ms:>15.2f}")
        print("-" * 46)
        print(f"{'sum':<10} {'':>7} {'':>10} {stage_avg_sum:>15.2f}  (sequential cost/frame)")
        wall_avg_ms = (wall_total / ref_frames * 1000) if ref_frames else 0.0
        print(f"{'pipeline':<10} {ref_frames:>7} {wall_total:>10.3f} {wall_avg_ms:>15.2f}  (wall-clock/frame)")
        print("-" * 46)

        stubEnc.Close(noArg)
        stubUpl.Close(noArg)
        stubDwn.Close(noArg)
        stubDec.Close(noArg)
        stubDev.Close(noArg)


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <IF_IP> <InFile.h264> <OutFile.h264>")
        sys.exit(1)

    IF_IP   = sys.argv[1]
    InFile  = sys.argv[2]
    OutFile = sys.argv[3]

    logging.basicConfig()
    run(IF_IP, InFile, OutFile)
