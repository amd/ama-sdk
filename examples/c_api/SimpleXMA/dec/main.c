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
        sprintf(fileName,"%s_%d.yuv",argv[OUT_FILE], 0);
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
 * @param downloadConf: Pointer to struct that holds download filter configuration
 * @return Fail/Pass
 */
void procClose(IOConf *ioConf, DevConf *devConf, DecConf *decConf, DownloadConf *downloadConf){


    fsync(ioConf->out_file[0]);
    close(ioConf->out_file[0]);

    close(ioConf->in_file);

    xma_data_buffer_free(decConf->xma_data);
    xma_frame_free(decConf->out_frame);
    xrm_dec_release(&decConf->xrm_ctx);
    xma_dec_session_destroy(decConf->session);

    xma_frame_free(downloadConf->out_frame);
    xma_filter_session_destroy(downloadConf->session);

    xma_release(devConf->handle);
    xma_log_release(devConf->logger);
}

/**
 * writeOut: Application specific routine to write to output descriptors
 * properties
 *
 * @param ioConf: Pointer to struct that holds IO configuration
 * @param downloadConf:  Pointer to struct that holds download filter configuration
 * @return Fail/Pass
 */
void writeOut(IOConf *ioConf, DownloadConf *downloadConf){

    uint8_t* y_buf = downloadConf->out_frame->data[0].host;
    int w = downloadConf->frame_props.width;
    for(int h=0; h < downloadConf->frame_props.height; h++) {
        int no_wrote=0;
        do {
            no_wrote += write(ioConf->out_file[0], y_buf+no_wrote, w-no_wrote);
        }while(no_wrote < w);
        y_buf += downloadConf->frame_props.linesize[0];
    }

    uint8_t* u_buf = downloadConf->out_frame->data[1].host;
    w = downloadConf->frame_props.width >> 1;
    for(int h=0; h < downloadConf->frame_props.height >> 1; h++) {
        int no_wrote=0;
        do {
            no_wrote += write(ioConf->out_file[0], u_buf+no_wrote, w-no_wrote);
        }while(no_wrote < w);
        u_buf += downloadConf->frame_props.linesize[1];
    }

    uint8_t* v_buf = downloadConf->out_frame->data[2].host;
    for(int h=0; h < downloadConf->frame_props.height >> 1; h++) {
        int no_wrote=0;
        do {
            no_wrote += write(ioConf->out_file[0], v_buf+no_wrote, w-no_wrote);
        }while(no_wrote < w);
        v_buf += downloadConf->frame_props.linesize[2];
    }
    fsync(ioConf->out_file[0]);
}

/**
 * Decode: Application specific routine to perform decoding
 * properties
 *
 * @param ioConf: Pointer to struct that holds IO configuration
 * @param decConf: Pointer to struct that holds decode configuration
 * @param downloadConf:  Pointer to struct that holds download filter configuration
 * @return Fail/Pass
 */
uint8_t Decode(IOConf *ioConf, DecConf *decConf, DownloadConf *downloadConf) {

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
        if (procDownload(downloadConf, decConf->out_frame) != XMA_SUCCESS) {
            continue;
        }

        writeOut(ioConf, downloadConf);
        itr++;
    }while(true);

    fprintf(stderr,"Flushing decoder\n");
    eos = true;
    while(true){
        ret = procDec(decConf, xma_data_buffer, vlc_start, vlc_end, eos);
        if ( ret == XMA_EOS || ret <= XMA_ERROR) {
            break;
        }

        if ((ret = procDownload(downloadConf, decConf->out_frame)) <= XMA_ERROR) {
            fprintf(stderr, "Downloader status: %d\n", ret);
        }
        writeOut(ioConf, downloadConf);
        itr++;
    }

    fprintf(stderr,"Flushing downloader\n");
    while(true){
        ret = procDownload(downloadConf, NULL);
        if ( ret == XMA_SUCCESS || ret == XMA_EOS) {
            break;
        }
        writeOut(ioConf, downloadConf);
        itr++;
    }

    fprintf(stderr,"Iterated %d times.\n", itr);
    return(XMA_SUCCESS);
}

/**
 * Sample application to perform decoding
 *
 */
int main(int argc, char *argv[]){

    IOConf ioConf;
    DevConf devConf;
    DecConf decConf;
    DownloadConf downloadConf;

    uint8_t ret;
    if (argc != 8 ){
        printf("xma-decoder-simple_0.0.0 input-clip input_type(1:AVC, 2:HEVC) output-prefix input-height input-width input-fps num input-fps-den\n");
        return -1;
    }
    ret  = initInput(argv, &ioConf);
    ret |= initCard(DEV_IDX, &devConf);
    ret |= initDec(devConf.handle, atoi(argv[WIDTH]),atoi(argv[HEIGHT]), atoi(argv[FPS_NUM]), atoi(argv[FPS_DEN]), atoi(argv[FORMAT]), &decConf, gDecDev, sizeof(gDecXmaParamConf)/sizeof(XmaParameter), gDecXmaParamConf);
    ret |= initDownload(devConf.handle, atoi(argv[WIDTH]),atoi(argv[HEIGHT]), 8, atoi(argv[FPS_NUM]), atoi(argv[FPS_DEN]), &downloadConf);
    ret |= Decode(&ioConf, &decConf, &downloadConf);
    fprintf(stderr,"Releasing resources\n");
    procClose(&ioConf, &devConf, &decConf, &downloadConf);
    printf("ret=%d\n", ret);
}

