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
#include <cstring>
#include <iostream>
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
        sprintf(fileName,"%s_%d.h264",argv[OUT_FILE], 0);
        ioConf->out_file[0] = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR |S_IWUSR | S_IRGRP | S_IROTH);
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
 * @param encConf:  Pointer to struct that holds encode configuration
 * @param uploadConf: Pointer to struct that holds upload filter configuration
 * @return Fail/Pass
 */
void procClose(IOConf *ioConf, DevConf *devConf, EncConf *encConf, UploadConf *uploadConf){

    fsync(ioConf->out_file[0]);
    close(ioConf->out_file[0]);

    close(ioConf->in_file);
    xma_data_buffer_free(encConf->xma_buffer[0]);
    xma_enc_session_destroy(encConf->session[0]);
    xrm_enc_release(&encConf->xrm_ctx[0]);

    xma_frame_free(uploadConf->out_frame);
    xma_filter_session_destroy(uploadConf->session);

    xma_release(devConf->handle);
    xma_log_release(devConf->logger);
}

/**
 * readIn: Application specific routine to read raw data
 * properties
 *
 * @param ioConf: Pointer to struct that holds IO configuration
 * @param devConf: Pointer to address of struct that holds the input frame
 * @param uploadConf: Pointer to struct that holds upload filter configuration
 * @return Fail/Pass
 */
uint8_t readIn(IOConf *ioConf, XmaFrame **in_frame, UploadConf *uploadConf) {

    int no_read=0;
    if(*in_frame){
        xma_frame_free(*in_frame);
    }
    *in_frame = xma_frame_alloc(uploadConf->xma_props.handle, &uploadConf->frame_props, false);
    uint8_t* y_buf = (*in_frame)->data[0].host;
    int w = uploadConf->frame_props.width;
    for(int h=0; h < uploadConf->frame_props.height; h++) {
        no_read = read(ioConf->in_file, y_buf, w);
        if (no_read < w) {
            return no_read;
        }
        y_buf += uploadConf->frame_props.linesize[0];
    }

    uint8_t* u_buf = (*in_frame)->data[1].host;
    w = uploadConf->frame_props.width >> 1;
    for(int h=0; h < uploadConf->frame_props.height >> 1; h++) {
        no_read = read(ioConf->in_file, u_buf, w);
        if (no_read < w) {
            return no_read;
        }
        u_buf += uploadConf->frame_props.linesize[1];
    }

    uint8_t* v_buf = (*in_frame)->data[2].host;
    for(int h=0; h < uploadConf->frame_props.height >> 1; h++) {
        no_read = read(ioConf->in_file, v_buf, w);
        if (no_read < w) {
            return no_read;
        }
        v_buf += uploadConf->frame_props.linesize[2];
    }
    return no_read;
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
                no_wrote += write(ioConf->out_file[i], static_cast<uint8_t *>(encConf->xma_buffer[i]->data.buffer)+no_wrote, encConf->enc_out_size[i]-no_wrote);
                //fprintf(stderr,"%d: Wrote %d/%d bytes for %d ch.\n", itr,ret,encConf->enc_out_size[i], i);
            }while(no_wrote < encConf->enc_out_size[i]);
        }
    }
}

/**
 * Encode: Application specific routine to perform encode operation
 * properties
 *
 * @param ioConf: Pointer to struct that holds IO configuration
 * @param encConf:  Pointer to struct that holds encode configuration
 * @param uploadConf: Pointer to struct that holds upload filter configuration
 * @return Fail/Pass
 */
uint8_t Encode(IOConf *ioConf, EncConf *encConf, UploadConf *uploadConf) {

    int8_t ret;
    int itr=0;
    XmaFrame *in_frame=NULL;

    do{
        if (!readIn(ioConf, &in_frame, uploadConf)) {
            break;
        }

        if (procUpload(uploadConf, in_frame)!= XMA_SUCCESS) {
            continue;
        }

        if ((ret = procEnc(encConf, &uploadConf->out_frame)) <= XMA_ERROR) {
            fprintf(stderr, "Encoder status: %d\n", ret);
        }

        writeOut(ioConf, encConf);
        itr++;
    }while(true);

    fprintf(stderr,"Flushing uploader\n");

    while(true){
        ret = procUpload(uploadConf, NULL);
        if ( ret == XMA_SUCCESS || ret == XMA_EOS) {
            break;
        }
        if ((ret = procEnc(encConf, &uploadConf->out_frame)) <= XMA_ERROR) {
            fprintf(stderr, "Encoder status: %d\n", ret);
        }

        writeOut(ioConf, encConf);
        itr++;
    }

    fprintf(stderr,"Flushing encoder\n");
    XmaFrame *out_frame[MAX_CH_OUTPUTS]={};
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
 * Sample C++ application to perform encode operation
 *
 */
int main(int argc, char *argv[]){

    IOConf ioConf;
    DevConf devConf;
    EncConf encConf;
    UploadConf uploadConf;

    uint8_t ret;
    if (argc != 3 ){
        printf("xma-cpp-encoder-simple input-clip output-prefix\n");
        return -1;
    }
    std::cout << "C++ Encoder \n";
    ret  = initInput(argv, &ioConf);
    ret |= initCard(DEV_IDX, &devConf);
    ret |= initUpload(devConf.handle, gEncXrmConf[0].width, gEncXrmConf[0].height, gBitsPerPixel[0], gEncXrmConf[0].fps_num, gEncXrmConf[0].fps_den, &uploadConf);
    ret |= initEnc(devConf.handle, uploadConf.xma_props.output.format, uploadConf.xma_props.output.sw_format, sizeof(gBitsPerPixel)/sizeof(uint32_t), gBitsPerPixel, &encConf, gEncDev, gEncXrmConf, gEncSlice, gEncXav1, gEncUll, gEncCodec, sizeof(gEncXmaParamConf)/sizeof(XmaParameter), gEncXmaParamConf, gEncRates, gEncPreset);
    ret |= Encode(&ioConf, &encConf, &uploadConf);
    fprintf(stderr,"Releasing resources\n");
    procClose(&ioConf, &devConf, &encConf, &uploadConf);
    printf("ret=%d\n", ret);
}

