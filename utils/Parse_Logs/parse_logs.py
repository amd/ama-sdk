#/*****************************************************************************
# Copyright (C) 2023 - 2024, Xilinx Inc - All rights reserved
#
# Licensed under the Apache License, Version 2.0 (the "License"). You may
# not use this file except in compliance with the License. A copy of the
# License is located at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.
#*****************************************************************************/
import re
import argparse
import logging
import json
from collections import defaultdict
from statistics import mean

from pprint import pprint

# Anatomy of a performance log line
# 2023.08.17 04-05-58-525 [INFO ] [0x7fcb273c3880] AMA.VPI.ENC.h264_ama@0x55f2e649cc00:{70839} PerfEnd [452,1692259558525040037] ENCODER
#
# 1. 2023.08.17 04-05-58-525                --> Human readable timestamp (Optional)
# 2. [INFO]                                 --> log_type (Required)
# 3. [0x7fcb273c3880]                       --> Thread id (Optional)
# 4. AMA.VPI.ENC.h264_ama@0x55f2e649cc00:   --> Instance name (Required)
#        format= AMA.<LAYER>.<ACCELERATOR>.<UNIQUE_INSTANCE_NAME>
# 5. PerfBeg/PerfEnd                        --> Event type (Required)
# 6. [452,1692259558525040037]              --> frame_num, timestamp (Required)
# 4. ENCODER                                --> User given event tag (Required)

def parse_log_instance(instance_name):
    accel_regex = re.compile(r"AMA\.(?P<layer_type>\w+)\.(?P<accel_type>\w+)\.(?P<unique_inst>.*)")
    m = accel_regex.match(instance_name)
    if m:
        logging.debug("\t Accelerator {} found in log_instance {}".format(m.group('accel_type'),
                                                                 instance_name))
        logging.debug("\t Unique inst  {} found in log_instance {}".format(m.group('unique_inst'),
                                                                 instance_name))
        unique_inst_name = re.sub(r'\.\d+\.\d+$', '', m.group('unique_inst'))
        return m.group('accel_type'), unique_inst_name
    else:
        logging.debug("\t No Accelerator found in {} returning GEN".format(instance_name))
        logging.debug("\t No Unique inst found in {} returning UNDEFINED".format(instance_name))
        return "GEN", "UNDEFINED"

def get_line_data(groups):
    frm = int(groups.group('frame_num'))
    log_inst = groups.group('log_instance')

    accel, inst = parse_log_instance(log_inst)

    # Special handling for IP's where some logs
    # are on a different thread
    if accel == "DEC" and ".dec_thread" in inst:
        inst = ".".join(inst.split(".")[:-1])

    if accel == "ENC" and ".event_thread" in inst:
        inst = ".".join(inst.split(".")[:-1])

    tag = groups.group('event_tag')
    event = groups.group('event_type')
    timestamp = int(groups.group('timestamp_ns'))

    logging.debug("\t Framenum     = {}".format(frm))
    logging.debug("\t Accelerator  = {}".format(accel))
    logging.debug("\t Instance     = {}".format(inst))
    logging.debug("\t User tag     = {}".format(tag))
    logging.debug("\t Event type   = {}".format(event))
    logging.debug("\t Timestamp    = {}".format(timestamp))

    metadata = {}
    global pid

    if groups.group('pid'):
        metadata['pid'] = groups.group('pid')
        logging.debug("\t pid          = {}".format(groups.group('pid')))
        if(pid == 0):
            pid = int(groups.group('pid'))
        elif(pid != int(groups.group('pid'))):
            print("ERROR: more multiple process id's (pid) found!")
            exit()
    if groups.group('tid'):
        metadata['tid'] = groups.group('tid')
        logging.debug("\t tid          = {}".format(groups.group('tid')))
    if groups.group('timestamp_hr'):
        metadata['timestamp_hr'] = groups.group('timestamp_hr')
        logging.debug("\t Timestamp_hr = {}".format(groups.group('timestamp_hr')))



    return accel, inst, tag, event, (frm, timestamp, metadata)

def add_dict_to(data_dict, key):
    if key not in data_dict:
        data_dict[key] = {}
    return data_dict[key]

def parse_log_file(args):
    filename = args.log_file
    perf_regex = re.compile(r"(?P<timestamp_hr>\d+\.\d+\.\d+\s\d+-\d+-\d+-\d+)?\s?"
                            r"\[?(?P<log_type>[A-Z]+\s*)\]?\s"
                            r"\[?(?P<tid>0x\w+)?\]?\s?"
                            r"(?P<log_instance>AMA\..*):"
                            r"\{?(?P<pid>\d+)?\}?\s?"
                            r"(?P<event_type>Perf\w+)\s"
                            r"\[(?P<frame_num>\d+),(?P<timestamp_ns>\d+)\]\s"
                            r"(?P<event_tag>.*)$")
    
    #events = {}
    data = {}

    with open(filename, "r") as fp:
        for line_no, line in enumerate(fp):
            m = perf_regex.match(line)
            if m:
                logging.debug("line no {} matched".format(line_no))
                accel, inst, tag, event, (frm, timestamp, metadata) = get_line_data(m)
                if int(frm) < args.frame_start:
                    continue
                if int(frm) > args.frame_end:
                    continue
                accel_data = add_dict_to(data, accel)
                inst_data = add_dict_to(accel_data, inst)
                tag_data = add_dict_to(inst_data, tag)
                if event not in tag_data:
                    tag_data[event] = []

                metadata['line_no'] = line_no
                metadata['original_line'] = line.strip()
                tag_data[event].append((frm, timestamp, metadata))
            else:
                logging.debug("line no {} did not match".format(line_no))

    return data


#Work In progress !!
def to_excel(data):
    try:
        import pandas as pd
    except ModuleNotFoundError:
        print("Pandas not installed. Writing to excel will not work")
        print("Try running pip3 install pandas ")
        return
    
    dfs = {}
    for accel in data:
        for inst in data[accel]:
            for tag in data[accel][inst]:
                df = pd.DataFrame(data[accel][inst][tag])
                #print(df.to_string(sparsify=False))
                #print("\n\n\n")
                dfs[str(inst)+"-"+str(tag)] = df.T

    df = pd.concat(dfs)
    df.T.to_excel('dataframe.xlsx')

def get_frame_data(event_info):
    frame_data = defaultdict(list)
    for frm, ts, _ in event_info:
        frame_data[frm].append(ts)
    return frame_data

def get_latency(s_data, e_data):
    diffs = [e_data[frm][0] - s_data[frm][0] for frm in e_data.keys() if frm in s_data]
    return mean(diffs)/1000000.0, min(diffs)/1000000.0, max(diffs)/1000000.0


def print_latency(data,
                s_accel, s_inst, s_tag, s_event,
                e_accel, e_inst, e_tag, e_event, log_prefix):
    s_data = get_frame_data(data[s_accel][s_inst][s_tag][s_event])
    e_data = get_frame_data(data[e_accel][e_inst][e_tag][e_event])
    lat, min, max = get_latency(s_data, e_data)

    if(s_accel == "ENC" and s_tag == "ENCODER"):
        print("{} latency (avg) = {:.3f} ms (APPLICATION LEVEL) [min = {:.3f} ms, max = {:.3f} ms]".format(log_prefix, lat, min, max))
    else:
        print("{} latency (avg) = {:.3f} ms [min = {:.3f} ms, max = {:.3f} ms]".format(log_prefix, lat, min, max))
    print("\t ({}@{}-{} --> {}@{}-{})\n".format(s_event, s_inst, s_tag,
                                                e_event, e_inst, e_tag))

# Compute latencies for all event tags present under an accelerator.
def compute_accel_latency(data, accel):
    for idx, inst in enumerate(data[accel]):
        if accel != 'ABR':
            for tag in data[accel][inst]:
                print_latency(data, 
                    accel, inst, tag, 'PerfBeg',
                    accel, inst, tag, 'PerfEnd',
                    "{}_{}.{}".format(accel, idx, tag))
        else:
            # Special handling for XABR as the only Single IN
            # Multi-Out Ip Assuming output tag names are in format
            # <basename>-<channelnuminfo> eg. XABR-CH0
            base_tags = set([tg.split('-')[0] for tg in data[accel][inst].keys()])
            for bt in base_tags:
                for tag in data[accel][inst]:
                    if bt != tag.split('-')[0]:
                        continue
                    if bt == tag:
                        continue
                    print_latency(data,
                        accel, inst, bt, 'PerfBeg',
                        accel, inst, tag, 'PerfEnd',
                        "{}_{}.{}".format(accel, idx, tag))

# If Only one decoder is present compute enc to end latency
# TODO: Figure out behaviour for >1 decoders
# Assumption Event tag for decoder is 'DECODER'
#            Event tag for encoder is 'ENCODER' 
def compute_e2e_latency(data):
    if 'DEC' not in data:
        logging.warning("No perf logs from decoder can't compute e2e_latency")
        return
    if 'ENC' not in data:
        logging.warning("No perf logs from encoder can't compute e2e_latency")
        return
    num_decoders = len(data['DEC'].keys())
    if num_decoders != 1:
        print(">>>> num decoders are {}".format(num_decoders) + ". Printing End2End latency for all:")
    
    for dec_inst in data['DEC']:
        for idx, enc_inst in enumerate(data['ENC']):
            print_latency(data,
                'DEC', dec_inst, 'DECODER', 'PerfBeg',
                'ENC', enc_inst, 'ENCODER', 'PerfEnd',
                "End2End_CH{}".format(idx+1))

def compute_postdecode_latency(data):
    if 'DEC' not in data:
        logging.warning("No perf logs from decoder can't compute post decode latency")
        return
    if 'ENC' not in data:
        logging.warning("No perf logs from encoder can't compute post decode latency")
        return

    num_decoders = len(data['DEC'].keys())
    if num_decoders != 1:
        print(">>>> num decoders are {}".format(num_decoders) + ". Printing PostDecode latency for all:")

    for idx, enc_inst in enumerate(data['ENC']):
        for dec_inst in data['DEC']:
            print_latency(data,
                'DEC', dec_inst, 'DECODER', 'PerfEnd',
                'ENC', enc_inst, 'ENCODER', 'PerfEnd',
                "PostDecode_CH{}".format(idx+1))

def get_frame_trace_data(event_info):
    frame_data = defaultdict(list)
    for frm, ts, md in event_info:
        frame_data[frm].append((ts, md))
    return frame_data

def write_trace_events(data,
                s_accel, s_inst, s_tag, s_event,
                e_accel, e_inst, e_tag, e_event, fp):
    s_data = get_frame_trace_data(data[s_accel][s_inst][s_tag][s_event])
    e_data = get_frame_trace_data(data[e_accel][e_inst][e_tag][e_event])

    events = [( 
        {
        'cat'     : s_accel,
        'name'    : "{}_{}-{}".format(frm, s_inst, s_tag), #Better name ??
        'ph'      : "B", 
        'ts'      : s_data[frm][0][0]//1000,
        'pid'     : frm//1000,
        'tid'     : 1000000 + frm%1000,
        'args'    :{
                     'begin_line':"{} : {}".format(s_data[frm][0][1]['line_no'],
                                                    s_data[frm][0][1]['original_line']),
                     'end_line':"{} : {}".format(e_data[frm][0][1]['line_no'],
                                                  e_data[frm][0][1]['original_line'])
                    }
        },
        {
        'cat'     : s_accel,
        'name'    : "{}_{}-{}".format(frm, s_inst, s_tag),  #Better name ??
        'ph'      : "E", 
        'ts'      : e_data[frm][0][0]//1000,
        'pid'     : frm//1000,
        'tid'     : 1000000 + frm%1000,
        'args'    :{
                     'begin_line':"{} : {}".format(s_data[frm][0][1]['line_no'],
                                                    s_data[frm][0][1]['original_line']),
                     'end_line':"{} : {}".format(e_data[frm][0][1]['line_no'],
                                                  e_data[frm][0][1]['original_line'])
                    }
        }
        ) for frm in sorted(e_data.keys()) if frm in s_data]
    
    for s_event, e_event in events:
        json.dump(s_event, fp)
        fp.write(",\n")
        json.dump(e_event, fp)
        fp.write(",\n")



def to_chrome_trace(data, trace_filename):
    with open(trace_filename,"w") as fp:
        fp.write("[\n")
        for accel in data:
            for idx, inst in enumerate(data[accel]):
                if accel != 'ABR':
                    for tag in data[accel][inst]:
                        write_trace_events(data, 
                            accel, inst, tag, 'PerfBeg',
                            accel, inst, tag, 'PerfEnd',
                            fp)
                else:
                    # Special handling for XABR as the only Single IN
                    # Multi-Out Ip Assuming output tag names are in format
                    # <basename>-<channelnuminfo> eg. XABR-CH0
                    base_tags = set([tg.split('-')[0] for tg in data[accel][inst].keys()])
                    for bt in base_tags:
                        for tag in data[accel][inst]:
                            if bt != tag.split('-')[0]:
                                continue
                            if bt == tag:
                                continue
                            write_trace_events(data,
                                accel, inst, bt, 'PerfBeg',
                                accel, inst, tag, 'PerfEnd',
                                fp)
        fp.write("]")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("log_file", 
            help="AMA Log File containing performance logs",
            nargs='?',
            type=str, default=None)
    parser.add_argument("--log_level",
            help="logging level (used for development)",
            choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'],
            type=str,
            default='INFO')
    parser.add_argument("--dump_json",
            help="dump the parsed data to a json file",
            type=str,
            default=None)
    parser.add_argument("--json_cache",
            help="Load parsed data from json file",
            type=str,
            default=None)
    parser.add_argument("--dump_trace",
            help="dump parsed data in chrome tracing format (open using ui.perfetto.dev)",
            type=str,
            default=None)
    parser.add_argument("--frame_start",
            help="Start frame number for latency calculation",
            type=int,
            default=0)
    parser.add_argument("--frame_end",
            help="End frame number for latency calculation",
            type=int,
            default=999999999)
    
    args = parser.parse_args()
    logging.basicConfig(filename="latency_parsing.log",
            filemode="a",
            format='%(asctime)s [%(levelname)s] %(message)s',
            level=args.log_level)


    if args.json_cache and args.log_file:
        print("Log file and json cache both specified using cache\n")

    logging.info("===========================================================")
    pid = 0 #check for duplicate pid
    if args.json_cache:
        logging.info("Input cache file :: {}".format(args.json_cache))
        data = json.load(open(args.json_cache))
    elif args.log_file:
        logging.info("Input file :: {}".format(args.log_file))
        data = parse_log_file(args)
    else:
        print("ERROR either log_file or --json_cache file needed")
        exit()

    if args.dump_json:
        print("Dumping parsed data as json to {}".format(args.dump_json))
        with open(args.dump_json, 'w') as fp:
            json.dump(data, fp, indent=4)


    if len(data.keys()) == 0:
        print("No perf logs found in file {}".format(args.log_file))
        exit()

    print("\n============================ INSTANCE INFO START ===============================\n")
    for accel in data:
        print(accel + " :: ")
        for idx, inst in enumerate(data[accel]):
            print("\t{}. {}".format(idx, inst))
            for tag in data[accel][inst]:
                events = ", ".join(data[accel][inst][tag].keys())
                print("\t\t{} {{ {} }}".format(tag, events))
        print("------------------------------------------------------------\n\n")
    print("\n============================= INSTANCE INFO END =================================\n")

    for accel in data:
        compute_accel_latency(data, accel)
    compute_postdecode_latency(data)
    compute_e2e_latency(data)

    if(args.dump_trace):
        print("\nWriting Chrome trace file to {}\n".format(args.dump_trace))
        to_chrome_trace(data, args.dump_trace)
