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
#define PIXEL_FORMAT XMA_YUV420P_FMT_TYPE

enum argPosDownload {IN_FILE=1, FORMAT, OUT_FILE, WIDTH, HEIGHT, FPS_NUM, FPS_DEN};

uint8_t initInput(char *argv[], IOConf *ioConf);
void procClose(IOConf *ioConf, DevConf *devConf, DecConf *decConf, DownloadConf *downloadConf);

int gDecDev = DEV_IDX;
uint32_t gDecXmaParamValue[] = {0,1,XMA_YUV420P_FMT_TYPE};
XmaParameter gDecXmaParamConf[] = {
        {.name="low_latency", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gDecXmaParamValue[0]},
        {.name="prop_change_callback", .type=XMA_FUNC_PTR, .length=sizeof(void*), .value=dec_res_chng_callback},
        {.name="latency_logging", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gDecXmaParamValue[1]},
        {.name="out_fmt", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&gDecXmaParamValue[2]}
};

#endif /* _AMACONF_H_ */
