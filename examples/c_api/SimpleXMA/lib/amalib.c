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

#include <sched.h>
#include "amalib.h"
#include <unistd.h>

/**
 * dec_res_chng_callback: Decoder resolution change callback function
 * properties
 *
 * @param frame_props: Pointer to new frame property
 * @param custom: Custom parameter
 * @return Fail/Pass
 */
int32_t dec_res_chng_callback(XmaFrameProperties* frame_props, void* custom) {
    frame_props = frame_props;
    custom = custom;
    return XMA_SUCCESS;
}

/**
 * InitCard: 1st initialization routine for any app
 * properties
 *
 * @param device: Target device number
 * @param devConf: Pointer to struct that holds device configuration
 * @return Fail/Pass
 */
uint8_t initCard(uint32_t device, DevConf *devConf) {

    XmaInitParameter xma_init_param;
    char m_app_name[] = APP_NAME;

    uint8_t ret = xma_log_init(XMA_EMERGENCY_LOG, XMA_LOG_TYPE_CONSOLE,
                       &devConf->logger ,APP_LOG_NAME);
    if(ret != XMA_SUCCESS) {
       printf("XMA log Initialization failed\n");
       return XMA_ERROR;
    }
    memset(&xma_init_param, 0, sizeof(XmaInitParameter));
    xma_init_param.app_name      = m_app_name;
    xma_init_param.device        = device;

    XmaParameter params[1];
    uint32_t api_version = XMA_API_VERSION_1_1;

    params[0].name = (char*)XMA_API_VERSION;
    params[0].type = XMA_UINT32;
    params[0].length = sizeof(uint32_t);
    params[0].value = &api_version;

    xma_init_param.params        = params;
    xma_init_param.param_cnt     = 1;

    if((ret = xma_initialize(devConf->logger, &xma_init_param, &devConf->handle)) != XMA_SUCCESS) {
        fprintf(stderr,"XMA Initialization failed\n");
        return XMA_ERROR;
    }
    return(XMA_SUCCESS);
}

/**
 * InitDec: Decoder initialization routine
 * properties
 *
 * @param handle: XMA handle
 * @param sw_format: Device side format
 * @param width, height, fps_num and fps_den: Expected parameters of the incoming stream
 * @param hwdecoder_type: Decoder type
 * @param decConf: Pointer to struct that holds decode related items
 * @param decDev: Device number to perform decoding on
 * @param decXmaParamConfCnt: Number of decode TLVs
 * @param decXmaParamConf: Decode TLVs
 * @return Fail/Pass
 */
uint8_t initDec(XmaHandle handle, XmaFormatType sw_format, int width, int height, int fps_num, int fps_den, XmaDecoderType hwdecoder_type, DecConf *decConf, int decDev, uint32_t decXmaParamConfCnt, XmaParameter* decXmaParamConf) {

    int32_t            planes, plane;

    decConf->xrm_props.width = width;
    decConf->xrm_props.height = height;
    decConf->xrm_props.fps_num = fps_num;
    decConf->xrm_props.fps_den = fps_den;
    decConf->dev_index = decDev;

    if (xrm_dec_reserve(&decConf->xrm_ctx, decConf->dev_index, &decConf->xrm_props) == XRM_ERROR) {
        return XMA_ERROR;
    }

    decConf->frame_props.format    = XMA_VPE_FMT_TYPE;
    decConf->frame_props.sw_format = sw_format;
    decConf->frame_props.width     = decConf->xrm_props.width;
    decConf->frame_props.height    = decConf->xrm_props.height;
    decConf->frame_props.bits_per_pixel = 8;
    planes = xma_frame_planes_get(handle, &decConf->frame_props);
    for(plane = 0; plane < planes; plane++) {
        decConf->frame_props.linesize[plane] = xma_frame_get_plane_stride(handle, &decConf->frame_props, plane);
    }

    decConf->xma_props.width = decConf->frame_props.width;
    decConf->xma_props.height = decConf->frame_props.height;
    decConf->xma_props.bits_per_pixel = decConf->frame_props.bits_per_pixel;
    decConf->xma_props.framerate.numerator = fps_num;
    decConf->xma_props.framerate.denominator = fps_den;
    decConf->xma_props.hwdecoder_type        = hwdecoder_type;
    decConf->xma_props.handle = handle;
    decConf->xma_props.param_cnt = decXmaParamConfCnt;
    decConf->xma_props.params = decXmaParamConf;
    decConf->session = xma_dec_session_create(&decConf->xma_props);

    if(!decConf->session) {
        fprintf(stderr, "Failed decoder session create\n");
        return XMA_ERROR;
    }

    return XMA_SUCCESS;
}

/**
 * InitDownload: Download filter initialization routine
 * properties
 *
 * @param handle: XMA handle
 * @param format: Host side format
 * @param sw_format: Device side format
 * @param width, height, bits_per_pixel, fps_num and fps_den: Expected parameters of the stream
 * @param downloadConf: Pointer to struct that holds download filter related items
 * @return Fail/Pass
 */
uint8_t initDownload(XmaHandle handle, XmaFormatType format, XmaFormatType sw_format, int width, int height, int32_t bits_per_pixel, int fps_num, int fps_den, DownloadConf *downloadConf) {

    int32_t            planes, plane;
    downloadConf->frame_props.format    = format;
    downloadConf->frame_props.sw_format = sw_format;
    downloadConf->frame_props.width     = width;
    downloadConf->frame_props.height    = height;
    downloadConf->frame_props.bits_per_pixel = bits_per_pixel;
    planes = xma_frame_planes_get(handle, &downloadConf->frame_props);
    for(plane = 0; plane < planes; plane++) {
        downloadConf->frame_props.linesize[plane] = xma_frame_get_plane_stride(handle, &downloadConf->frame_props, plane);
    }

    downloadConf->xma_props.hwfilter_type                = XMA_DOWNLOAD_FILTER_TYPE;
    downloadConf->xma_props.param_cnt                    = 0;
    downloadConf->xma_props.params                       = NULL;
    downloadConf->xma_props.handle                       = handle;
    downloadConf->xma_props.input.format                 = XMA_VPE_FMT_TYPE;
    downloadConf->xma_props.input.sw_format              = sw_format;
    downloadConf->xma_props.input.width                  = downloadConf->frame_props.width;
    downloadConf->xma_props.input.height                 = downloadConf->frame_props.height;
    downloadConf->xma_props.input.framerate.numerator    = fps_num;
    downloadConf->xma_props.input.framerate.denominator  = fps_den;
    downloadConf->xma_props.output.format                = format;
    downloadConf->xma_props.output.sw_format             = sw_format;
    downloadConf->xma_props.output.width                 = downloadConf->frame_props.width;
    downloadConf->xma_props.output.height                = downloadConf->frame_props.height;
    downloadConf->xma_props.output.framerate.numerator   = downloadConf->xma_props.input.framerate.numerator;
    downloadConf->xma_props.output.framerate.denominator = downloadConf->xma_props.input.framerate.denominator;
    downloadConf->session = xma_filter_session_create(&downloadConf->xma_props);
    if(!downloadConf->session) {
        fprintf(stderr, "Failed to create upload session\n");
        return XMA_ERROR;
    }

    return XMA_SUCCESS;
}

/**
 * InitUpload: Upload filter initialization routine
 * properties
 *
 * @param handle: XMA handle
 * @param format: Host side format
 * @param sw_format: Device side format
 * @param width, height, bits_per_pixel, fps_num and fps_den: Expected parameters of the stream
 * @param downloadConf: Pointer to struct that holds upload filter related items
 * @return Fail/Pass
 */
uint8_t initUpload(XmaHandle handle, XmaFormatType format, XmaFormatType sw_format, int width, int height, int32_t bits_per_pixel, int fps_num, int fps_den, UploadConf *uploadConf) {

    int32_t            planes, plane;
    uploadConf->frame_props.format    = format;
    uploadConf->frame_props.sw_format = sw_format;
    uploadConf->frame_props.width     = width;
    uploadConf->frame_props.height    = height;
    uploadConf->frame_props.bits_per_pixel = bits_per_pixel;
    planes = xma_frame_planes_get(handle, &uploadConf->frame_props);
    for(plane = 0; plane < planes; plane++) {
        uploadConf->frame_props.linesize[plane] = xma_frame_get_plane_stride(handle, &uploadConf->frame_props, plane);
    }

    uploadConf->xma_props.hwfilter_type                = XMA_UPLOAD_FILTER_TYPE;
    uploadConf->xma_props.param_cnt                    = 0;
    uploadConf->xma_props.params                       = NULL;
    uploadConf->xma_props.handle                       = handle;
    uploadConf->xma_props.input.format                 = format;
    uploadConf->xma_props.input.sw_format              = sw_format;
    uploadConf->xma_props.input.width                  = uploadConf->frame_props.width;
    uploadConf->xma_props.input.height                 = uploadConf->frame_props.height;
    uploadConf->xma_props.input.framerate.numerator    = fps_num;
    uploadConf->xma_props.input.framerate.denominator  = fps_den;
    uploadConf->xma_props.output.format                = XMA_VPE_FMT_TYPE;
    uploadConf->xma_props.output.sw_format             = sw_format;
    uploadConf->xma_props.output.width                 = uploadConf->frame_props.width;
    uploadConf->xma_props.output.height                = uploadConf->frame_props.height;
    uploadConf->xma_props.output.framerate.numerator   = uploadConf->xma_props.input.framerate.numerator;
    uploadConf->xma_props.output.framerate.denominator = uploadConf->xma_props.input.framerate.denominator;
    uploadConf->session = xma_filter_session_create(&uploadConf->xma_props);
    if(!uploadConf->session) {
        fprintf(stderr, "Failed to create upload session\n");
        return XMA_ERROR;
    }

    return XMA_SUCCESS;
}

/**
 * InitScaler: Scaler initialization routine
 * properties
 *
 * @param handle: XMA handle
 * @param sw_format: Device side format
 * @param width, height, bits_per_pixel and framerate: Expected parameters of the incoming stream
 * @param in_xrm_props: Pointer to input XRM properties
 * @param scalerConf: Pointer to struct that holds scaler related items
 * @param scalerDev: Device number to perform scaling operation
 * @param scalerXmaParamConfCnt: Number of scale TLVs
 * @param scalerXmaParamConf: Scale TLVs
 * @param scalerOutputConfCnt: Number of scaled outputs
 * @param scalerOutputConf: Pointer to scaled output configurations
 * @return Fail/Pass
 */
uint8_t initScaler(XmaHandle handle, XmaFormatType sw_format, int32_t width, int32_t height, int32_t bits_per_pixel, XmaFraction framerate, XrmInterfaceProperties *in_xrm_props, ScalerConf *scalerConf, int scalerDev, uint32_t scalerXmaParamConfCnt, XmaParameter* scalerXmaParamConf, int32_t scalerOutputConfCnt, XmaScalerInOutProperties *scalerOutputConf) {

    scalerConf->xma_props.num_outputs = scalerOutputConfCnt;
    for(int32_t i = 0; i < scalerConf->xma_props.num_outputs; i++) {
        scalerConf->frame_props[i].width = scalerOutputConf[i].width;
        scalerConf->frame_props[i].height = scalerOutputConf[i].height;
        scalerConf->frame_props[i].format = XMA_VPE_FMT_TYPE;
        scalerConf->frame_props[i].sw_format = sw_format;
        for(int plane_id = 0; plane_id < xma_frame_planes_get(handle, &scalerConf->frame_props[i]); plane_id++) {
            scalerConf->frame_props[i].linesize[plane_id] =
                    xma_frame_get_plane_stride(handle, &scalerConf->frame_props[i], plane_id);
        }
        scalerConf->xrm_props[i].width = scalerOutputConf[i].width;
        scalerConf->xrm_props[i].height = scalerOutputConf[i].height;
        scalerConf->xrm_props[i].fps_num = scalerOutputConf[i].framerate.numerator;
        scalerConf->xrm_props[i].fps_den = scalerOutputConf[i].framerate.denominator;
    }

    scalerConf->dev_index = scalerDev;
    scalerConf->xma_props.hwscaler_type = XMA_ABR_SCALER_TYPE;
    scalerConf->xma_props.handle = handle;
    scalerConf->xma_props.input.format = XMA_VPE_FMT_TYPE;
    scalerConf->xma_props.input.sw_format = sw_format;
    scalerConf->xma_props.input.width = width;
    scalerConf->xma_props.input.height = height;
    scalerConf->xma_props.input.bits_per_pixel = bits_per_pixel;
    scalerConf->xma_props.input.framerate.numerator   = framerate.numerator;
    scalerConf->xma_props.input.framerate.denominator = framerate.denominator;
    memcpy(&scalerConf->xma_props.output, scalerOutputConf, scalerOutputConfCnt*sizeof(XmaScalerInOutProperties));
    scalerConf->xma_props.param_cnt = scalerXmaParamConfCnt;
    scalerConf->xma_props.params = scalerXmaParamConf;

    if (xrm_scale_reserve(&scalerConf->xrm_ctx, scalerConf->dev_index, in_xrm_props, scalerConf->xrm_props, scalerConf->xma_props.num_outputs) == XRM_ERROR) {
        return XMA_ERROR;
    }
    scalerConf->session = xma_scaler_session_create(&scalerConf->xma_props);
    return XMA_SUCCESS;
}

/**
 * InitEnc: Encoder initialization routine
 * properties
 *
 * @param handle: XMA handle
 * @param sw_format: Device side format
 * @param encOutputCnt: Number of encode outputs
 * @param bits_per_pixel and framerate: Array of bpp
 * @param encConf: Pointer to struct that holds encode related items
 * @param encDev: Array of device numbers to perform encoding operation
 * @param encXrmConf: Pointer to encode XRM properties
 * @param encSlice: Array of encode slice
 * @param encXav1: Array of type-1 AV1 selection
 * @param encUll: Array of ULL selection
 * @param encCodec: Array of encoder type selection
 * @param encXmaParamConfCnt: Number of encode TLVs
 * @param encXmaParamConf: Encode TLVs
 * @param encRates: Array of encoding rates
 * @param encPreset: Array of encoding preset
 * @return Fail/Pass
 */
uint8_t initEnc(XmaHandle handle, XmaFormatType sw_format, uint32_t encOutputCnt, int32_t *bits_per_pixel, EncConf *encConf, uint8_t *encDev, XrmInterfaceProperties *encXrmConf, int8_t *encSlice, uint8_t *encXav1, uint8_t *encUll, XmaEncoderType *encCodec, uint32_t encXmaParamConfCnt, XmaParameter* encXmaParamConf, int32_t *encRates, uint8_t *encPreset) {

    int i, ret;
    encConf->dev_index = encDev;
    encConf->xrm_props = encXrmConf;
    encConf->slice = encSlice;
    encConf->is_xav1 = encXav1;
    encConf->is_ull_enabled = encUll;
    encConf->num_outputs = encOutputCnt;
    memset(encConf->xma_props, 0, encConf->num_outputs*sizeof(XmaEncoderProperties));

    for(i = 0; i < encConf->num_outputs ; i++) {
        encConf->xma_props[i].handle = handle;
        encConf->xma_props[i].hwencoder_type = encCodec[i];
        encConf->xma_props[i].param_cnt = encXmaParamConfCnt;
        encConf->xma_props[i].params = encXmaParamConf;
        encConf->xma_props[i].format = XMA_VPE_FMT_TYPE;
        encConf->xma_props[i].sw_format = sw_format;
        encConf->xma_props[i].bits_per_pixel = bits_per_pixel[i];
        encConf->xma_props[i].width = encConf->xrm_props[i].width;
        encConf->xma_props[i].height = encConf->xrm_props[i].height;
        encConf->xma_props[i].bitrate = encRates[i];
        encConf->xma_props[i].framerate.numerator = encConf->xrm_props[i].fps_num;
        encConf->xma_props[i].framerate.denominator = encConf->xrm_props[i].fps_den;
        encConf->xma_props[i].preset = encPreset[i];
        encConf->xma_props[i].level = 0;
        encConf->xma_props[i].qp = encConf->xma_props[i].gop_size = encConf->xma_props[i].lookahead_depth
                                 = encConf->xma_props[i].minQP = encConf->xma_props[i].maxQP = encConf->xma_props[i].rc_mode
                                 = encConf->xma_props[i].profile = -1;
        encConf->xma_props[i].temp_aq_gain = encConf->xma_props[i].spat_aq_gain = 255;

        ret = xrm_enc_reserve(&encConf->xrm_ctx[i], encConf->dev_index[i], encConf->slice[i], encConf->is_xav1[i], encConf->is_ull_enabled[i], &encConf->xrm_props[i]);
        if (ret == XRM_ERROR) {
            return XMA_ERROR;
        }
        if (!(encConf->session[i] = xma_enc_session_create(&encConf->xma_props[i]))) {
            printf("Could not create encoder session %d\n", i);
            return XMA_ERROR;
        }
    }
    return(XMA_SUCCESS);
}

/**
 * extractVCL: Simple AVC & HEVC NAL parser
 * properties
 *
 * @param xma_data_buffer: Received bit stream buffer
 * @param vcl_start: Index to beginning of VCL
 * @param vcl_end: Index to end of VCL
 * @param dataread: Size of received buffer
 * @param dataproc: Number of processed bytes
 * @param codecType: Decode stream type
 * @return Fail/Pass
 */
uint8_t extractVCL(uint8_t *xma_data_buffer, uint64_t *vcl_start, uint64_t *vcl_end, uint64_t dataread, uint64_t *dataproc, uint8_t codecType) {

    int vcl_end_flg=0, first_start_flg=0, vcl_start_flg=0;
    *vcl_start = 0;
    uint8_t startCode[] = {0,0,1};

    for( int i=0; i < READ_CHUNK - 2; i++ ) {
        *vcl_end = i;
        if (!memcmp(xma_data_buffer+i,startCode, sizeof(startCode))){
            if(!first_start_flg){
                *vcl_start = i;
                first_start_flg = 1;
            }
            if (!vcl_start_flg) {
                uint8_t nalutype;
                uint8_t vcl = false;
                if (codecType == XMA_H264_DECODER_TYPE) {
                    nalutype = xma_data_buffer[i + 3] & 0x1F;
                    vcl = ((1 <= nalutype) && (nalutype <= 5)) ? 1: 0;
                }else {
                    nalutype = (xma_data_buffer[i + 3] & 0x7E) >> 1;
                    vcl = ((((16 <= nalutype) && (nalutype <= 21)) ||  nalutype <=9) &&
                            xma_data_buffer[i + 5] >> 7) ? 1: 0;
                }
                if(vcl) {
                    vcl_start_flg = 1;
                }
            } else if (vcl_start_flg){
                vcl_end_flg = 1;
                break;
            }
        }
    }
    *dataproc += (*vcl_end - *vcl_start);
    if (*dataproc >= dataread) {
        return XMA_ERROR;
    }
    if (!vcl_end_flg) {
        return XMA_ERROR;
    }
    return XMA_SUCCESS;
}

/**
 * procDec: Decode Processing
 * properties
 *
 * @param decConf: Pointer to struct that holds decode related items
 * @param xma_data_buffer: Received bit stream buffer
 * @param vcl_start: Index to beginning of VCL
 * @param vcl_end: Index to end of VCL
 * @param eos: Boolean to signal EOS
 * @return Decoder send/receive status
 */
uint8_t procDec(DecConf *decConf, uint8_t *xma_data_buffer, uint64_t vcl_start, uint64_t vcl_end, bool eos) {

    int rc_snd = XMA_SUCCESS, rc_rcv = XMA_SUCCESS, data_used=0;

    int size = vcl_end - vcl_start;
    if (!eos) {
        if(decConf->xma_data){
            xma_data_buffer_free(decConf->xma_data);
        }

        decConf->xma_data = xma_data_buffer_alloc(decConf->xma_props.handle,size , false);

        if(!decConf->xma_data) {
            fprintf(stderr,"%s: xma_data_buffer_alloc failed\n", __FUNCTION__);
            return XMA_ERROR;
        }

        memcpy(decConf->xma_data->data.buffer, xma_data_buffer+vcl_start, size);
        decConf->xma_data->pts         = XMA_AV_NOPTS_VALUE;
        decConf->xma_data->alloc_size  = size;
        decConf->xma_data->is_eof      = 0;
        rc_snd = xma_dec_session_send_data(decConf->session, decConf->xma_data, &data_used);
    } else {
        rc_snd = xma_dec_session_send_data(decConf->session, NULL, 0);
    }

    if ( rc_snd == XMA_SUCCESS ) {
        if(decConf->out_frame){
            xma_frame_free(decConf->out_frame);
        }
        decConf->out_frame = xma_frame_alloc(decConf->xma_props.handle, &decConf->frame_props, true);
        if(!decConf->out_frame) {
            fprintf(stderr,"%s: xma_frame_alloc failed\n", __FUNCTION__);
            return XMA_ERROR;
        }
        rc_rcv = xma_dec_session_recv_frame(decConf->session, decConf->out_frame);
        if ( rc_rcv != XMA_SUCCESS ) {
            fprintf(stderr,"xma_dec_session_recv_frame returned status code %d\n", rc_rcv);
        }
        return rc_rcv;
    } else if( rc_snd != XMA_SUCCESS) {
        fprintf(stderr,"xma_dec_session_send_data returned status code %d\n",rc_snd);
    }

    return rc_snd;
}

/**
 * procDownload: Download filter Processing
 * properties
 *
 * @param downloadConf: Pointer to struct that holds download filter related items
 * @param out_frame: Output buffer to send out to host
 * @return Download filter send/receive status
 */
uint8_t procDownload(DownloadConf *downloadConf, XmaFrame *out_frame) {

    int rc = xma_filter_session_send_frame(downloadConf->session, out_frame);
    if(rc != XMA_SUCCESS) {
        fprintf(stderr,"xma_filter_session_send_frame returned status code %d\n", rc);
    } else if(rc == XMA_SUCCESS) {
        if(downloadConf->out_frame) {
            xma_frame_free(downloadConf->out_frame);
        }
        downloadConf->out_frame = xma_frame_alloc(downloadConf->xma_props.handle , &downloadConf->frame_props, true);
        rc   = xma_filter_session_recv_frame(downloadConf->session, downloadConf->out_frame);
        if(rc < 0) {
            fprintf(stderr,"xma_filter_session_send_frame returned status code %d\n", rc);
        } else if(rc != XMA_SUCCESS) {
            xma_frame_free(downloadConf->out_frame);
            downloadConf->out_frame = NULL;
        }
    }
    return rc;
}

/**
 * procUpload: Upload filter Processing
 * properties
 *
 * @param uploadConf: Pointer to struct that holds upload filter related items
 * @param in_frame: Input buffer to send out to device
 * @return Upload filter send/receive status
 */
uint8_t procUpload(UploadConf *uploadConf, XmaFrame *in_frame) {
    int rc;
    uint32_t counter = 0;
    do {
        rc = xma_filter_session_send_frame(uploadConf->session, in_frame);
    } while ((rc == XMA_TRY_AGAIN) && (counter++ < UPLOAD_NO_TRY));
    if(rc != XMA_SUCCESS) {
        fprintf(stderr,"%s: xma_filter_session_send_frame returned status code %d\n", __FUNCTION__, rc);
    } else if(rc == XMA_SUCCESS) {
        if(uploadConf->out_frame) {
            xma_frame_free(uploadConf->out_frame);
        }
        uploadConf->out_frame = xma_frame_alloc(uploadConf->xma_props.handle , &uploadConf->frame_props, true);
        if (!uploadConf->out_frame) {
            fprintf(stderr,"%s: xma_frame_alloc failed\n", __FUNCTION__);
            return XMA_ERROR;
        }
        counter = 0;
        do {
            rc   = xma_filter_session_recv_frame(uploadConf->session, uploadConf->out_frame);
            if (rc == XMA_TRY_AGAIN) {
                usleep(MA35_SLEEP);
            }
        } while ((rc == XMA_TRY_AGAIN) && (counter++ < UPLOAD_NO_TRY));
        if(rc != XMA_SUCCESS) {
            fprintf(stderr,"%s: session_recv_frame_list returned status code %d\n", __FUNCTION__, rc);
            xma_frame_free(uploadConf->out_frame);
            uploadConf->out_frame = NULL;
        }
    }
    return rc;
}

/**
 * procScaler: Scaler Processing
 * properties
 *
 * @param uploadConf: Pointer to struct that holds upload filter related items
 * @param in_frame: Input buffer to send out to device
 * @return Upload filter send/receive status
 */
uint8_t procScaler(ScalerConf *scalerConf, XmaFrame* in_frame) {

    int32_t rc = xma_scaler_session_send_frame(scalerConf->session, in_frame);

    if (rc != XMA_ERROR && rc !=XMA_SEND_MORE_DATA) {
        uint32_t counter = 0;
        for(int i = 0; i < scalerConf->xma_props.num_outputs; i++) {
            if(scalerConf->out_frame[i]){
                xma_frame_free(scalerConf->out_frame[i]);
            }
            scalerConf->out_frame[i] = xma_frame_alloc(scalerConf->xma_props.handle, &scalerConf->frame_props[i], true);
            if(!scalerConf->out_frame[i]) {
                fprintf(stderr,"%s: xma_frame_alloc failed on %d ch.\n", __FUNCTION__, i);
                continue;
            }
        }
        do{
            rc = xma_scaler_session_recv_frame_list(scalerConf->session, scalerConf->out_frame);
            if (rc == XMA_TRY_AGAIN) {
                usleep(MA35_SLEEP);
            }
        }while ((rc == XMA_TRY_AGAIN) && (counter++ < SCALER_NO_TRY));

        if ( rc != XMA_SUCCESS ) {
            fprintf(stderr,"session_recv_frame_list returned status code %d\n", rc);
        }
    } else {
        fprintf(stderr,"session_send_data failed with error %d\n", rc);
    }
    return(rc);
}

/**
 * procEnc: Encode Processing
 * properties
 *
 * @param encConf: Pointer to struct that holds encode related items
 * @param out_frame: Array of pointers to raw input buffers
 * @return Encoder send/receive status
 */
uint8_t procEnc(EncConf *encConf, XmaFrame *out_frame[]) {

    int i, snd_ret, rcv_ret=XMA_ERROR, error_flg=0;
    for(i=0; i < encConf->num_outputs; i++) {
        int j=0;
        encConf->enc_out_size[i] = 0;
        do {
            usleep(MA35_SLEEP);
            snd_ret = xma_enc_session_send_frame(encConf->session[i], out_frame[i]);
            if(snd_ret ==  XMA_ERROR || snd_ret == XMA_SEND_MORE_DATA) {
                fprintf(stderr,"xma_enc_session_send_frame returned status code %d for %d ch.\n", snd_ret, i);
                break;
            }
            if(encConf->xma_buffer[i]){
                xma_data_buffer_free( encConf->xma_buffer[i]);
            }
            encConf->xma_buffer[i] = xma_data_buffer_alloc(encConf->xma_props[i].handle, 0, true);
            if(!encConf->xma_buffer[i]){
                fprintf(stderr,"%s: xma_data_buffer_alloc failed on %d ch.\n", __FUNCTION__, i);
                error_flg=1;
                break;
            }
            rcv_ret = xma_enc_session_recv_data(encConf->session[i], encConf->xma_buffer[i], &encConf->enc_out_size[i]);
            if(rcv_ret != XMA_SUCCESS) {
              fprintf(stderr,"xma_enc_session_recv_data return status code %d for ch. %d\n", rcv_ret, i);
              error_flg=1;
            } else if (rcv_ret == XMA_SUCCESS) {
                if (!encConf->enc_out_size[i]) {
                    fprintf(stderr,"No encoded data for ch. %d\n", i);
                    error_flg=1;
                }
            }
            j++;
        }while(snd_ret == XMA_TRY_AGAIN && rcv_ret == XMA_RESEND_AND_RECV && j < ENC_NO_TRY);
        if (j>= ENC_NO_TRY){
            fprintf(stderr, "Iterated over %d to encode ch. %d\n", ENC_NO_TRY, i);
            error_flg=1;
        }
    }
    if(error_flg) {
        return(rcv_ret); //Cheesy...
    }
    return(XMA_SUCCESS);
}

