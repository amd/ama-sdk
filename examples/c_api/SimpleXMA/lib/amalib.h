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

#ifndef _AMALIB_H_
#define _AMALIB_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_CH_OUTPUTS 16
#define APP_NAME "xma_simple_app"
#define APP_LOG_NAME (APP_NAME ".log")
#define ENC_NO_TRY 10
#define UPLOAD_NO_TRY 100
#define SCALER_NO_TRY 1000000
#define MA35_SLEEP 100
#define READ_CHUNK (1<<20)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IOConf {
    uint32_t in_file;
    uint32_t out_file[MAX_CH_OUTPUTS];
} IOConf;

typedef struct DevConf {
    XmaLogHandle    logger;
    XmaHandle       handle;
} DevConf;

typedef struct DecConf {
    uint8_t dev_index;
    XrmInterfaceProperties xrm_props;
    XrmDecodeContext xrm_ctx;
    XmaFrameProperties frame_props;
    XmaDecoderSession *session;
    XmaDecoderProperties xma_props;
    XmaFrame *out_frame;
    XmaDataBuffer *xma_data;
    uint8_t xma_data_buffer[READ_CHUNK];
} DecConf;

typedef struct DownloadConf {
    uint8_t dev_index;
    XmaFrameProperties frame_props;
    XmaFilterProperties xma_props;
    XmaFilterSession *session;
    XmaFrame *out_frame;
} DownloadConf;

typedef struct UploadConf {
    uint8_t dev_index;
    XmaFrameProperties frame_props;
    XmaFilterProperties xma_props;
    XmaFilterSession *session;
    XmaFrame *out_frame;
} UploadConf;


typedef struct ScalerConf {
    uint8_t dev_index;
    XrmInterfaceProperties xrm_props[MAX_CH_OUTPUTS];
    XrmScaleContext xrm_ctx;
    XmaScalerProperties xma_props;
    XmaScalerSession *session;
    XmaFrame *out_frame[MAX_CH_OUTPUTS];
    XmaFrameProperties frame_props[MAX_CH_OUTPUTS];
} ScalerConf;

typedef struct EncConf {
    uint8_t *dev_index;
    int8_t *slice;
    uint8_t *is_xav1;
    uint8_t *is_ull_enabled;
    int32_t num_outputs;
    int32_t enc_out_size[MAX_CH_OUTPUTS];
    XrmInterfaceProperties *xrm_props;
    XrmEncodeContext      xrm_ctx[MAX_CH_OUTPUTS];
    XmaEncoderSession *session[MAX_CH_OUTPUTS];
    XmaEncoderProperties xma_props[MAX_CH_OUTPUTS];
    XmaDataBuffer *xma_buffer[MAX_CH_OUTPUTS];
} EncConf;

uint8_t initCard(uint32_t device, DevConf *devConf);
uint8_t initDec(XmaHandle handle, XmaFormatType sw_format, int width, int height, int fps_num, int fps_den, XmaDecoderType hwdecoder_type, DecConf *decConf, int decDev, uint32_t decXmaParamConfCnt, XmaParameter* decXmaParamConf);
uint8_t initDownload(XmaHandle handle, XmaFormatType format, XmaFormatType sw_format, int width, int height, int32_t bits_per_pixel, int fps_num, int fps_den, DownloadConf *downloadConf);
uint8_t initUpload(XmaHandle handle, XmaFormatType format, XmaFormatType sw_format, int width, int height, int32_t bits_per_pixel, int fps_num, int fps_den, UploadConf *uploadConf);
uint8_t initScaler(XmaHandle handle, XmaFormatType sw_format, int32_t width, int32_t height, int32_t bits_per_pixel, XmaFraction framerate, XrmInterfaceProperties *in_xrm_props, ScalerConf *scalerConf, int scalerDev, uint32_t scalerXmaParamConfCnt, XmaParameter* scalerXmaParamConf, int32_t scalerOutputConfCnt, XmaScalerInOutProperties *scalerOutputConf);
uint8_t initEnc(XmaHandle handle, XmaFormatType sw_format, uint32_t encOutputCnt, int32_t *bits_per_pixel, EncConf *encConf, uint8_t *encDev, XrmInterfaceProperties *encXrmConf, int8_t *encSlice, uint8_t *encXav1, uint8_t *encUll, XmaEncoderType *encCodec, uint32_t encXmaParamConfCnt, XmaParameter* encXmaParamConf, int32_t *encRates, uint8_t *encPreset);
uint8_t extractVCL(uint8_t *xma_data_buffer, uint64_t *vcl_start, uint64_t *vcl_end, uint64_t dataread, uint64_t *dataproc, uint8_t codecType);
uint8_t procDec(DecConf *decConf, uint8_t *xma_data_buffer, uint64_t vcl_start, uint64_t vcl_end, bool eos);
uint8_t procDownload(DownloadConf *downloadConf, XmaFrame *out_frame);
uint8_t procUpload(UploadConf *uploadConf, XmaFrame *in_frame);
uint8_t procScaler(ScalerConf *scalerConf, XmaFrame* in_frame);
uint8_t procEnc(EncConf *encConf, XmaFrame *out_frame[]);
int32_t dec_res_chng_callback(XmaFrameProperties*, void*);

#ifdef __cplusplus
}
#endif

#endif /* _AMALIB_H_ */
