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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>
#include "amaconf.h"
#include "amalib.h"

/**
 * initInput: Application specific routine to prepare input and output descriptors
 * properties
 *
 * @param argv: Input parameters
 * @param ioConf:  Pointer to struct that holds IO configuration
 * @return Fail/Pass
 */
uint8_t initInput(char *argv[], IOConf *ioConf) {
    char fileName[128];
    if ( strncmp ( argv[IN_FILE],"-", 1) == 0 ){
        ioConf->in_file = STDIN_FILENO;
     }else{
        ioConf->in_file = open(argv[IN_FILE], O_RDONLY);
     }

    if ( strncmp ( argv[OUT_FILE],"-", 1) == 0 ){
        ioConf->out_file[0] = STDOUT_FILENO;
     }else{
         int num_outputs = sizeof(gScalerOutputConf)/sizeof(XmaScalerInOutProperties);
         for (int i=0; i < num_outputs; i++){
             sprintf(fileName,"%s_%d.h264",argv[OUT_FILE],i);
             ioConf->out_file[i] = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR |S_IWUSR | S_IRGRP | S_IROTH);
         }
     }

    return(XMA_SUCCESS);
}

/**
 * procClose: Application specific routine to cleanup
 * properties
 *
 * @param ioConf: Pointer to struct that holds IO configuration
 * @param devConf: Pointer to struct that holds device configuration
 * @param decConf: Pointer to struct that holds decode configuration
 * @param scalerConf: Pointer to struct that holds scaler configuration
 * @param encConf:  Pointer to struct that holds encode configuration
 * @return Fail/Pass
 */
void procClose(IOConf *ioConf, DevConf *devConf, DecConf *decConf, ScalerConf *scalerConf, EncConf *encConf){

    for (int i=0; i < scalerConf->xma_props.num_outputs; i++){
        fsync(ioConf->out_file[i]);
        close(ioConf->out_file[i]);
    }
    close(ioConf->in_file);
    xma_data_buffer_free(decConf->xma_data);
    xma_frame_free(decConf->out_frame);
    xrm_dec_release(&decConf->xrm_ctx);
    xma_dec_session_destroy(decConf->session);
    for(int i = 0; i < scalerConf->xma_props.num_outputs; i++) {
        xma_frame_free(scalerConf->out_frame[i]);
        xma_data_buffer_free(encConf->xma_buffer[i]);
        xma_enc_session_destroy(encConf->session[i]);
        xrm_enc_release(&encConf->xrm_ctx[i]);
    }
    xma_scaler_session_destroy(scalerConf->session);
    xrm_scale_release(&scalerConf->xrm_ctx);
    xma_release(devConf->handle);
    xma_log_release(devConf->logger);
}

/**
 * writeOut: Application specific routine to write to output descriptors
 * properties
 *
 * @param ioConf: Pointer to struct that holds IO configuration
 * @param encConf:  Pointer to struct that holds encode configuration
 * @return Fail/Pass
 */
void writeOut(IOConf *ioConf, EncConf *encConf){
    for(int i=0; i< encConf->num_outputs; i++){
        if(encConf->xma_buffer[i] && encConf->enc_out_size[i]) {
            int no_wrote=0;
            do {
                no_wrote += write(ioConf->out_file[i], encConf->xma_buffer[i]->data.buffer+no_wrote, encConf->enc_out_size[i]-no_wrote);
                //fprintf(stderr,"%d: Wrote %d/%d bytes for %d ch.\n", itr,ret,encConf->enc_out_size[i], i);
            }while(no_wrote < encConf->enc_out_size[i]);
        }
    }
}

/**
 * procTXC: Application specific routine to perform transcoding
 * properties
 *
 * @param ioConf: Pointer to struct that holds IO configuration
 * @param decConf: Pointer to struct that holds decode configuration
 * @param scalerConf: Pointer to struct that holds scaler configuration
 * @param encConf:  Pointer to struct that holds encode configuration
 * @return Fail/Pass
 */
uint8_t procTXC(IOConf *ioConf, DecConf *decConf, ScalerConf *scalerConf, EncConf *encConf) {

    uint64_t vlc_end = READ_CHUNK, vlc_start=0;
    uint8_t xma_data_buffer[READ_CHUNK];
    int8_t ret;
    int itr = 0;
    bool eos = false;
    uint64_t dataread=0, dataproc=0;

    do{
        int tail = READ_CHUNK - vlc_end;
        memmove(xma_data_buffer, xma_data_buffer + vlc_end, tail);
        dataread += read(ioConf->in_file, xma_data_buffer + tail, vlc_end);

        if (dataproc >= dataread) {
            break;
        }

        if (extractVCL(xma_data_buffer, &vlc_start, &vlc_end, dataread, &dataproc, decConf->xma_props.hwdecoder_type) != XMA_SUCCESS) {
            continue;
        }
        if (procDec(decConf, xma_data_buffer, vlc_start, vlc_end, eos) != XMA_SUCCESS) {
            continue;
        }
        if (procScaler(scalerConf, decConf->out_frame) != XMA_SUCCESS) {
            continue;
        }
        if ((ret = procEnc(encConf, scalerConf->out_frame)) != XMA_SUCCESS) {
            fprintf(stderr, "Encoder status: %d\n", ret);
        }
        writeOut(ioConf, encConf);
        itr++;
    }while(true);

    fprintf(stderr,"Flushing decoder\n");
    eos = true;
    while(true){
        ret = procDec(decConf, xma_data_buffer, vlc_start, vlc_end, eos);
        if ( ret == XMA_EOS || ret <= XMA_ERROR) {
            break;
        }
        if (procScaler(scalerConf, decConf->out_frame) != XMA_SUCCESS) {
            continue;
        }
        if ((ret = procEnc(encConf, scalerConf->out_frame)) <= XMA_ERROR) {
            fprintf(stderr, "Encoder status: %d\n", ret);
        }
        writeOut(ioConf, encConf);
        itr++;
    }

    fprintf(stderr,"Flushing scaler\n");
    while(true){
        ret = procScaler(scalerConf, NULL);
        if ( ret == XMA_SUCCESS || ret == XMA_EOS) {
            break;
        }
        if ((ret = procEnc(encConf, scalerConf->out_frame)) <= XMA_ERROR) {
            fprintf(stderr, "Encoder status: %d\n", ret);
        }
        writeOut(ioConf, encConf);
        itr++;
    }

    fprintf(stderr,"Flushing encoder\n");
    XmaFrame *out_frame[MAX_CH_OUTPUTS];
    while(true){
        if ((ret = procEnc(encConf, out_frame)) <= XMA_ERROR) {
            fprintf(stderr, "Encoder status: %d\n", ret);
        }
        writeOut(ioConf, encConf);
        if ( ret == XMA_EOS) {
            break;
        }
        itr++;
    }
    fprintf(stderr,"Iterated %d times.\n", itr);
    return(XMA_SUCCESS);
}

/**
 * Sample application to perform transcoding
 *
 */
int main(int argc, char *argv[]){

    IOConf ioConf;
    DevConf devConf;
    DecConf decConf;
    ScalerConf scalerConf;
    EncConf encConf;

    uint8_t ret;
    if (argc != 6 ){
        printf("xma-transcoder-simple_0.0.0 input-clip input_type(1:AVC, 2:HEVC) output-prefix input-height input-width\n");
        return -1;
    }
    ret  = initInput(argv, &ioConf);
    ret |= initCard(DEV_IDX, &devConf);
    ret |= initDec(devConf.handle, atoi(argv[WIDTH]),atoi(argv[HEIGHT]), FPS_NUM, FPS_DEN, atoi(argv[FORMAT]), &decConf, gDecDev, sizeof(gDecXmaParamConf)/sizeof(XmaParameter), gDecXmaParamConf);
    ret |= initScaler(devConf.handle, decConf.frame_props.format, decConf.frame_props.sw_format, decConf.frame_props.width, decConf.frame_props.height, decConf.frame_props.bits_per_pixel, decConf.xma_props.framerate, &decConf.xrm_props, &scalerConf, gScalerDev, sizeof(gScalerXmaParamConf)/sizeof(XmaParameter), gScalerXmaParamConf, sizeof(gScalerOutputConf)/sizeof(XmaScalerInOutProperties), gScalerOutputConf);
    ret |= initEnc(devConf.handle, scalerConf.xma_props.input.format, scalerConf.xma_props.input.sw_format, sizeof(gBitsPerPixel)/sizeof(int32_t), gBitsPerPixel, &encConf, gEncDev, gEncXrmConf, gEncSlice, gEncXav1, gEncUll, gEncCodec, sizeof(gEncXmaParamConf)/sizeof(XmaParameter), gEncXmaParamConf, gEncRates, gEncPreset);
    ret |= procTXC(&ioConf, &decConf, &scalerConf, &encConf);
    fprintf(stderr,"Releasing resources\n");
    procClose(&ioConf, &devConf, &decConf, &scalerConf, &encConf);
    printf("ret=%d\n", ret);
}
