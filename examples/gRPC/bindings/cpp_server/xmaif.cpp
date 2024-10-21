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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/impl/service_type.h>

#include <thread>
#include <map>

#include "xmaif.grpc.pb.h"
#include "amalib.h"

using namespace std;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using grpc::Service;

using xmaif::Device;
using xmaif::DevId;
using xmaif::DevOut;
using xmaif::MA35DStatus;
using xmaif::NoArgs;

using xmaif::Decoder;
using xmaif::DecArg;
using xmaif::DecOut;
using xmaif::VclIn;
using xmaif::VclOut;

using xmaif::Download;
using xmaif::DownloadArg;
using xmaif::DownloadLineSize;
using xmaif::DownloadIn;
using xmaif::DownloadOut;

using xmaif::Upload;
using xmaif::UploadArg;
using xmaif::UploadLineSize;
using xmaif::UploadIn;
using xmaif::UploadOut;

using xmaif::Encoder;
using xmaif::EncArg;
using xmaif::EncIn;
using xmaif::EncOut;

#define MAX_EXPERT_OPTION_SIZE 2048
#define MAX_XMA_NAME_SIZE 128
#define PIXEL_FORMAT XMA_YUV420P_FMT_TYPE


// The following classes are stub implementation of services defined in xma interface proto file.

class DeviceImpl final : public Device::Service {
 public:
    DevConf devConf;

  Status Init(ServerContext* context, const DevId* dev_id, DevOut *dev_out) override {
      auto status = initCard(dev_id->device_id(), &devConf);
      if (status != XMA_SUCCESS){
          return Status::CANCELLED;
      }
      uint64_t ptr_device = reinterpret_cast<decltype(ptr_device)>(&devConf);
      dev_out->set_ptr_dev_conf(ptr_device);
      return Status::OK;
  };

  Status Close(ServerContext* context, const NoArgs *no_args, MA35DStatus *status) override  {
      xma_release(devConf.handle);
      xma_log_release(devConf.logger);
      status->set_status(XMA_SUCCESS);
      return Status::OK;
  };
};

struct cmp_str
{
   bool operator()(char const *a, char const *b) const
   {
      return std::strcmp(a, b) < 0;
   }
};

class DecoderImpl final : public Decoder::Service {
 public:
  DecConf decConf;
  uint64_t dataproc;

  explicit DecoderImpl(): dataproc(0) {}

  Status Init(ServerContext* context, const DecArg *dec_arg, NoArgs *no_args) override{
      DevConf* ptr_devConf = reinterpret_cast<decltype(ptr_devConf)>(dec_arg->ptr_dev_conf());
      void* func_ptr = reinterpret_cast<decltype(func_ptr)>(dec_res_chng_callback);
      uint32_t defaultDecXmaParamValue[] = {0,0,XMA_YUV420P_FMT_TYPE};

      XmaParameter defaultDecXmaParamConf[] = {
              {.name=(char *)"low_latency", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&defaultDecXmaParamValue[0]},
              {.name=(char *)"prop_change_callback", .type=XMA_FUNC_PTR, .length=sizeof(void*), .value=func_ptr},
              {.name=(char *)"latency_logging", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&defaultDecXmaParamValue[1]},
              {.name=(char *)"out_fmt", .type=XMA_INT32, .length=sizeof(uint32_t), .value=&defaultDecXmaParamValue[2]}
      };
      std::map<char*, pair<XmaParameter*, int>, cmp_str> mapXmaParameter = {
              {defaultDecXmaParamConf[0].name, {&defaultDecXmaParamConf[0], 0}},
              {defaultDecXmaParamConf[1].name, {&defaultDecXmaParamConf[1],-1}},
              {defaultDecXmaParamConf[2].name, {&defaultDecXmaParamConf[2], 1}},
              {defaultDecXmaParamConf[3].name, {&defaultDecXmaParamConf[3], 2}}
      };

      for (int i=0; i < dec_arg->dec_xma_param_conf().size(); i++) {
          char* key = const_cast<char*>(dec_arg->dec_xma_param_conf(i).name().c_str());
          auto idx = mapXmaParameter.find(key);
          if(idx != mapXmaParameter.end()) {
              auto value = dec_arg->dec_xma_param_conf(i).value();
              defaultDecXmaParamValue[idx->second.second] = value;
          }
      }

      auto status = initDec(ptr_devConf->handle, PIXEL_FORMAT, dec_arg->width(),dec_arg->height(), dec_arg->fps_num(), dec_arg->fps_den(),
              (XmaDecoderType)dec_arg->hwdecoder_type(), &decConf, dec_arg->decode_id(), 4, defaultDecXmaParamConf);
      if ( status != XMA_SUCCESS ) {
          return Status::CANCELLED;
      }
      return Status::OK;
  };

  Status ExtractVCL(ServerContext* context, const VclIn *vcl_in, VclOut *vcl_out) override {

      uint8_t *xma_data_buffer = reinterpret_cast<decltype(xma_data_buffer)>(const_cast<char *>((vcl_in->xma_data_buffer().data())));
      uint64_t vcl_end, vcl_start;
      vcl_out->set_cont(false);

      if (extractVCL(xma_data_buffer, &vcl_start, &vcl_end, vcl_in->dataread(), &dataproc,
                     decConf.xma_props.hwdecoder_type) != XMA_SUCCESS) {
          vcl_out->set_cont(true);
      }
      vcl_out->set_vcl_start(vcl_start);
      vcl_out->set_vcl_end(vcl_end);
      vcl_out->set_dataproc(dataproc);
      return Status::OK;
  }

  Status Proc(ServerContext* context, const VclOut *vcl_out, DecOut* dec_out) override {
      uint8_t *xma_data_buffer = reinterpret_cast<decltype(xma_data_buffer)>(const_cast<char *>((vcl_out->xma_data_buffer().data())));
      uint8_t ret;
      dec_out->set_cont(false);
      dec_out->set_flush(false);
      ret =  procDec(&decConf, xma_data_buffer, vcl_out->vcl_start(), vcl_out->vcl_end(), vcl_out->flush());
      if (!vcl_out->flush()) {
          if (ret != XMA_SUCCESS) {
              dec_out->set_cont(true);
          }
      } else {
          if (ret == XMA_EOS || ret <= XMA_ERROR){
              dec_out->set_flush(true);
          }
      }
      uint64_t ptr_out_frame = reinterpret_cast<decltype(ptr_out_frame)>(decConf.out_frame);
      dec_out->set_ptr_out_frame(ptr_out_frame);
      return Status::OK;
  }

  Status Close(ServerContext* context, const NoArgs* no_args, MA35DStatus *status) override {
      xma_data_buffer_free(decConf.xma_data);
      xma_frame_free(decConf.out_frame);
      xrm_dec_release(&decConf.xrm_ctx);
      xma_dec_session_destroy(decConf.session);
      status->set_status(XMA_SUCCESS);
      return Status::OK;
  };
};

class DownloadImpl final : public Download::Service {
 public:
  DownloadConf downloadConf;

  Status Init(ServerContext* context, const DownloadArg *download_arg, DownloadLineSize *download_linesize) override {
      DevConf* ptr_devConf = reinterpret_cast<decltype(ptr_devConf)>(download_arg->ptr_dev_conf());
      auto status = initDownload(ptr_devConf->handle, PIXEL_FORMAT, PIXEL_FORMAT, download_arg->width(),download_arg->height(), download_arg->bits_per_pixel(), download_arg->fps_num(), download_arg->fps_den(), &downloadConf);
     if ( status != XMA_SUCCESS ) {
         return Status::CANCELLED;
     }
     download_linesize->set_p0(downloadConf.frame_props.linesize[0]);
     download_linesize->set_p1(downloadConf.frame_props.linesize[1]);
     download_linesize->set_p2(downloadConf.frame_props.linesize[2]);
     return Status::OK;
  };

  Status Proc(ServerContext* context, const DownloadIn* download_in, DownloadOut *download_out) override {
      XmaFrame *out_frame;
      uint8_t ret;
      if ( download_in->flush() ) {
          out_frame = NULL;
      } else {
          out_frame = reinterpret_cast<decltype(out_frame)>(download_in->ptr_out_frame());
      }
      download_out->set_cont(false);
      download_out->set_flush(false);
      ret = procDownload(&downloadConf, out_frame);
      if ( ret != XMA_SUCCESS) {
          download_out->set_cont(true);
      }
      if ( ret == XMA_EOS || ret <= XMA_ERROR) {
          download_out->set_flush(true);
          return Status::OK;
      }
      std::string ptr_out_frame_host_p0(reinterpret_cast<const char *>(downloadConf.out_frame->data[0].host), downloadConf.frame_props.linesize[0]*downloadConf.frame_props.height);
      std::string ptr_out_frame_host_p1(reinterpret_cast<const char *>(downloadConf.out_frame->data[1].host), downloadConf.frame_props.linesize[1]*downloadConf.frame_props.height>>1);
      std::string ptr_out_frame_host_p2(reinterpret_cast<const char *>(downloadConf.out_frame->data[2].host), downloadConf.frame_props.linesize[2]*downloadConf.frame_props.height>>1);
      download_out->set_ptr_out_frame_host_p0(ptr_out_frame_host_p0);
      download_out->set_ptr_out_frame_host_p1(ptr_out_frame_host_p1);
      download_out->set_ptr_out_frame_host_p2(ptr_out_frame_host_p2);
      return Status::OK;
  };

  Status Close(ServerContext* context, const NoArgs* no_args, MA35DStatus *status) override {
      xma_frame_free(downloadConf.out_frame);
      xma_filter_session_destroy(downloadConf.session);
      status->set_status(XMA_SUCCESS);
      return Status::OK;
  };
 };

class UploadImpl final : public Upload::Service {
 public:
  UploadConf uploadConf;
  XmaFrame *in_frame;
  XmaFrameData data;

  explicit UploadImpl(): in_frame(NULL) {}

  Status Init(ServerContext* context, const UploadArg *upload_arg, UploadLineSize *upload_linesize) override {
      DevConf* ptr_devConf = reinterpret_cast<decltype(ptr_devConf)>(upload_arg->ptr_dev_conf());
      auto status = initUpload(ptr_devConf->handle, PIXEL_FORMAT, PIXEL_FORMAT, upload_arg->width(),upload_arg->height(), upload_arg->bits_per_pixel(), upload_arg->fps_num(), upload_arg->fps_den(), &uploadConf);
     if ( status != XMA_SUCCESS ) {
         return Status::CANCELLED;
     }

     upload_linesize->set_p0(uploadConf.frame_props.linesize[0]);
     upload_linesize->set_p1(uploadConf.frame_props.linesize[1]);
     upload_linesize->set_p2(uploadConf.frame_props.linesize[2]);

     data.data[0] = (uint8_t *)calloc(uploadConf.frame_props.linesize[0]*upload_arg->height(),1);
     data.data[1] = (uint8_t *)calloc(uploadConf.frame_props.linesize[1]*upload_arg->height()>>1,1);
     data.data[2] = (uint8_t *)calloc(uploadConf.frame_props.linesize[2]*upload_arg->height()>>1,1);

     return Status::OK;
  };


  Status Proc(ServerContext* context, const UploadIn *upload_in, UploadOut *upload_out) override {
      XmaFrame *out_frame;
      uint8_t ret;
      uint8_t *d0, *d1, *d2;

      if ( upload_in->flush() ) {
          in_frame = NULL;
      } else {
          if(in_frame){
              xma_frame_free(in_frame);
          }

          d0 = reinterpret_cast<uint8_t *>(const_cast<UploadIn *>(upload_in)->mutable_ptr_out_frame_host_p0()->data());
          d1 = reinterpret_cast<uint8_t *>(const_cast<UploadIn *>(upload_in)->mutable_ptr_out_frame_host_p1()->data());
          d2 = reinterpret_cast<uint8_t *>(const_cast<UploadIn *>(upload_in)->mutable_ptr_out_frame_host_p2()->data());

          /*data.data[0] = d0;
          data.data[1] = d1;
          data.data[2] = d2;*/
          memcpy(data.data[0], d0, upload_in->ptr_out_frame_host_p0().size());
          memcpy(data.data[1], d1, upload_in->ptr_out_frame_host_p1().size());
          memcpy(data.data[2], d2, upload_in->ptr_out_frame_host_p2().size());

           in_frame = xma_frame_from_buffers_clone(uploadConf.xma_props.handle, &uploadConf.frame_props, &data, NULL, NULL);
           xma_frame_inc_ref(in_frame);
      }

      upload_out->set_cont(false);
      upload_out->set_flush(false);
      ret = procUpload(&uploadConf, in_frame);
      if ( ret != XMA_SUCCESS) {
          upload_out->set_cont(true);
      }
      if ( ret == XMA_EOS || ret <= XMA_ERROR) {
          upload_out->set_flush(true);
          return Status::OK;
      }
      uint64_t ptr_out_frame = reinterpret_cast<decltype(ptr_out_frame)>(uploadConf.out_frame);
      upload_out->set_ptr_out_frame(ptr_out_frame);
      return Status::OK;
  };

  Status Close(ServerContext* context, const NoArgs* no_args, MA35DStatus *status) override {
      if (data.data[0]) {
          free(data.data[0]);
          data.data[0] = NULL;
          free(data.data[1]);
          data.data[1] = NULL;
          free(data.data[2]);
          data.data[2] = NULL;
      }
      xma_frame_free(uploadConf.out_frame);
      xma_filter_session_destroy(uploadConf.session);
      status->set_status(XMA_SUCCESS);
      return Status::OK;
  };
 };

class EncoderImpl final : public Encoder::Service {
 public:
  EncConf encConf;

  Status Init(ServerContext* context, const EncArg *enc_arg, NoArgs *no_args) override {
      DevConf* ptr_devConf = reinterpret_cast<decltype(ptr_devConf)>(enc_arg->ptr_dev_conf());
      int32_t bits_per_pixel[enc_arg->enc_output_cnt()];
      uint8_t enc_dev[enc_arg->enc_output_cnt()];
      int8_t enc_slice[enc_arg->enc_output_cnt()];
      uint8_t enc_xav1[enc_arg->enc_output_cnt()];
      uint8_t enc_ull[enc_arg->enc_output_cnt()];
      XmaEncoderType enc_codec[enc_arg->enc_output_cnt()];
      int32_t enc_rate[enc_arg->enc_output_cnt()];
      uint8_t enc_preset[enc_arg->enc_output_cnt()];
      XrmInterfaceProperties enc_xrm_conf[enc_arg->enc_xrm_conf().size()];
      XmaParameter enc_xma_param_conf[enc_arg->enc_xma_param_conf().size()];
      char enc_xma_param_conf_name[enc_arg->enc_xma_param_conf().size()][MAX_XMA_NAME_SIZE];
      uint32_t enc_xma_param_conf_value[enc_arg->enc_xma_param_conf().size()];
      char enc_xma_param_conf_expert_option_val[MAX_EXPERT_OPTION_SIZE];

      for (int i=0; i < enc_arg->enc_output_cnt(); i++){
          bits_per_pixel[i] = enc_arg->bits_per_pixel(i);
          enc_dev[i] = enc_arg->enc_dev(i);
          enc_slice[i] = enc_arg->enc_slice(i);
          enc_xav1[i] = enc_arg->enc_xav1(i);
          enc_ull[i] = enc_arg->enc_ull(i);
          enc_codec[i] = static_cast<XmaEncoderType>(enc_arg->enc_codec(i));
          enc_rate[i] = enc_arg->enc_rate(i);
          enc_preset[i] = enc_arg->enc_preset(i);
      }
      for (int i=0; i < enc_arg->enc_xrm_conf().size(); i++){
          enc_xrm_conf[i].width = enc_arg->enc_xrm_conf(i).width();
          enc_xrm_conf[i].height = enc_arg->enc_xrm_conf(i).height();
          enc_xrm_conf[i].fps_num = enc_arg->enc_xrm_conf(i).fps_num();
          enc_xrm_conf[i].fps_den = enc_arg->enc_xrm_conf(i).fps_den();
          enc_xrm_conf[i].is_la_enabled = enc_arg->enc_xrm_conf(i).is_la_enabled();
          enc_xrm_conf[i].enc_cores = enc_arg->enc_xrm_conf(i).enc_cores();
          strncpy(enc_xrm_conf[i].preset, const_cast<char*>(enc_arg->enc_xrm_conf(i).preset().c_str()), 16);
      }
      for (int i=0; i < enc_arg->enc_xma_param_conf().size(); i++){
          enc_xma_param_conf[i].name = enc_xma_param_conf_name[i];
          strncpy(enc_xma_param_conf[i].name, const_cast<char*>(enc_arg->enc_xma_param_conf(i).name().c_str()), enc_arg->enc_xma_param_conf(i).name().size());
          if (!strcmp("expert_options", enc_xma_param_conf[i].name)){
              enc_xma_param_conf[i].value = enc_xma_param_conf_expert_option_val;
              enc_xma_param_conf[i].type = XMA_STRING;
              enc_xma_param_conf[i].length = MAX_EXPERT_OPTION_SIZE;
              strncpy(static_cast<char *>(enc_xma_param_conf[i].value), enc_arg->enc_xma_param_conf(i).expert_value().c_str(),
                      enc_arg->enc_xma_param_conf(i).expert_value().size());
          } else {
              enc_xma_param_conf[i].value = &enc_xma_param_conf_value[i];
              enc_xma_param_conf[i].type = XMA_INT32;
              enc_xma_param_conf[i].length = sizeof(uint32_t);
              enc_xma_param_conf_value[i] = enc_arg->enc_xma_param_conf(i).value();
          }
      }
      auto status = initEnc(ptr_devConf->handle, PIXEL_FORMAT,
                            enc_arg->enc_output_cnt(), bits_per_pixel,
                            &encConf, enc_dev, enc_xrm_conf, enc_slice,
                            enc_xav1, enc_ull, enc_codec, enc_arg->enc_xma_param_conf().size(),
                            enc_xma_param_conf, enc_rate, enc_preset);
     if ( status != XMA_SUCCESS ) {
         return Status::CANCELLED;
     }
     return Status::OK;
  };

  Status Proc(ServerContext* context, const EncIn *enc_in, EncOut *enc_out) override {
      XmaFrame *out_frame[MAX_CH_OUTPUTS]={};
      uint8_t ret;

      if ( ! enc_in->flush() ) {
          for (int i=0; i < enc_in->ptr_out_frames().size(); i++) {
              out_frame[i] = reinterpret_cast<XmaFrame *>(enc_in->ptr_out_frames()[i]);
          }
      }
      enc_out->set_cont(false);
      enc_out->set_flush(false);
      if ((ret = procEnc(&encConf, out_frame)) <= XMA_ERROR) {
          fprintf(stderr, "Encoder status: %d\n", ret);
      } else if ( ret == XMA_SEND_MORE_DATA ) {
          enc_out->set_cont(true);
      } else if ( ret == XMA_EOS ) {
          enc_out->set_flush(true);
      } else if (ret == XMA_SUCCESS) {
          for (int i=0; i< enc_in->ptr_out_frames().size(); i++) {
              if (encConf.enc_out_size[i]) {
                  std::string xma_data_buffers(reinterpret_cast<const char *>(encConf.xma_buffer[i]->data.buffer), encConf.enc_out_size[i]);
                  enc_out->add_xma_data_buffers(xma_data_buffers);
              }
          }
      }
      return Status::OK;
  };

  Status Close(ServerContext* context, const NoArgs* no_args, MA35DStatus *status) override {
      xma_data_buffer_free(encConf.xma_buffer[0]);
      xma_enc_session_destroy(encConf.session[0]);
      xrm_enc_release(&encConf.xrm_ctx[0]);
      status->set_status(XMA_SUCCESS);
      return Status::OK;
  };
 };


void RunServer(std::string server_address, Service *service, std::string service_name) {

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << service_name << " server listening on " << server_address << std::endl;
  server->Wait();
}


int main(int argc, char** argv) {
  std::string server_address = argv[1];
  DeviceImpl device_service;
  DecoderImpl decoder_service;
  DownloadImpl download_service;
  UploadImpl upload_service;
  EncoderImpl encoder_service;
  std::vector<std::thread> servers;

  servers.push_back(std::thread(RunServer, server_address+":50051", &device_service, "Device"));
  servers.push_back(std::thread(RunServer, server_address+":50052", &decoder_service, "Decoder"));
  servers.push_back(std::thread(RunServer, server_address+":50053", &download_service, "Download"));
  servers.push_back(std::thread(RunServer, server_address+":50054", &upload_service, "Upload"));
  servers.push_back(std::thread(RunServer, server_address+":50055", &encoder_service, "Encoder"));
  for (auto& server : servers) server.join();

  return 0;
}
