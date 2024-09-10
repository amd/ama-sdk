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

"""The Python implementation of the gRPC decoder client."""

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
from xmaif_pb2 import NoArgs, DevId, DecArg, DownloadArg, VclOut, VclIn, DownloadIn

READ_CHUNK = 1
READ_CHUNK <<= 20

devId = DevId(**{'device_id':0})

decArg = DecArg(**{'ptr_dev_conf':0, 'width':1920, 'height':1080,
                   'fps_num':60, 'fps_den':1, 'hwdecoder_type':1, 'decode_id':0})
decArg.dec_xma_param_conf.add(name='low_latency', value=1)
decArg.dec_xma_param_conf.add(name='latency_logging', value=0)
decArg.dec_xma_param_conf.add(name='out_fmt', value=5)

dwnArg = DownloadArg(**{'ptr_dev_conf':0, 'width':1920, 'height':1080,
                   'fps_num':60, 'fps_den':1, 'bits_per_pixel':8})
noArg = NoArgs()

def dump(obj):
  for attr in dir(obj):
    print("obj.%s = %r" % (attr, getattr(obj, attr)))

def writeOut(outFile, lineSize, dwnOut, width, height):
    start = 0
    w=width

    p0_buf = dwnOut.ptr_out_frame_host_p0
    for h in range(height):
        no_wrote=0;
        while no_wrote < w:
            no_wrote += os.write(outFile, p0_buf[start+no_wrote:start+w-no_wrote])
        start += lineSize.p0

    start = 0
    w=width//2
    p1_buf = dwnOut.ptr_out_frame_host_p1
    for h in range(height//2):
        no_wrote=0;
        while no_wrote < w:
            no_wrote += os.write(outFile, p1_buf[start+no_wrote:start+w-no_wrote])
        start += lineSize.p1

    start = 0
    w=width//2
    p2_buf = dwnOut.ptr_out_frame_host_p2
    for h in range(height//2):
        no_wrote=0;
        while no_wrote < w:
            no_wrote += os.write(outFile, p2_buf[start+no_wrote:start+w-no_wrote])
        start += lineSize.p2

    os.fsync(outFile)

def run(IF_IP):
    noArgs = xmaif_pb2.NoArgs()
    with grpc.insecure_channel(f'{IF_IP}:50051') as devServer, \
         grpc.insecure_channel(f'{IF_IP}:50052') as decServer, \
         grpc.insecure_channel(f'{IF_IP}:50053') as dwnServer:

        stubDev = xmaif_pb2_grpc.DeviceStub(devServer)
        stubDec = xmaif_pb2_grpc.DecoderStub(decServer)
        stubDwn = xmaif_pb2_grpc.DownloadStub(dwnServer)

        devRsp = stubDev.Init(devId)
        decArg.ptr_dev_conf = devRsp.ptr_dev_conf
        decRsp = stubDec.Init(decArg)
        dwnArg.ptr_dev_conf = devRsp.ptr_dev_conf
        lineSize = stubDwn.Init(dwnArg)
        inFile = os.open(InFile, os.O_RDONLY)
        outFile = os.open(OutFile, os.O_RDWR | os.O_CREAT)
        vcl_end = READ_CHUNK
        xma_data_buffer = bytearray(READ_CHUNK)
        dataproc = 0
        vcl_start = 0
        vclIn = VclIn()
        dwnIn = DownloadIn()
        while True:
            tail = READ_CHUNK - vcl_end;
            xma_data_buffer[0:tail] = xma_data_buffer[vcl_end:vcl_end+tail]
            xma_data_buffer[tail:tail+vcl_end] = os.read(inFile, vcl_end)
            dataread = os.lseek(inFile, 0, os.SEEK_CUR)
            if dataproc >= dataread:
                break
            vclIn.xma_data_buffer, vclIn.dataread  = bytes(xma_data_buffer), dataread
            vclOut = stubDec.ExtractVCL(vclIn)
            vcl_end, dataproc = vclOut.vcl_end, vclOut.dataproc
            if vclOut.cont:
                continue
            vclOut.xma_data_buffer = vclIn.xma_data_buffer
            vclOut.flush = False
            decOut = stubDec.Proc(vclOut)
            if decOut.cont:
                continue
            dwnIn = decOut
            dwnOut = stubDwn.Proc(dwnIn)
            if dwnOut.cont:
                continue
            writeOut(outFile, lineSize, dwnOut, dwnArg.width, dwnArg.height)

        print("Flushing decoder")
        while True:
            vclOut.flush = True
            decOut = stubDec.Proc(vclOut)
            if decOut.flush:
                break
            dwnIn = decOut
            dwnOut = stubDwn.Proc(dwnIn)
            if dwnOut.cont:
                continue
            writeOut(outFile, lineSize, dwnOut, dwnArg.width, dwnArg.height)

        print("Flushing downloader")
        while True:
            dwnIn.flush = True
            dwnOut = stubDwn.Proc(dwnIn)
            if dwnOut.flush:
                break
            writeOut(outFile, lineSize, dwnOut, dwnArg.width, dwnArg.height)

        stubDwn.Close(noArg)
        stubDec.Close(noArg)
        stubDev.Close(noArg)

if __name__ == '__main__':
#   pydevd.settrace(sys.argv[4], port=5678, stdoutToServer=True, stderrToServer=True)
    IF_IP = sys.argv[1]
    InFile = sys.argv[2]
    OutFile = sys.argv[3]

    logging.basicConfig()
    run(IF_IP)
