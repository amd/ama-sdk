/*
 * Copyright (C) 2024, Xilinx Inc - All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef _AMACONF_H_
#define _AMACONF_H_
#include <xma.h>
#include <xrm_interface.h>
#include <xrm_dec_interface.h>
#include <xrm_scale_interface.h>
#include <xrm_enc_interface.h>
#include <semaphore.h>
#include <xmaparam.h>
#include <xmadecoder.h>
#include <xmaencoder.h>
#include <xmascaler.h>
#include <xmabuffers.h>
#include <sys/types.h>
#include "amalib.h"

/**
 * Sample application configuration
 *
 */
#define DEV_IDX 0
#define BITS_PER_PIXEL 8

enum argPosDownload {IN_FILE=1, OUT_FILE};

uint8_t initInput(char *argv[], IOConf *ioConf);
void procClose(IOConf *ioConf, DevConf *devConf, EncConf *encConf, UploadConf *uploadConf);

int32_t gBitsPerPixel[] = {BITS_PER_PIXEL};
uint8_t gEncDev[] = {DEV_IDX};
uint8_t gEncPreset[] = {XMA_ENC_PRESET_MEDIUM};
int8_t gEncSlice[] = {-1};
uint8_t gEncXav1[] = {0};
uint8_t gEncUll[] = {0};
uint32_t gEncCodec[]={XMA_H264_ENCODER_TYPE};
uint32_t gEncXmaParamValue[] = {-1,1,-1,-1,0,1,0,1,0,-1,-1,-1,-1,-1};
uint8_t gExpertOption[2048];
int32_t gEncRates[] = {5E6};
XmaParameter gEncXmaParamConf[] ={
        {.name="slice", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[0]},
        {.name="cores", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[1]},
        {.name="spatial_aq", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[2]},
        {.name="temporal_aq", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[3]},
        {.name="latency_logging", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[4]},
        {.name="tune_metrics", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[5]},
        {.name="qp_mode", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[6]},
        {.name="forced_idr", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[7]},
        {.name="crf", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[8]},
        {.name="max_bitrate", .type=XMA_INT64, .length=sizeof(uint64_t), .value=&gEncXmaParamValue[9]},
        {.name="bf", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[10]},
        {.name="dynamic_gop", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[11]},
        {.name="latency_ms", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[12]},
        {.name="bufsize", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gEncXmaParamValue[13]},
        {.name="expert_options", .type=XMA_STRING, .length=sizeof(gExpertOption), .value=gExpertOption},

};

XrmInterfaceProperties gEncXrmConf[]={
        {.width=1920, .height=1080,  .fps_num=60, .fps_den=1, .is_la_enabled=1, .enc_cores=1, .preset="medium"}
};


#endif /* _AMACONF_H_ */
