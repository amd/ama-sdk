#!/usr/bin/python3

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

"""The Python implementation of the gRPC encoder client."""

from __future__ import print_function
import pydevd

import logging
import time
import os
import sys
import codecs
import grpc
import xmaif_pb2
import xmaif_pb2_grpc
from xmaif_pb2 import NoArgs, DevId, XrmInterfaceProperties, EncArg, EncIn, UploadArg, UploadIn

XMA_H264_ENCODER_TYPE = 1
XMA_ENC_PRESET_MEDIUM = 2
noArgs = NoArgs
devId = DevId(**{'device_id':0})
encArg = EncArg(**{'ptr_dev_conf':0, 'enc_output_cnt':1, 'bits_per_pixel':{8},
                   'enc_dev':{0},  'enc_slice':{-1}, 'enc_xav1':{0},
                   'enc_ull':{0}, 'enc_codec':{XMA_H264_ENCODER_TYPE},
                   'enc_rate':{int(5E6)}, 'enc_preset':{XMA_ENC_PRESET_MEDIUM}
                   })

encArg.enc_xrm_conf.add(width=1920, height=1080, fps_num=60, fps_den=1, 
                        is_la_enabled=1, enc_cores=1, preset='medium')
encArg.enc_xma_param_conf.add(name='bf', value=3)
encArg.enc_xma_param_conf.add(name='tune_metrics', value=4)
encArg.enc_xma_param_conf.add(name='forced_idr', value=1)
encIn = EncIn(**{'flush':False, 'ptr_out_frames':{0}})
uplArg = UploadArg(**{'ptr_dev_conf':0, 'width':1920, 'height':1080,
                   'fps_num':60, 'fps_den':1, 'bits_per_pixel':8})
uplIn = UploadIn(**{'flush':False})
noArg = NoArgs()

def dump(obj):
  for attr in dir(obj):
    print("obj.%s = %r" % (attr, getattr(obj, attr)))

def readIn(inFile, p0_buf, p1_buf, p2_buf, lineSize, width, height):
    start = 0
    bytesRead = 0
    w = width
    for h in range(height):
        p0_buf[start:start+w] = os.read(inFile, w)
        bytesRead += w
        start += lineSize.p0

    start = 0
    w = width//2
    for h in range(height//2):
        p1_buf[start:start+w] = os.read(inFile, w)
        bytesRead += w
        start += lineSize.p1

    start = 0
    w = width//2
    for h in range(height//2):
        p2_buf[start:start+w] = os.read(inFile, w)
        bytesRead += w
        start += lineSize.p2

    return bytesRead

def writeOut(outFile, num_outputs_xma_data_buffers):
    for xma_data_buffer in num_outputs_xma_data_buffers.xma_data_buffers:
        os.write(outFile, xma_data_buffer)

    os.fsync(outFile)


def run(IF_IP):
    noArgs = xmaif_pb2.NoArgs()
    with grpc.insecure_channel(f'{IF_IP}:50051') as devServer, \
         grpc.insecure_channel(f'{IF_IP}:50055') as encServer, \
         grpc.insecure_channel(f'{IF_IP}:50054') as uplServer:

        stubDev = xmaif_pb2_grpc.DeviceStub(devServer)
        stubEnc = xmaif_pb2_grpc.EncoderStub(encServer)
        stubUpl = xmaif_pb2_grpc.UploadStub(uplServer)

        devRsp = stubDev.Init(devId)
        uplArg.ptr_dev_conf = devRsp.ptr_dev_conf
        lineSize = stubUpl.Init(uplArg)
        encArg.ptr_dev_conf = devRsp.ptr_dev_conf
        encRsp = stubEnc.Init(encArg)

        inFile = os.open(InFile, os.O_RDONLY)
        outFile = os.open(OutFile, os.O_RDWR | os.O_CREAT)
        p0_buf = bytearray()
        p1_buf=  bytearray()
        p2_buf = bytearray()
        inFileSize = os.stat(InFile).st_size
        dataread = 0
        while True:
            dataread += readIn(inFile, p0_buf, p1_buf, p2_buf, lineSize, uplArg.width, uplArg.height)
            
            if dataread  >= inFileSize:
               break

            uplIn.ptr_out_frame_host_p0 = bytes(p0_buf)
            uplIn.ptr_out_frame_host_p1 = bytes(p1_buf)
            uplIn.ptr_out_frame_host_p2 = bytes(p2_buf)

            uplOut = stubUpl.Proc(uplIn)
            if uplOut.cont:
                continue
            encIn.ptr_out_frames[:] = [uplOut.ptr_out_frame]
            encOut = stubEnc.Proc(encIn)
            if encOut.cont:
                continue
            if encOut.xma_data_buffers:
                writeOut(outFile, encOut)

        print("Flushing uploader")
        while True:
            uplIn.flush = True
            uplOut = stubUpl.Proc(uplIn)
            if uplOut.flush:
                break
            encIn.ptr_out_frames[:] = [uplOut.ptr_out_frame]
            encOut = stubEnc.Proc(encIn)
            if encOut.cont:
                continue
            if encOut.xma_data_buffers:
                writeOut(outFile, encOut)
    
        print("Flushing encoder")
        while True:
            encIn.flush = True
            encOut = stubEnc.Proc(encIn)
            if encOut.flush:
                break
            if encOut.xma_data_buffers:
                writeOut(outFile, encOut)

        stubEnc.Close(noArg)
        stubUpl.Close(noArg)
        stubDev.Close(noArg)

if __name__ == '__main__':
#   pydevd.settrace(sys.argv[4], port=5678, stdoutToServer=True, stderrToServer=True)
    IF_IP = sys.argv[1]
    InFile = sys.argv[2]
    OutFile = sys.argv[3]

    logging.basicConfig()
    run(IF_IP)
