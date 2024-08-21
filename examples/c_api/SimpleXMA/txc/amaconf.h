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
#define FPS_NUM 24000
#define FPS_DEN 1001
enum argPosDec {IN_FILE=1, FORMAT, OUT_FILE, WIDTH, HEIGHT};

uint8_t initInput(char *argv[], IOConf *ioConf);
void procClose(IOConf *ioConf, DevConf *devConf, DecConf *decConf, ScalerConf *scalerConf, EncConf *encConf);

int gDecDev = DEV_IDX;
uint32_t gDecXmaParamValue[] = {0,1,XMA_YUV420P_FMT_TYPE};
XmaParameter gDecXmaParamConf[] = {
        {.name="low_latency", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gDecXmaParamValue[0]},
        {.name="prop_change_callback", .type=XMA_FUNC_PTR, .length=sizeof(void*), .value=dec_res_chng_callback},
        {.name="latency_logging", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gDecXmaParamValue[1]},
        {.name="out_fmt", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gDecXmaParamValue[2]}
};

int gScalerDev = DEV_IDX;
int32_t gBitsPerPixel[] = {BITS_PER_PIXEL, BITS_PER_PIXEL, BITS_PER_PIXEL, BITS_PER_PIXEL};
XmaScalerInOutProperties gScalerOutputConf[] = {
        {.format=XMA_VPE_FMT_TYPE, .sw_format=XMA_YUV420P_FMT_TYPE, .width=1280, .height=720,  .framerate={.numerator=FPS_NUM, .denominator=FPS_DEN}, .stride=1280, .bits_per_pixel=BITS_PER_PIXEL},
        {.format=XMA_VPE_FMT_TYPE, .sw_format=XMA_YUV420P_FMT_TYPE, .width=720,  .height=480,  .framerate={.numerator=FPS_NUM, .denominator=FPS_DEN}, .stride=720 , .bits_per_pixel=BITS_PER_PIXEL},
        {.format=XMA_VPE_FMT_TYPE, .sw_format=XMA_YUV420P_FMT_TYPE, .width=480,  .height=360,  .framerate={.numerator=FPS_NUM, .denominator=FPS_DEN}, .stride=480 , .bits_per_pixel=BITS_PER_PIXEL},
        {.format=XMA_VPE_FMT_TYPE, .sw_format=XMA_YUV420P_FMT_TYPE, .width=288,  .height=160,  .framerate={.numerator=FPS_NUM, .denominator=FPS_DEN}, .stride=288 , .bits_per_pixel=BITS_PER_PIXEL}
};

uint32_t gScalerXmaParamValue[] = {1};
XmaParameter gScalerXmaParamConf[] = {
        {.name="latency_logging", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gScalerXmaParamValue[0]}
};

uint8_t gEncDev[] = {DEV_IDX,DEV_IDX,DEV_IDX,DEV_IDX};
//uint8_t gEncPreset[] = {XMA_ENC_PRESET_FAST, XMA_ENC_PRESET_MEDIUM, XMA_ENC_PRESET_MEDIUM, XMA_ENC_PRESET_FAST};
uint8_t gEncPreset[] = {XMA_ENC_PRESET_MEDIUM, XMA_ENC_PRESET_MEDIUM, XMA_ENC_PRESET_MEDIUM, XMA_ENC_PRESET_MEDIUM};
//uint8_t gEncSlice[] = {0,0,1,1};
int8_t gEncSlice[] = {-1,-1,-1,-1};
//uint8_t gEncXav1[] = {0,0,1,0};
uint8_t gEncXav1[] = {0,0,0,0};
//uint8_t gEncUll[] = {1,0,1,0};
uint8_t gEncUll[] = {0,0,0,0};
//XmaEncoderType gEncCodec[]={XMA_H264_ENCODER_TYPE, XMA_HEVC_ENCODER_TYPE, XMA_AV1_ENCODER_TYPE, XMA_AV1_ENCODER_TYPE};
XmaEncoderType gEncCodec[]={XMA_H264_ENCODER_TYPE, XMA_HEVC_ENCODER_TYPE, XMA_H264_ENCODER_TYPE, XMA_H264_ENCODER_TYPE};
uint32_t gEncXmaParamValue[] = {-1,1,-1,-1,0,1,0,1,0,-1,-1,-1,-1,-1};
uint8_t gExpertOption[2048];
int32_t gEncRates[] = {5E6, 3E6, 2E6, 1E6};
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
        {.width=1280, .height=720,  .fps_num=FPS_NUM, .fps_den=FPS_DEN, .is_la_enabled=1, .enc_cores=1, .preset="medium"},
        {.width=720,  .height=480,  .fps_num=FPS_NUM, .fps_den=FPS_DEN, .is_la_enabled=1, .enc_cores=1, .preset="medium"},
        {.width=480,  .height=360,  .fps_num=FPS_NUM, .fps_den=FPS_DEN, .is_la_enabled=1, .enc_cores=1, .preset="medium"},
        {.width=288,  .height=160,  .fps_num=FPS_NUM, .fps_den=FPS_DEN, .is_la_enabled=1, .enc_cores=1, .preset="medium"}
};


#endif /* _AMACONF_H_ */
